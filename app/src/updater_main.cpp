#include "cycode/update/detail/UpdateFiles.h"
#include "UpdaterRecoveryUI.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include "cycode/Updates.h"
#include <chrono>
#include <fstream>
#include <set>
#include <thread>
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    using namespace cycode;
    using namespace cycode::updates;
    std::filesystem::path package, installation;
    std::string expected, digest;
    bool trustedPackage = false;
    bool installed = false;
    std::optional<Prepared> prepared;
    auto report = [&](const char *state, InstallFailure failure = InstallFailure::none) {
        try {
            if (prepared)
                recordOutcome(*prepared, state, detail::installFailureCode(failure));
        } catch (...) {
            // Outcome reporting must not turn successful installation into failure.
        }
    };
    try {
        int count = 0;
        auto raw = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!raw)
            throw std::runtime_error("Cannot read update arguments");
        std::vector<std::wstring> arguments;
        for (int i = 1; i < count; ++i)
            arguments.emplace_back(raw[i]);
        LocalFree(raw);
        if (const auto recovery = recoveryEntry(arguments))
            return *recovery;
        std::set<std::wstring> seen;
        for (size_t i = 0; i < arguments.size(); i += 2) {
            if (i + 1 == arguments.size() || !seen.insert(arguments[i]).second)
                throw std::runtime_error("Invalid update arguments");
            const auto &key = arguments[i], &value = arguments[i + 1];
            if (key == L"--package")
                package = value;
            else if (key == L"--installation")
                installation = value;
            else if (key == L"--sha256")
                digest = pathUtf8(std::filesystem::path(value));
            else if (key == L"--version")
                expected = pathUtf8(std::filesystem::path(value));
            else
                throw std::runtime_error("Unknown update argument");
        }
        if (!package.is_absolute() || !installation.is_absolute() || !detail::digest(digest))
            throw std::runtime_error("Invalid update arguments");
        (void)version(expected);
        detail::safe(installation);
        detail::safe(package);
        const auto relative = package.lexically_relative(installation / "data/updates");
        if (relative.empty() || std::distance(relative.begin(), relative.end()) != 2 ||
            !pathUtf8(*relative.begin()).starts_with("download-") ||
            !packageArchiveName(pathUtf8(package.filename()), expected, platform(), installedInstallation(installation)))
            throw std::runtime_error("Update package is outside the application staging area");
        trustedPackage = true;
        if (!portableInstallation(installation))
            throw std::runtime_error("Cannot update a development or unsupported installation");
        prepared =
            Prepared{package, package.parent_path() / "CyCodeUpdater.exe", installation, expected, digest};
        const std::atomic<bool> cancel{false};
        verifyPrepared(*prepared, cancel);
        auto cache = detail::cacheLease(installation, NativeFileLeaseMode::shared);
        report("waiting");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
        std::unique_ptr<NativeFileLease> lease;
        do {
            auto next = std::make_unique<NativeFileLease>(installation / "data/updates/instances.lock");
            if (next->acquired()) {
                lease = std::move(next);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        } while (std::chrono::steady_clock::now() < deadline);
        if (!lease)
            throw std::runtime_error("Close all CyCode instances using this installation, then retry the "
                                     "update. No application was forcibly closed.");
        if (sha256File(package) != digest)
            throw std::runtime_error("Update package SHA-256 verification failed");
        if (installedInstallation(installation)) {
            verifyPrepared(*prepared, cancel);
            // Setup takes the same ownership file before writing. If another
            // instance starts in this handoff gap, Setup asks the user to close it.
            lease.reset();
            std::wstring command = L"\"" + package.wstring() + L"\" /SP- /NORESTART /NOCLOSEAPPLICATIONS /DIR=\"" + installation.wstring() + L"\"";
            STARTUPINFOW startup{}; startup.cb = sizeof(startup);
            PROCESS_INFORMATION process{};
            if (!CreateProcessW(package.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                                package.parent_path().c_str(), &startup, &process))
                throw std::runtime_error("Cannot start the verified Windows installer");
            CloseHandle(process.hThread);
            const auto wait = WaitForSingleObject(process.hProcess, INFINITE);
            DWORD exitCode = 1;
            const auto gotExit = GetExitCodeProcess(process.hProcess, &exitCode);
            CloseHandle(process.hProcess);
            if (wait != WAIT_OBJECT_0 || !gotExit || exitCode != 0)
                throw std::runtime_error("Windows installation was cancelled or failed");
            const auto result = nlohmann::json::parse(detail::read(installation / "manifest.json", 2 * 1024 * 1024));
            if (result.at("version") != expected)
                throw std::runtime_error("Windows installation did not produce the expected version");
        } else apply(package, installation, expected);
        installed = true;
        report("installed");
        lease.reset();
        restartApplication(installation);
        return 0;
    } catch (const std::exception &error) {
        const auto *installationError = dynamic_cast<const InstallationError *>(&error);
        const auto failure =
            !installed && installationError ? installationError->failure() : InstallFailure::none;
        report(installed ? "restart-failed" : "failed", failure);
        // Never write through an untrusted command-line path in the failure path.
        try {
            if (trustedPackage) {
                const auto log = package.parent_path() / "update-error.txt";
                detail::safe(log);
                const auto baseline = std::filesystem::exists(log)
                                          ? std::optional<std::string>(detail::read(log, 16384))
                                          : std::nullopt;
                writeTextFileAtomic(log, std::string(error.what()).substr(0, 16384), baseline);
            }
        } catch (...) {
        }
        try {
            if (trustedPackage)
                showInstallationFailure(installation, package.parent_path(), failure, error.what(),
                                        installed);
            else {
                const auto message = utf8Path(error.what()).wstring();
                MessageBoxW(nullptr, message.c_str(), L"CyCode update", MB_OK | MB_ICONERROR);
            }
        } catch (...) {
            MessageBoxW(nullptr, L"Update stopped. Keep data/updates and all backups.", L"CyCode update",
                        MB_OK | MB_ICONERROR);
        }
        return 1;
    }
}
#endif
