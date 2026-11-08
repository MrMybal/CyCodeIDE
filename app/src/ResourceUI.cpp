#include "cycode/Application.h"
#include "cycode/UiFonts.h"
#include "cycode/FileIcons.h"
#include <algorithm>
#include <cyui/Core.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
namespace cycode {
std::string Application::tabTitle(const std::shared_ptr<Tab> &tab) {
    if (tab->windowKey.empty()) {
        auto key = tab->path;
        while (std::any_of(tabs_.begin(), tabs_.end(),
                           [&](const auto &other) { return other != tab && other->windowKey == key; }))
            key = tab->path + ":tab:" + std::to_string(nextTabWindow_++);
        tab->windowKey = key;
    }
    auto title = documentTitle(tab->path);
    return title.substr(0, title.find("###")) + "###document:" + tab->windowKey;
}
void Application::resourceTabPath(const std::shared_ptr<Tab> &tab, const std::string &path) {
    if (tab->path == path)
        return;
    (void)tabTitle(tab); // Preserve a detached window's identity before changing its visible title.
    const auto oldPath = tab->path;
    sessionDraftTabs_.erase(oldPath);
    for (auto *history : {&historyBack_, &historyForward_})
        for (auto &position : *history) if (position.path == oldPath) position.path = path;
    for (auto *items : {&recentFiles_, &closedFiles_})
        for (auto &item : *items) if (item == oldPath) item = path;
    for (auto *current : {&active_, &focusTab_, &selectedResource_, &revealPath_, &pendingClose_})
        if (*current == oldPath)
            *current = path;
    tab->path = path;
    tab->displayPath = isUntitledDocument(path) ? untitledDocumentName(path) : path;
    tab->snippet.cancel();
    tab->queuedCompletion = nullptr;
    ++tab->completionGeneration;
    tab->completionSession.clear();
    tab->highlightRevision = 0;
    tab->highlightCount = 0;
    if (!tab->image) {
        tab->editor.SetAutoCompleteSuggestions({});
        configureEditor(tab);
    }
}
void Application::resourceEvent(const Json &event) {
    const std::string from = event.at("from"), to = event.at("to");
    if (!from.empty() && !to.empty()) {
        for (auto &tab : tabs_)
            if (resourceContains(from, tab->path))
                resourceTabPath(tab, resourceRebase(tab->path, from, to));
        for (auto *path : {&active_, &focusTab_, &revealPath_, &selectedResource_, &pendingClose_})
            *path = resourceRebase(*path, from, to);
        for (auto *history : {&historyBack_, &historyForward_})
            for (auto &position : *history)
                position.path = resourceRebase(position.path, from, to);
        for (auto *paths : {&recentFiles_, &closedFiles_})
            for (auto &path : *paths)
                path = resourceRebase(path, from, to);
        resourceFolder_ = resourceRebase(resourceFolder_, from, to);
    } else if (event.at("removed").get<bool>() && resourceContains(from, resourceFolder_)) {
        resourceFolder_ = pathUtf8(utf8Path(from).parent_path());
        if (resourceFolder_.empty())
            resourceFolder_ = ".";
    }
    for(const auto &tab:tabs_)if(tab->image&&(resourceContains(from,tab->path)||resourceContains(to,tab->path)))reloadImage(tab);
    for (const auto &warning : event.value("warnings", Json::array()))
        error(warning.get<std::string>());
    resourceItems_ = Json::object();
    resourceTrash_ = Json::object();
    lastResourceRefresh_ = -10;
    navigation_ = Json::object();
    symbolResults_ = Json::object();
    refreshFiles(false);
    refreshGit();
}
void Application::resourceTask(const std::string &method, Json params) {
    if (resourceFuture_.valid())
        return;
    resourceError_.clear();
    resourceMethod_ = method;
    auto engine = engine_;
    if (method == "resources.children" || method == "resources.trash")
        lastResourceRefresh_ = ImGui::GetTime();
    resourceFuture_ =
        std::async(std::launch::async, [engine, method, params] { return engine->dispatch(method, params); });
}
void Application::beginResource(const std::string &operation, const std::string &path,
                                const std::string &destination) {
    if (resourceFuture_.valid())
        return;
    resourceOperation_ = operation;
    resourceSource_ = path;
    resourceDestination_ = destination;
    resourcePlan_ = Json::object();
    resourceError_.clear();
    resourceModal_ = true;
    showPanel("Files", "resources");
    if ((operation == "delete" && !path.empty()) || (operation == "restore" && !resourceTrashId_.empty())) {
        Json params = {{"operation", operation}};
        if (operation == "restore")
            params["trash"] = resourceTrashId_;
        else
            params["path"] = path;
        resourceTask("resources.preview", params);
    }
}
void Application::updateResources() {
    if (resourceFuture_.valid() &&
        resourceFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            const auto result = resourceFuture_.get();
            if (resourceMethod_ == "resources.preview")
                resourcePlan_ = result;
            else if (resourceMethod_ == "resources.children")
                resourceItems_ = result;
            else if (resourceMethod_ == "resources.trash")
                resourceTrash_ = result;
            else {
                resourceLast_ = result;
                resourcePlan_ = Json::object();
                resourceModal_ = false;
                if (result.at("operation") == "createFile")
                    open(result.at("destination"));
                cyui::notify(tr("File operation completed"), cyui::Tone::success);
            }
        } catch (const std::exception &e) {
            resourceError_ = e.what();
            error(e.what());
        }
    }
    // The event query never waits for resource hashing or filesystem mutations.
    const auto events = engine_->dispatch("resources.events", {{"after", lastResourceRevision_}});
    if (events.at("gap").get<bool>()) {
        historyBack_.clear();
        historyForward_.clear();
        recentFiles_.clear();
        closedFiles_.clear();
        cyui::notify(tr("File history was truncated; open documents are reconciled by identity."),
                     cyui::Tone::warning, 8);
    }
    for (const auto &event : events.at("events"))
        resourceEvent(event);
    lastResourceRevision_ = events.at("revision");
}
void Application::resourceMenu(const std::string &path, bool folder) {
    selectedResource_ = path;
    cyui::ScopedDisabled disabled(resourceFuture_.valid());
    const auto parent = folder ? path : pathUtf8(utf8Path(path).parent_path());
    const auto prefix = parent.empty() ? std::string{} : parent + "/";
    if (ImGui::MenuItem(tr("New file...")))
        beginResource("createFile", prefix);
    if (ImGui::MenuItem(tr("New folder...")))
        beginResource("createDirectory", prefix);
    ImGui::Separator();
    if (ImGui::MenuItem(tr("Rename or move..."), "F2"))
        beginResource("move", path, path);
    if (ImGui::MenuItem(tr("Move to project trash..."), "Delete"))
        beginResource("delete", path);
    if (ImGui::MenuItem(tr("Show in Files"))) {
        resourceFolder_ = parent.empty() ? "." : parent;
        resourceItems_ = Json::object();
        showPanel("Files", "resources");
    }
}
void Application::resourcePanel() {
    if (panels_["resources"]) {
        if (const auto *project = ImGui::FindWindowByName(label("Project", "project").c_str());
            project && project->DockId)
            ImGui::SetNextWindowDockID(project->DockId, ImGuiCond_FirstUseEver);
    }
    if (beginPanel("Files", "resources")) {
        resourceFocused_ = resourceFocused_ || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        {
            cyui::ScopedDisabled disabled(resourceFuture_.valid());
            if (cyui::iconButton(cyui::icon::add, tr("New file...")))
                beginResource("createFile", resourceFolder_ == "." ? "" : resourceFolder_ + "/");
            ImGui::SameLine();
            if (cyui::iconButton(cyui::icon::folder, tr("New folder...")))
                beginResource("createDirectory", resourceFolder_ == "." ? "" : resourceFolder_ + "/");
            ImGui::SameLine();
            if (cyui::iconButton(cyui::icon::refresh, tr("Refresh"))) {
                resourceItems_ = Json::object();
                resourceTrash_ = Json::object();
                lastResourceRefresh_ = -10;
            }
            ImGui::SameLine();
            cyui::ScopedDisabled noUndo(!commandEnabled("project.undoFile"));
            if (cyui::iconButton(cyui::icon::undo, tr("Undo file operation")))
                executeCommand("project.undoFile");
        }
        if (resourceFuture_.valid()) {
            ImGui::SameLine();
            cyui::spinner("##resources-pending");
        }
        if (!resourceError_.empty())
            cyui::callout(cyui::Tone::error, resourceError_.c_str());
        if (ImGui::BeginTabBar("##file-views")) {
            if (ImGui::BeginTabItem(tr("Workspace files"))) {
                resourceTrashView_ = false;
                cyui::textDim("%s", resourceFolder_.c_str());
                if (!resourceFuture_.valid() &&
                    (resourceItems_.empty() ||
                     resourceItems_.value("path", std::string{}) != resourceFolder_ ||
                     ImGui::GetTime() - lastResourceRefresh_ > 5))
                    resourceTask("resources.children", {{"path", resourceFolder_}});
                if (resourceItems_.value("truncated", false))
                    cyui::callout(cyui::Tone::warning,
                                  tr("Folder listing truncated. Narrow the folder scope."));
                std::vector<cyui::AssetItem> items;
                resourceAssetPaths_.clear();
                const auto current = resourceFolder_ == "." ? std::string{} : resourceFolder_;
                auto folderPath = utf8Path(current);
                while (!folderPath.empty()) {
                    const auto name = pathUtf8(folderPath), parent = pathUtf8(folderPath.parent_path());
                    const auto id = uiId("resource/" + name);
                    resourceAssetPaths_[id] = name;
                    items.push_back({id, parent.empty() ? 0 : uiId("resource/" + parent),
                                     pathUtf8(folderPath.filename()), "", 0, true, false});
                    folderPath = folderPath.parent_path();
                }
                for (const auto &entry : resourceItems_.value("items", Json::array())) {
                    const std::string path = entry.at("path");
                    const auto id = uiId("resource/" + path);
                    resourceAssetPaths_[id] = path;
                    const bool folder = entry.at("directory");
                    auto name = entry.at("name").get<std::string>();
                    if (applicationIconAvailable(folder ? cyui::icon::folder : cyui::icon::document))
                        name = std::string(folder ? cyui::icon::folder : cyui::icon::document) + "  " + name;
                    items.push_back({id, current.empty() ? 0 : uiId("resource/" + current), name,
                                     pathUtf8(utf8Path(path).extension()), 0, folder,
                                     resourceFavorites_.contains(path)});
                }
                resourceAssets_.folder = current.empty() ? 0 : uiId("resource/" + current);
                if (resourceSearchFocus_) {
                    ImGui::SetKeyboardFocusHere();
                    resourceSearchFocus_ = false;
                }
                const auto result =
                    cyui::assetBrowser("##workspace-resources", resourceAssets_, items,
                                       {0, std::max(cyui::dp(140), ImGui::GetContentRegionAvail().y)});
                if (resourceAssets_.folder != (current.empty() ? 0 : uiId("resource/" + current))) {
                    resourceFolder_ =
                        resourceAssets_.folder == 0 ? "." : resourceAssetPaths_[resourceAssets_.folder];
                    resourceItems_ = Json::object();
                    resourceAssets_.query.clear();
                }
                if (!resourceAssets_.selection.empty() &&
                    resourceAssetPaths_.contains(resourceAssets_.selection.back()))
                    selectedResource_ = resourceAssetPaths_.at(resourceAssets_.selection.back());
                if (result.id && resourceAssetPaths_.contains(result.id)) {
                    const auto path = resourceAssetPaths_.at(result.id);
                    if (result.action == cyui::AssetAction::open) {
                        const auto item = std::find_if(items.begin(), items.end(),
                                                       [&](const auto &v) { return v.id == result.id; });
                        if (item != items.end() && item->folder) {
                            resourceFolder_ = path;
                            resourceItems_ = Json::object();
                        } else
                            open(path);
                    } else if (result.action == cyui::AssetAction::rename)
                        beginResource("move", path, path);
                    else if (result.action == cyui::AssetAction::remove)
                        beginResource("delete", path);
                    else if (result.action == cyui::AssetAction::favorite) {
                        if (!resourceFavorites_.erase(path))
                            resourceFavorites_.insert(path);
                    }
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(tr("Project trash"))) {
                resourceTrashView_ = true;
                if (!resourceFuture_.valid() &&
                    (resourceTrash_.empty() || ImGui::GetTime() - lastResourceRefresh_ > 5))
                    resourceTask("resources.trash");
                viewFilter("##trash-filter", resourceTrashFilter_);
                cyui::textDim("%s",
                              tr("Deleted files stay here until explicitly restored. No automatic expiry."));
                for (const auto &entry : resourceTrash_.value("errors", Json::array()))
                    cyui::callout(cyui::Tone::error, entry.value("error", std::string{}).c_str());
                if (resourceTrash_.value("truncated", false))
                    cyui::callout(cyui::Tone::warning,
                                  tr("Trash list truncated. Restore entries and refresh."));
                ImGui::BeginChild("##trash-entries");
                for (const auto &item : resourceTrash_.value("items", Json::array())) {
                    const std::string path = item.at("path"), id = item.at("id");
                    if (!resourceTrashFilter_.matches(path, id))
                        continue;
                    ImGui::PushID(id.c_str());
                    ImGui::TextUnformatted(path.c_str());
                    cyui::textDim("%s", uiTimestamp(item.at("createdAt").get<uint64_t>()).c_str());
                    {
                        cyui::ScopedDisabled disabled(resourceFuture_.valid());
                        if (cyui::button(tr("Restore..."))) {
                            resourceTrashId_ = id;
                            beginResource("restore");
                        }
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }
    const bool requested = resourceModal_;
    if (cyui::beginModal(label("File operation", "resource-operation").c_str(), &resourceModal_, 620)) {
        if (!requested)
            ImGui::CloseCurrentPopup();
        ImGui::TextUnformatted(tr(resourceOperation_ == "delete"       ? "Move to project trash..."
                                  : resourceOperation_ == "restore"    ? "Restore..."
                                  : resourceOperation_ == "move"       ? "Rename or move..."
                                  : resourceOperation_ == "createFile" ? "New file..."
                                                                       : "New folder..."));
        cyui::callout(cyui::Tone::warning, tr("Physical paths only: includes, project manifests and debugger "
                                              "breakpoints are not rewritten."));
        if (!resourceError_.empty())
            cyui::callout(cyui::Tone::error, resourceError_.c_str());
        bool changed = false;
        ImGui::BeginDisabled(resourceFuture_.valid());
        if (cyui::beginProperties("##resource-fields")) {
            if (resourceOperation_ != "restore") {
                cyui::property(tr("Workspace-relative path"));
                changed |= ImGui::InputText("##source", &resourceSource_);
            }
            if (resourceOperation_ == "move" || resourceOperation_ == "restore") {
                cyui::property(tr("Destination"));
                changed |= ImGui::InputTextWithHint("##destination", tr("Original path by default"),
                                                    &resourceDestination_);
            }
            cyui::endProperties();
        }
        ImGui::EndDisabled();
        if (changed)
            resourcePlan_ = Json::object();
        {
            cyui::ScopedDisabled disabled(resourceFuture_.valid());
            if (cyui::button(tr("Preview operation"))) {
                Json params = {{"operation", resourceOperation_}};
                if (resourceOperation_ == "restore")
                    params["trash"] = resourceTrashId_;
                else
                    params["path"] = resourceSource_;
                if ((resourceOperation_ == "restore" || resourceOperation_ == "move") &&
                    !resourceDestination_.empty())
                    params["destination"] = resourceDestination_;
                resourceTask("resources.preview", params);
            }
        }
        if (resourceFuture_.valid()) {
            ImGui::SameLine();
            cyui::spinner("##resource-operation-pending");
        }
        if (resourcePlan_.contains("id")) {
            const auto &snapshot = resourcePlan_.at("snapshot");
            cyui::textDim("%llu %s | %llu B", snapshot.at("entries").get<unsigned long long>(), tr("Entries"),
                          snapshot.at("bytes").get<unsigned long long>());
            if (resourcePlan_.at("blocked").get<bool>())
                cyui::callout(cyui::Tone::warning,
                              tr("Save or reconcile affected documents, then preview again."));
            if (ImGui::BeginChild("##affected-resources", cyui::dp(0, 110), ImGuiChildFlags_Borders))
                for (const auto &doc : resourcePlan_.at("openDocuments"))
                    ImGui::TextUnformatted(doc.at("path").get_ref<const std::string &>().c_str());
            ImGui::EndChild();
        }
        {
            cyui::ScopedDisabled disabled(resourceFuture_.valid() || !resourcePlan_.contains("id") ||
                                          resourcePlan_.value("blocked", true));
            if (cyui::primaryButton(tr("Apply file operation")))
                resourceTask("resources.apply", {{"id", resourcePlan_.at("id")}});
        }
        ImGui::SameLine();
        if (cyui::button(tr("Cancel")) ||
            (!resourceFuture_.valid() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))) {
            resourceModal_ = false;
            ImGui::CloseCurrentPopup();
        }
        cyui::endModal();
    }
}
} // namespace cycode
