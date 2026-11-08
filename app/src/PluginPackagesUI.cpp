#include "cycode/Application.h"
#include "cycode/PluginPackages.h"
#include <algorithm>
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
void Application::choosePluginPackage() {
    if (packagePickerPending_)
        return;
    packagePickerPending_ = true;
    auto *state = new std::shared_ptr<Dialog>(packageDialog_);
    SDL_ShowOpenFolderDialog(
        [](void *user, const char *const *paths, int) {
            auto dialog = *static_cast<std::shared_ptr<Dialog> *>(user);
            delete static_cast<std::shared_ptr<Dialog> *>(user);
            std::lock_guard lock(dialog->mutex);
            dialog->ready = true;
            dialog->path = paths && paths[0] ? paths[0] : "";
            dialog->error = paths ? "" : SDL_GetError();
        },
        state, window_, nullptr, false);
}
void Application::packageTask(const std::string &action, Json parameters) {
    if (packageFuture_.valid())
        return;
    const auto root = root_;
    packageError_.clear();
    packageFuture_ = std::async(std::launch::async, [root, action, parameters] {
        PluginPackages manager(root);
        Json result = {{"action", action}};
        if (action == "inspect")
            result["preview"] = manager.inspect(utf8Path(parameters.at("source")));
        else if (action == "catalog")
            result["catalog"] = manager.catalog();
        else if (action == "install")
            result["catalog"] =
                manager.install(utf8Path(parameters.at("source")), parameters.at("expectedRevision"),
                                parameters.at("expectedTrustRevision"), parameters.at("expectedPackage"));
        else if (action == "remove")
            result["catalog"] = manager.remove(parameters.at("id"), parameters.at("expectedRevision"));
        else if (action == "rollback")
            result["catalog"] =
                manager.rollback(parameters.at("id"), parameters.at("digest"),
                                 parameters.at("expectedRevision"), parameters.at("expectedTrustRevision"));
        else if (action == "clean")
            result["catalog"] = manager.clean(parameters.at("id"), parameters.at("digest"),
                                              parameters.at("expectedRevision"));
        else if (action == "trust")
            result["catalog"] =
                manager.trust(parameters.at("publicKey"), parameters.at("label"), parameters.at("enabled"),
                              parameters.at("expectedTrustRevision"));
        else
            throw std::runtime_error("Unknown package UI operation");
        return result;
    });
}
void Application::packageControls() {
    if (smoke_ || testSession_)
        return; // Existing GUI fixtures must not change the real installation store.
    if (packageFuture_.valid() &&
        packageFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            const auto result = packageFuture_.get();
            if (result.contains("catalog"))
                packageCatalog_ = result.at("catalog");
            if (result.contains("preview")) {
                packagePreview_ = result.at("preview");
                packageSource_ = packagePreview_.at("source");
                packageTrustAcknowledged_ = false;
                packagePublisherLabel_.clear();
            }
            if (result.at("action") == "trust" && !packageSource_.empty()) {
                packagePreview_ = Json::object();
                packageInspectPending_ = true;
            }
            if (result.at("action") == "install")
                packagePreview_ = Json::object();
            if (result.at("action") == "clean" && packageCatalog_.value("action", "") == "inUse")
                cyui::notify(tr("This version is still used by an open instance."), cyui::Tone::warning);
            else if (result.at("action") != "catalog" && result.at("action") != "inspect")
                cyui::notify(tr("Plugin package settings saved."), cyui::Tone::success);
        } catch (const std::exception &e) {
            packageError_ = e.what();
            cyui::notify(e.what(), cyui::Tone::error);
        }
    }
    {
        std::lock_guard lock(packageDialog_->mutex);
        if (packageDialog_->ready) {
            packagePickerPending_ = false;
            packageDialog_->ready = false;
            if (!packageDialog_->error.empty())
                packageError_ = packageDialog_->error;
            else if (!packageDialog_->path.empty()) {
                packageSource_ = packageDialog_->path;
                packagePreview_ = Json::object();
                packageInspectPending_ = true;
            }
        }
    }
    if (packageInspectPending_ && !packageFuture_.valid()) {
        packageInspectPending_ = false;
        packageTask("inspect", {{"source", packageSource_}});
    }
    if (packageCatalog_.empty() && packageError_.empty() && !packageFuture_.valid())
        packageTask("catalog");
    const bool busy = packageFuture_.valid();
    if (!cyui::beginSection(tr("Plugin packages")))
        return;
    cyui::textDim("%s", tr("Signed local packages. Changes apply to the next startup."));
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::button(tr("Choose package folder")))
            choosePluginPackage();
        ImGui::SameLine();
        if (cyui::ghostButton(tr("Refresh package catalogue")))
            packageTask("catalog");
        if (cyui::beginProperties("##package-source")) {
            cyui::property(tr("Package folder"));
            if (ImGui::InputText("##package-path", &packageSource_))
                packagePreview_ = Json::object();
            cyui::endProperties();
        }
        {
            cyui::ScopedDisabled noSource(packageSource_.empty());
            if (cyui::button(tr("Inspect package")))
                packageTask("inspect", {{"source", packageSource_}});
        }
    }
    if (busy) {
        ImGui::SameLine();
        cyui::spinner("##package-operation");
    }
    if (!packageError_.empty())
        cyui::callout(cyui::Tone::error, packageError_.c_str());
    if (packageCatalog_.contains("catalogError"))
        cyui::callout(cyui::Tone::warning,
                      packageCatalog_.at("catalogError").get_ref<const std::string &>().c_str());
    std::map<std::string, std::string> loaded, desired;
    bool revokedPublisher = false;
    for (const auto &item : pluginInfo_.value("plugins", Json::array()))
        if (item.contains("managed")) {
            loaded[item.at("id")] = item.at("managed").at("digest");
            const auto key = item.at("managed").at("publicKey");
            const auto publishers = packageCatalog_.value("publishers", Json::array());
            if (std::none_of(publishers.begin(), publishers.end(), [&](const Json &p) {
                    return p.at("publicKey") == key && p.at("enabled").get<bool>();
                }))
                revokedPublisher = true;
        }
    for (const auto &item : packageCatalog_.value("entries", Json::array()))
        desired[item.at("id")] = item.at("digest");
    if (!packageCatalog_.empty() && (loaded != desired || revokedPublisher))
        cyui::callout(
            cyui::Tone::info,
            tr("The selected packages differ from this host. Restart CyCode after resolving any errors."));
    if (!packagePreview_.empty()) {
        const auto &p = packagePreview_;
        cyui::heading(p.value("name", p.at("id").get<std::string>()).c_str(), 3);
        cyui::textDim("%s | %s | %s", p.at("id").get_ref<const std::string &>().c_str(),
                      p.at("version").get_ref<const std::string &>().c_str(),
                      p.at("platform").get_ref<const std::string &>().c_str());
        cyui::badge(tr("Signature and payload verified"), cyui::Tone::success);
        cyui::textDim("%s: %llu / %llu", tr("Files / bytes"), p.at("fileCount").get<unsigned long long>(),
                      p.at("bytes").get<unsigned long long>());
        cyui::sectionTitle(tr("Publisher fingerprint (SHA-512)"));
        ImGui::TextWrapped("%s", p.at("fingerprint").get_ref<const std::string &>().c_str());
        if (cyui::ghostButton(tr("Copy publisher fingerprint")))
            ImGui::SetClipboardText(p.at("fingerprint").get_ref<const std::string &>().c_str());
        cyui::textDim("%s", tr("Requested permissions"));
        for (const auto &permission : p.at("permissions"))
            ImGui::BulletText("%s", permission.get_ref<const std::string &>().c_str());
        if (p.at("permissions").empty())
            cyui::textDim("%s", tr("No exported host permissions requested."));
        if (!p.at("compatible").get<bool>())
            cyui::callout(cyui::Tone::error,
                          tr("This package is incompatible with the current host/platform."));
        if (!p.at("trusted").get<bool>()) {
            cyui::callout(cyui::Tone::warning, tr("Native plugins run with your account privileges. Verify "
                                                  "the publisher fingerprint before trusting it."));
            cyui::ScopedDisabled disabled(busy);
            if (cyui::beginProperties("##publisher-trust")) {
                cyui::property(tr("Publisher label (local)"));
                ImGui::InputText("##publisher-label", &packagePublisherLabel_);
                cyui::endProperties();
            }
            ImGui::Checkbox(tr("I trust this publisher to run native code"), &packageTrustAcknowledged_);
            {
                cyui::ScopedDisabled noTrust(!packageTrustAcknowledged_ || packagePublisherLabel_.empty());
                if (cyui::button(tr("Trust publisher")))
                    packageTask("trust", {{"publicKey", p.at("publicKey")},
                                          {"label", packagePublisherLabel_},
                                          {"enabled", true},
                                          {"expectedTrustRevision", p.at("trustRevision")}});
            }
        }
        {
            cyui::ScopedDisabled disabled(busy || !p.at("trusted").get<bool>() ||
                                          !p.at("compatible").get<bool>());
            if (cyui::primaryButton(tr("Install / update package")))
                packageTask("install", {{"source", p.at("source")},
                                        {"expectedRevision", p.at("revision")},
                                        {"expectedTrustRevision", p.at("trustRevision")},
                                        {"expectedPackage", p.at("digest")}});
        }
    }
    auto confirm = [&](const char *action, const Json &p, const std::string &digest = std::string{}) {
        packageConfirmation_ = {{"action", action},
                                {"id", p.at("id")},
                                {"version", p.value("version", std::string{})},
                                {"digest", digest},
                                {"expectedRevision", packageCatalog_.at("revision")},
                                {"expectedTrustRevision", packageCatalog_.at("trustRevision")}};
        ImGui::OpenPopup(label("Confirm package change", "package-confirm").c_str());
    };
    cyui::sectionTitle(tr("Selected for next startup"));
    for (const auto &item : packageCatalog_.value("entries", Json::array())) {
        ImGui::PushID(item.at("id").get_ref<const std::string &>().c_str());
        ImGui::TextUnformatted(item.value("name", item.at("id").get<std::string>()).c_str());
        ImGui::SameLine();
        cyui::textDim("%s", item.value("version", std::string{}).c_str());
        if (item.contains("error"))
            cyui::callout(cyui::Tone::error, item.at("error").get_ref<const std::string &>().c_str());
        if (!item.value("trusted", false))
            cyui::badge(tr("Publisher not trusted"), cyui::Tone::warning);
        {
            cyui::ScopedDisabled disabled(busy);
            if (cyui::dangerButton(tr("Remove from selection")))
                confirm("remove", item);
        }
        for (const auto &old : item.value("versions", Json::array())) {
            if (old.contains("error")) {
                cyui::textDim("%s", old.at("error").get_ref<const std::string &>().c_str());
                continue;
            }
            const auto digest = old.at("digest").get<std::string>();
            ImGui::PushID(digest.c_str());
            cyui::textDim("%s: %s", tr("Retained version"),
                          old.at("version").get_ref<const std::string &>().c_str());
            ImGui::SameLine();
            {
                cyui::ScopedDisabled disabled(busy);
                if (cyui::ghostButton(tr("Select previous version")))
                    confirm("rollback", old, digest);
            }
            ImGui::PopID();
        }
        ImGui::PopID();
    }
    if (cyui::beginSection(tr("Trusted publishers"))) {
        for (const auto &publisher : packageCatalog_.value("publishers", Json::array())) {
            const auto key = publisher.at("publicKey").get<std::string>();
            ImGui::PushID(key.c_str());
            ImGui::TextUnformatted(publisher.at("label").get_ref<const std::string &>().c_str());
            ImGui::TextWrapped("%s", key.c_str());
            {
                cyui::ScopedDisabled disabled(busy);
                if (cyui::ghostButton(tr(publisher.at("enabled").get<bool>() ? "Revoke publisher trust"
                                                                             : "Restore publisher trust")))
                    packageTask("trust", {{"publicKey", key},
                                          {"label", publisher.at("label")},
                                          {"enabled", !publisher.at("enabled").get<bool>()},
                                          {"expectedTrustRevision", packageCatalog_.at("trustRevision")}});
            }
            ImGui::PopID();
        }
        cyui::endSection();
    }
    if (!packageCatalog_.value("storageErrors", Json::array()).empty()) {
        cyui::callout(cyui::Tone::warning,
                      tr("Some stored packages could not be verified and were left untouched."));
        for (const auto &entry : packageCatalog_.at("storageErrors"))
            ImGui::TextWrapped("%s: %s", entry.at("path").get_ref<const std::string &>().c_str(),
                               entry.at("error").get_ref<const std::string &>().c_str());
    }
    if (cyui::beginSection(tr("Unused package versions"))) {
        for (const auto &item : packageCatalog_.value("garbage", Json::array())) {
            const auto digest = item.at("digest").get<std::string>();
            ImGui::PushID(digest.c_str());
            ImGui::TextWrapped("%s | %s", item.at("id").get_ref<const std::string &>().c_str(),
                               item.value("version", std::string{}).c_str());
            if (item.contains("error"))
                cyui::textDim("%s", item.at("error").get_ref<const std::string &>().c_str());
            {
                cyui::ScopedDisabled disabled(busy || item.contains("error"));
                if (cyui::dangerButton(tr("Delete unused version files")))
                    confirm("clean", item, digest);
            }
            ImGui::PopID();
        }
        if (packageCatalog_.value("garbageTruncated", false))
            cyui::textDim("%s", tr("More unused versions exist. Refresh after cleaning this page."));
        cyui::endSection();
    }
    if (!packageConfirmation_.empty()) {
        bool open = true;
        if (cyui::beginModal(label("Confirm package change", "package-confirm").c_str(), &open)) {
            ImGui::TextWrapped("%s | %s",
                               packageConfirmation_.at("id").get_ref<const std::string &>().c_str(),
                               packageConfirmation_.at("version").get_ref<const std::string &>().c_str());
            const auto action = packageConfirmation_.at("action").get<std::string>();
            ImGui::TextWrapped("%s", tr(action == "clean" ? "Only the verified unused version files will be "
                                                            "deleted. Open instances keep their versions."
                                        : action == "rollback"
                                            ? "Select this retained signed version for the next startup?"
                                            : "Remove this plugin from the next startup selection? Running "
                                              "instances keep their loaded code."));
            const auto answer = cyui::modalButtons(tr("Confirm"), tr("Cancel"), action != "rollback");
            if (answer == 1 && !busy) {
                auto params = packageConfirmation_;
                packageConfirmation_ = Json::object();
                packageTask(action, params);
            } else if (answer == 2)
                packageConfirmation_ = Json::object();
            cyui::endModal();
        }
        if (!open)
            packageConfirmation_ = Json::object();
    }
    cyui::endSection();
}
} // namespace cycode
