#include "UpdaterRecoveryUI.h"
#include "cycode/update/detail/UpdateFiles.h"
#include "cycode/update/detail/UpdateManifest.h"
#include "UpdateRecoveryFeedback.h"
#include "cycode/Paths.h"
#include "cycode/UpdateRecovery.h"
#include "cycode/UpdateRecoveryProgress.h"
#include <chrono>
#include <cstring>
#include <future>
#include <stdexcept>
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <commctrl.h>
// clang-format on

namespace cycode::updates {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
fs::path executable() {
    std::wstring value(32768, 0);
    const auto count = GetModuleFileNameW(nullptr, value.data(), static_cast<DWORD>(value.size()));
    if (!count || count >= value.size())
        throw std::runtime_error("Cannot locate the running update helper");
    value.resize(count);
    detail::safe(fs::path(value));
    return fs::canonical(fs::path(value));
}
// Thin adapter for the native helper's system dialogs. The application remains
// owner of EN/FR messages/preferences; no ImGui context or second catalogue.
struct Messages {
    Json catalogue = Json::object();
    std::string language = "en";
    Messages(const fs::path &root, const fs::path &staging) {
        try {
            const auto prefs =
                Json::parse(detail::read(root / "data/settings/ui-preferences.json", 2 * 1024 * 1024));
            if (prefs.value("language", std::string{}) == "fr")
                language = "fr";
        } catch (...) {
        }
        for (const auto &file : {staging / "updater-messages.json", root / "app/locales/ui.json"}) {
            try {
                catalogue = Json::parse(detail::read(file, 4 * 1024 * 1024));
                break;
            } catch (...) {
            }
        }
    }
    std::string tr(const char *key) const {
        try {
            const auto result = catalogue.at(key).at(language).get<std::string>();
            if (!result.empty() && result.size() <= 4096)
                return result;
        } catch (...) {
        }
        return key;
    }
    int box(const std::string &message, UINT flags, const char *titleKey = "CyCode update recovery") const {
        const auto body = utf8Path(message).wstring(), title = utf8Path(tr(titleKey)).wstring();
        return MessageBoxW(nullptr, body.c_str(), title.c_str(), flags);
    }
};
struct RecoveryRun {
    RecoverySnapshot snapshot;
    bool restored = false, cancelled = false;
    std::string technical;
};
struct LiveRecovery {
    const Recovery &review;
    const Messages &messages;
    RecoveryProgress progress;
    std::atomic<bool> cancel{false};
    std::future<void> worker;
    std::exception_ptr callbackError;
    bool finished = false, destroyed = false;
    std::optional<bool> marquee;
    std::wstring lastContent, lastTitle;
    LiveRecovery(const Recovery &r, const Messages &m)
        : review(r), messages(m), progress(r.planDigest, pathUtf8(r.transaction)) {}
    void paint(HWND window) {
        const auto s = progress.snapshot();
        const auto title = utf8Path(messages.tr(detail::recoveryPhaseLabel(s.phase))).wstring();
        const auto content =
            utf8Path(
                messages.tr("Prepared copies") + ": " + std::to_string(s.prepared) + " / " +
                std::to_string(s.prepareTotal) + "\n" + messages.tr("Restored entries") + ": " +
                std::to_string(s.completed) + " / " + std::to_string(s.total) + "\n" +
                messages.tr("Copied during this attempt") + ": " + std::to_string(s.copiedBytes) + " " +
                messages.tr("bytes") + "\n\n" +
                messages.tr(
                    cancel ? "Stopping after the current operation..."
                           : "Stop requests are cooperative. Keep this window open until recovery finishes."))
                .wstring();
        if (title != lastTitle) {
            SendMessageW(window, TDM_SET_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION,
                         reinterpret_cast<LPARAM>(title.c_str()));
            lastTitle = title;
        }
        if (content != lastContent) {
            SendMessageW(window, TDM_SET_ELEMENT_TEXT, TDE_CONTENT,
                         reinterpret_cast<LPARAM>(content.c_str()));
            lastContent = content;
        }
        const auto percent = s.percent();
        const bool animate = !percent.has_value();
        if (!marquee || *marquee != animate) {
            SendMessageW(window, TDM_SET_MARQUEE_PROGRESS_BAR, animate, 0);
            SendMessageW(window, TDM_SET_PROGRESS_BAR_MARQUEE, animate, 0);
            marquee = animate;
        }
        if (percent)
            SendMessageW(window, TDM_SET_PROGRESS_BAR_POS, *percent, 0);
    }
    static HRESULT CALLBACK callback(HWND window, UINT event, WPARAM button, LPARAM, LONG_PTR data) noexcept {
        auto &self = *reinterpret_cast<LiveRecovery *>(data);
        try {
            if (event == TDN_DESTROYED) {
                self.destroyed = true;
                return S_OK;
            }
            if (event == TDN_CREATED) {
                SendMessageW(window, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(0, 100));
                // Start only once the visible dialog exists. The worker never
                // reads UI state or sends window messages.
                self.worker = std::async(std::launch::async,
                                         [&self] { recover(self.review, self.cancel, &self.progress); });
                self.paint(window);
            } else if (event == TDN_TIMER) {
                if (self.worker.valid() &&
                    self.worker.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    self.finished = true;
                    SendMessageW(window, TDM_ENABLE_BUTTON, IDCANCEL, TRUE);
                    SendMessageW(window, TDM_CLICK_BUTTON, IDCANCEL, 0);
                } else if (!self.callbackError)
                    self.paint(window);
            } else if (event == TDN_BUTTON_CLICKED) {
                if (self.finished)
                    return S_OK;
                self.cancel = true;
                SendMessageW(window, TDM_ENABLE_BUTTON, IDCANCEL, FALSE);
                self.paint(window);
                return S_FALSE; // Closing/Escape requests a stop, never detaches work.
            }
        } catch (...) {
            self.callbackError = std::current_exception();
            self.cancel = true;
            if (!self.worker.valid() && !self.destroyed) {
                self.finished = true;
                SendMessageW(window, TDM_CLICK_BUTTON, IDCANCEL, 0);
            }
            return event == TDN_BUTTON_CLICKED ? S_FALSE : S_OK;
        }
        return S_OK;
    }
};
RecoveryRun runRecovery(const Recovery &review, const Messages &messages) {
    LiveRecovery live(review, messages);
    const auto title = utf8Path(messages.tr("CyCode update recovery")).wstring();
    const auto stop = utf8Path(messages.tr("Stop recovery")).wstring();
    const auto text = utf8Path(messages.tr("Inspecting recovery files...")).wstring();
    const TASKDIALOG_BUTTON button{IDCANCEL, stop.c_str()};
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.dwFlags =
        TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER | TDF_ALLOW_DIALOG_CANCELLATION | TDF_CAN_BE_MINIMIZED;
    config.pszWindowTitle = title.c_str();
    config.pszMainInstruction = text.c_str();
    config.pszContent = text.c_str();
    config.cButtons = 1;
    config.pButtons = &button;
    config.nDefaultButton = IDCANCEL;
    config.pfCallback = LiveRecovery::callback;
    config.lpCallbackData = reinterpret_cast<LONG_PTR>(&live);
    const auto dialog = TaskDialogIndirect(&config, nullptr, nullptr, nullptr);
    RecoveryRun result;
    if (!live.finished || FAILED(dialog))
        live.cancel = true;
    if (live.worker.valid()) {
        // Always collect/join before destroying the cancellation/progress/review.
        try {
            live.worker.get();
            result.restored = true;
        } catch (const detail::Cancellation &) {
            result.cancelled = true;
        } catch (const std::exception &error) {
            result.technical = std::string(error.what()).substr(0, 16384);
        } catch (...) {
            result.technical = "Unexpected recovery worker failure";
        }
    } else {
        result.technical = "The recovery progress window or worker could not be created. No installation "
                           "files were changed.";
        live.progress.phase = RecoveryPhase::failed;
    }
    if (live.callbackError && !result.restored && result.technical.empty()) {
        try {
            std::rethrow_exception(live.callbackError);
        } catch (const std::exception &error) {
            result.technical = std::string(error.what()).substr(0, 16384);
        } catch (...) {
            result.technical = "Recovery progress display failed";
        }
    }
    result.snapshot = live.progress.snapshot();
    return result;
}
void copyReport(HWND window, const std::wstring &text) {
    const auto bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory)
        throw std::runtime_error("Cannot allocate the recovery clipboard report");
    auto destination = GlobalLock(memory);
    if (!destination) {
        GlobalFree(memory);
        throw std::runtime_error("Cannot access recovery clipboard memory");
    }
    std::memcpy(destination, text.c_str(), bytes);
    GlobalUnlock(memory);
    if (!OpenClipboard(window)) {
        GlobalFree(memory);
        throw std::runtime_error("Cannot open the clipboard");
    }
    const bool copied = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory);
    CloseClipboard();
    if (!copied) {
        GlobalFree(memory);
        throw std::runtime_error("Cannot copy the recovery report");
    }
}
struct ResultCopy {
    const Messages &messages;
    std::wstring report;
    static HRESULT CALLBACK callback(HWND window, UINT event, WPARAM button, LPARAM, LONG_PTR data) noexcept {
        if (event != TDN_BUTTON_CLICKED || button != 1001)
            return S_OK;
        auto &self = *reinterpret_cast<ResultCopy *>(data);
        try {
            copyReport(window, self.report);
        } catch (...) {
            try {
                const auto text =
                    utf8Path(self.messages.tr("The recovery report could not be copied.")).wstring();
                MessageBoxW(window, text.c_str(), L"CyCode", MB_OK | MB_ICONWARNING);
            } catch (...) {
            }
        }
        return S_FALSE;
    }
};
int showRecoveryResult(const Recovery &review, const RecoveryRun &run, const Messages &messages) {
    const auto *label =
        run.restored
            ? "The previous installation was restored. Original backups were retained. Restart CyCode now?"
        : run.cancelled ? (run.snapshot.installationChanged
                               ? "This recovery attempt stopped after installed files moved. Keep "
                                 "pending.json and all backups, then run this helper again."
                               : "This recovery attempt was cancelled before installed files moved. Keep "
                                 "pending.json and all backups.")
                        : "Recovery stopped. Keep pending.json, transaction backups and recovery files.";
    const auto title = utf8Path(messages.tr("CyCode update recovery")).wstring();
    const auto content = utf8Path(messages.tr(label)).wstring();
    const auto instruction = utf8Path(messages.tr(detail::recoveryPhaseLabel(run.snapshot.phase))).wstring();
    const auto technical = utf8Path(run.technical).wstring();
    const auto details = utf8Path(messages.tr("Technical details")).wstring();
    const auto close = utf8Path(messages.tr("Close")).wstring();
    const auto restart = utf8Path(messages.tr("Restart CyCode")).wstring();
    const auto copy = utf8Path(messages.tr("Copy recovery report")).wstring();
    ResultCopy clipboard{messages, utf8Path(recoveryReport(review, run.snapshot)).wstring()};
    const TASKDIALOG_BUTTON buttons[] = {
        {run.restored ? IDNO : IDCLOSE, close.c_str()}, {1001, copy.c_str()}, {IDYES, restart.c_str()}};
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = title.c_str();
    config.pszMainInstruction = instruction.c_str();
    config.pszContent = content.c_str();
    config.pszExpandedInformation = technical.empty() ? nullptr : technical.c_str();
    config.pszExpandedControlText = details.c_str();
    config.pszCollapsedControlText = details.c_str();
    config.pButtons = buttons;
    config.cButtons = run.restored ? 3 : 2;
    config.nDefaultButton = run.restored ? IDNO : IDCLOSE;
    config.pfCallback = ResultCopy::callback;
    config.lpCallbackData = reinterpret_cast<LONG_PTR>(&clipboard);
    int selected = 0;
    if (FAILED(TaskDialogIndirect(&config, &selected, nullptr, nullptr)))
        return messages.box(messages.tr(label) + (run.technical.empty() ? "" : "\n\n" + run.technical),
                            run.restored ? (MB_YESNO | MB_DEFBUTTON2 | MB_ICONINFORMATION)
                                         : (MB_OK | MB_ICONERROR));
    return selected;
}
} // namespace
void showInstallationFailure(const fs::path &root, const fs::path &staging, InstallFailure failure,
                             const std::string &details, bool installed) {
    Messages messages(root, staging);
    const auto label = installed
                           ? "The update was installed, but CyCode could not restart. Start it manually."
                           : detail::installFailureLabel(failure).data();
    messages.box(messages.tr(label) + "\n\n" + details.substr(0, 16384), MB_OK | MB_ICONERROR,
                 "CyCode update");
}
void restartApplication(const fs::path &root) {
    // Keep replacement excluded between manifest verification and process creation.
    auto lease = installationUse(root);
    if (!lease)
        throw std::runtime_error("Cannot restart an unsupported installation");
    const auto files =
        detail::managedManifest(Json::parse(detail::read(root / "manifest.json", 2 * 1024 * 1024)));
    const auto &expected = files.at("CyCode.exe");
    const auto file = root / "CyCode.exe";
    detail::safe(file);
    if (!fs::is_regular_file(file) || fs::hard_link_count(file) != 1 ||
        fs::file_size(file) != expected.size ||
        detail::hashFile(file, 64 * 1024 * 1024, nullptr) != expected.digest)
        throw std::runtime_error("Installed CyCode executable changed; restart was refused");
    std::wstring command = L"\"" + file.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(file.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, root.c_str(),
                        &startup, &process))
        throw std::runtime_error(
            "Installation is complete, but CyCode could not restart. Retain data/updates for recovery.");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}
std::optional<int> recoveryEntry(const std::vector<std::wstring> &arguments) {
    if (!arguments.empty() && arguments.front() != L"--recover")
        return std::nullopt;
    if (!arguments.empty() && arguments.size() != 2)
        throw std::runtime_error("Use --recover followed by the installation directory");
    const auto self = executable(), staging = self.parent_path();
    const bool staged = detail::workName(pathUtf8(staging.filename()), "download-") &&
                        staging.parent_path().filename() == "updates" &&
                        staging.parent_path().parent_path().filename() == "data";
    const auto root = fs::canonical(
        arguments.empty() ? (staged ? staging.parent_path().parent_path().parent_path() : staging)
                          : fs::path(arguments[1]));
    Messages messages(root, staging);
    bool restored = false;
    try {
        detail::safe(root);
        if (!staged || staging.parent_path() != root / "data/updates" ||
            self.filename() != "CyCodeUpdater.exe") {
            messages.box(messages.tr("To recover an interrupted update, run the staged CyCodeUpdater.exe "
                                     "inside data/updates/download-... . Keep pending.json and all backups."),
                         MB_OK | MB_ICONINFORMATION);
            return 2;
        }
        if (!fs::exists(root / "data/updates/pending.json")) {
            messages.box(
                messages.tr(
                    "No interrupted installation is pending. Open CyCode to install downloaded updates."),
                MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        const std::atomic<bool> cancel{false};
        auto cache = detail::cacheLease(root, NativeFileLeaseMode::shared);
        const auto review = reviewRecovery(root, cancel);
        const auto hash = detail::hashFile(self, 64 * 1024 * 1024, &cancel);
        if (fs::hard_link_count(self) != 1 || (hash != review.previousHelper && hash != review.nextHelper))
            throw std::runtime_error("This recovery helper differs from both recorded package versions");
        auto prepared = detail::stagedMetadata(root, staging);
        if (prepared.version != review.version || prepared.helper != self)
            throw std::runtime_error("Choose the staged helper for this interrupted release");
        auto summary = messages.tr("Installation") + ": " + pathUtf8(root) + "\n" + review.version + " -> " +
                       review.previousVersion + "\n\n" + messages.tr("Managed files to restore") + ": " +
                       std::to_string(review.restore) + "\n" +
                       messages.tr("New files to retain outside the installation") + ": " +
                       std::to_string(review.remove) + "\n" +
                       messages.tr("Files already in their previous state") + ": " +
                       std::to_string(review.unchanged) + "\n";
        if (review.conflictCount) {
            summary +=
                "\n" + messages.tr("Recovery conflicts") + ": " + std::to_string(review.conflictCount) + "\n";
            for (const auto &conflict : review.conflicts)
                summary += conflict + "\n";
            messages.box(summary + "\n" +
                             messages.tr("Local changes or missing backups prevent recovery. No installation "
                                         "files were changed."),
                         MB_OK | MB_ICONWARNING);
            return 2;
        }
        if (!review.space.known) {
            messages.box(
                summary + "\n" +
                    messages.tr(
                        "Recovery storage could not be inspected. No installation files were changed.") +
                    "\n\n" + review.space.error,
                MB_OK | MB_ICONWARNING);
            return 2;
        }
        summary += "\n" + messages.tr("Estimated additional recovery space") + ": " +
                   std::to_string(review.space.requiredBytes) + " " + messages.tr("bytes") + "\n" +
                   messages.tr("Last observed available space") + ": " +
                   std::to_string(review.space.availableBytes) + " " + messages.tr("bytes") + "\n";
        summary += messages.tr("Verified prepared copies available") + ": " +
                   std::to_string(review.space.reusedFiles) + "\n" +
                   messages.tr("Files to copy from backups") + ": " +
                   std::to_string(review.space.copyFiles - review.space.reusedFiles) + "\n";
        if (review.space.preparedIgnored || review.space.preparedLimited)
            summary += "\n" +
                       messages.tr("Some preparation records were ignored. Existing files are retained and "
                                   "required originals will be copied from backups.") +
                       "\n";
        if (review.space.reusedFiles)
            summary += "\n" +
                       messages.tr("Prepared copies are verified again before use. Original backups remain "
                                   "required and are retained.") +
                       "\n";
        if (review.space.availableBytes < review.space.requiredBytes) {
            messages.box(summary + "\n" +
                             messages.tr("There is not enough space to prepare recovery. No installation "
                                         "files were changed.") +
                             "\n\n" +
                             messages.tr("Free space on the installation drive, then run this helper again. "
                                         "Keep pending.json and all backups."),
                         MB_OK | MB_ICONWARNING);
            return 2;
        }
        summary += "\n" +
                   messages.tr("Recovery copies are prepared and verified before installed files move. This "
                               "space estimate is not a reservation.") +
                   "\n";
        summary += "\n" + messages.tr("Restore the previous installation? Projects and user data are "
                                      "preserved. Close all CyCode instances before continuing.");
        if (messages.box(summary, MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES)
            return 0;
        const auto run = runRecovery(review, messages);
        if (!run.restored) {
            showRecoveryResult(review, run, messages);
            return run.cancelled && run.technical.empty() ? 0 : 1;
        }
        restored = true;
        try {
            recordOutcome(prepared, "recovered");
        } catch (...) {
        }
        if (showRecoveryResult(review, run, messages) == IDYES)
            restartApplication(root);
        return 0;
    } catch (const std::exception &e) {
        messages.box(
            messages.tr(
                restored ? "The previous installation was restored, but CyCode could not restart. Start it "
                           "manually."
                         : "Recovery stopped. Keep pending.json, transaction backups and recovery files.") +
                "\n\n" + e.what(),
            MB_OK | MB_ICONERROR);
        return 1;
    }
}
} // namespace cycode::updates
#endif
