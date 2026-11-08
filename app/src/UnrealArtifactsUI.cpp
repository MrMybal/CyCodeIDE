#include "cycode/Application.h"
#include <cyui/Core.h>
namespace cycode {
void Application::unrealArtifactsControls() {
    if (!ImGui::CollapsingHeader(tr("Unreal build outputs")))
        return;
    const bool busy =
        taskFuture_.valid() || debugFuture_.valid() || unrealAnalysisFuture_.valid() || pendingDebugBuild_;
    {
        cyui::ScopedDisabled disabled(busy);
        if (!debugUnrealLaunch_.is_null())
            ImGui::Checkbox(tr("Inspect bound launch outputs"), &unrealArtifactsBound_);
        if (cyui::button(tr("Inspect Unreal build outputs"))) {
            const auto params = unrealArtifactsBound_ && !debugUnrealLaunch_.is_null()
                                    ? debugUnrealLaunch_.at("profile")
                                    : buildParameters();
            task("unreal.artifacts", params);
        }
    }
    if (unrealArtifacts_.empty()) {
        ImGui::TextWrapped("%s", tr("Inspect the selected Editor receipt, project libraries and module "
                                    "manifests without running Unreal."));
        return;
    }
    if (unrealArtifacts_.contains("profile")) {
        const auto &profile = unrealArtifacts_.at("profile");
        cyui::textDim("%s: %s | %s | %s", tr("Last output inspection"),
                      profile.at("target").get_ref<const std::string &>().c_str(),
                      profile.at("platform").get_ref<const std::string &>().c_str(),
                      profile.at("configuration").get_ref<const std::string &>().c_str());
    }
    cyui::callout(unrealArtifacts_.value("ready", false) ? cyui::Tone::success : cyui::Tone::warning,
                  tr(unrealArtifacts_.value("ready", false)
                         ? "Inspected output metadata and files passed the scoped checks."
                         : "Build outputs are missing, mismatched or not fully inspected."));
    ImGui::TextWrapped("%s", tr("This checks project Editor outputs, not every engine/plugin dependency or "
                                "binary ABI. Results are observations; actual launch rechecks them."));
    for (const auto &issue : unrealArtifacts_.at("issues")) {
        ImGui::TextWrapped("%s: %s", issue.at("code").get_ref<const std::string &>().c_str(),
                           issue.at("detail").get_ref<const std::string &>().c_str());
        if (!issue.at("path").get_ref<const std::string &>().empty())
            cyui::textDim("%s", issue.at("path").get_ref<const std::string &>().c_str());
    }
    if (cyui::button(tr("Copy output inspection")))
        ImGui::SetClipboardText(unrealArtifacts_.dump(2).c_str());
    if (ImGui::TreeNode(tr("Inspected project files"))) {
        ImGui::RadioButton(tr("Libraries"), &unrealArtifactsGroup_, 0);
        ImGui::SameLine();
        ImGui::RadioButton(tr("Module manifests"), &unrealArtifactsGroup_, 1);
        ImGui::SameLine();
        ImGui::RadioButton(tr("Modules"), &unrealArtifactsGroup_, 2);
        const auto &entries = unrealArtifacts_.at(unrealArtifactsGroup_ == 0   ? "products"
                                                  : unrealArtifactsGroup_ == 1 ? "manifests"
                                                                               : "modules");
        ImGui::BeginChild("##unreal-artifact-files", {0, cyui::dp(200)}, ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);
        ImGuiListClipper rows;
        rows.Begin(static_cast<int>(entries.size()));
        while (rows.Step())
            for (int i = rows.DisplayStart; i < rows.DisplayEnd; ++i) {
                const auto &entry = entries.at(static_cast<size_t>(i));
                const auto &path = entry.at("path").get_ref<const std::string &>();
                if (!entry.value("present", true) || !entry.value("matchesEngine", true))
                    cyui::textColoured(cyui::palette().warning, "%s", path.c_str());
                else
                    ImGui::TextUnformatted(path.c_str());
                cyui::tooltip(path.c_str());
            }
        ImGui::EndChild();
        ImGui::TreePop();
    }
}
} // namespace cycode
