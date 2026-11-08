#include "UpdateFeedback.h"
#include "cycode/update/detail/UpdateInstall.h"
#include "cycode/Application.h"
#include <SDL3/SDL.h>
#include <chrono>

namespace cycode {
bool Application::applicationUpdatePending() const {
    return updateCheckFuture_.valid() || updateDownloadFuture_.valid() || updateInspectFuture_.valid() ||
           updateStorageFuture_.valid() || updateStorageActionFuture_.valid() || updateHistoryFuture_.valid();
}
void Application::refreshApplicationUpdates(bool startup, bool afterDownload) {
#ifndef CYCODE_IDE_ENABLE_UPDATES
    updateStartupChecked_ = true;
    updateNotice_ = tr("Updates are unavailable in this development build.");
    return;
#endif
    if (smoke_ || testSession_ || applicationUpdatePending())
        return;
    updateStartupChecked_ = true;
    updateStartupNetwork_ = startup && !afterDownload && checkUpdatesAtStartup_;
    updateInspectAfterDownload_ = afterDownload;
    updateCacheNotice_.clear();
    if (!afterDownload) {
        updateError_.clear();
        updateNotice_.clear();
        updateDownloadDetails_.clear();
        updateDownloadIssue_ = updates::DownloadIssue::none;
        preparedUpdate_.reset();
    }
    updateCancellation_ = std::make_shared<std::atomic<bool>>(false);
    const auto cancel = updateCancellation_;
    const auto root = root_;
    updateInspectFuture_ =
        std::async(std::launch::async, [root, cancel] { return updates::inspect(root, *cancel); });
}
void Application::checkApplicationUpdates(bool manual) {
#ifndef CYCODE_IDE_ENABLE_UPDATES
    updateStartupChecked_ = true;
    updateNotice_ = tr("Updates are unavailable in this development build.");
    return;
#endif
    if (smoke_ || testSession_ || applicationUpdatePending())
        return;
    updateStartupChecked_ = true;
    updateCheckManual_ = manual;
    updateError_.clear();
    updateNotice_.clear();
    updateCacheNotice_.clear();
    updateDownloadDetails_.clear();
    updateDownloadIssue_ = updates::DownloadIssue::none;
    updateChecked_ = false;
    latestUpdate_.reset();
    updateCancellation_ = std::make_shared<std::atomic<bool>>(false);
    const auto cancel = updateCancellation_;
    const auto root = root_;
    updateCheckFuture_ = std::async(std::launch::async, [cancel, root] {
        return updates::check(*cancel, updates::installedInstallation(root));
    });
    if (manual)
        showPanel("Application updates", "updates");
}
void Application::downloadApplicationUpdate(updates::DownloadMode mode) {
    if (smoke_ || testSession_ || !latestUpdate_ || latestUpdate_->asset.url.empty() ||
        applicationUpdatePending() || !updates::portableInstallation(root_))
        return;
    updateError_.clear();
    updateNotice_.clear();
    updateCacheNotice_.clear();
    updateDownloadDetails_.clear();
    updateDownloadIssue_ = updates::DownloadIssue::none;
    updateDownloadMode_ = mode;
    preparedUpdate_.reset();
    updateOffer_ = false;
    updateCancellation_ = std::make_shared<std::atomic<bool>>(false);
    updateProgress_ = std::make_shared<std::atomic<uint64_t>>(0);
    updateDownloadStatus_ =
        std::make_shared<updates::DownloadStatus>(latestUpdate_->asset.size, latestUpdate_->version);
    updateDownloadMeter_ = {};
    const auto cancel = updateCancellation_;
    const auto progress = updateProgress_;
    const auto release = *latestUpdate_;
    const auto root = root_;
    const auto status = updateDownloadStatus_;
    updateDownloadFuture_ = std::async(std::launch::async, [release, root, cancel, progress, mode, status] {
        return updates::download(release, root, *cancel, *progress, mode, status);
    });
    showPanel("Application updates", "updates");
}
void Application::updateApplicationUpdates() {
    if (smoke_ || testSession_)
        return;
    updateUpdateStorage();
    updateUpdateHistory();
    if (!quitRequested_)
        updateInstallRequested_ = false;
    if (!quitRequested_ && !updateStartupChecked_ && ImGui::GetTime() - updateStartupTime_ >= 1.5) {
        updateStartupChecked_ = true;
        if (updates::platform() != "unsupported")
            refreshApplicationUpdates(true);
    }
    if (updateInspectFuture_.valid() &&
        updateInspectFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const bool cancelled = updateCancellation_->load();
        const bool afterDownload = updateInspectAfterDownload_;
        updateInspectAfterDownload_ = false;
        const bool checkNetwork = updateStartupNetwork_ && !cancelled && !quitRequested_;
        updateStartupNetwork_ = false;
        try {
            auto inventory = updateInspectFuture_.get();
            if (!cancelled) {
                updateInventory_ = std::move(inventory);
                preparedUpdate_ = updateInventory_.prepared;
                if (preparedUpdate_ && latestUpdate_ && latestUpdate_->version == preparedUpdate_->version &&
                    !latestUpdate_->asset.digest.empty() &&
                    latestUpdate_->asset.digest != preparedUpdate_->digest)
                    preparedUpdate_.reset();
                if (!afterDownload && preparedUpdate_ && !quitRequested_ && checkUpdatesAtStartup_ &&
                    preparedUpdate_->version != skippedUpdate_)
                    updateOffer_ = true;
            }
        } catch (const std::exception &e) {
            if (afterDownload && !cancelled)
                updateCacheNotice_ = "The local download list could not be refreshed. Refresh it manually.";
            else if (!cancelled)
                updateError_ = e.what();
        }
        if (cancelled) {
            if (afterDownload)
                updateCacheNotice_ = "Cache refresh cancelled. The download result is unchanged.";
            else
                updateNotice_ = "Update cancelled";
        }
        if (checkNetwork && updateError_.empty())
            checkApplicationUpdates(false);
    }
    if (updateCheckFuture_.valid() &&
        updateCheckFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            latestUpdate_ = updateCheckFuture_.get();
            updateChecked_ = !updateCancellation_->load();
            if (updateCancellation_->load()) {
                latestUpdate_.reset();
                updateNotice_ = "Update cancelled";
            } else if (latestUpdate_) {
                if (preparedUpdate_ && latestUpdate_->version == preparedUpdate_->version &&
                    !latestUpdate_->asset.digest.empty() &&
                    latestUpdate_->asset.digest != preparedUpdate_->digest) {
                    preparedUpdate_.reset();
                    updateNotice_ =
                        "The downloaded package differs from the current release. Download it again.";
                }
                if (!quitRequested_ && (updateCheckManual_ || latestUpdate_->version != skippedUpdate_))
                    updateOffer_ = true;
            }
        } catch (const std::exception &e) {
            if (updateCancellation_->load())
                updateNotice_ = "Update cancelled";
            else
                updateError_ = e.what();
        }
    }
    if (updateDownloadFuture_.valid() &&
        updateDownloadFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            preparedUpdate_ = updateDownloadFuture_.get();
            if (updateCancellation_->load()) {
                preparedUpdate_.reset();
                if (updateDownloadStatus_)
                    updateDownloadStatus_->phase = updates::DownloadPhase::cancelled;
                updateNotice_ = "Download cancelled. Received bytes are retained for an explicit retry.";
            } else if (!quitRequested_)
                updateOffer_ = true;
        } catch (const updates::DownloadError &e) {
            if (updateCancellation_->load())
                updateNotice_ = "Download cancelled. Received bytes are retained for an explicit retry.";
            else {
                updateDownloadIssue_ = e.issue();
                updateDownloadDetails_ = e.what();
            }
        } catch (const std::filesystem::filesystem_error &e) {
            if (updateCancellation_->load())
                updateNotice_ = "Download cancelled. Received bytes are retained for an explicit retry.";
            else {
                updateDownloadIssue_ = updates::DownloadIssue::storage;
                updateDownloadDetails_ = e.what();
            }
        } catch (const std::exception &e) {
            if (updateCancellation_->load())
                updateNotice_ = "Download cancelled. Received bytes are retained for an explicit retry.";
            else {
                updateDownloadIssue_ = updates::DownloadIssue::other;
                updateDownloadDetails_ = e.what();
            }
        }
        if (updateCancellation_->load() && updateDownloadStatus_)
            updateDownloadStatus_->phase = updates::DownloadPhase::cancelled;
        // One local observation after an interrupted/failed attempt; no new
        // network request, automatic retry or restored installation proposal.
        if (!preparedUpdate_ && !quitRequested_)
            refreshApplicationUpdates(false, true);
    }
    updateDownloadMeter_.observe(
        ImGui::GetTime(), updateDownloadStatus_ ? updateDownloadStatus_->networkBytes.load() : 0,
        updateDownloadFuture_.valid() && updateDownloadStatus_ &&
            updateDownloadStatus_->phase.load() == updates::DownloadPhase::receiving);
}
void Application::applicationDownloadProgress() {
    if (!updateDownloadStatus_ || !updateProgress_)
        return;
    constexpr double mib = 1024.0 * 1024.0;
    const auto &status = *updateDownloadStatus_;
    const auto phase = status.phase.load();
    const auto received = updateProgress_->load();
    if (!updateDownloadFuture_.valid())
        cyui::sectionTitle(tr("Last download observation"));
    cyui::textDim("%s: %s", tr("Download target"), status.version.c_str());
    if (!updateDownloadFuture_.valid())
        cyui::textDim("%s", tr(updates::detail::downloadPhaseFeedback(phase).label.data()));
    if (status.totalBytes) {
        cyui::progressBar(float(std::min(1.0, double(received) / double(status.totalBytes))));
        cyui::textDim("%.2f / %.2f %s", double(received) / mib, double(status.totalBytes) / mib, tr("MiB"));
    }
    cyui::textDim("%s: %.2f %s | %s: %.2f %s", tr("Initial retained prefix"),
                  double(status.resumed.load()) / mib, tr("MiB"), tr("Received this attempt"),
                  double(status.networkBytes.load()) / mib, tr("MiB"));
    if (updateDownloadFuture_.valid() && phase == updates::DownloadPhase::receiving) {
        if (updateDownloadMeter_.bytesPerSecond > 0)
            cyui::textDim("%s: %.2f %s", tr("Transfer rate"), updateDownloadMeter_.bytesPerSecond / mib,
                          tr("MiB/s"));
        const auto remaining = updateDownloadMeter_.remaining(received, status.totalBytes);
        if (remaining)
            cyui::textDim("%s: %llu %s", tr("Estimated transfer time remaining"),
                          static_cast<unsigned long long>(*remaining), tr("seconds"));
        else
            cyui::textDim("%s", tr("Estimating remaining time..."));
    }
    if (status.spaceKnown.load()) {
        cyui::textDim("%s: %.2f %s | %s: %.2f %s", tr("Estimated additional space"),
                      double(status.requiredBytes.load()) / mib, tr("MiB"),
                      tr("Last observed available space"), double(status.availableBytes.load()) / mib,
                      tr("MiB"));
        cyui::helpMarker(tr("Space is checked before transfer, full-download fallback and staging. This "
                            "estimate is not a reservation or an installation-space guarantee."));
    }
    if (status.fullFallback.load()) {
        ImGui::PushTextWrapPos(0.0f);
        cyui::textDim("%s", tr("The server ignored resume. The earlier partial is retained; a full download "
                               "needs its own space."));
        ImGui::PopTextWrapPos();
    }
}
bool Application::finishQuit() {
    if (!updateInstallRequested_)
        return true;
    if (smoke_ || testSession_ || !preparedUpdate_) {
        done_ = false;
        quitRequested_ = false;
        updateInstallRequested_ = false;
        return false;
    }
    try {
        updates::launch(*preparedUpdate_);
        updateInstallRequested_ = false;
        return true;
    } catch (const std::exception &e) {
        updateError_ = e.what();
        preparedUpdate_.reset();
        done_ = false;
        quitRequested_ = false;
        updateInstallRequested_ = false;
        showPanel("Application updates", "updates");
        return false;
    }
}
void Application::applicationUpdatesContent() {
#ifndef CYCODE_IDE_ENABLE_UPDATES
    cyui::textDim("CyCodeIDE %s", CYCODE_IDE_VERSION);
    cyui::textDim("%s",tr("Updates are unavailable in this development build."));
    if (cyui::button(tr("Open GitHub Releases")))
        SDL_OpenURL("https://github.com/MrMybal/CyCodeIDE/releases");
    return;
#endif
    cyui::textDim("CyCode %s | %s", CYCODE_IDE_VERSION, updates::repository);
    const bool pending = applicationUpdatePending();
    {
        cyui::ScopedDisabled disabled(pending || smoke_ || testSession_ ||
                                      updates::platform() == "unsupported");
        if (cyui::button(tr("Check for updates")))
            checkApplicationUpdates(true);
        if (cyui::button(tr("Refresh downloaded updates")))
            refreshApplicationUpdates();
    }
    if (cyui::button(tr("Open GitHub Releases")))
        SDL_OpenURL("https://github.com/MrMybal/CyCode/releases");
    if (cyui::toggleSwitch(tr("Check for updates at startup"), &checkUpdatesAtStartup_)) {
        try {
            saveSettings();
        } catch (const std::exception &e) {
            updateError_ = e.what();
        }
    }
    cyui::helpMarker(tr("Private releases use CYCODE_UPDATE_TOKEN, GH_TOKEN or GITHUB_TOKEN from the "
                        "environment. Credentials are never saved in preferences."));
    if (!skippedUpdate_.empty()) {
        cyui::textDim("%s: %s", tr("Skipped version"), skippedUpdate_.c_str());
        if (cyui::button(tr("Reset skipped version"))) {
            skippedUpdate_.clear();
            try {
                saveSettings();
            } catch (const std::exception &e) {
                updateError_ = e.what();
            }
        }
    }
    if (pending) {
        cyui::spinner("##application-update");
        ImGui::SameLine();
        const auto label =
            (updateStorageFuture_.valid() || updateStorageActionFuture_.valid())
                ? "Managing update storage..."
            : updateInspectFuture_.valid() ? "Verifying downloaded updates..."
            : updateHistoryFuture_.valid() ? "Reading update history..."
            : updateDownloadFuture_.valid() && updateDownloadStatus_
                ? updates::detail::downloadPhaseFeedback(updateDownloadStatus_->phase.load()).label.data()
            : updateDownloadFuture_.valid() ? "Downloading update..."
                                            : "Checking GitHub Releases...";
        cyui::textDim("%s", tr(label));
        {
            cyui::ScopedDisabled disabled(!updateCancellation_ || updateCancellation_->load());
            if (cyui::button(tr("Cancel update request")) && updateCancellation_)
                *updateCancellation_ = true;
        }
        if (updateCancellation_ && updateCancellation_->load())
            cyui::textDim("%s", tr("Cancelling update request..."));
    }
    applicationDownloadProgress();
    if (!updateNotice_.empty())
        cyui::callout(cyui::Tone::neutral, tr(updateNotice_.c_str()));
    if (!updateCacheNotice_.empty())
        cyui::callout(cyui::Tone::neutral, tr(updateCacheNotice_.c_str()));
    if (updateDownloadIssue_ != updates::DownloadIssue::none) {
        const auto feedback = updates::detail::downloadFeedback(updateDownloadIssue_);
        cyui::callout(cyui::Tone::warning, tr(feedback.title.data()));
        ImGui::PushTextWrapPos(0.0f);
        cyui::textDim("%s", tr(feedback.action.data()));
        if (!updateDownloadDetails_.empty() && ImGui::CollapsingHeader(tr("Download technical details")))
            ImGui::TextWrapped("%s", updateDownloadDetails_.c_str());
        ImGui::PopTextWrapPos();
    }
    if (!updateError_.empty())
        cyui::callout(cyui::Tone::warning, updateError_.c_str());
    if (updateInventory_.rejected)
        cyui::textDim("%s: %u", tr("Invalid cached update records ignored"), updateInventory_.rejected);
    if (updateInventory_.limited)
        cyui::callout(cyui::Tone::neutral,
                      tr("Update cache inspection reached its budget. Older entries may be omitted."));
    if (!updateInventory_.partials.empty() && ImGui::CollapsingHeader(tr("Incomplete update downloads"))) {
        ImGui::PushTextWrapPos(0.0f);
        cyui::textDim("%s", tr("These observed bytes are not verified or installable. Check GitHub Releases, "
                               "then download again to resume the exact matching archive when possible."));
        ImGui::PopTextWrapPos();
        for (const auto &partial : updateInventory_.partials) {
            cyui::textDim("%s | %llu / %llu %s", partial.version.c_str(),
                          static_cast<unsigned long long>(partial.received),
                          static_cast<unsigned long long>(partial.asset.size), tr("bytes"));
            cyui::tooltip(pathUtf8(partial.directory).c_str());
        }
    }
    if (updates::platform() == "unsupported")
        cyui::callout(cyui::Tone::neutral, tr("Automatic installation currently supports Windows x64 "
                                              "installed and portable packages. Use GitHub Releases for other platforms."));
    if (ImGui::CollapsingHeader(tr("Update history"))) {
        updateHistoryContent();
        if (!updateHistory_ && updateInventory_.attempts.empty())
            cyui::textDim("%s", tr("No recorded installation attempts."));
        if (!updateHistory_)
            for (const auto &attempt : updateInventory_.attempts) {
                const auto label = !attempt.failure.empty()
                                       ? updates::detail::installFailureLabel(
                                             updates::detail::installFailure(attempt.failure))
                                             .data()
                                   : attempt.state == "recovered"      ? "Previous installation restored"
                                   : attempt.state == "installed"      ? "Installation completed"
                                   : attempt.state == "restart-failed" ? "Installed; restart failed"
                                   : attempt.state == "waiting" ? "Waiting for instances or interrupted"
                                                                : "Installation failed";
                cyui::textDim("%s | %s", attempt.version.c_str(), tr(label));
                cyui::tooltip(pathUtf8(attempt.directory).c_str());
            }
        if (cyui::button(tr("Copy update report"))) {
            try {
                Json attempts = Json::array();
                for (const auto &attempt : updateInventory_.attempts)
                    attempts.push_back({{"version", attempt.version},
                                        {"state", attempt.state},
                                        {"failure", attempt.failure},
                                        {"time", attempt.time},
                                        {"directory", pathUtf8(attempt.directory)}});
                Json report{{"version", CYCODE_IDE_VERSION},
                            {"platform", updates::platform()},
                            {"installation", pathUtf8(root_)},
                            {"attempts", attempts},
                            {"rejected", updateInventory_.rejected},
                            {"limited", updateInventory_.limited}};
                report["download"] = {
                    {"attempted", bool(updateProgress_)},
                    {"mode", updateDownloadMode_ == updates::DownloadMode::fresh ? "fresh" : "resume"},
                    {"issue", updates::detail::downloadFeedback(updateDownloadIssue_).code},
                    {"received", updateProgress_ ? updateProgress_->load() : uint64_t{0}}};
                if (updateDownloadStatus_) {
                    const auto &status = *updateDownloadStatus_;
                    report["download"]["status"] = {
                        {"version", status.version},
                        {"total", status.totalBytes},
                        {"phase", updates::detail::downloadPhaseFeedback(status.phase.load()).code},
                        {"initialOffset", status.resumed.load()},
                        {"networkBytes", status.networkBytes.load()},
                        {"fullFallback", status.fullFallback.load()},
                        {"spaceKnown", status.spaceKnown.load()},
                        {"requiredBytes", status.requiredBytes.load()},
                        {"availableBytes", status.availableBytes.load()}};
                }
                report["partials"] = Json::array();
                for (const auto &partial : updateInventory_.partials)
                    report["partials"].push_back({{"version", partial.version},
                                                  {"received", partial.received},
                                                  {"size", partial.asset.size},
                                                  {"directory", pathUtf8(partial.directory)}});
                if (preparedUpdate_)
                    report["prepared"] = {{"version", preparedUpdate_->version},
                                          {"sha256", preparedUpdate_->digest}};
                if (updateHistory_)
                    report["history"] = Json::parse(updates::historyReport(*updateHistory_));
                if (!SDL_SetClipboardText(report.dump(2).c_str()))
                    updateError_ = SDL_GetError();
            } catch (const std::exception &error) {
                updateError_ = error.what();
            }
        }
        ImGui::PushTextWrapPos(0.0f);
        cyui::textDim("%s", tr("Installation errors are kept in update-error.txt beside the downloaded "
                               "archive. Backups remain in data/updates."));
        ImGui::PopTextWrapPos();
    }
    updateStorageContent();
    // Advisory only, cached per window. Every native action revalidates the installation.
    auto *storage = ImGui::GetStateStorage();
    const auto modeId = ImGui::GetID(("##update-installation:" + pathUtf8(root_)).c_str());
    auto mode = storage->GetInt(modeId, -1);
    if (mode < 0) {
        mode = updates::portableInstallation(root_) ? 1 : 0;
        storage->SetInt(modeId, mode);
    }
    const bool portable = mode == 1;
    if (!portable && updates::platform() != "unsupported")
        cyui::callout(cyui::Tone::neutral,
                      tr("This copy is a development checkout or a legacy package. Download a current "
                         "installed or portable release to enable installation updates."));
    if (preparedUpdate_) {
        cyui::sectionTitle(tr("Verified downloaded update"));
        cyui::textDim("%s -> %s", CYCODE_IDE_VERSION, preparedUpdate_->version.c_str());
        cyui::callout(cyui::Tone::success,
                      tr("Download verified. Installation begins only after the normal save and close flow, "
                         "once all instances of this copy have closed."));
        cyui::ScopedDisabled disabled(pending || smoke_ || testSession_ || !portable);
        if (cyui::primaryButton(tr("Install update and restart"))) {
            updateInstallRequested_ = true;
            updateOffer_ = false;
            requestQuit();
        }
    }
    if (!latestUpdate_) {
        if (updateChecked_ && !preparedUpdate_)
            cyui::callout(cyui::Tone::success, tr("You are using the latest available stable version."));
        else if (updateChecked_)
            cyui::callout(cyui::Tone::neutral,
                          tr("GitHub reports no newer stable release. The previously downloaded update "
                             "remains available for explicit installation."));
        return;
    }
    cyui::sectionTitle(tr("Update available"));
    cyui::textDim("%s -> %s", CYCODE_IDE_VERSION, latestUpdate_->version.c_str());
    if (!latestUpdate_->notes.empty()) {
        ImGui::BeginChild("##release-notes", cyui::dp(-1, 160), ImGuiChildFlags_Borders);
        ImGui::TextWrapped("%s", latestUpdate_->notes.c_str());
        ImGui::EndChild();
    }
    if (cyui::button(tr("View this release")))
        SDL_OpenURL(latestUpdate_->page.c_str());
    if (portable && latestUpdate_->asset.url.empty())
        cyui::callout(cyui::Tone::neutral, tr("This release has no compatible verified update archive. Open "
                                              "the release to download it manually."));
    if (!preparedUpdate_ || preparedUpdate_->version != latestUpdate_->version) {
        cyui::ScopedDisabled disabled(pending || smoke_ || testSession_ || !portable ||
                                      latestUpdate_->asset.url.empty());
        const auto label = preparedUpdate_ ? "Download or resume newer update" : "Download or resume update";
        if (cyui::button(tr(label)))
            downloadApplicationUpdate();
        if (cyui::button(tr("Start a fresh download")))
            downloadApplicationUpdate(updates::DownloadMode::fresh);
        cyui::helpMarker(tr("A fresh download ignores existing checkpoints and starts in a new folder. "
                            "Retained files are not deleted and can use additional disk space."));
    }
}
void Application::applicationUpdatesPanel() {
    panels_.try_emplace("updates", false);
    if (beginPanel("Application updates", "updates")) {
        applicationUpdatesContent();
        ImGui::End();
    }
    if (updateOffer_ && !quitRequested_ &&
        cyui::beginModal(tr("Application update available"), &updateOffer_, 600)) {
        applicationUpdatesContent();
        if (!updateOffer_) {
            ImGui::CloseCurrentPopup();
            cyui::endModal();
            return;
        }
        if (cyui::button(tr("Later"))) {
            updateOffer_ = false;
            ImGui::CloseCurrentPopup();
        }
        if ((preparedUpdate_ || latestUpdate_) && cyui::button(tr("Skip this version"))) {
            skippedUpdate_ = latestUpdate_ ? latestUpdate_->version : preparedUpdate_->version;
            updateOffer_ = false;
            try {
                saveSettings();
            } catch (const std::exception &e) {
                updateError_ = e.what();
            }
            ImGui::CloseCurrentPopup();
        }
        cyui::endModal();
    }
}
} // namespace cycode
