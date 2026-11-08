#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
namespace {
std::string blocker(const std::string &code) {
    if (code == "notMerge")
        return cyui::tr("No merge is active.");
    if (code == "otherOperation")
        return cyui::tr("Finish the other Git operation first.");
    if (code == "autostash")
        return cyui::tr("This merge has an autostash; finish it with Git before using these actions.");
    if (code == "metadataOpen")
        return cyui::tr("Close open Git metadata documents.");
    if (code == "headChanged")
        return cyui::tr("HEAD changed since the merge started; inspect Git state.");
    if (code == "unmerged")
        return cyui::tr("Resolve all index conflicts first.");
    if (code == "workingChanges")
        return cyui::tr("Working files differ from the index. Review or synchronize them first.");
    if (code == "dirtyBuffers")
        return cyui::tr("Save or reconcile modified repository documents.");
    if (code == "hiddenIndexFlags")
        return cyui::tr(
            "Clear skip-worktree, assume-unchanged or intent-to-add flags before committing here.");
    return cyui::tr("This workflow completes a merge with one incoming parent.");
}
} // namespace
void Application::invalidateGitMerge() {
    vcsMergeState_ = vcsMergePlan_ = Json::object();
    vcsMergeRequested_ = vcsMergeReviewRequested_ = false;
}
void Application::clearGitMerge() {
    invalidateGitMerge();
    vcsMergeResult_ = Json::object();
    vcsMergeMessage_.clear();
    vcsMergeHeadKey_.clear();
    vcsMergeHooks_ = vcsMergeMessageEdited_ = false;
    vcsMergeAction_ = 0;
}
void Application::updateGitMerge(const Json &result, bool oldRepository) {
    if (vcsOperation_ == "vcs.merge.state") {
        if (oldRepository)
            return;
        vcsMergeState_ = result;
        vcsMergePlan_ = Json::object();
        vcsMergeReviewRequested_ = false;
        const auto key = result.at("mergeHeads").dump() + result.at("head").get<std::string>();
        if (key != vcsMergeHeadKey_ || !vcsMergeMessageEdited_) {
            vcsMergeMessage_ = result.at("message").get<std::string>();
            vcsMergeMessage_ = vcsMergeMessage_.substr(0, vcsMergeMessage_.find('\n'));
            vcsMergeMessageEdited_ = false;
            vcsMergeHeadKey_ = key;
        }
    } else if (vcsOperation_ == "vcs.merge.plan") {
        const auto &request = result.at("request");
        if (!oldRepository && vcsMergeReviewRequested_ && vcsMergeState_.contains("revision") &&
            result.at("stateRevision") == vcsMergeState_.at("revision") &&
            request.at("action") == (vcsMergeAction_ == 0 ? "commit" : "quit") &&
            request.at("message") == (vcsMergeAction_ == 0 ? vcsMergeMessage_ : std::string{}) &&
            request.at("runHooks") == (vcsMergeAction_ == 0 && vcsMergeHooks_))
            vcsMergePlan_ = result;
    } else {
        invalidateGitMerge();
        if (!oldRepository) {
            vcsMergeResult_ = result;
            if (result.contains("state")) {
                vcsMergeState_ = result.at("state");
                vcsMergeRequested_ = true;
            }
            if (result.value("exitCode", 0) == 0 && !result.value("timedOut", false) &&
                !result.value("truncated", false) && !result.contains("inspectionError"))
                vcsMergeMessageEdited_ = false;
        }
        output_ += "\n> " + vcsOperation_ + " [" + vcsOperationRepository_ + "]\n" +
                   result.value("output", std::string{});
        if (result.value("exitCode", 0) || result.value("timedOut", false) ||
            result.value("truncated", false))
            error(tr("Task failed. See output."));
        else if (!result.contains("inspectionError"))
            cyui::notify(tr("Merge action completed."), cyui::Tone::success);
        if (result.contains("inspectionError"))
            error(result.at("inspectionError"));
        refreshFiles(false);
    }
}
void Application::gitMergeControls() {
    const bool busy = vcsFuture_.valid() || taskFuture_.valid() || gitFuture_.valid();
    if (!vcsMergeRequested_ && !busy && vcsContextPath_ == vcsRequestPath()) {
        vcsMergeRequested_ = true;
        vcsTask("vcs.merge.state");
    }
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::button(tr("Refresh merge state")))
            vcsTask("vcs.merge.state");
    }
    if (vcsMergeResult_.contains("receipt")) {
        cyui::textDim("%s: %s", tr("Merge intent receipt"),
                      vcsMergeResult_.at("receipt").get_ref<const std::string &>().c_str());
        if (vcsMergeResult_.contains("inspectionError"))
            ImGui::TextWrapped("%s",
                               vcsMergeResult_.at("inspectionError").get_ref<const std::string &>().c_str());
        if (!vcsMergeResult_.value("newCommit", std::string{}).empty())
            ImGui::Text("%s: %s", tr("Commit"),
                        vcsMergeResult_.at("newCommit").get_ref<const std::string &>().c_str());
    }
    if (!vcsMergeState_.contains("revision"))
        return;
    if (vcsMergeState_.at("kind") != "merge") {
        ImGui::TextWrapped("%s",
                           tr(vcsMergeState_.at("kind") == "none"
                                  ? "No merge is active."
                                  : "Another Git operation is active; these merge actions are unavailable."));
        return;
    }
    ImGui::Text("%s: %s", tr("Branch"), vcsMergeState_.at("branch").get_ref<const std::string &>().c_str());
    for (const auto &head : vcsMergeState_.at("mergeHeads"))
        cyui::textDim("%s: %s", tr("Incoming merge parent"), head.get_ref<const std::string &>().c_str());
    if (!vcsMergeState_.at("conflicts").empty()) {
        ImGui::TextUnformatted(tr("Unresolved paths"));
        for (const auto &path : vcsMergeState_.at("conflicts"))
            ImGui::BulletText("%s", path.get_ref<const std::string &>().c_str());
        if (cyui::button(tr("Review merge conflicts"))) {
            vcsSection_ = "Conflicts";
            vcsConflictRequested_ = false;
        }
    }
    {
        cyui::ScopedDisabled disabled(busy);
        if (ImGui::RadioButton(tr("Complete merge commit"), &vcsMergeAction_, 0)) {
            vcsMergePlan_ = Json::object();
            vcsMergeReviewRequested_ = false;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(tr("Quit merge tracking"), &vcsMergeAction_, 1)) {
            vcsMergePlan_ = Json::object();
            vcsMergeReviewRequested_ = false;
        }
        if (vcsMergeAction_ == 0) {
            if (ImGui::InputTextMultiline(tr("Merge commit message"), &vcsMergeMessage_,
                                          {0, cyui::dp(100)})) {
                vcsMergeMessageEdited_ = true;
                vcsMergePlan_ = Json::object();
                vcsMergeReviewRequested_ = false;
            }
            if (ImGui::Checkbox(tr("Run configured Git commit hooks"), &vcsMergeHooks_)) {
                vcsMergePlan_ = Json::object();
                vcsMergeReviewRequested_ = false;
            }
        }
    }
    ImGui::TextWrapped("%s",
                       tr(vcsMergeAction_ == 0
                              ? "The complete staged index becomes an unsigned merge commit. Review all "
                                "paths. Untracked files stay outside it."
                              : "Quit removes merge bookkeeping while keeping working files and index "
                                "entries. The next ordinary commit will not retain these merge parents."));
    if (vcsMergeHooks_ && vcsMergeAction_ == 0)
        ImGui::TextWrapped("%s", tr("Configured hooks can run project tools and network commands. Close "
                                    "repository documents before enabling them."));
    const auto &blocks = vcsMergeState_.at(vcsMergeAction_ == 0 ? "commitBlockers" : "quitBlockers");
    for (const auto &code : blocks)
        ImGui::TextWrapped("%s", blocker(code.get<std::string>()).c_str());
    if (vcsMergeAction_ == 0 && vcsMergeHooks_ && vcsMergeState_.at("openDocuments").get<size_t>())
        ImGui::TextWrapped("%s", tr("Close repository documents before running hooks."));
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::button(tr("Review staged merge diff")))
            vcsTask("vcs.diff", {{"staged", true}});
    }
    if (ImGui::CollapsingHeader(tr("Staged merge paths"))) {
        for (const auto &entry : vcsMergeState_.at("stagedPaths"))
            ImGui::Text("%s  %s", entry.at("status").get_ref<const std::string &>().c_str(),
                        entry.at("path").get_ref<const std::string &>().c_str());
        ImGui::TextUnformatted(vcsMergeState_.at("stagedSummary").get_ref<const std::string &>().c_str());
    }
    {
        cyui::ScopedDisabled disabled(
            busy || !blocks.empty() ||
            (vcsMergeAction_ == 0 && (vcsMergeMessage_.empty() ||
                                      (vcsMergeHooks_ && vcsMergeState_.at("openDocuments").get<size_t>()))));
        if (cyui::button(tr("Preview merge action"))) {
            vcsMergeReviewRequested_ = true;
            vcsMergePlan_ = Json::object();
            vcsTask("vcs.merge.plan", {{"action", vcsMergeAction_ == 0 ? "commit" : "quit"},
                                       {"message", vcsMergeAction_ == 0 ? vcsMergeMessage_ : std::string{}},
                                       {"runHooks", vcsMergeAction_ == 0 && vcsMergeHooks_},
                                       {"expectedRevision", vcsMergeState_.at("revision")}});
        }
    }
    if (vcsMergePlan_.contains("revision")) {
        ImGui::TextUnformatted(tr("Reviewed merge action"));
        if (vcsMergeAction_ == 0) {
            ImGui::TextWrapped(
                "%s", vcsMergePlan_.at("request").at("message").get_ref<const std::string &>().c_str());
            for (const auto &parent : vcsMergePlan_.at("expectedParents"))
                cyui::textDim("%s", parent.get_ref<const std::string &>().c_str());
        }
        ImGui::TextWrapped("%s", tr("An intent receipt is retained before dispatch. Inspect actual Git state "
                                    "after errors or uncertainty; this action has no editor Undo."));
        {
            cyui::ScopedDisabled disabled(busy);
            if (cyui::primaryButton(tr("Apply reviewed merge action"))) {
                auto params = vcsMergePlan_.at("request");
                params["expectedRevision"] = vcsMergePlan_.at("stateRevision");
                params["expectedPlanRevision"] = vcsMergePlan_.at("revision");
                vcsTask(vcsMergeAction_ == 0 ? "vcs.merge.commit" : "vcs.merge.quit", params);
            }
        }
    }
    if (cyui::button(tr("Cancel merge preview"))) {
        vcsMergePlan_ = Json::object();
        vcsMergeReviewRequested_ = false;
    }
}
} // namespace cycode
