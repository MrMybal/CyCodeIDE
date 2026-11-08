#include "cycode/update/detail/UpdateHistoryPolicy.h"
#include "cycode/update/detail/UpdateInstall.h"
#include "UpdateRecoveryFeedback.h"
#include "cycode/Application.h"
#include <algorithm>
#include <chrono>
#include <ctime>

namespace cycode {
namespace {
const char *historyLabel(const updates::HistoryEntry &entry) {
    if (entry.kind == updates::HistoryKind::recovery)
        return entry.state == "restored"    ? "Recorded recovery completed"
               : entry.state == "cancelled" ? "Recorded recovery cancelled"
                                            : "Recorded recovery failed";
    if (!entry.failure.empty())
        return updates::detail::installFailureLabel(updates::detail::installFailure(entry.failure)).data();
    if (entry.state == "recovered")
        return "Previous installation restored";
    if (entry.state == "installed")
        return "Installation completed";
    if (entry.state == "restart-failed")
        return "Installed; restart failed";
    if (entry.state == "waiting")
        return "Waiting for instances or interrupted";
    return "Installation failed";
}
std::string historyTime(const std::optional<int64_t> &value) {
    if (!value)
        return {};
    const auto stamp = static_cast<std::time_t>(*value);
    if (int64_t(stamp) != *value)
        return {};
    std::tm utc{};
#ifdef _WIN32
    if (gmtime_s(&utc, &stamp))
        return {};
#else
    if (!gmtime_r(&stamp, &utc))
        return {};
#endif
    char text[32]{};
    if (!std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S UTC", &utc))
        return {};
    return text;
}
} // namespace
void Application::inspectUpdateHistory() {
    if (smoke_ || testSession_ || applicationUpdatePending() || quitRequested_)
        return;
    updateStartupChecked_ = true;
    updateError_.clear();
    updateCacheNotice_.clear();
    updateCancellation_ = std::make_shared<std::atomic<bool>>(false);
    const auto root = root_;
    const auto cancel = updateCancellation_;
    updateHistoryFuture_ =
        std::async(std::launch::async, [root, cancel] { return updates::inspectHistory(root, *cancel); });
}
void Application::updateUpdateHistory() {
    if (!updateHistoryFuture_.valid() ||
        updateHistoryFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    try {
        auto history = updateHistoryFuture_.get();
        if (!updateCancellation_->load()) {
            updateHistory_ = std::move(history);
            updateHistorySelection_.reset();
            updateHistoryPage_ = 0;
            updateHistoryKey_.clear();
        } else
            updateCacheNotice_ = "History inspection cancelled. The previous snapshot is unchanged.";
    } catch (const std::exception &error) {
        if (updateCancellation_->load())
            updateCacheNotice_ = "History inspection cancelled. The previous snapshot is unchanged.";
        else
            updateError_ = error.what();
    }
}
void Application::updateHistoryContent() {
    {
        cyui::ScopedDisabled disabled(applicationUpdatePending() || smoke_ || testSession_ ||
                                      quitRequested_ || updates::platform() == "unsupported");
        if (cyui::button(tr("Inspect detailed update history")))
            inspectUpdateHistory();
    }
    if (!updateHistory_)
        return;
    const auto &history = *updateHistory_;
    ImGui::PushTextWrapPos(0.0f);
    cyui::textDim("%s", tr("History is a cached metadata observation, not verification of the current "
                           "installation. Refresh explicitly; no archive or backup payload is read."));
    ImGui::PopTextWrapPos();
    cyui::filterBox("##updateHistoryFilter", updateHistoryFilter_,
                    {tr("Filter versions, results, dates or paths...")});
    const char *scopes[] = {tr("All history entries"), tr("Needs attention"), tr("Installations"),
                            tr("Recoveries")};
    ImGui::Combo(tr("History category"), &updateHistoryScope_, scopes, 4);
    const auto key = updateHistoryFilter_.query + ":" + std::to_string(updateHistoryScope_) + ":" +
                     std::string(cyui::locale());
    if (key != updateHistoryKey_) {
        updateHistoryKey_ = key;
        updateHistoryMatches_.clear();
        updateHistoryLabels_.clear();
        for (size_t i = 0; i < history.entries.size(); ++i) {
            const auto &entry = history.entries[i];
            const auto date = historyTime(entry.time);
            const auto kind = entry.kind == updates::HistoryKind::recovery ? "Recoveries" : "Installations";
            const auto title = entry.version + " | " + tr(historyLabel(entry)) + " | " +
                               (date.empty() ? tr("Date unavailable") : date);
            updateHistoryLabels_.push_back(title);
            const bool scope =
                updateHistoryScope_ == 0 ||
                (updateHistoryScope_ == 1 && updates::detail::historyAttention(entry.kind, entry.state)) ||
                (updateHistoryScope_ == 2 && entry.kind == updates::HistoryKind::installation) ||
                (updateHistoryScope_ == 3 && entry.kind == updates::HistoryKind::recovery);
            if (scope && updateHistoryFilter_.matches(title, pathUtf8(entry.directory) + " " + entry.state +
                                                                 " " + entry.failure + " " + kind + " " +
                                                                 historyLabel(entry) + " " + tr(kind)))
                updateHistoryMatches_.push_back(i);
        }
        updateHistoryPage_ = 0;
        updateHistorySelection_.reset();
    }
    constexpr size_t perPage = 12;
    const auto pages = std::max(size_t{1}, (updateHistoryMatches_.size() + perPage - 1) / perPage);
    updateHistoryPage_ = std::min(updateHistoryPage_, pages - 1);
    cyui::textDim("%zu / %zu | %s %zu / %zu", updateHistoryMatches_.size(), history.entries.size(),
                  tr("Page"), updateHistoryPage_ + 1, pages);
    {
        cyui::ScopedDisabled disabled(updateHistoryPage_ == 0);
        if (cyui::button(tr("Previous history page"))) {
            --updateHistoryPage_;
            updateHistorySelection_.reset();
        }
    }
    ImGui::SameLine();
    {
        cyui::ScopedDisabled disabled(updateHistoryPage_ + 1 >= pages);
        if (cyui::button(tr("Next history page"))) {
            ++updateHistoryPage_;
            updateHistorySelection_.reset();
        }
    }
    if (history.limited)
        cyui::callout(cyui::Tone::warning, tr("History inspection reached its budget. Some records are "
                                              "omitted; pages cover this snapshot only."));
    if (history.rejected)
        cyui::textDim("%s: %u", tr("Invalid history records ignored"), history.rejected);
    if (updateHistoryMatches_.empty())
        cyui::textDim("%s", tr("No history entries match the filters."));
    if (ImGui::BeginChild("##updateHistoryList", ImVec2(0, cyui::dp(190)), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        const auto end = std::min(updateHistoryMatches_.size(), (updateHistoryPage_ + 1) * perPage);
        for (size_t row = updateHistoryPage_ * perPage; row < end; ++row) {
            const auto index = updateHistoryMatches_[row];
            ImGui::PushID(int(index));
            if (ImGui::Selectable(updateHistoryLabels_[index].c_str(), updateHistorySelection_ == index))
                updateHistorySelection_ = index;
            cyui::tooltip(pathUtf8(history.entries[index].directory).c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    if (!updateHistorySelection_ || *updateHistorySelection_ >= history.entries.size())
        return;
    const auto &entry = history.entries[*updateHistorySelection_];
    cyui::sectionTitle(tr("Selected history entry"));
    ImGui::PushTextWrapPos(0.0f);
    cyui::textDim("%s", pathUtf8(entry.directory).c_str());
    if (entry.recovery) {
        const auto &s = *entry.recovery;
        cyui::textDim("%s", tr(updates::detail::recoveryPhaseLabel(s.phase)));
        cyui::textDim("%s: %u / %u | %s: %u / %u", tr("Prepared copies"), s.prepared, s.prepareTotal,
                      tr("Restored entries"), s.completed, s.total);
        cyui::textDim("%s: %llu %s", tr("Copied during this attempt"),
                      static_cast<unsigned long long>(s.copiedBytes), tr("bytes"));
        cyui::textDim("%s", tr(s.installationChanged ? "This attempt recorded installed-file moves."
                                                     : "This attempt recorded no installed-file moves."));
        cyui::textDim("%s", tr(entry.pendingCleared ? "The receipt records completed pending-marker removal."
                                                    : "The receipt does not record pending-marker removal."));
    }
    ImGui::PopTextWrapPos();
    if (cyui::button(tr("Copy selected history entry"))) {
        try {
            const auto report = updates::historyReport(history, updateHistorySelection_);
            if (!SDL_SetClipboardText(report.c_str()))
                updateError_ = SDL_GetError();
        } catch (const std::exception &error) {
            updateError_ = error.what();
        }
    }
}
} // namespace cycode
