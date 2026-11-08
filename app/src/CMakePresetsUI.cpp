#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
std::string Application::presetActionKey() const {
    return Json{{"source", presetSource_}, {"kind", presetKind_}, {"preset", presetName_}}.dump();
}
void Application::presetRead(const std::string &method) {
    if (presetFuture_.valid())
        return;
    Json params = {{"source", presetSource_}};
    presetRequestKey_ = method == "build.presets" ? presetSource_ : presetActionKey();
    if (method == "build.presetPlan") {
        const char *kinds[] = {"test", "package", "workflow"};
        params["kind"] = kinds[presetKind_];
        params["preset"] = presetName_;
    }
    presetReadMethod_ = method;
    presetError_.clear();
    auto engine = engine_;
    presetFuture_ =
        std::async(std::launch::async, [engine, method, params] { return engine->dispatch(method, params); });
}
void Application::updatePresetActions() {
    if (!presetFuture_.valid() ||
        presetFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    const auto current = presetReadMethod_ == "build.presets" ? presetSource_ : presetActionKey();
    try {
        auto value = presetFuture_.get();
        if (presetRequestKey_ != current)
            return;
        if (presetReadMethod_ == "build.presets")
            presetCatalogue_ = std::move(value);
        else {
            presetPlan_ = std::move(value);
            presetPlanKey_ = presetRequestKey_;
        }
    } catch (const std::exception &e) {
        if (presetRequestKey_ == current)
            presetError_ = e.what();
    }
}
void Application::presetActionsPanel() {
    if (!beginPanel("CMake preset actions", "cmakePresets"))
        return;
    if (!presetInitialized_) {
        presetInitialized_ = true;
        presetSource_ = buildSource_;
    }
    if (presetCatalogue_.is_null() && !presetFuture_.valid()) {
        presetCatalogue_ = Json::object();
        presetRead("build.presets");
    }
    const bool reading = presetFuture_.valid();
    if (cyui::beginProperties("##preset-selection", 150)) {
        cyui::property(tr("CMake source directory"));
        if (ImGui::InputText("##preset-source", &presetSource_)) {
            presetCatalogue_ = nullptr;
            presetPlan_ = nullptr;
            presetName_.clear();
            presetError_.clear();
        }
        cyui::property(tr("Preset action"));
        const char *labels[] = {tr("Test preset"), tr("Package preset"), tr("Workflow preset")};
        if (ImGui::Combo("##preset-kind", &presetKind_, labels, 3)) {
            presetName_.clear();
            presetPlan_ = nullptr;
            presetError_.clear();
        }
        cyui::property(tr("Preset"));
        if (ImGui::BeginCombo("##action-preset",
                              presetName_.empty() ? tr("Select a preset") : presetName_.c_str())) {
            const char *families[] = {"testPresets", "packagePresets", "workflowPresets"};
            const auto entries = presetCatalogue_.is_object()
                                     ? presetCatalogue_.value(families[presetKind_], Json::array())
                                     : Json::array();
            for (const auto &entry : entries) {
                const auto name = entry.at("name").get<std::string>();
                const auto error = entry.value("error", std::string{});
                cyui::ScopedID id(name.c_str());
                {
                    cyui::ScopedDisabled disabled(!error.empty());
                    if (ImGui::Selectable(entry.value("displayName", name).c_str(), presetName_ == name)) {
                        presetName_ = name;
                        presetPlan_ = nullptr;
                        presetError_.clear();
                    }
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    cyui::tooltip((error.empty() ? entry.value("description", name) : error).c_str());
            }
            ImGui::EndCombo();
        }
        cyui::endProperties();
    }
    {
        cyui::ScopedDisabled disabled(reading);
        if (cyui::iconButton(cyui::icon::refresh, tr("Refresh CMake presets"))) {
            presetCatalogue_ = Json::object();
            presetPlan_ = nullptr;
            presetRead("build.presets");
        }
        ImGui::SameLine();
        cyui::ScopedDisabled missing(presetName_.empty());
        if (cyui::button(tr("Preview preset steps"))) {
            presetPlan_ = nullptr;
            presetRead("build.presetPlan");
        }
    }
    if (reading) {
        ImGui::SameLine();
        cyui::spinner("##preset-inspection");
    }
    if (!presetError_.empty())
        cyui::callout(cyui::Tone::warning, presetError_.c_str());
    if (presetCatalogue_.is_object() && !presetCatalogue_.value("error", std::string{}).empty())
        cyui::callout(cyui::Tone::warning,
                      presetCatalogue_.at("error").get_ref<const std::string &>().c_str());
    if (presetName_.empty())
        cyui::emptyState(tr("CMake preset actions"),
                         tr("Choose a test, package or workflow preset, then preview its steps."),
                         cyui::icon::repair);
    if (presetPlan_.is_object()) {
        const char *kinds[] = {"test", "package", "workflow"};
        const bool matching = presetPlanKey_ == presetActionKey();
        if (cyui::beginProperties("##preset-plan", 150)) {
            cyui::property(tr("Build directory"));
            cyui::textDim("%s", presetPlan_.at("buildDirectory").get_ref<const std::string &>().c_str());
            cyui::property(tr("Configure preset"));
            cyui::textDim("%s", presetPlan_.at("configurePreset").get_ref<const std::string &>().c_str());
            cyui::property(tr("Minimum CMake"));
            cyui::textDim("%s", presetPlan_.at("minimumCMake").get_ref<const std::string &>().c_str());
            cyui::property(tr("Process timeout"));
            cyui::textDim("%d s", configuredBuildTimeout());
            cyui::endProperties();
        }
        if (ImGui::BeginTable("##preset-steps", 3,
                              ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV,
                              {0, cyui::dp(180)})) {
            ImGui::TableSetupColumn(tr("Step"));
            ImGui::TableSetupColumn(tr("Preset"));
            ImGui::TableSetupColumn(tr("Configuration"));
            ImGui::TableHeadersRow();
            for (const auto &step : presetPlan_.at("steps")) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                cyui::textDim("%s", step.at("type").get_ref<const std::string &>().c_str());
                ImGui::TableNextColumn();
                cyui::textDim("%s", step.at("name").get_ref<const std::string &>().c_str());
                ImGui::TableNextColumn();
                std::string configurations;
                for (const auto &value : step.at("configurations")) {
                    if (!configurations.empty())
                        configurations += ", ";
                    configurations += value.get<std::string>();
                }
                cyui::textDim("%s", configurations.c_str());
            }
            ImGui::EndTable();
        }
        if (cyui::beginSection(tr("Declared output paths"))) {
            for (const auto &step : presetPlan_.at("steps"))
                for (const auto &path : step.at("outputs"))
                    cyui::textDim("%s", path.get_ref<const std::string &>().c_str());
            cyui::endSection();
        }
        cyui::textDim(
            "%s", tr("Execution uses trusted project scripts. Failed or cancelled steps may leave outputs."));
        {
            cyui::ScopedDisabled disabled(reading || !matching || !commandEnabled("build.run"));
            if (cyui::primaryButton(tr("Run reviewed preset"))) {
                task("build.presetRun", {{"source", presetSource_},
                                         {"kind", kinds[presetKind_]},
                                         {"preset", presetName_},
                                         {"expectedRevision", presetPlan_.at("revision")}});
                if (taskFuture_.valid() && taskMethod_ == "build.presetRun")
                    presetPlan_ = nullptr;
            }
        }
    }
    {
        cyui::ScopedDisabled disabled(!engine_->dispatch("build.status").value("running", false));
        if (cyui::button(tr("Cancel build")))
            cancelBuild();
    }
    ImGui::End();
}
} // namespace cycode
