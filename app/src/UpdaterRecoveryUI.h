#pragma once
#include "cycode/update/detail/UpdateInstall.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace cycode::updates {
std::optional<int> recoveryEntry(const std::vector<std::wstring> &);
void restartApplication(const std::filesystem::path &);
void showInstallationFailure(const std::filesystem::path &root, const std::filesystem::path &staging,
                             InstallFailure, const std::string &details, bool installed = false);
} // namespace cycode::updates
