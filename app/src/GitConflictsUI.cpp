#include "cycode/Application.h"
#include <cyui/Core.h>
namespace cycode {
void Application::clearGitConflicts() {
    vcsConflictCatalog_ = vcsConflictChoices_ = vcsConflictReview_ = vcsConflictDetail_ = Json::object();
    vcsConflictRequested_ = false;
    vcsConflictInspectPath_.clear();
}
Json Application::gitConflictChoices() const {
    Json result = Json::array();
    for (const auto &[path, choice] : vcsConflictChoices_.items())
        result.push_back({{"path", path}, {"choice", choice}});
    return result;
}
void Application::updateGitConflicts(const Json &result, bool oldRepository) {
    if (vcsOperation_ == "vcs.conflict.list") {
        if (!oldRepository) {
            vcsConflictCatalog_ = result;
            vcsConflictChoices_ = vcsConflictReview_ = vcsConflictDetail_ = Json::object();
            vcsConflictInspectPath_.clear();
            vcsSection_ = "Conflicts";
        }
    } else if (vcsOperation_ == "vcs.conflict.inspect") {
        if (!oldRepository && vcsConflictCatalog_.contains("revision") &&
            result.at("catalogRevision") == vcsConflictCatalog_.at("revision") &&
            result.at("entry").at("path") == vcsConflictInspectPath_)
            vcsConflictDetail_ = result;
    } else if (vcsOperation_ == "vcs.conflict.plan") {
        if (!oldRepository && vcsConflictCatalog_.contains("revision") &&
            result.at("catalogRevision") == vcsConflictCatalog_.at("revision") &&
            result.at("choices") == gitConflictChoices() && !vcsConflictChoices_.empty())
            vcsConflictReview_ = result;
    } else {
        clearGitConflicts();
        vcsConflict_ = Json::object();
        if (!oldRepository && result.contains("changes")) {
            const bool confirmed = result.value("exitCode", 0) == 0 && !result.value("timedOut", false) &&
                                   !result.value("truncated", false) && !result.contains("catalogError");
            vcsCheckoutCandidates_ = confirmed ? result.at("changes") : Json::array();
            vcsCheckoutPlan_ = Json::object();
            vcsCheckoutReviewRequested_ = false;
            if (!vcsCheckoutCandidates_.empty()) {
                vcsCheckoutPath_ = vcsCheckoutCandidates_[0].at("path");
                vcsCheckoutAction_ = vcsCheckoutCandidates_[0].at("deleted").get<bool>() ? 1 : 0;
            }
        }
        if (!oldRepository && result.contains("catalog")) {
            vcsConflictCatalog_ = result.at("catalog");
            vcsConflictRequested_ = true;
        }
        output_ += "\n> " + vcsOperation_ + " [" + vcsOperationRepository_ + "]\n" +
                   result.value("output", std::string{});
        if (result.value("exitCode", 0) || result.value("timedOut", false) ||
            result.value("truncated", false))
            error(tr("Task failed. See output."));
        else if (!result.contains("catalogError"))
            cyui::notify(tr("Selected versions staged; working files unchanged."), cyui::Tone::success);
        if (result.contains("catalogError"))
            error(result.at("catalogError"));
    }
}
void Application::gitConflictControls() {
    const bool busy = vcsFuture_.valid() || taskFuture_.valid() || gitFuture_.valid();
    if (!vcsConflictRequested_ && !busy && vcsContextPath_ == vcsRequestPath()) {
        vcsConflictRequested_ = true;
        vcsTask("vcs.conflict.list");
    }
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::button(tr("Refresh conflict catalogue"))) {
            vcsConflictReview_ = Json::object();
            vcsTask("vcs.conflict.list");
        }
    }
    ImGui::TextWrapped("%s", tr("Choose versions for each repository-relative path. For a rename, review all "
                                "affected paths together; rename relationships are not inferred."));
    ImGui::TextWrapped(
        "%s", tr("Index-only resolution preserves local files and open buffers. Staged content can differ "
                 "from the editor, including conflict markers. A deletion removes only the index entry."));
    cyui::textDim("%s", tr("Ours and theirs name Git index stages 2 and 3; during rebase their branch roles "
                           "can be reversed."));
    if (!vcsConflictCatalog_.contains("revision"))
        return;
    if (vcsConflictCatalog_.at("entries").empty()) {
        cyui::textDim("%s", tr("No unresolved index entries."));
        return;
    }
    if (ImGui::BeginTable("##git-conflict-catalog", 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                          {0, cyui::dp(190)})) {
        ImGui::TableSetupColumn(tr("Status"), ImGuiTableColumnFlags_WidthFixed, cyui::dp(55));
        ImGui::TableSetupColumn(tr("File"));
        ImGui::TableSetupColumn(tr("Index resolution"));
        ImGui::TableSetupColumn(tr("Actions"));
        ImGui::TableHeadersRow();
        for (const auto &entry : vcsConflictCatalog_.at("entries")) {
            const auto path = entry.at("path").get<std::string>();
            ImGui::PushID(path.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.at("status").get_ref<const std::string &>().c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(path.c_str());
            if (!entry.at("supported").get<bool>() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", entry.at("reason").get_ref<const std::string &>().c_str());
            ImGui::TableNextColumn();
            {
                cyui::ScopedDisabled disabled(busy || !entry.at("supported").get<bool>());
                const std::string chosen = vcsConflictChoices_.value(path, std::string{});
                auto title = [&](const std::string &choice) {
                    return choice.empty()       ? tr("Keep unresolved")
                           : choice == "ours"   ? tr("Stage ours")
                           : choice == "theirs" ? tr("Stage theirs")
                           : choice == "base"   ? tr("Stage base")
                                                : tr("Stage deletion");
                };
                if (ImGui::BeginCombo("##resolution", title(chosen))) {
                    for (const auto *choice : {"", "ours", "theirs", "base", "delete"})
                        if (ImGui::Selectable(title(choice), chosen == choice)) {
                            if (*choice)
                                vcsConflictChoices_[path] = choice;
                            else
                                vcsConflictChoices_.erase(path);
                            vcsConflictReview_ = Json::object();
                        }
                    ImGui::EndCombo();
                }
            }
            ImGui::TableNextColumn();
            {
                cyui::ScopedDisabled disabled(busy);
                if (cyui::button(tr("Inspect versions"))) {
                    vcsConflictInspectPath_ = path;
                    vcsConflictDetail_ = Json::object();
                    vcsTask("vcs.conflict.inspect",
                            {{"path", path}, {"expectedRevision", vcsConflictCatalog_.at("revision")}});
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (vcsConflictDetail_.contains("entry")) {
        const auto &entry = vcsConflictDetail_.at("entry");
        ImGui::TextUnformatted(entry.at("path").get_ref<const std::string &>().c_str());
        bool text = true;
        for (const auto &[name, version] : vcsConflictDetail_.at("versions").items()) {
            const auto kind = version.at("kind").get<std::string>();
            const char *label = name == "base" ? tr("Base") : name == "ours" ? tr("Ours") : tr("Theirs");
            const char *type = kind == "absent"   ? tr("Absent")
                               : kind == "binary" ? tr("Binary or unsupported encoding")
                               : kind == "large"  ? tr("Too large for text preview")
                               : kind == "text"   ? tr("Text")
                                                  : tr("Unsupported");
            cyui::textDim("%s: %s (%llu)", label, type,
                          static_cast<unsigned long long>(version.at("bytes").get<uint64_t>()));
            if (kind != "text" && kind != "absent")
                text = false;
        }
        cyui::ScopedDisabled disabled(busy || !text || !entry.at("supported").get<bool>());
        if (cyui::button(tr("Open working text conflict")))
            vcsTask("vcs.conflicts", {{"path", entry.at("documentPath")}});
    }
    {
        cyui::ScopedDisabled disabled(busy || vcsConflictChoices_.empty());
        if (cyui::button(tr("Preview index resolution")))
            vcsTask("vcs.conflict.plan", {{"choices", gitConflictChoices()},
                                          {"expectedRevision", vcsConflictCatalog_.at("revision")}});
    }
    if (vcsConflictReview_.contains("revision")) {
        ImGui::TextUnformatted(tr("Reviewed index changes"));
        for (const auto &change : vcsConflictReview_.at("changes")) {
            ImGui::Text("%s: %s", change.at("path").get_ref<const std::string &>().c_str(),
                        change.at("deleted").get<bool>()
                            ? tr("Stage deletion")
                            : change.at("version").at("oid").get_ref<const std::string &>().c_str());
        }
        ImGui::TextWrapped("%s",
                           tr("This action stages Git objects without checkout, text conversion or editor "
                              "Undo. Refresh after an uncertain result; do not automatically retry."));
        {
            cyui::ScopedDisabled disabled(busy);
            if (cyui::primaryButton(tr("Apply reviewed index resolution")))
                vcsTask("vcs.conflict.apply", {{"choices", vcsConflictReview_.at("choices")},
                                               {"expectedRevision", vcsConflictReview_.at("catalogRevision")},
                                               {"expectedPlanRevision", vcsConflictReview_.at("revision")}});
        }
    }
    if (cyui::button(tr("Clear conflict choices"))) {
        vcsConflictChoices_ = vcsConflictReview_ = Json::object();
    }
    ImGui::Separator();
}
} // namespace cycode
