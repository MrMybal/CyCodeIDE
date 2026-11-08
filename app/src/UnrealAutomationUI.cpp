#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
#include <sstream>
namespace cycode {
Json Application::unrealAutomationParameters() const {
    Json maps = Json::array();
    std::istringstream lines(unrealAutomationMaps_);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty())
            maps.push_back(line);
    }
    return {{"project", unrealProject_},
            {"engineRoot", unrealEngine_},
            {"target", unrealAutomationTarget_},
            {"platform", unrealPlatform_},
            {"configuration", unrealConfiguration_},
            {"action", unrealAutomationAction_ ? "package" : "cook"},
            {"maps", maps},
            {"build", unrealAutomationBuild_},
            {"pak", unrealAutomationPak_},
            {"ioStore", unrealAutomationIoStore_},
            {"compressed", unrealAutomationCompressed_},
            {"timeoutSeconds", unrealAutomationTimeout_}};
}
void Application::clearUnrealAutomation() {
    unrealAutomationPlan_ = unrealAutomationResult_ = unrealAutomationPending_ = Json::object();
    unrealAutomationTarget_.clear();
    unrealAutomationMaps_.clear();
    unrealAutomationAction_ = 0;
    unrealAutomationTimeout_ = 3600;
    unrealAutomationBuild_ = unrealAutomationPak_ = true;
    unrealAutomationIoStore_ = unrealAutomationCompressed_ = false;
}
void Application::unrealAutomationResult(const Json &result) {
    if (taskMethod_ == "unreal.automationPlan") {
        if (unrealAutomationPending_ == unrealAutomationParameters())
            unrealAutomationPlan_ = result;
    } else {
        unrealAutomationPlan_ = Json::object();
        unrealAutomationPending_ = Json::object();
        unrealAutomationResult_ = result;
        if (result.contains("receiptError"))
            error(result.at("receiptError"));
        refreshFiles(false);
    }
}
void Application::unrealAutomationControls() {
    if (!ImGui::CollapsingHeader(tr("Cook and package Unreal content")))
        return;
    const bool busy = taskFuture_.valid() || unrealAnalysisFuture_.valid();
    if (unrealAutomationTarget_.empty())
        for (const auto &target : unreal_.at("targets")) {
            const auto name = target.at("name").get<std::string>();
            if (!name.ends_with("Editor")) {
                unrealAutomationTarget_ = name;
                break;
            }
        }
    const auto previous = unrealAutomationParameters();
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::beginProperties("##ue-automation-properties", 160)) { // CyUI scales this logical width.
            cyui::property(tr("Operation"));
            ImGui::RadioButton(tr("Cook content"), &unrealAutomationAction_, 0);
            ImGui::SameLine();
            ImGui::RadioButton(tr("Package and archive"), &unrealAutomationAction_, 1);
            cyui::property(tr("Game target"));
            if (ImGui::BeginCombo("##uat-target", unrealAutomationTarget_.c_str())) {
                for (const auto &target : unreal_.at("targets")) {
                    const auto name = target.at("name").get<std::string>();
                    if (name.ends_with("Editor"))
                        continue;
                    if (ImGui::Selectable(name.c_str(), name == unrealAutomationTarget_))
                        unrealAutomationTarget_ = name;
                }
                ImGui::EndCombo();
            }
            cyui::property(tr("Maps to cook"));
            ImGui::InputTextMultiline("##uat-maps", &unrealAutomationMaps_, {0, cyui::dp(76)});
            cyui::property(tr("Build game code"));
            ImGui::Checkbox("##uat-build", &unrealAutomationBuild_);
            if (unrealAutomationAction_) {
                cyui::property(tr("Pak containers"));
                if (ImGui::Checkbox("##uat-pak", &unrealAutomationPak_) && !unrealAutomationPak_)
                    unrealAutomationIoStore_ = unrealAutomationCompressed_ = false;
                cyui::ScopedDisabled noPak(!unrealAutomationPak_);
                cyui::property(tr("IoStore containers"));
                ImGui::Checkbox("##uat-iostore", &unrealAutomationIoStore_);
                cyui::property(tr("Compress package"));
                ImGui::Checkbox("##uat-compressed", &unrealAutomationCompressed_);
            }
            cyui::property(tr("Deadline (seconds)"));
            ImGui::InputInt("##uat-timeout", &unrealAutomationTimeout_);
            cyui::endProperties();
        }
    }
    const auto current = unrealAutomationParameters();
    if (previous != current || (!unrealAutomationPending_.empty() && unrealAutomationPending_ != current))
        unrealAutomationPlan_ = Json::object();
    ImGui::TextWrapped(
        "%s", tr("Use one /Game/Folder/Map package per line, without .umap. Leave empty to use project "
                 "packaging defaults. Platform and configuration come from the Unreal settings above."));
    ImGui::TextWrapped(
        "%s",
        tr("AutomationTool executes trusted project rules and content tools. Existing cook/build caches can "
           "change; packaging uses a new staging/archive folder. The packaged game is not launched."));
    const bool desktop = unrealPlatform_ == "Win64" || unrealPlatform_ == "Linux" ||
                         unrealPlatform_ == "LinuxArm64" || unrealPlatform_ == "Mac";
    if (!desktop)
        cyui::textColoured(cyui::palette().warning, "%s",
                           tr("Cooking and packaging currently require a desktop target platform."));
    {
        cyui::ScopedDisabled disabled(busy || !desktop || unrealAutomationTarget_.empty());
        if (cyui::button(tr("Preview Unreal automation"))) {
            unrealAutomationPlan_ = Json::object();
            unrealAutomationPending_ = current;
            task("unreal.automationPlan", current);
        }
    }
    if (unrealAutomationPlan_.contains("id")) {
        cyui::textDim("%s: %s", tr("Output directory"),
                      unrealAutomationPlan_.at("outputDirectory").get_ref<const std::string &>().c_str());
        if (!unrealAutomationPlan_.at("archiveDirectory").get_ref<const std::string &>().empty())
            cyui::textDim(
                "%s: %s", tr("Archive directory"),
                unrealAutomationPlan_.at("archiveDirectory").get_ref<const std::string &>().c_str());
        if (ImGui::CollapsingHeader(tr("Reviewed AutomationTool arguments")))
            for (const auto &arg : unrealAutomationPlan_.at("arguments"))
                ImGui::TextWrapped("%s", arg.get_ref<const std::string &>().c_str());
        ImGui::TextWrapped("%s", tr("The preview expires after ten minutes and permits one attempt. Intent, "
                                    "output log and result remain in the workspace after errors or "
                                    "cancellation; inspect them before creating a new preview."));
        {
            cyui::ScopedDisabled disabled(busy);
            if (cyui::button(tr("Run reviewed Unreal automation"))) {
                auto params =
                    Json{{"planId", unrealAutomationPlan_.at("id")},
                         {"expectedRevision", unrealAutomationPlan_.at("revision")},
                         {"timeoutSeconds", unrealAutomationPlan_.at("request").at("timeoutSeconds")}};
                unrealAutomationPlan_ = Json::object();
                task("unreal.automationRun", params);
            }
        }
        ImGui::SameLine();
        if (cyui::button(tr("Cancel automation preview"))) {
            unrealAutomationPlan_ = Json::object();
            unrealAutomationPending_ = Json::object();
        }
    }
    if (unrealAutomationResult_.contains("outputDirectory")) {
        const auto path = unrealAutomationResult_.at("outputDirectory").get<std::string>();
        ImGui::TextWrapped("%s: %s", tr("Last automation output"), path.c_str());
        if (unrealAutomationResult_.contains("inspectionError"))
            ImGui::TextWrapped(
                "%s", unrealAutomationResult_.at("inspectionError").get_ref<const std::string &>().c_str());
        if (unrealAutomationResult_.contains("receiptError"))
            ImGui::TextWrapped(
                "%s", unrealAutomationResult_.at("receiptError").get_ref<const std::string &>().c_str());
        if (cyui::button(tr("Copy automation output path")))
            ImGui::SetClipboardText(pathUtf8(engine_->workspace() / utf8Path(path)).c_str());
        const auto log = path + "/output.log";
        std::error_code logError;
        if (std::filesystem::is_regular_file(engine_->workspace() / utf8Path(log), logError)) {
            ImGui::SameLine();
            if (cyui::button(tr("Open automation log")))
                open(log);
        }
        cyui::textDim("%s", tr(unrealAutomationResult_.value("archiveAvailable", false)
                                   ? "An archive file was observed. Package installation remains unverified."
                                   : "No archive file was confirmed for this result."));
    }
}
} // namespace cycode
