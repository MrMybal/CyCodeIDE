#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>
namespace cycode {
// Call once, with this application's ImGui context current, before its first frame.
nlohmann::json loadApplicationFonts(const std::filesystem::path &installation);
bool applicationIconAvailable(const char *glyph);
}
