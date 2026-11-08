#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
namespace {
const char *replayAction(int index) {
    return index == 0 ? "cherryPick" : index == 1 ? "revert" : index == 2 ? "continue" : "quit";
}
std::string replayBlocker(const std::string &code) {
    if (code == "metadataOpen")
        return cyui::tr("Close open Git metadata documents.");
    if (code == "autostash")
        return cyui::tr("This merge has an autostash; finish it with Git before using these actions.");
    if (code == "activeOperation")
        return cyui::tr("Another Git operation is active.");
    if (code == "notSingleReplay")
        return cyui::tr("This action requires an active single-commit cherry-pick or revert.");
    if (code == "unborn")
        return cyui::tr("HEAD has no commit.");
    if (code == "openDocuments")
        return cyui::tr("Close repository documents before replaying commits.");
    if (code == "workingChanges")
        return cyui::tr("Working files differ from the index. Review or synchronize them first.");
    if (code == "hiddenIndexFlags")
        return cyui::tr(
            "Clear skip-worktree, assume-unchanged or intent-to-add flags before committing here.");
    if (code == "unmerged")
        return cyui::tr("Resolve all index conflicts first.");
    return cyui::tr("Index changes must be committed or removed before starting a replay.");
}
} // namespace
void Application::invalidateGitReplay() {
    vcsReplayState_ = vcsReplayPlan_ = Json::object();
    vcsReplayRequested_ = vcsReplayReviewRequested_ = false;
}
void Application::clearGitReplay() {
    invalidateGitReplay();
    vcsReplayResult_ = Json::object();
    vcsReplayCommit_.clear();
    vcsReplayAction_ = 0;
}
void Application::updateGitReplay(const Json &result, bool oldRepository) {
    if (vcsOperation_ == "vcs.replay.state") {
        if (!oldRepository) {
            vcsReplayState_ = result;
            vcsReplayPlan_ = Json::object();
            vcsReplayReviewRequested_ = false;
        }
    } else if (vcsOperation_ == "vcs.replay.plan") {
        if (!oldRepository && vcsReplayReviewRequested_ && vcsReplayState_.contains("revision") &&
            result.at("stateRevision") == vcsReplayState_.at("revision") &&
            result.at("request").at("action") == replayAction(vcsReplayAction_) &&
            result.at("request").at("commit") == (vcsReplayAction_ < 2 ? vcsReplayCommit_ : std::string{}))
            vcsReplayPlan_ = result;
    } else {
        invalidateGitReplay();
        if (!oldRepository) {
            vcsReplayResult_ = result;
            if (result.contains("state")) {
                vcsReplayState_ = result.at("state");
                vcsReplayRequested_ = true;
            }
        }
        output_ += "\n> " + vcsOperation_ + " [" + vcsOperationRepository_ + "]\n" +
                   result.value("output", std::string{});
        if (result.value("exitCode", 0) || result.value("timedOut", false) ||
            result.value("truncated", false))
            error(tr("Task failed. See output."));
        else if (!result.contains("inspectionError"))
            cyui::notify(tr("Replay action completed."), cyui::Tone::success);
        if (result.contains("inspectionError"))
            error(result.at("inspectionError"));
        refreshFiles(false);
    }
}
void Application::gitReplayControls() {
    const bool busy = vcsFuture_.valid() || taskFuture_.valid() || gitFuture_.valid();
    if (!vcsReplayRequested_ && !busy && vcsContextPath_ == vcsRequestPath()) {
        vcsReplayRequested_ = true;
        vcsTask("vcs.replay.state");
    }
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::button(tr("Refresh replay state")))
            vcsTask("vcs.replay.state");
    }
    if (vcsReplayResult_.contains("receipt")) {
        cyui::textDim("%s: %s", tr("Replay intent receipt"),
                      vcsReplayResult_.at("receipt").get_ref<const std::string &>().c_str());
        if (vcsReplayResult_.contains("inspectionError"))
            ImGui::TextWrapped("%s",
                               vcsReplayResult_.at("inspectionError").get_ref<const std::string &>().c_str());
        if (!vcsReplayResult_.value("newCommit", std::string{}).empty())
            ImGui::Text("%s: %s", tr("Commit"),
                        vcsReplayResult_.at("newCommit").get_ref<const std::string &>().c_str());
    }
    if (!vcsReplayState_.contains("revision"))
        return;
    const auto kind = vcsReplayState_.at("kind").get<std::string>();
    const char *kindLabel = kind == "cherryPick" ? "Cherry-pick in progress"
                            : kind == "revert"   ? "Revert in progress"
                            : kind == "none"     ? "No replay is active."
                                                 : "Another Git operation is active.";
    ImGui::TextUnformatted(tr(kindLabel));
    if (!vcsReplayState_.at("operationCommit").get_ref<const std::string &>().empty())
        cyui::textDim("%s", vcsReplayState_.at("operationCommit").get_ref<const std::string &>().c_str());
    for (const auto &path : vcsReplayState_.at("conflicts"))
        ImGui::BulletText("%s", path.get_ref<const std::string &>().c_str());
    if (!vcsReplayState_.at("conflicts").empty() && cyui::button(tr("Review replay conflicts"))) {
        vcsSection_ = "Conflicts";
        vcsConflictRequested_ = false;
    }
    {
        cyui::ScopedDisabled disabled(busy);
        const char *labels[] = {"Cherry-pick commit", "Revert commit", "Continue resolved replay",
                                "Quit replay tracking"};
        for (int i = 0; i < 4; ++i) {
            if (i % 2)
                ImGui::SameLine();
            if (ImGui::RadioButton(tr(labels[i]), &vcsReplayAction_, i)) {
                vcsReplayPlan_ = Json::object();
                vcsReplayReviewRequested_ = false;
            }
        }
        if (vcsReplayAction_ < 2 &&
            ImGui::InputTextWithHint(tr("Source commit ID"), tr("Complete commit hash; choose from History"),
                                     &vcsReplayCommit_)) {
            vcsReplayPlan_ = Json::object();
            vcsReplayReviewRequested_ = false;
        }
    }
    ImGui::TextWrapped(
        "%s", tr(vcsReplayAction_ == 3
                     ? "Quit keeps working files and index entries. It does not restore the pre-replay state."
                     : "Replay creates an unsigned commit. Hooks are disabled; trusted Git filters or merge "
                       "drivers may run. Close repository documents first."));
    const auto &blocks = vcsReplayState_.at(vcsReplayAction_ < 2    ? "startBlockers"
                                            : vcsReplayAction_ == 2 ? "continueBlockers"
                                                                    : "quitBlockers");
    for (const auto &code : blocks)
        ImGui::TextWrapped("%s", replayBlocker(code.get<std::string>()).c_str());
    {
        cyui::ScopedDisabled disabled(busy);
        if (vcsReplayAction_ >= 2 && cyui::button(tr("Review staged replay diff")))
            vcsTask("vcs.diff", {{"staged", true}});
    }
    {
        cyui::ScopedDisabled disabled(busy || !blocks.empty() ||
                                      (vcsReplayAction_ < 2 && vcsReplayCommit_.empty()));
        if (cyui::button(tr("Preview replay action"))) {
            vcsReplayReviewRequested_ = true;
            vcsReplayPlan_ = Json::object();
            vcsTask("vcs.replay.plan", {{"action", replayAction(vcsReplayAction_)},
                                        {"commit", vcsReplayAction_ < 2 ? vcsReplayCommit_ : std::string{}},
                                        {"expectedRevision", vcsReplayState_.at("revision")}});
        }
    }
    if (vcsReplayPlan_.contains("revision")) {
        ImGui::TextUnformatted(tr("Reviewed replay action"));
        ImGui::TextWrapped("%s", vcsReplayPlan_.at("sourceMessage").get_ref<const std::string &>().c_str());
        cyui::textDim("%s", vcsReplayPlan_.at("sourceCommit").get_ref<const std::string &>().c_str());
        ImGui::TextUnformatted(vcsReplayPlan_.at("summary").get_ref<const std::string &>().c_str());
        for (const auto &path : vcsReplayPlan_.at("paths"))
            ImGui::BulletText("%s  %s", path.at("status").get_ref<const std::string &>().c_str(),
                              path.at("path").get_ref<const std::string &>().c_str());
        ImGui::TextWrapped("%s",
                           tr("Source changes are not a simulation of the resulting working tree. Conflicts "
                              "can stop the operation; review actual state before continuing."));
        ImGui::TextWrapped("%s", tr("An intent receipt is retained before dispatch. Inspect actual Git state "
                                    "after errors or uncertainty; this action has no editor Undo."));
        cyui::ScopedDisabled disabled(busy);
        if (vcsReplayAction_ < 2 && cyui::button(tr("Review source commit patch")))
            vcsTask("vcs.show", {{"revision", vcsReplayPlan_.at("sourceCommit")}});
        if (cyui::primaryButton(tr("Apply reviewed replay action"))) {
            auto params = vcsReplayPlan_.at("request");
            params["expectedRevision"] = vcsReplayPlan_.at("stateRevision");
            params["expectedPlanRevision"] = vcsReplayPlan_.at("revision");
            vcsTask("vcs.replay.apply", params);
        }
    }
    if (cyui::button(tr("Cancel replay preview"))) {
        vcsReplayPlan_ = Json::object();
        vcsReplayReviewRequested_ = false;
    }
}
} // namespace cycode
