#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
void Application::clearGitCheckout() {
    vcsCheckoutPath_.clear();
    vcsCheckoutAction_ = 0;
    vcsCheckoutReviewRequested_ = false;
    vcsCheckoutPlan_ = vcsCheckoutReceipt_ = Json::object();
    vcsCheckoutCandidates_ = Json::array();
}
void Application::updateGitCheckout(const Json &result, bool oldRepository) {
    if (vcsOperation_ == "vcs.checkout.plan") {
        if (!oldRepository && vcsCheckoutReviewRequested_ && result.at("path") == vcsCheckoutPath_ &&
            result.at("action") == (vcsCheckoutAction_ == 0 ? "materialize" : "remove"))
            vcsCheckoutPlan_ = result;
        return;
    }
    vcsCheckoutPlan_ = Json::object();
    vcsCheckoutReviewRequested_ = false;
    output_ += "\n> " + vcsOperation_ + " [" + vcsOperationRepository_ + "] " +
               result.at("path").get<std::string>() + "\n" + result.value("error", std::string{});
    if (!oldRepository)
        vcsCheckoutReceipt_ = result;
    if (result.at("state") == "partial")
        error(tr(
            "Working-file action is incomplete. Inspect the receipt and project trash before continuing."));
    else if (!result.contains("inspectionError"))
        cyui::notify(tr("Working-file action completed; Git index retained."), cyui::Tone::success);
    if (result.contains("inspectionError"))
        error(result.at("inspectionError"));
    refreshFiles(false);
}
void Application::gitCheckoutControls() {
    const bool busy = vcsFuture_.valid() || taskFuture_.valid() || gitFuture_.valid();
    ImGui::Separator();
    ImGui::TextUnformatted(tr("Synchronize working file"));
    ImGui::TextWrapped("%s",
                       tr("Copy exact staged bytes or remove a path absent from the index. Existing content "
                          "is retained in project trash. Close affected text documents first."));
    ImGui::TextWrapped("%s", tr("One file per transaction. Git filters and line-ending conversion are not "
                                "applied; the index stays unchanged. This file action has a separate Undo."));
    {
        cyui::ScopedDisabled disabled(busy);
        if (!vcsCheckoutCandidates_.empty() &&
            ImGui::BeginCombo(tr("Resolved paths"), vcsCheckoutPath_.c_str())) {
            for (const auto &item : vcsCheckoutCandidates_)
                if (ImGui::Selectable(item.at("path").get_ref<const std::string &>().c_str(),
                                      item.at("path") == vcsCheckoutPath_)) {
                    vcsCheckoutPath_ = item.at("path");
                    vcsCheckoutAction_ = item.at("deleted").get<bool>() ? 1 : 0;
                    vcsCheckoutPlan_ = Json::object();
                    vcsCheckoutReviewRequested_ = false;
                }
            ImGui::EndCombo();
        }
        if (ImGui::InputText(tr("Repository-relative working file"), &vcsCheckoutPath_)) {
            vcsCheckoutPlan_ = Json::object();
            vcsCheckoutReviewRequested_ = false;
        }
        if (ImGui::RadioButton(tr("Copy staged bytes"), &vcsCheckoutAction_, 0)) {
            vcsCheckoutPlan_ = Json::object();
            vcsCheckoutReviewRequested_ = false;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(tr("Remove untracked working file"), &vcsCheckoutAction_, 1)) {
            vcsCheckoutPlan_ = Json::object();
            vcsCheckoutReviewRequested_ = false;
        }
        if (!vcsCheckoutPath_.empty() && cyui::button(tr("Preview working-file action"))) {
            vcsCheckoutReviewRequested_ = true;
            vcsCheckoutPlan_ = Json::object();
            vcsTask("vcs.checkout.plan", {{"path", vcsCheckoutPath_},
                                          {"action", vcsCheckoutAction_ == 0 ? "materialize" : "remove"}});
        }
    }
    if (vcsCheckoutPlan_.contains("id")) {
        cyui::textDim(
            "%s: %llu -> %llu", vcsCheckoutPlan_.at("path").get_ref<const std::string &>().c_str(),
            static_cast<unsigned long long>(vcsCheckoutPlan_.at("originalBytes").get<uint64_t>()),
            static_cast<unsigned long long>(vcsCheckoutPlan_.at("replacementBytes").get<uint64_t>()));
        if (vcsCheckoutPlan_.at("blocked").get<bool>())
            ImGui::TextWrapped("%s", tr("Close affected documents and prepare a fresh preview."));
        {
            cyui::ScopedDisabled disabled(busy || vcsCheckoutPlan_.at("blocked").get<bool>());
            if (cyui::primaryButton(tr("Apply reviewed working-file action")))
                vcsTask("vcs.checkout.apply", {{"id", vcsCheckoutPlan_.at("id")},
                                               {"expectedPlanRevision", vcsCheckoutPlan_.at("revision")}});
        }
    }
    if (cyui::button(tr("Cancel working-file preview"))) {
        vcsCheckoutPlan_ = Json::object();
        vcsCheckoutReviewRequested_ = false;
    }
    if (vcsCheckoutReceipt_.contains("id")) {
        const auto status = vcsCheckoutReceipt_.at("state").get<std::string>();
        cyui::textDim("%s: %s", vcsCheckoutReceipt_.at("path").get_ref<const std::string &>().c_str(),
                      tr(status == "applied"  ? "Working file updated"
                         : status == "undone" ? "Working file restored"
                                              : "Working file requires inspection"));
        const auto backup = vcsCheckoutReceipt_.value("backup", std::string{}),
                   undoBackup = vcsCheckoutReceipt_.value("undoBackup", std::string{});
        if (!backup.empty())
            ImGui::Text("%s: %s", tr("Original file backup"), backup.c_str());
        if (!undoBackup.empty())
            ImGui::Text("%s: %s", tr("Undo displaced-file backup"), undoBackup.c_str());
        if (!vcsCheckoutReceipt_.value("error", std::string{}).empty())
            ImGui::TextWrapped("%s", vcsCheckoutReceipt_.at("error").get_ref<const std::string &>().c_str());
        if (vcsCheckoutReceipt_.contains("inspectionError"))
            ImGui::TextWrapped(
                "%s", vcsCheckoutReceipt_.at("inspectionError").get_ref<const std::string &>().c_str());
        if (cyui::button(tr("Open project trash"))) {
            resourceTrashView_ = true;
            showPanel("Files", "resources");
            resourceTrash_ = Json::object();
        }
        {
            cyui::ScopedDisabled disabled(busy || !vcsCheckoutReceipt_.value("canUndo", false));
            if (cyui::button(tr("Undo working-file synchronization")))
                vcsTask("vcs.checkout.undo", {{"id", vcsCheckoutReceipt_.at("id")},
                                              {"expectedPlanRevision", vcsCheckoutReceipt_.at("revision")}});
        }
    }
    ImGui::Separator();
}
} // namespace cycode
