#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>

namespace cycode {
void Application::configureLanguageServer(const std::string &key, bool enabled) {
    try {
        const auto preferences = engine_->dispatch("language.preferences");
        Json params = {{"language", key}, {"enabled", enabled}};
        if (preferences.at("available") && preferences.at("error") == "") {
            params["save"] = true;
            params["expectedPreferencesRevision"] = preferences.at("revision");
        }
        languageState_ = engine_->dispatch("language.configure", params);
        languageDecisions_.insert(key);
    } catch (const std::exception &e) {
        error(e.what());
    }
}
void Application::languageServerControls() {
    const auto servers = languageState_.value("servers", Json::object());
    const auto preferences = languageState_.value("preferences", Json::object());
    const auto preferenceError = preferences.value("error", "");
    if (!preferenceError.empty())
        ImGui::TextWrapped("%s", preferenceError.c_str());
    for (const auto &[key, server] : servers.items()) {
        ImGui::PushID(key.c_str());
        const auto state = server.value("ready", false)     ? "Connected"
                           : server.value("running", false) ? "Starting..."
                           : server.value("enabled", false) ? "Idle"
                                                            : "Disabled";
        const auto label = server.value("name", "") + " : " + tr(state);
        cyui::badge(label.c_str(), server.value("ready", false) ? cyui::Tone::success : cyui::Tone::neutral);
        ImGui::SameLine();
        {
            cyui::ScopedDisabled disabled(smoke_ || !server.value("available", false));
            if (cyui::button(tr(server.value("enabled", false) ? "Restart language server" : "Enable")))
                configureLanguageServer(key, true);
        }
        if (server.value("enabled", false)) {
            ImGui::SameLine();
            if (cyui::button(tr("Disable")))
                configureLanguageServer(key, false);
        } else if (!server.value("available", false)) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", tr("Language server is not installed"));
        }
        ImGui::SameLine();
        if (cyui::button(tr("Configure language server..."))) {
            try {
                languageSettingsPreferences_ = engine_->dispatch("language.preferences");
                const auto activeConfiguration=server.value("configuration",Json::object());
                // A session-only Cargo override must remain reviewable/removable
                // even when an older Rust launch exists in saved preferences.
                const auto configuration = key=="rust"&&activeConfiguration.contains("cargoProject")
                                               ? activeConfiguration
                                               : languageSettingsPreferences_.at("profiles").contains(key)
                                                   ? languageSettingsPreferences_.at("profiles").at(key)
                                                   : activeConfiguration;
                languageSettingsArguments_ = configuration.value("arguments", Json::array()).dump(2);
                languageSettingsJson_ = configuration.value("settings", Json::object()).dump(2);
                languageSettingsExecutable_ = configuration.value("executable", "");
                languageSettingsCargo_=configuration.value("cargoProject",Json());
                languageSettingsSave_ = languageSettingsPreferences_.at("available") &&
                                        languageSettingsPreferences_.at("error") == "";
                languageSettingsError_ = languageSettingsPreferences_.at("error");
                languageSettingsBackend_ = key;
            } catch (const std::exception &e) {
                error(e.what());
            }
        }
        const auto logs = server.value("logs", "");
        if (!logs.empty() && ImGui::TreeNode(tr("Server log"))) {
            ImGui::TextUnformatted(logs.c_str());
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    if (!languageSettingsBackend_.empty() && !ImGui::IsPopupOpen(tr("Language server settings")))
        ImGui::OpenPopup(tr("Language server settings"));
    if (!ImGui::BeginPopupModal(tr("Language server settings"), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::TextUnformatted(languageSettingsBackend_.c_str());
    if (!languageSettingsExecutable_.empty())
        cyui::textDim("%s: %s", tr("Program"), languageSettingsExecutable_.c_str());
    ImGui::TextUnformatted(tr("Executable arguments (JSON array)"));
    ImGui::InputTextMultiline("##server-arguments", &languageSettingsArguments_,
                              {cyui::dp(540), cyui::dp(110)});
    ImGui::TextUnformatted(tr("Server settings (JSON object)"));
    ImGui::InputTextMultiline("##server-settings", &languageSettingsJson_, {cyui::dp(540), cyui::dp(160)});
    if (cyui::button(tr("Use installed defaults"))) {
        languageSettingsExecutable_.clear();
        languageSettingsArguments_ = "[]";
        languageSettingsJson_ = "{}";
        languageSettingsCargo_=Json();
    }
    cyui::textDim("%s", tr("Empty arguments use the installed server. No shell is used."));
    if(languageSettingsBackend_=="rust"&&!languageSettingsCargo_.is_null()) {
        cyui::textDim("%s: %s",tr("Cargo analysis manifest"),languageSettingsCargo_.value("manifest",std::string{}).c_str());
        const auto toolchain=languageSettingsCargo_.value("toolchain",std::string{});
        if(!toolchain.empty())cyui::textDim("%s: %s",tr("Rust toolchain"),toolchain.c_str());
        if(cyui::button(tr("Remove Cargo analysis override")))languageSettingsCargo_=Json();
        cyui::textDim("%s",tr("The Cargo override is separate from raw server settings. Apply to confirm changes."));
    }
    {
        cyui::ScopedDisabled disabled(!languageSettingsPreferences_.value("available", false) ||
                                      languageSettingsPreferences_.value("error", "") != "");
        ImGui::Checkbox(tr("Remember for this project"), &languageSettingsSave_);
    }
    cyui::textDim("%s", tr("Saved in local CyCode preferences, outside project configuration."));
    if (!languageSettingsError_.empty())
        ImGui::TextWrapped("%s", languageSettingsError_.c_str());
    {
        cyui::ScopedDisabled disabled(smoke_);
        if (cyui::primaryButton(tr("Apply and restart"))) {
            try {
                auto arguments = Json::parse(languageSettingsArguments_);
                auto settings = Json::parse(languageSettingsJson_);
                if (!arguments.is_array() || !settings.is_object())
                    throw std::runtime_error("Expected an argv array and settings object");
                Json params = {
                    {"language", languageSettingsBackend_}, {"arguments", arguments}, {"settings", settings}};
                if(languageSettingsBackend_=="rust")params["cargoProject"]=languageSettingsCargo_;
                if (arguments.empty() && settings.empty() && !languageSettingsExecutable_.empty())
                    params["executable"] = languageSettingsExecutable_;
                if (languageSettingsSave_) {
                    params["save"] = true;
                    params["expectedPreferencesRevision"] = languageSettingsPreferences_.at("revision");
                }
                languageState_ = engine_->dispatch("language.configure", params);
                languageDecisions_.insert(languageSettingsBackend_);
                languageSettingsBackend_.clear();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception &e) {
                languageSettingsError_ = e.what();
            }
        }
    }
    ImGui::SameLine();
    if (cyui::button(tr("Reload saved settings"))) {
        try {
            languageSettingsPreferences_ = engine_->dispatch("language.preferences");
            const auto configuration =
                languageSettingsPreferences_.at("profiles").contains(languageSettingsBackend_)
                    ? languageSettingsPreferences_.at("profiles").at(languageSettingsBackend_)
                    : engine_->languageStatus()
                          .at("servers")
                          .at(languageSettingsBackend_)
                          .at("configuration");
            languageSettingsArguments_ = configuration.value("arguments", Json::array()).dump(2);
            languageSettingsJson_ = configuration.value("settings", Json::object()).dump(2);
            languageSettingsExecutable_ = configuration.value("executable", "");
            languageSettingsCargo_=configuration.value("cargoProject",Json());
            languageSettingsError_ = languageSettingsPreferences_.at("error");
            if (!languageSettingsPreferences_.value("available", false) || !languageSettingsError_.empty())
                languageSettingsSave_ = false;
        } catch (const std::exception &e) {
            languageSettingsError_ = e.what();
        }
    }
    if (languageSettingsPreferences_.value("profiles", Json::object()).contains(languageSettingsBackend_)) {
        ImGui::SameLine();
        if (cyui::button(tr("Forget saved settings"))) {
            try {
                languageSettingsPreferences_ = engine_->dispatch(
                    "language.preferencesForget",
                    {{"language", languageSettingsBackend_},
                     {"expectedPreferencesRevision", languageSettingsPreferences_.at("revision")}});
                languageSettingsError_.clear();
            } catch (const std::exception &e) {
                languageSettingsError_ = e.what();
            }
        }
        cyui::textDim("%s", tr("Forgetting keeps the current server running; defaults return next session."));
    }
    if (cyui::button(tr("Cancel"))) {
        languageSettingsBackend_.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
} // namespace cycode
