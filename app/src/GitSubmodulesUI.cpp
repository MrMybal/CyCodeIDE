#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
void Application::gitSubmoduleControls() {
    ImGui::PushTextWrapPos(0.0f);
    const bool busy = vcsFuture_.valid() || taskFuture_.valid() || gitFuture_.valid();
    if (!vcsSubmodulesRequested_ && !busy && vcsContextPath_ == vcsRequestPath()) {
        vcsSubmodulesRequested_ = true;
        vcsTask("vcs.submodules");
    }
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::button(cyui::trLabel("Refresh submodules").c_str())) {
            vcsSubmodulesRequested_ = true;
            vcsTask("vcs.submodules");
        }
    }
    cyui::textDim("%s", tr("Select a submodule to review an explicit action. Updates use the commit recorded "
                           "by the parent project."));
    if (!vcsSubmodules_.contains("revision")) {
        cyui::emptyState(tr("Submodule catalogue unavailable"), tr("Refresh before changing submodules."),
                         cyui::icon::folder);
        ImGui::PopTextWrapPos();
        return;
    }
    if (vcsSubmodules_.at("repository") != vcsContext_.value("gitRepository", std::string{}) ||
        vcsSubmodules_.at("identity") != vcsContext_.value("gitIdentity", std::string{})) {
        ImGui::PopTextWrapPos();
        return;
    }
    gitSubmoduleAddControls(busy);
    cyui::filterBox("##submodule-filter", vcsSubmoduleFilter_);
    if (vcsSubmodules_.at("entries").empty())
        cyui::textDim("%s", tr("No configured submodules in this repository."));
    for (const auto &entry : vcsSubmodules_.at("entries")) {
        const auto path = entry.at("path").get<std::string>();
        const auto state = entry.at("state").get<std::string>();
        const char *stateLabel = state == "matching"        ? "At recorded commit"
                                 : state == "different"     ? "Different commit"
                                 : state == "uninitialized" ? "Not checked out"
                                 : state == "conflicted"    ? "Conflicted gitlink"
                                 : state == "notIndexed"    ? "Not a staged gitlink"
                                                            : "Submodule unavailable";
        if (!vcsSubmoduleFilter_.matches(path, entry.at("name").get<std::string>() + " " + stateLabel))
            continue;
        cyui::ScopedID id(path.c_str());
        if (cyui::beginCard("##submodule", path.c_str())) {
            cyui::badge(tr(stateLabel), state == "matching" ? cyui::Tone::success : cyui::Tone::warning);
            if (entry.at("dirty") == true) {
                ImGui::SameLine();
                cyui::badge(tr("Local modifications"), cyui::Tone::warning);
            }
            ImGui::TextUnformatted(entry.at("name").get_ref<const std::string &>().c_str());
            cyui::textDim("%s: %s", tr("Recorded commit"),
                          entry.at("expectedCommit").get_ref<const std::string &>().c_str());
            cyui::textDim("HEAD: %s", entry.at("head").get_ref<const std::string &>().c_str());
            if (!entry.at("branch").get_ref<const std::string &>().empty())
                ImGui::TextUnformatted(entry.at("branch").get_ref<const std::string &>().c_str());
            if (!entry.at("error").get_ref<const std::string &>().empty())
                cyui::textColoured(cyui::palette().warning, "%s",
                                   entry.at("error").get_ref<const std::string &>().c_str());
            const bool editable = state == "matching" || state == "different" || state == "uninitialized";
            auto review = [&](const char *action) {
                vcsSubmoduleAddOpen_ = false;
                vcsSubmoduleAddPlan_ = Json::object();
                vcsSubmoduleReview_ = {{"action", action},
                                       {"entry", entry},
                                       {"repository", vcsSubmodules_.at("repository")},
                                       {"expectedRepository", vcsSubmodules_.at("identity")},
                                       {"expectedRevision", vcsSubmodules_.at("revision")}};
                vcsSubmoduleFetch_ = false;
            };
            {
                cyui::ScopedDisabled disabled(busy || !editable || entry.at("registered") == true);
                if (cyui::button(cyui::trLabel("Register submodule").c_str()))
                    review("init");
            }
            {
                cyui::ScopedDisabled disabled(busy || !editable || entry.at("registered") != true);
                if (cyui::button(cyui::trLabel("Synchronize submodule URL").c_str()))
                    review("sync");
            }
            {
                cyui::ScopedDisabled disabled(busy || !editable || entry.at("dirty") == true);
                if (cyui::primaryButton(cyui::trLabel("Update to recorded commit").c_str()))
                    review("update");
            }
            {
                cyui::ScopedDisabled disabled(busy || !editable);
                if (cyui::button(cyui::trLabel("Edit submodule settings").c_str())) {
                    review("settings");
                    vcsSubmoduleReview_["setting"] = "url";
                    vcsSubmoduleReview_["value"] = "";
                }
            }
            {
                cyui::ScopedDisabled disabled(busy || state != "matching" || entry.at("dirty") != false ||
                                              entry.at("registered") != true ||
                                              entry.at("checkedOut") != true);
                if (cyui::button(cyui::trLabel("Review submodule deinitialization").c_str())) {
                    review("deinit");
                    vcsTask("vcs.submodule.deinitPlan",
                            {{"path", entry.at("path")},
                             {"repository", vcsSubmoduleReview_.at("repository")},
                             {"expectedRepository", vcsSubmoduleReview_.at("expectedRepository")},
                             {"expectedRevision", vcsSubmoduleReview_.at("expectedRevision")}});
                }
            }
        }
        cyui::endCard();
    }
    if (!vcsSubmoduleReview_.contains("action")) {
        ImGui::PopTextWrapPos();
        return;
    }
    cyui::sectionTitle(tr("Review submodule action"));
    const auto action = vcsSubmoduleReview_.at("action").get<std::string>();
    const auto &entry = vcsSubmoduleReview_.at("entry");
    ImGui::TextUnformatted(entry.at("path").get_ref<const std::string &>().c_str());
    if (action == "update") {
        ImGui::TextWrapped("%s: %s", tr("Recorded commit"),
                           entry.at("expectedCommit").get_ref<const std::string &>().c_str());
        cyui::textDim("%s", tr("Checkout detaches HEAD at this commit. Close documents in the submodule and "
                               "keep its working tree clean. Git may execute configured hooks."));
        ImGui::Checkbox(cyui::trLabel("Allow clone/fetch").c_str(), &vcsSubmoduleFetch_);
    } else if (action == "settings") {
        {
            cyui::ScopedDisabled disabled(busy);
            if (cyui::beginProperties("##submodule-settings")) {
                auto setting = vcsSubmoduleReview_.at("setting").get<std::string>();
                auto value = vcsSubmoduleReview_.at("value").get<std::string>();
                cyui::property(tr("Setting"));
                if (ImGui::BeginCombo("##submodule-setting-kind",
                                      tr(setting == "url" ? "Repository address" : "Tracking branch"))) {
                    for (const auto *choice : {"url", "branch"}) {
                        if (ImGui::Selectable(
                                tr(std::string(choice) == "url" ? "Repository address" : "Tracking branch"),
                                setting == choice)) {
                            setting = choice;
                            value.clear();
                            vcsSubmoduleReview_["setting"] = setting;
                            vcsSubmoduleReview_["value"] = value;
                            vcsSubmoduleReview_.erase("plan");
                        }
                    }
                    ImGui::EndCombo();
                }
                cyui::property(tr("New value"));
                if (ImGui::InputText("##submodule-setting-value", &value)) {
                    vcsSubmoduleReview_["value"] = value;
                    vcsSubmoduleReview_.erase("plan");
                }
                cyui::endProperties();
            }
        }
        cyui::textDim("%s", tr("An empty tracking branch uses remote HEAD; '.' follows the parent branch "
                               "name. The recorded-commit update keeps using the gitlink."));
        cyui::textDim(
            "%s",
            tr("Close .gitmodules; URL changes also require closed submodule documents. Existing metadata "
               "edits are preserved. New changes remain unstaged, without checkout or fetch."));
        {
            cyui::ScopedDisabled disabled(busy || (vcsSubmoduleReview_.at("setting") == "url" &&
                                                   vcsSubmoduleReview_.at("value") == ""));
            if (cyui::button(cyui::trLabel("Preview submodule settings").c_str())) {
                vcsSubmoduleReview_.erase("plan");
                vcsTask("vcs.submodule.settingsPlan",
                        {{"path", entry.at("path")},
                         {"setting", vcsSubmoduleReview_.at("setting")},
                         {"value", vcsSubmoduleReview_.at("value")},
                         {"repository", vcsSubmoduleReview_.at("repository")},
                         {"expectedRepository", vcsSubmoduleReview_.at("expectedRepository")},
                         {"expectedRevision", vcsSubmoduleReview_.at("expectedRevision")}});
            }
        }
        if (vcsSubmoduleReview_.contains("plan")) {
            const auto &plan = vcsSubmoduleReview_.at("plan");
            cyui::sectionTitle(tr("Submodule settings preview"));
            cyui::textDim("%s", tr("Previous configured value"));
            if (plan.at("previousRedacted") == true)
                cyui::textDim("%s",
                              tr("The previous value is hidden because it is sensitive or unsupported."));
            else if (!plan.at("previousValue").is_null())
                ImGui::TextUnformatted(plan.at("previousValue").get_ref<const std::string &>().c_str());
            else
                cyui::textDim("%s", tr("No configured value"));
            cyui::textDim("%s", tr("New value"));
            if (plan.at("request").at("setting") == "branch" && plan.at("request").at("value") == "")
                cyui::textDim("%s", tr("Remote default branch"));
            else
                ImGui::TextUnformatted(plan.at("request").at("value").get_ref<const std::string &>().c_str());
            if (plan.at("maySynchronizeLocalUrl") == true)
                cyui::textDim("%s", tr("Git also synchronizes local URL settings according to its "
                                       "active-submodule configuration. No remote connection is requested."));
            if (plan.at("hasLocalBranchOverride") == true)
                cyui::textColoured(cyui::palette().warning, "%s",
                                   tr("A local tracking-branch override exists and remains in effect. This "
                                      "action edits .gitmodules only."));
            if (plan.at("alreadyConfigured") == true)
                cyui::textDim("%s", tr("The tracking branch is already configured as requested."));
        }
    } else if (action == "deinit") {
        cyui::textColoured(cyui::palette().warning, "%s",
                           tr("Deinitialization removes this checkout and its local registration. The "
                              "project gitlink, .gitmodules and Git repository remain."));
        cyui::textDim("%s", tr("Close submodule documents and .gitmodules. Modified, untracked, ignored "
                               "files, hidden index flags and nested submodules block this action."));
        if (!vcsSubmoduleReview_.contains("plan")) {
            cyui::ScopedDisabled disabled(busy);
            if (cyui::button(cyui::trLabel("Preview deinitialization again").c_str()))
                vcsTask("vcs.submodule.deinitPlan",
                        {{"path", entry.at("path")},
                         {"repository", vcsSubmoduleReview_.at("repository")},
                         {"expectedRepository", vcsSubmoduleReview_.at("expectedRepository")},
                         {"expectedRevision", vcsSubmoduleReview_.at("expectedRevision")}});
        } else {
            cyui::textDim("%s: %zu", tr("Tracked files to remove"),
                          vcsSubmoduleReview_.at("plan").at("trackedFiles").get<size_t>());
            cyui::textDim("%s",
                          tr("This local Git action has no editor Undo. Reinitializing the retained "
                             "repository is a separate action. Inspect partial outcomes before retrying."));
        }
    } else
        cyui::textDim("%s",
                      tr(action == "init"
                             ? "Register the configured URL locally without downloading files."
                             : "Copy the URL from .gitmodules to the registered submodule configuration."));
    const bool changed = vcsSubmoduleReview_.at("repository") != vcsSubmodules_.at("repository") ||
                         vcsSubmoduleReview_.at("expectedRepository") != vcsSubmodules_.at("identity") ||
                         vcsSubmoduleReview_.at("expectedRevision") != vcsSubmodules_.at("revision");
    if (changed)
        cyui::textColoured(cyui::palette().warning, "%s",
                           tr("Submodule state changed. Review the action again."));
    {
        cyui::ScopedDisabled disabled(
            busy || changed ||
            ((action == "deinit" || action == "settings") && !vcsSubmoduleReview_.contains("plan")) ||
            (action == "settings" && vcsSubmoduleReview_.contains("plan") &&
             vcsSubmoduleReview_.at("plan").at("alreadyConfigured") == true) ||
            (action == "update" && !vcsSubmoduleFetch_ && entry.at("checkedOut") != true));
        const bool apply =
            action == "deinit"
                ? cyui::dangerButton(cyui::trLabel("Remove reviewed submodule checkout").c_str())
                : cyui::primaryButton(cyui::trLabel("Apply submodule action").c_str());
        if (apply) {
            Json parameters = {{"path", entry.at("path")},
                               {"repository", vcsSubmoduleReview_.at("repository")},
                               {"expectedRepository", vcsSubmoduleReview_.at("expectedRepository")},
                               {"expectedRevision", vcsSubmoduleReview_.at("expectedRevision")}};
            if (action == "update")
                parameters["fetch"] = vcsSubmoduleFetch_;
            if (action == "deinit" || action == "settings")
                parameters["expectedPlanRevision"] = vcsSubmoduleReview_.at("plan").at("revision");
            if (action == "settings") {
                parameters["setting"] = vcsSubmoduleReview_.at("setting");
                parameters["value"] = vcsSubmoduleReview_.at("value");
            }
            vcsTask("vcs.submodule." + action, parameters);
            vcsSubmoduleReview_ = Json::object();
        }
    }
    ImGui::SameLine();
    if (cyui::button(cyui::trLabel("Cancel submodule action").c_str()) ||
        ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        vcsSubmoduleReview_ = Json::object();
    ImGui::PopTextWrapPos();
}
} // namespace cycode
