#pragma once
#include <TextEditor.h>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>
#include <vector>

namespace cycode {
// Borrows a stable LF/UTF-8 snapshot; decoded columns are shared by annotation owners.
class EditorTextMap {
  public:
    explicit EditorTextMap(std::string_view);
    const std::vector<std::string_view> &lines() const {
        return lines_;
    }
    std::optional<size_t> glyph(size_t line, size_t utf16);
    size_t glyphCount(size_t line);
    std::optional<TextEditor::DocPos> position(const nlohmann::json &);

  private:
    std::vector<std::string_view> lines_;
    std::map<size_t, std::vector<size_t>> columns_;
};
} // namespace cycode
