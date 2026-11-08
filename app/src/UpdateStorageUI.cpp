#include "cycode/Application.h"
#include <chrono>

namespace cycode {
void Application::inspectUpdateStorage() {
    if (smoke_ || testSession_ || applicationUpdatePending() || quitRequested_)
        return;
    updateStartupChecked_ = true;
    updateStorageReview_.reset();
    updateStorageSelection_.reset();
    updateStorageAcknowledged_ = false;
    updateError_.clear();
    updateDownloadIssue_ = updates::DownloadIssue::none;
    updateDownloadDetails_.clear();
    updateCacheNotice_.clear();
    updateCancellation_ = std::make_shared<std::atomic<bool>>(false);
    const auto cancel = updateCancellation_;
    const auto root = root_;
    updateStorageFuture_ =
        std::async(std::launch::async, [root, cancel] { return updates::reviewStorage(root, *cancel); });
}
void Application::actOnUpdateStorage() {
    if (smoke_ || testSession_ || applicationUpdatePending() || quitRequested_ ||
        !updateStorageAcknowledged_ || !updateStorageReview_ || !updateStorageSelection_)
        return;
    const auto review = *updateStorageReview_;
    const auto index = *updateStorageSelection_;
    const auto restore = updateStorageRestore_;
    if (restore ? index >= review.retired.size() || review.retired[index].conflicts ||
                      !review.retired[index].restore
                : index >= review.downloads.size())
        return;
    updateError_.clear();
    updateNotice_.clear();
    updateDownloadIssue_ = updates::DownloadIssue::none;
    updateDownloadDetails_.clear();
    updateCacheNotice_.clear();
    updateStorageResult_.reset();
    updateStorageAcknowledged_ = false;
    updateCancellation_ = std::make_shared<std::atomic<bool>>(false);
    const auto cancel = updateCancellation_;
    updateStorageActionFuture_ = std::async(std::launch::async, [review, index, restore, cancel] {
        return restore ? updates::restoreDownload(review, index, *cancel)
                       : updates::retireDownload(review, index, *cancel);
    });
}
void Application::updateUpdateStorage() {
    if (smoke_ || testSession_)
        return;
    if (updateStorageFuture_.valid() &&
        updateStorageFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto review = updateStorageFuture_.get();
            if (!updateCancellation_->load())
                updateStorageReview_ = std::move(review);
            else
                updateNotice_ = "Update cancelled";
        } catch (const std::exception &e) {
            if (updateCancellation_->load())
                updateNotice_ = "Update cancelled";
            else
                updateError_ = e.what();
        }
    }
    if (updateStorageActionFuture_.valid() &&
        updateStorageActionFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            updateStorageResult_ = updateStorageActionFuture_.get();
            if (!updateStorageResult_->error.empty())
                updateError_ = updateStorageResult_->error;
        } catch (const std::exception &e) {
            if (updateCancellation_->load())
                updateNotice_ = "Update cancelled";
            else
                updateError_ = e.what();
        }
        updateStorageReview_.reset();
        updateStorageSelection_.reset();
        updateStorageAcknowledged_ = false;
    }
}
void Application::updateStorageContent() {
    if (!ImGui::CollapsingHeader(tr("Update storage")))
        return;
    ImGui::PushTextWrapPos(0.0f);
    cyui::textDim("%s", tr("Retiring obsolete downloads is reversible and does not free disk space. Backups, "
                           "recovery data, current releases and unknown files are protected."));
    ImGui::PopTextWrapPos();
    const bool pending = applicationUpdatePending();
    {
        cyui::ScopedDisabled disabled(pending || smoke_ || testSession_ || quitRequested_ ||
                                      updates::platform() == "unsupported");
        if (cyui::button(tr("Inspect update storage")))
            inspectUpdateStorage();
    }
    if (updateStorageResult_) {
        cyui::sectionTitle(tr("Last cache operation"));
        const auto &r = *updateStorageResult_;
        cyui::callout(r.complete ? cyui::Tone::success : cyui::Tone::warning,
                      tr(r.complete ? "Cache operation completed. Inspect again to review the new state."
                                    : "Cache operation stopped. Retained files and its receipt allow a fresh "
                                      "restoration review."));
        cyui::textDim("%s: %u | %llu bytes", tr("Files moved"), r.files,
                      static_cast<unsigned long long>(r.bytes));
        ImGui::TextWrapped("%s", pathUtf8(r.directory).c_str());
    }
    if (!updateStorageReview_)
        return;
    const auto &review = *updateStorageReview_;
    cyui::textDim("%s: %u | %s: %u", tr("Protected directories"), review.protectedEntries,
                  tr("Protected or unrecognized cache entries"), review.rejected);
    if (review.limited)
        cyui::callout(cyui::Tone::neutral,
                      tr("Storage inspection reached its budget. Some entries are omitted."));
    if (review.downloads.empty() && review.retired.empty())
        cyui::textDim("%s", tr("No eligible obsolete downloads or retired caches were found."));
    cyui::ScopedDisabled disabled(pending || smoke_ || testSession_ || quitRequested_);
    for (size_t i = 0; i < review.downloads.size(); ++i) {
        const auto &d = review.downloads[i];
        ImGui::PushID(static_cast<int>(i));
        cyui::textDim("%s | %llu bytes | %s", d.version.c_str(), static_cast<unsigned long long>(d.bytes),
                      pathUtf8(d.directory.filename()).c_str());
        if (cyui::button(tr("Review retirement"))) {
            updateStorageSelection_ = i;
            updateStorageRestore_ = false;
            updateStorageAcknowledged_ = false;
        }
        ImGui::PopID();
    }
    for (size_t i = 0; i < review.retired.size(); ++i) {
        const auto &d = review.retired[i];
        ImGui::PushID(static_cast<int>(i + 100));
        cyui::textDim("%s | %s: %u | %s: %u", d.version.c_str(), tr("Files to restore"), d.restore,
                      tr("Conflicts"), d.conflicts);
        cyui::textDim("%s", pathUtf8(d.directory.filename()).c_str());
        {
            cyui::ScopedDisabled blocked(d.conflicts || !d.restore);
            if (cyui::button(tr("Review cache restoration"))) {
                updateStorageSelection_ = i;
                updateStorageRestore_ = true;
                updateStorageAcknowledged_ = false;
            }
        }
        ImGui::PopID();
    }
    if (!updateStorageSelection_)
        return;
    const auto selected = *updateStorageSelection_;
    cyui::sectionTitle(tr("Selected cache action"));
    if (updateStorageRestore_) {
        if (selected >= review.retired.size())
            return;
        const auto &d = review.retired[selected];
        ImGui::TextWrapped("%s\n%s -> %s", d.version.c_str(), pathUtf8(d.directory).c_str(),
                           pathUtf8(d.source).c_str());
        cyui::textDim("%s: %u | %s: %u", tr("Files to restore"), d.restore, tr("Conflicts"), d.conflicts);
        for (const auto &file : d.files)
            ImGui::TextWrapped("%s | %llu bytes", file.name.c_str(),
                               static_cast<unsigned long long>(file.size));
    } else {
        if (selected >= review.downloads.size())
            return;
        const auto &d = review.downloads[selected];
        ImGui::TextWrapped("%s\n%s", d.version.c_str(), pathUtf8(d.directory).c_str());
        for (const auto &file : d.files)
            ImGui::TextWrapped("%s | %llu bytes", file.name.c_str(),
                               static_cast<unsigned long long>(file.size));
    }
    ImGui::Checkbox(tr("I reviewed this cache action"), &updateStorageAcknowledged_);
    {
        cyui::ScopedDisabled notReviewed(!updateStorageAcknowledged_);
        const auto label =
            updateStorageRestore_ ? "Restore reviewed cache files" : "Retire reviewed download";
        if (cyui::button(tr(label)))
            actOnUpdateStorage();
    }
}
} // namespace cycode
