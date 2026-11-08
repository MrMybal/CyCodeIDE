#include "cycode/Application.h"
#include "cycode/UnrealLaunch.h"
#include <algorithm>
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
void Application::restoreUnrealLaunch(const Json &binding) {
    clearUnrealLaunch();
    const auto &request = binding.at("request");
    unrealLaunchMode_ = request.at("mode") == "game" ? 1 : request.at("mode") == "server" ? 2 : 0;
    unrealLaunchMap_ = request.at("map");
    unrealLaunchListen_ = request.at("listen");
    unrealLaunchLog_ = request.at("log");
    unrealLaunchWindowed_ = request.at("windowed");
    unrealLaunchWidth_ = request.at("width");
    unrealLaunchHeight_ = request.at("height");
    unrealLaunchPort_ = request.at("port");
    unrealLaunchBuild_ = debugBuildBefore_;
    unrealLaunchStop_ = debugStopOnEntry_;
}
void Application::startUnrealDebug(bool build, const Json &draft) {
    const auto saved = engine_->debugger().profile();
    auto parameters = saved;
    parameters.update(draft);
    (void)unrealBoundLaunchParameters(parameters);
    saveAll();
    const auto documents = engine_->dispatch("documents.list");
    if (std::any_of(tabs_.begin(), tabs_.end(), [](const auto &tab) { return tab->dirty; }))
        return;
    validateUnrealDebugDocuments(documents.at("documents"), documents.at("documents"));
    if (!build || !parameters.at("buildBeforeLaunch").get<bool>()) {
        parameters["expectedProfile"] = saved;
        debugTask("debug.launch", parameters);
        return;
    }
    pendingUnrealDebug_ = {{"draft", draft},
                           {"parameters", parameters},
                           {"expectedProfile", saved},
                           {"documents", documents.at("documents")}};
    pendingDebugBuild_ = true;
    try {
        task("unreal.build", parameters.at("unrealLaunch").at("profile"));
        if (taskFuture_.valid())
            pendingUnrealDebug_["operationId"] = taskOperationId_;
        else {
            pendingUnrealDebug_ = Json::object();
            pendingDebugBuild_ = false;
        }
    } catch (...) {
        pendingUnrealDebug_ = Json::object();
        pendingDebugBuild_ = false;
        throw;
    }
}
bool Application::continueUnrealDebug(const Json &result) {
    if (pendingUnrealDebug_.empty())
        return false;
    const auto snapshot = pendingUnrealDebug_;
    pendingUnrealDebug_ = Json::object();
    pendingDebugBuild_ = false;
    if (taskMethod_ != "unreal.build")
        throw std::runtime_error("Unexpected operation completed during Unreal debug preparation");
    auto parameters = unrealDebugContinuation(snapshot, debugParameters(), engine_->debugger().profile(),
                                              result, taskOperationId_);
    const auto documents = engine_->dispatch("documents.list");
    if (std::any_of(tabs_.begin(), tabs_.end(), [](const auto &tab) { return tab->dirty; }))
        throw std::runtime_error(
            "Documents changed during the Unreal build; save/reconcile and start debugging again");
    validateUnrealDebugDocuments(snapshot.at("documents"), documents.at("documents"));
    debugTask("debug.launch", parameters);
    return true;
}
Json Application::unrealLaunchParameters() const {
    return {{"project", unrealProject_},
            {"engineRoot", unrealEngine_},
            {"target", unrealTarget_},
            {"platform", unrealPlatform_},
            {"configuration", unrealConfiguration_},
            {"mode", unrealLaunchMode_ == 1   ? "game"
                     : unrealLaunchMode_ == 2 ? "server"
                                              : "editor"},
            {"map", unrealLaunchMap_},
            {"listen", unrealLaunchListen_},
            {"log", unrealLaunchLog_},
            {"windowed", unrealLaunchWindowed_},
            {"width", unrealLaunchWidth_},
            {"height", unrealLaunchHeight_},
            {"port", unrealLaunchPort_},
            {"buildBeforeLaunch", unrealLaunchBuild_},
            {"stopOnEntry", unrealLaunchStop_}};
}
void Application::clearUnrealLaunch() {
    unrealLaunchPreview_ = unrealLaunchPending_ = unrealLaunchDebugDraft_ = Json::object();
    unrealLaunchMode_ = 0;
    unrealLaunchWidth_ = 1280;
    unrealLaunchHeight_ = 720;
    unrealLaunchPort_ = 7777;
    unrealLaunchListen_ = unrealLaunchLog_ = unrealLaunchStop_ = false;
    unrealLaunchWindowed_ = unrealLaunchBuild_ = true;
    unrealLaunchMap_.clear();
}
void Application::unrealLaunchResult(const Json &result) {
    if (unrealLaunchPending_ == unrealLaunchParameters() && unrealLaunchDebugDraft_ == debugParameters())
        unrealLaunchPreview_ = result;
}
void Application::unrealLaunchControls() {
    if (!ImGui::CollapsingHeader(tr("Unreal launch profile")))
        return;
    const bool busy =
        taskFuture_.valid() || unrealAnalysisFuture_.valid() || debugFuture_.valid() || pendingDebugBuild_;
    if (!debugUnrealLaunch_.is_null()) {
        const auto &context = debugUnrealLaunch_.at("profile");
        cyui::textDim("%s: %s | %s", tr("Bound Unreal build"),
                      context.at("target").get_ref<const std::string &>().c_str(),
                      context.at("configuration").get_ref<const std::string &>().c_str());
        cyui::ScopedDisabled disabled(busy);
        if (cyui::button(tr("Load bound Unreal build settings"))) {
            unrealProject_ = context.at("project");
            unrealEngine_ = context.at("engineRoot");
            unrealTarget_ = context.at("target");
            unrealPlatform_ = context.at("platform");
            unrealConfiguration_ = context.at("configuration");
            restoreUnrealLaunch(debugUnrealLaunch_);
            task("unreal.inspect", context);
        }
    }
    const auto previous = unrealLaunchParameters();
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::beginProperties("##ue-launch-properties", 160)) {
            cyui::property(tr("Launch mode"));
            ImGui::RadioButton(tr("Unreal Editor"), &unrealLaunchMode_, 0);
            ImGui::SameLine();
            ImGui::RadioButton(tr("Standalone game"), &unrealLaunchMode_, 1);
            ImGui::SameLine();
            ImGui::RadioButton(tr("Uncooked server"), &unrealLaunchMode_, 2);
            if (previous.at("mode") != unrealLaunchParameters().at("mode")) {
                // Do not strand an invalid, now-hidden dimension/port in another mode.
                unrealLaunchWidth_ = std::clamp(unrealLaunchWidth_, 320, 16384);
                unrealLaunchHeight_ = std::clamp(unrealLaunchHeight_, 200, 16384);
                unrealLaunchPort_ = std::clamp(unrealLaunchPort_, 1, 65535);
            }
            if (unrealLaunchMode_ != 1)
                unrealLaunchListen_ = false;
            cyui::property(tr("Startup map"));
            ImGui::InputText("##ue-launch-map", &unrealLaunchMap_);
            cyui::property(tr("Launch options"));
            ImGui::Checkbox(tr("Unreal log window"), &unrealLaunchLog_);
            ImGui::Checkbox(tr("Build before debugging"), &unrealLaunchBuild_);
            ImGui::Checkbox(tr("Stop on entry"), &unrealLaunchStop_);
            if (unrealLaunchMode_ == 1) {
                ImGui::Checkbox(tr("Listen server"), &unrealLaunchListen_);
                ImGui::Checkbox(tr("Windowed game"), &unrealLaunchWindowed_);
                cyui::property(tr("Game width"));
                ImGui::InputInt("##ue-launch-width", &unrealLaunchWidth_);
                cyui::property(tr("Game height"));
                ImGui::InputInt("##ue-launch-height", &unrealLaunchHeight_);
            }
            if (unrealLaunchMode_ == 2 || unrealLaunchListen_) {
                cyui::property(tr("Game port"));
                ImGui::InputInt("##ue-launch-port", &unrealLaunchPort_);
            }
            cyui::endProperties();
        }
    }
    if (previous != unrealLaunchParameters())
        unrealLaunchPreview_ = unrealLaunchPending_ = Json::object();
    ImGui::TextWrapped(
        "%s", tr("Use an existing /Game/Folder/Map without .umap, or leave empty for the project default. "
                 "Game and server modes use uncooked content through the installed editor."));
    ImGui::TextWrapped(
        "%s", tr("Development and DebugGame use the current host platform. Building before debugging "
                 "requires the Editor target selected above; disable it for content-only projects."));
    {
        cyui::ScopedDisabled disabled(busy || !debugProfileLoaded_);
        if (cyui::button(tr("Preview Unreal launch profile"))) {
            try {
                unrealLaunchDebugDraft_ = debugParameters();
                unrealLaunchPending_ = unrealLaunchParameters();
                unrealLaunchPreview_ = Json::object();
                task("unreal.launchProfile", unrealLaunchPending_);
            } catch (const std::exception &e) {
                error(e.what());
            }
        }
    }
    if (unrealLaunchPreview_.empty())
        return;
    // A preview must not overwrite an unsaved manual edit in the debugger panel.
    try {
        if (unrealLaunchDebugDraft_ != debugParameters()) {
            unrealLaunchPreview_ = Json::object();
            return;
        }
    } catch (const std::exception &) {
        unrealLaunchPreview_ = Json::object();
        return;
    }
    const auto &settings = unrealLaunchPreview_.at("settings");
    cyui::textDim("%s", tr("Reviewed Unreal launch arguments"));
    ImGui::TextWrapped("%s", settings.at("program").get_ref<const std::string &>().c_str());
    for (const auto &arg : settings.at("args"))
        ImGui::TextWrapped("%s", arg.get_ref<const std::string &>().c_str());
    ImGui::TextWrapped(
        "%s", tr("Applying saves the shared debug profile and opens Debugger. Use Run there without "
                 "compilation, or Debug with the selected build option. No process starts while applying."));
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::primaryButton(tr("Use this Unreal launch profile"))) {
            auto parameters = settings;
            parameters["expectedProfile"] = unrealLaunchPreview_.at("expectedProfile");
            unrealLaunchPreview_ = unrealLaunchPending_ = Json::object();
            debugTask("debug.configure", parameters);
        }
    }
    ImGui::SameLine();
    if (cyui::button(tr("Cancel launch preview")))
        unrealLaunchPreview_ = unrealLaunchPending_ = Json::object();
}
} // namespace cycode
