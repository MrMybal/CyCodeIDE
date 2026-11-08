#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
std::string Application::vcsRequestPath() const {
    if (!vcsRepository_.empty())
        return vcsRepository_;
    return active_.empty() || active_.find("://") != std::string::npos ? std::string(".") : active_;
}
void Application::gitRepositoryControls() {
    if (vcsRepositoriesFuture_.valid() &&
        vcsRepositoriesFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            vcsRepositories_ = vcsRepositoriesFuture_.get();
        } catch (const std::exception &e) {
            vcsRepositories_ = {{"repositories", Json::array()},
                                {"errors", Json::array({{{"error", e.what()}}})},
                                {"truncated", false}};
        }
    }
    if (vcsRepositories_.is_null() && !vcsRepositoriesFuture_.valid()) {
        vcsRepositories_ = {{"repositories", Json::array()}, {"errors", Json::array()}, {"truncated", false}};
        auto engine = engine_;
        vcsRepositoriesFuture_ =
            std::async(std::launch::async, [engine] { return engine->dispatch("vcs.repositories"); });
    }
    const bool busy = vcsFuture_.valid() || taskFuture_.valid() || gitFuture_.valid();
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::beginProperties("##git-repositories", 130)) {
            cyui::property(tr("Git repository"));
            const auto automatic = tr("Automatic (active file)");
            if (ImGui::BeginCombo("##git-repository",
                                  vcsRepository_.empty() ? automatic : vcsRepository_.c_str())) {
                auto choose = [&](const std::string &root, const std::string &identity) {
                    vcsRepository_ = root;
                    vcsRepositoryIdentity_ = identity;
                    vcsContextPath_.clear();
                    lastGitRefresh_ = -10;
                    refreshGit();
                };
                if (ImGui::Selectable(automatic, vcsRepository_.empty()))
                    choose("", "");
                for (const auto &entry : vcsRepositories_.value("repositories", Json::array())) {
                    const auto root = entry.at("root").get<std::string>();
                    if (ImGui::Selectable(root.c_str(), root == vcsRepository_))
                        choose(root, entry.at("identity"));
                }
                ImGui::EndCombo();
            }
            cyui::endProperties();
        }
    }
    {
        cyui::ScopedDisabled disabled(vcsRepositoriesFuture_.valid());
        if (cyui::ghostButton(tr("Rescan Git folders"))) {
            vcsRepositories_ = nullptr;
            return;
        }
    }
    ImGui::SameLine();
    if (vcsRepositoriesFuture_.valid())
        cyui::spinner("##git-discovery");
    else
        cyui::textDim("%s", vcsContext_.value("gitRepository", std::string{}).c_str());
    if (vcsRepositories_.value("truncated", false))
        cyui::callout(
            cyui::Tone::warning,
            tr("Git folder discovery reached its limit. Automatic selection still follows the active file."));
    for (const auto &error : vcsRepositories_.value("errors", Json::array()))
        cyui::textDim("%s", error.value("error", std::string{}).c_str());
}
void Application::gitWorktreeControls() {
    const bool busy = vcsFuture_.valid() || taskFuture_.valid() || gitFuture_.valid();
    if (!vcsWorktreesRequested_ && !busy && vcsContextPath_ == vcsRequestPath()) {
        vcsWorktreesRequested_ = true;
        vcsTask("vcs.worktrees");
    }
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::button(tr("Refresh worktrees"))) {
            vcsWorktreesRequested_ = true;
            vcsTask("vcs.worktrees");
        }
    }
    if (!vcsWorktrees_.contains("revision")) {
        cyui::emptyState(tr("Worktree catalogue unavailable"),
                         tr("Refresh the catalogue before creating or changing a worktree."),
                         cyui::icon::folder);
        return;
    }
    auto parameters = [&](const std::string &path) {
        return Json{{"path", path},
                    {"expectedRevision", vcsWorktrees_.at("revision")},
                    {"repository", vcsWorktrees_.at("repository")}};
    };
    if (cyui::beginSection(tr("Create linked worktree"))) {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::beginProperties("##new-worktree", 150)) {
            cyui::property(tr("New workspace folder"));
            ImGui::InputText("##worktree-folder", &vcsWorktreeDestination_);
            cyui::property(tr("Checkout mode"));
            const char *modes[] = {tr("New branch"), tr("Existing branch"), tr("Detached HEAD")};
            ImGui::Combo("##worktree-mode", &vcsWorktreeMode_, modes, 3);
            if (vcsWorktreeMode_ != 2) {
                cyui::property(tr("Branch"));
                ImGui::InputText("##worktree-branch", &vcsWorktreeBranch_);
            }
            if (vcsWorktreeMode_ != 1) {
                cyui::property(tr("Start revision"));
                ImGui::InputText("##worktree-start", &vcsWorktreeStart_);
            }
            cyui::endProperties();
        }
        cyui::textDim("%s", tr("The parent folder must exist. Git checkout can execute configured hooks."));
        if (cyui::primaryButton(tr("Create worktree"))) {
            const char *modes[] = {"newBranch", "existingBranch", "detached"};
            vcsTask("vcs.worktree.add", {{"destination", vcsWorktreeDestination_},
                                         {"branch", vcsWorktreeBranch_},
                                         {"start", vcsWorktreeStart_},
                                         {"mode", modes[vcsWorktreeMode_]},
                                         {"expectedRevision", vcsWorktrees_.at("revision")},
                                         {"repository", vcsWorktrees_.at("repository")}});
        }
        cyui::endSection();
    }
    if (cyui::beginProperties("##worktree-lock-options", 150)) {
        cyui::property(tr("Worktree lock reason"));
        ImGui::InputText("##worktree-reason", &vcsWorktreeReason_);
        cyui::endProperties();
    }
    for (const auto &entry : vcsWorktrees_.at("entries")) {
        const auto path = entry.at("path").get<std::string>();
        cyui::ScopedID id(path.c_str());
        if (cyui::beginCard("##worktree", path.c_str())) {
            cyui::textDim("%s", entry.value("branch", std::string{}).c_str());
            cyui::textFaint("%s", entry.value("head", std::string{}).c_str());
            if (entry.at("current").get<bool>())
                cyui::badge(tr("Selected repository"), cyui::Tone::accent);
            if (entry.at("locked").get<bool>()) {
                cyui::badge(tr("Locked"), cyui::Tone::warning);
                cyui::textDim("%s", entry.at("lockReason").get_ref<const std::string &>().c_str());
            }
            if (entry.at("prunable").get<bool>())
                cyui::textDim("%s", entry.at("pruneReason").get_ref<const std::string &>().c_str());
            if (cyui::button(tr("Open in new window")))
                newWindow(utf8Path(path));
            const bool writable = entry.at("insideWorkspace").get<bool>() &&
                                  entry.at("available").get<bool>() && !entry.at("current").get<bool>() &&
                                  !entry.at("primary").get<bool>() && !entry.at("bare").get<bool>();
            if (!entry.at("insideWorkspace").get<bool>())
                cyui::textDim("%s", tr("Outside workspace: catalogue only."));
            {
                cyui::ScopedDisabled disabled(busy || entry.at("insideWorkspace") != true ||
                                              entry.at("primary") == true || entry.at("current") == true ||
                                              entry.at("bare") == true || entry.at("locked") == true);
                const bool available = entry.at("available").get<bool>();
                if (cyui::button(cyui::trLabel(available ? "Move linked worktree" : "Repair moved worktree")
                                     .c_str())) {
                    vcsWorktreeReview_ = {{"action", available ? "move" : "repair"},
                                          {"path", entry.at("root")},
                                          {"destination", ""},
                                          {"repository", vcsWorktrees_.at("repository")},
                                          {"expectedRepository", vcsWorktrees_.at("identity")},
                                          {"expectedRevision", vcsWorktrees_.at("revision")}};
                    vcsWorktreeRemoveOpen_ = false;
                }
            }
            cyui::ScopedDisabled disabled(busy || !writable);
            if (entry.at("locked").get<bool>()) {
                if (cyui::button(tr("Unlock worktree")))
                    vcsTask("vcs.worktree.unlock", parameters(entry.at("root")));
            } else {
                if (cyui::button(tr("Lock worktree"))) {
                    auto p = parameters(entry.at("root"));
                    p["reason"] = vcsWorktreeReason_;
                    vcsTask("vcs.worktree.lock", p);
                }
                ImGui::SameLine();
                if (cyui::dangerButton(tr("Remove clean worktree"))) {
                    vcsWorktreeRemoval_ = parameters(entry.at("root"));
                    vcsWorktreeRemoveOpen_ = true;
                }
            }
        }
        cyui::endCard();
    }
    gitWorktreeMaintenanceControls(busy);
}
void Application::gitWorktreeMaintenanceControls(bool busy) {
    if (!vcsWorktreeReview_.contains("action"))
        return;
    const bool moving = vcsWorktreeReview_.at("action") == "move";
    const bool changed = vcsWorktreeReview_.at("repository") != vcsWorktrees_.at("repository") ||
                         vcsWorktreeReview_.at("expectedRepository") != vcsWorktrees_.at("identity") ||
                         vcsWorktreeReview_.at("expectedRevision") != vcsWorktrees_.at("revision");
    ImGui::PushTextWrapPos(0.0f);
    cyui::sectionTitle(tr(moving ? "Review worktree move" : "Review worktree link repair"));
    ImGui::TextUnformatted(vcsWorktreeReview_.at("path").get_ref<const std::string &>().c_str());
    {
        cyui::ScopedDisabled disabled(busy || changed);
        if (cyui::beginProperties("##worktree-maintenance")) {
            cyui::property(tr(moving ? "New workspace folder" : "Moved worktree folder"));
            auto destination = vcsWorktreeReview_.at("destination").get<std::string>();
            if (ImGui::InputText("##worktree-maintenance-destination", &destination)) {
                vcsWorktreeReview_["destination"] = destination;
                vcsWorktreeReview_.erase("plan");
            }
            cyui::endProperties();
        }
    }
    cyui::textDim(
        "%s",
        tr(moving
               ? "The destination must be new and its parent must exist. Local files and staged changes move "
                 "with the checkout. Close its documents first."
               : "Select the existing folder moved manually from this missing path. Repair requires the main "
                 "checkout and all linked worktrees inside this workspace, with closed linked documents."));
    cyui::textDim("%s", tr("No force, checkout, staging, commit or Git transport is requested. Partial "
                           "outcomes require inspection; this has no editor Undo."));
    if (changed)
        cyui::textColoured(cyui::palette().warning, "%s",
                           tr("Worktree state changed. Cancel and review the action again."));
    {
        cyui::ScopedDisabled disabled(busy || changed || vcsWorktreeReview_.at("destination") == "");
        if (cyui::button(cyui::trLabel("Preview worktree maintenance").c_str())) {
            vcsWorktreeReview_.erase("plan");
            auto input = vcsWorktreeReview_;
            input.erase("plan");
            vcsTask("vcs.worktree.plan", input);
        }
    }
    if (vcsWorktreeReview_.contains("plan")) {
        const auto &plan = vcsWorktreeReview_.at("plan");
        cyui::textDim("%s", tr("Verified worktree paths"));
        for (const auto &folder : plan.at("checkedRoots"))
            ImGui::BulletText("%s", folder.get_ref<const std::string &>().c_str());
        if (!moving)
            cyui::textColoured(cyui::palette().warning, "%s",
                               tr("Git repair can update linking files in every listed worktree. Other "
                                  "missing, external, locked or unreviewed links block this action."));
        cyui::ScopedDisabled disabled(busy || changed);
        if (cyui::primaryButton(cyui::trLabel("Apply worktree maintenance").c_str())) {
            auto input = vcsWorktreeReview_;
            input.erase("plan");
            input.erase("action");
            input["expectedPlanRevision"] = plan.at("revision");
            vcsTask(moving ? "vcs.worktree.move" : "vcs.worktree.repair", input);
            vcsWorktreeReview_ = Json::object();
        }
    }
    if (cyui::button(cyui::trLabel("Cancel worktree maintenance").c_str()) ||
        ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        vcsWorktreeReview_ = Json::object();
    ImGui::PopTextWrapPos();
}
void Application::gitWorktreeConfirmation() {
    const bool busy = vcsFuture_.valid() || taskFuture_.valid() || gitFuture_.valid();
    const auto title = label("Remove linked worktree?", "remove-git-worktree");
    if (cyui::beginModal(title.c_str(), &vcsWorktreeRemoveOpen_, 560)) {
        cyui::textDim("%s", vcsWorktreeRemoval_.value("path", std::string{}).c_str());
        cyui::callout(cyui::Tone::warning, tr("This removes the clean linked checkout. The branch is "
                                              "retained. Close its open documents first."));
        const bool changed = vcsWorktreeRemoval_.value("repository", std::string{}) !=
                             vcsContext_.value("gitRepository", std::string{});
        if (changed)
            cyui::callout(cyui::Tone::warning, tr("Repository changed. Cancel and review the action again."));
        {
            cyui::ScopedDisabled disabled(busy || changed);
            if (cyui::dangerButton(tr("Remove clean worktree"))) {
                vcsTask("vcs.worktree.remove", vcsWorktreeRemoval_);
                vcsWorktreeRemoveOpen_ = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (cyui::button(tr("Cancel")) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            vcsWorktreeRemoveOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        cyui::endModal();
    }
}
} // namespace cycode
