#include "cycode/EditorTextMap.h"
#include <algorithm>

namespace cycode {
EditorTextMap::EditorTextMap(std::string_view text) {
    size_t first = 0;
    for (size_t at = 0; at <= text.size(); ++at)
        if (at == text.size() || text[at] == '\n') {
            lines_.push_back(text.substr(first, at - first));
            first = at + 1;
        }
}
std::optional<size_t> EditorTextMap::glyph(size_t line, size_t units) {
    if (line >= lines_.size())
        return {};
    auto [it, fresh] = columns_.try_emplace(line);
    if (fresh) {
        auto &positions = it->second;
        positions.push_back(0);
        size_t count = 0;
        for (size_t byte = 0; byte < lines_[line].size();) {
            const auto c = static_cast<unsigned char>(lines_[line][byte]);
            const size_t length = c < 0x80                 ? 1
                                  : c >= 0xc2 && c < 0xe0  ? 2
                                  : c < 0xf0 && c >= 0xe0  ? 3
                                  : c >= 0xf0 && c <= 0xf4 ? 4
                                                           : 0;
            if (!length || byte + length > lines_[line].size()) {
                positions.clear();
                return {};
            }
            for (size_t n = 1; n < length; ++n)
                if ((static_cast<unsigned char>(lines_[line][byte + n]) & 0xc0) != 0x80) {
                    positions.clear();
                    return {};
                }
            byte += length;
            count += length == 4 ? 2 : 1;
            positions.push_back(count);
        }
    }
    const auto &positions = it->second;
    const auto found = std::lower_bound(positions.begin(), positions.end(), units);
    if (found == positions.end() || *found != units)
        return {};
    return size_t(found - positions.begin());
}
size_t EditorTextMap::glyphCount(size_t line) {
    if (!glyph(line, 0))
        return 0;
    return columns_.at(line).size() - 1;
}
std::optional<TextEditor::DocPos> EditorTextMap::position(const nlohmann::json &value) {
    if (!value.is_object() || !value.contains("line") || !value.contains("character"))
        return {};
    const auto &line = value.at("line"), &column = value.at("character");
    if (!line.is_number_integer() || line < 0 || line > 10000000 || !column.is_number_integer() ||
        column < 0 || column > 10000000)
        return {};
    const auto index = glyph(line.get<size_t>(), column.get<size_t>());
    if (!index)
        return {};
    return TextEditor::DocPos{line.get<size_t>(), *index};
}
} // namespace cycode
