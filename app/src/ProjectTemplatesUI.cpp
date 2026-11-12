#include "cycode/Application.h"
#include <algorithm>
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
void Application::beginProjectTemplate() {
    if (templateFuture_.valid())
        return;
    templateModal_ = true;
    templateError_.clear();
    templatePlan_ = Json::object();
    projectTemplates_.reset();
    templateParent_ = pathUtf8(engine_->workspace().parent_path());
    refreshProjectTemplates();
}
void Application::selectProjectTemplate(const std::string &id) {
    templateId_ = id;
    templateOptions_ = Json::object();
    for (const auto &item : templateCatalogue_.at("templates"))
        if (item.at("id") == id)
            for (const auto &field : item.value("fields", Json::array()))
                templateOptions_[field.at("id").get<std::string>()] = field.at("default");
}
void Application::refreshProjectTemplates() {
    const auto previousId = templateId_;
    const auto previousOptions = templateOptions_;
    templateCatalogue_ = engine_->dispatch("templates.list");
    const auto &items = templateCatalogue_.at("templates");
    const bool exists = std::any_of(items.begin(), items.end(),
                                    [&](const auto &item) { return item.at("id") == templateId_; });
    selectProjectTemplate(exists ? templateId_ : "cpp.console");
    if (templateId_ == previousId)
        for (auto &[key, value] : templateOptions_.items())
            if (previousOptions.contains(key) && previousOptions.at(key).type() == value.type())
                value = previousOptions.at(key);
}
void Application::selectTemplateFile(size_t index) {
    if (!templatePlan_.contains("files") || index >= templatePlan_.at("files").size())
        return;
    templateFile_ = index;
    templateCodeState_ = {};
    const auto &file = templatePlan_.at("files").at(index);
    templateCode_.setText(file.at("text").get<std::string>(),
                          cyui::codeLanguageForPath(file.at("path").get<std::string>()), 1);
}
void Application::projectTemplateTask(bool create) {
    if (templateFuture_.valid())
        return;
    templateError_.clear();
    templateCreating_ = create;
    if (create) {
        if (!projectTemplates_ || templatePlan_.value("state", std::string{}) != "preview")
            return;
        const auto generator = projectTemplates_;
        const std::string id = templatePlan_.at("id");
        templateFuture_ = std::async(std::launch::async, [generator, id] { return generator->create(id); });
    } else {
        // This root is explicitly selected in the desktop wizard. Engine/MCP
        // requests keep their existing workspace boundary and cannot grant it.
        const auto parent = utf8Path(templateParent_);
        Json params = {{"template", templateId_}, {"name", templateName_}, {"options", templateOptions_}};
        if (templateId_ == "csharp.console")
            params["framework"] = templateFramework_;
        templatePlan_ = Json::object();
        try {
            const auto engine = engine_;
            projectTemplates_ = std::make_shared<ProjectTemplates>(
                parent, [engine] { return engine->plugins().templateSnapshot(); },
                [engine](const std::string &owner, uint64_t revision, const std::function<Json()> &commit) {
                    return engine->plugins().createFromTemplate(owner, revision, commit);
                });
            templateParent_ = pathUtf8(std::filesystem::canonical(parent));
            const auto generator = projectTemplates_;
            templateFuture_ =
                std::async(std::launch::async, [generator, params] { return generator->preview(params); });
        } catch (const std::exception &e) {
            templateError_ = e.what();
        }
    }
}
void Application::openCreatedProject() {
    if (templatePlan_.value("state", std::string{}) != "created")
        return;
    const auto parent = std::filesystem::canonical(utf8Path(templateParent_));
    const auto entry = parent / utf8Path(templatePlan_.at("entry"));
    if (templateNewWindow_)
        newWindow(entry);
    else {
        // openProject's existing switch path refuses dirty buffers. Preserve the
        // generated project result in the wizard if it cannot switch yet.
        templateOpenEntry_ = pathUtf8(entry);
        templateOpenFile_ = templatePlan_.at("openFile");
        openProject(entry);
    }
    templateModal_ = false;
}
void Application::updateProjectTemplates() {
    if (templateModal_ && !templateFuture_.valid() &&
        templatePlan_.value("state", std::string{}) != "created" &&
        templateCatalogue_.value("revision", uint64_t(0)) != engine_->plugins().revision()) {
        refreshProjectTemplates();
        templatePlan_ = Json::object();
        projectTemplates_.reset();
        templateError_ = tr("Project templates changed; preview again.");
    }
    if (!templateFuture_.valid() ||
        templateFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    try {
        templatePlan_ = templateFuture_.get();
        selectTemplateFile(0);
        if (templateCreating_) {
            projectTemplates_.reset();
            cyui::notify(tr("Project created"), cyui::Tone::success);
            openCreatedProject();
        }
    } catch (const std::exception &e) {
        templateError_ = e.what();
        error(e.what());
    }
}
void Application::projectTemplatePanel() {
    const bool requested = templateModal_;
    const auto area = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSizeConstraints(cyui::dp(280, 100), {std::max(cyui::dp(280), area.x - cyui::dp(24)),
                                                             std::max(cyui::dp(100), area.y - cyui::dp(24))});
    if (!cyui::beginModal(label("New project...", "new-project").c_str(), &templateModal_, 820))
        return;
    if (!requested)
        ImGui::CloseCurrentPopup();
    const bool created = templatePlan_.value("state", std::string{}) == "created";
    if (!templateError_.empty())
        cyui::callout(cyui::Tone::error, templateError_.c_str());
    bool changed = false;
    {
        cyui::ScopedDisabled disabled(templateFuture_.valid() || created);
        if (cyui::beginProperties("##project-template-fields", 155)) {
            cyui::property(tr("Project template"));
            std::string title;
            for (const auto &item : templateCatalogue_.at("templates"))
                if (item.at("id") == templateId_)
                    title = item.at("title");
            if (ImGui::BeginCombo("##template", tr(title.c_str()))) {
                cyui::filterBox("##template-filter", templateFilter_);
                for (const auto &item : templateCatalogue_.at("templates")) {
                    const std::string id = item.at("id"), text = item.at("title");
                    if (templateFilter_.matches(text, id) &&
                        ImGui::Selectable(tr(text.c_str()), id == templateId_)) {
                        selectProjectTemplate(id);
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            cyui::property(tr("Project name"));
            changed |= ImGui::InputText("##project-name", &templateName_);
            cyui::property(tr("Parent directory"));
            changed |= ImGui::InputText("##project-parent", &templateParent_);
            if (templateId_ == "csharp.console") {
                cyui::property(tr("Target framework"));
                if (ImGui::BeginCombo("##framework", templateFramework_.c_str())) {
                    for (const auto &value : templateCatalogue_.at("frameworks")) {
                        const std::string framework = value;
                        if (ImGui::Selectable(framework.c_str(), framework == templateFramework_)) {
                            templateFramework_ = framework;
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            for (const auto &item : templateCatalogue_.at("templates"))
                if (item.at("id") == templateId_) {
                    for (const auto &field : item.value("fields", Json::array())) {
                        const std::string id = field.at("id"), title = field.at("title"),
                                          kind = field.at("type");
                        ImGui::PushID(id.c_str());
                        const auto help = field.value("help", std::string{});
                        cyui::property(tr(title.c_str()), help.empty() ? nullptr : tr(help.c_str()));
                        const auto current = templateOptions_.value(id, field.at("default"));
                        if (kind == "boolean") {
                            bool value = current.get<bool>();
                            if (ImGui::Checkbox("##value", &value)) {
                                templateOptions_[id] = value;
                                changed = true;
                            }
                        } else if (kind == "choice") {
                            const std::string selected = current;
                            if (ImGui::BeginCombo("##choice", selected.c_str())) {
                                for (const auto &choice : field.at("choices")) {
                                    const std::string value = choice;
                                    if (ImGui::Selectable(value.c_str(), value == selected)) {
                                        templateOptions_[id] = value;
                                        changed = true;
                                    }
                                }
                                ImGui::EndCombo();
                            }
                        } else {
                            auto value = current.get<std::string>();
                            if (ImGui::InputText("##value", &value)) {
                                templateOptions_[id] = value;
                                changed = true;
                            }
                        }
                        ImGui::PopID();
                    }
                }
            cyui::endProperties();
        }
    }
    if (changed) {
        templatePlan_ = Json::object();
        templateError_.clear();
        projectTemplates_.reset();
    }
    if (templatePlan_.contains("id")) {
        cyui::textDim(
            "%s: %s", tr("Destination"),
            pathUtf8(utf8Path(templateParent_) / utf8Path(templatePlan_.at("destination"))).c_str());
        std::string requirements;
        for (const auto &value : templatePlan_.at("requirements")) {
            if (!requirements.empty())
                requirements += "; ";
            requirements += value.get<std::string>();
        }
        ImGui::TextWrapped("%s: %s", tr("Required tools"), requirements.c_str());
        if (!templatePlan_.value("plugin", std::string{}).empty())
            cyui::textDim("%s: %s", tr("Plugin"),
                          templatePlan_.at("plugin").get_ref<const std::string &>().c_str());
        if (ImGui::BeginTable("##template-preview", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("##generated-files", ImGuiTableColumnFlags_WidthStretch, .3f);
            ImGui::TableSetupColumn("##generated-code", ImGuiTableColumnFlags_WidthStretch, .7f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const auto height = std::min(cyui::dp(260), area.y * .32f);
            ImGui::BeginChild("##template-file-list", {0, height});
            size_t index = 0;
            for (const auto &file : templatePlan_.at("files")) {
                const std::string path = file.at("path");
                if (ImGui::Selectable(path.c_str(), index == templateFile_))
                    selectTemplateFile(index);
                ++index;
            }
            ImGui::EndChild();
            ImGui::TableNextColumn();
            drawCodeView("##template-code", templateCodeState_, templateCode_, {0, height});
            ImGui::EndTable();
        }
    } else
        ImGui::TextWrapped("%s", tr("Preview the generated files before creating a new project directory."));
    {
        cyui::ScopedDisabled disabled(templateFuture_.valid());
        ImGui::Checkbox(tr("Open in new window"), &templateNewWindow_);
        if (!created) {
            if (cyui::button(tr("Preview project")))
                projectTemplateTask(false);
            ImGui::SameLine();
            cyui::ScopedDisabled noPlan(!templatePlan_.contains("id"));
            if (cyui::primaryButton(tr("Create and open")))
                projectTemplateTask(true);
        } else {
            cyui::badge(tr("Project created"), cyui::Tone::success);
            ImGui::SameLine();
            if (cyui::primaryButton(tr("Open created project"))) {
                try {
                    openCreatedProject();
                } catch (const std::exception &e) {
                    templateError_ = e.what();
                }
            }
        }
    }
    ImGui::SameLine();
    if (cyui::button(tr("Close")) ||
        (!templateFuture_.valid() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))) {
        templateModal_ = false;
        ImGui::CloseCurrentPopup();
    }
    if (templateFuture_.valid()) {
        ImGui::SameLine();
        cyui::spinner("##new-project-pending");
    }
    cyui::endModal();
}
} // namespace cycode
