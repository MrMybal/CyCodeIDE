#include "cycode/Application.h"
#include <algorithm>
#include <cyui/Core.h>
#include <imgui_internal.h>
namespace cycode {
namespace {
Json reference(const Json &entry) {
    return {{"session", entry.at("session")}, {"id", entry.at("id")}, {"checkpoint", entry.at("checkpoint")}};
}
bool sameEntry(const Json &a, const Json &b) {
    return a.contains("checkpoint") && b.contains("checkpoint") && a.at("session") == b.at("session") &&
           a.at("id") == b.at("id") && a.at("checkpoint") == b.at("checkpoint");
}
} // namespace
void Application::recoveryTask(const std::string &method, Json parameters) {
    if (recoveryFuture_.valid())
        return;
    auto engine = engine_;
    recoveryMethod_ = method;
    if (method == "recovery.list")
        lastRecoveryRefresh_ = ImGui::GetTime();
    recoveryError_.clear();
    recoveryFuture_ = std::async(
        std::launch::async, [engine, method, parameters] { return engine->dispatch(method, parameters); });
}
void Application::updateRecovery() {
    if (recoveryFuture_.valid() &&
        recoveryFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            const auto result = recoveryFuture_.get();
            if (recoveryMethod_ == "recovery.list") {
                recoveryInfo_ = result;
                lastRecoveryRefresh_ = ImGui::GetTime();
                if (recoveryInitialCheck_) {
                    recoveryInitialCheck_ = false;
                    const auto &items = result.at("items");
                    if (std::any_of(items.begin(), items.end(), [](const auto &item) {
                            return !item.at("active").template get<bool>();
                        })) {
                        showPanel("Recovery", "recovery");
                        cyui::notify(tr("Unsaved documents can be recovered."), cyui::Tone::warning, 8);
                    }
                }
            } else if (recoveryMethod_ == "recovery.preview") {
                recoveryPreview_ = result;
                recoveryCode_.setText(result.at("text"),
                                      cyui::codeLanguageForPath(result.at("path").get<std::string>()));
                recoveryCodeState_ = {};
            } else if (recoveryMethod_ == "recovery.restore") {
                const std::string path = result.at("document").at("path");
                const bool exists = std::any_of(tabs_.begin(), tabs_.end(),
                                                [&](const auto &tab) { return tab->path == path; });
                if (!exists) {
                    auto tab = std::make_shared<Tab>();
                    tab->path = tab->displayPath = path;
                    tab->codeEditor.setText(result.at("previousText").get<std::string>());
                    configureEditor(tab);
                    tabs_.push_back(tab);
                }
                // open() applies the recovered revision as one editor Undo transaction.
                open(path);
                if (const auto saved = sessionDraftTabs_.find(path); saved != sessionDraftTabs_.end()) {
                    for (auto &tab : tabs_) if (tab->path == path) {
                        restoreSessionTab(tab,saved->second);
                    }
                    sessionDraftTabs_.erase(saved);
                }
                if (result.value("sourceRetained", false))
                    cyui::notify(tr("Recovered in memory; the source checkpoint was retained."),
                                 cyui::Tone::warning, 8);
                if (!result.value("warning", std::string{}).empty())
                    error(result.at("warning"));
                recoveryPreview_ = Json::object();
                recoveryTask("recovery.list");
            } else if (recoveryMethod_ == "recovery.discard") {
                sessionDraftTabs_.erase(recoveryDiscard_.value("path",std::string{}));
                if (sameEntry(recoveryPreview_, recoveryDiscard_))
                    recoveryPreview_ = Json::object();
                recoveryDiscard_ = nullptr;
                recoveryTask("recovery.list");
            } else if (recoveryMethod_ == "recovery.flush") {
                recoveryInfo_["status"] = result;
                recoveryTask("recovery.list");
            }
        } catch (const std::exception &e) {
            recoveryError_ = e.what();
            error(e.what());
        }
    }
    if (!recoveryFuture_.valid() && panels_["recovery"] && ImGui::GetTime() - lastRecoveryRefresh_ > 5)
        recoveryTask("recovery.list");
}
void Application::recoveryPanel() {
    if (!panels_["recovery"])
        return;
    // Also dock the newly introduced view in layouts saved by older versions.
    if (const auto *build = ImGui::FindWindowByName(label("Build", "build").c_str()); build && build->DockId)
        ImGui::SetNextWindowDockID(build->DockId, ImGuiCond_FirstUseEver);
    if (!beginPanel("Recovery", "recovery"))
        return;
    pagedRecoveryControls();
    {
        cyui::ScopedDisabled disabled(recoveryFuture_.valid());
        if (cyui::button(tr("Refresh")))
            recoveryTask("recovery.list");
        ImGui::SameLine();
        if (cyui::button(tr("Checkpoint now")))
            recoveryTask("recovery.flush");
    }
    ImGui::SameLine();
    if (recoveryFuture_.valid())
        cyui::spinner("##recovery-pending");
    else
        cyui::textDim("%s",
                      tr("Ordinary-buffer checkpoints: every second. Paged checkpoints and source saves are explicit."));
    if (!recoveryError_.empty())
        cyui::callout(cyui::Tone::error, recoveryError_.c_str());
    for (const auto &entry : recoveryInfo_.value("errors", Json::array()))
        cyui::callout(cyui::Tone::warning, entry.value("error", std::string{}).c_str());
    if (recoveryInfo_.contains("status"))
        for (const auto &entry : recoveryInfo_.at("status").at("errors"))
            cyui::callout(cyui::Tone::error, entry.value("error", std::string{}).c_str());
    viewFilter("##recovery-filter", recoveryFilter_);
    if (recoveryInfo_.value("truncated", false))
        cyui::textDim("%s", tr("Recovery list truncated; resolve checkpoints and refresh."));
    const auto items = recoveryInfo_.value("items", Json::array());
    if (items.empty()) {
        cyui::emptyState(tr("Recovery"), tr("No recovery checkpoints in this workspace."), cyui::icon::info);
        ImGui::End();
        return;
    }
    if (ImGui::BeginTable("##recovery-layout", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("##checkpoints", ImGuiTableColumnFlags_WidthStretch, .42f);
        ImGui::TableSetupColumn("##preview", ImGuiTableColumnFlags_WidthStretch, .58f);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::BeginChild("##recovery-entries",
                          {0, std::max(cyui::dp(140), ImGui::GetContentRegionAvail().y)}, false);
        for (const auto &entry : items) {
            const std::string path = entry.at("path"), session = entry.at("session");
            if (!recoveryFilter_.matches(path, session))
                continue;
            const auto key = session + "/" + entry.at("id").get<std::string>();
            ImGui::PushID(key.c_str());
            {
                cyui::ScopedDisabled disabled(entry.at("active").get<bool>() || recoveryFuture_.valid());
                if (ImGui::Selectable(path.c_str(), sameEntry(entry, recoveryPreview_)))
                    recoveryTask("recovery.preview", reference(entry));
            }
            if (!entry.at("active").get<bool>() && ImGui::BeginPopupContextItem("##checkpoint-actions")) {
                if (ImGui::MenuItem(tr("Discard checkpoint..."), nullptr, false, !recoveryFuture_.valid())) {
                    recoveryDiscard_ = entry;
                    recoveryConfirm_ = true;
                }
                ImGui::EndPopup();
            }
            cyui::textDim("%s | %llu B | %s", uiTimestamp(entry.at("updatedAt").get<uint64_t>()).c_str(),
                          entry.at("bytes").get<unsigned long long>(),
                          tr(entry.at("active").get<bool>() ? "In use" : "Available for recovery"));
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::TableNextColumn();
        if (recoveryPreview_.contains("checkpoint")) {
            ImGui::TextUnformatted(recoveryPreview_.at("path").get_ref<const std::string &>().c_str());
            if (recoveryPreview_.at("missing").get<bool>())
                cyui::callout(cyui::Tone::warning, tr("The original file is missing. Restore recovers its "
                                                      "buffer; Save recreates the file."));
            else if (recoveryPreview_.at("diskChanged").get<bool>())
                cyui::callout(cyui::Tone::warning,
                              tr("The file changed on disk. Restore keeps a save conflict for review."));
            if (recoveryPreview_.at("targetDirty").get<bool>())
                cyui::callout(cyui::Tone::warning,
                              tr("Save or close the modified target, then preview again."));
            {
                cyui::ScopedDisabled disabled(recoveryFuture_.valid());
                {
                    cyui::ScopedDisabled target(recoveryPreview_.at("targetDirty").get<bool>());
                    if (cyui::primaryButton(tr("Restore buffer"))) {
                        auto params = reference(recoveryPreview_);
                        params["expectedRevision"] = recoveryPreview_.at("targetRevision");
                        params["expectedDiskState"] = recoveryPreview_.at("diskState");
                        params["includePreviousText"] = true;
                        recoveryTask("recovery.restore", params);
                    }
                }
                ImGui::SameLine();
                if (cyui::button(tr("Preview again")))
                    recoveryTask("recovery.preview", reference(recoveryPreview_));
                ImGui::SameLine();
                if (cyui::button(tr("Discard checkpoint..."))) {
                    recoveryDiscard_ = recoveryPreview_;
                    recoveryConfirm_ = true;
                }
            }
            if (recoveryPreview_.at("truncated").get<bool>())
                cyui::textDim("%s", tr("Preview truncated; restoration includes the complete buffer."));
            drawCodeView("##recovery-code", recoveryCodeState_, recoveryCode_,
                             {0, std::max(cyui::dp(100), ImGui::GetContentRegionAvail().y)});
        } else
            cyui::textDim("%s", tr("Select an inactive checkpoint to preview and restore it."));
        ImGui::EndTable();
    }
    const auto popup = label("Discard recovery checkpoint?", "recovery-discard");
    if (recoveryConfirm_) {
        ImGui::OpenPopup(popup.c_str());
        recoveryConfirm_ = false;
    }
    if (ImGui::BeginPopupModal(popup.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(recoveryDiscard_.value("path", std::string{}).c_str());
        ImGui::TextUnformatted(
            tr("This removes only the selected backup. The project file stays unchanged."));
        if (cyui::dangerButton(tr("Discard checkpoint"))) {
            recoveryTask("recovery.discard", reference(recoveryDiscard_));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (cyui::button(tr("Cancel")))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();
}
} // namespace cycode
