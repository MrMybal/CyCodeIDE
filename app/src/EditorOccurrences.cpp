#include "cycode/EditorOccurrences.h"
#include "cycode/EditorTextMap.h"
#include <algorithm>
#include <cyui/Core.h>
#include <imgui_internal.h>
#include <limits>
#include <stdexcept>

namespace cycode {
OccurrenceAnnotations occurrenceAnnotations(TextEditor &editor, const nlohmann::json &rows, unsigned kinds) {
    OccurrenceAnnotations result;
    const auto text = editor.GetText();
    EditorTextMap coordinates(text);
    std::map<size_t, std::vector<uint8_t>> lineWidths;
    auto widthsFor = [&](size_t line) -> const std::vector<uint8_t> & {
        auto [it, fresh] = lineWidths.try_emplace(line);
        if (fresh) {
            const auto source = coordinates.lines().at(line);
            size_t columns = 0;
            for (auto at = source.begin(); at != source.end();) {
                ImWchar cp = 0;
                at = TextEditor::CodePoint::read(at, source.end(), &cp);
                const auto width = cp == '\t' ? editor.GetTabSize() - (columns % editor.GetTabSize())
                                              : TextEditor::CodePoint::getGlyphWidth(cp);
                it->second.push_back(static_cast<uint8_t>(width));
                columns += width;
            }
        }
        return it->second;
    };
    size_t segments = 0, budget = 0, visited = 0;
    for (const auto &row : rows) {
        const auto kind = row.at("kind").get<unsigned>();
        if (kind < 1 || kind > 3 || !(kinds & (1u << (kind - 1))))
            continue;
        if (++visited > 512) {
            result.truncated = true;
            break;
        }
        const auto &range = row.at("range");
        const auto from = coordinates.position(range.at("start")), to = coordinates.position(range.at("end"));
        if (!from || !to || *to < *from)
            continue;
        const auto last = std::min(to->line, from->line + 63);
        result.truncated |= last != to->line;
        bool applied = false;
        for (size_t line = from->line; line <= last; ++line) {
            if (segments == 2048 || budget == 65536) {
                result.truncated = true;
                break;
            }
            const auto lo = line == from->line ? from->index : 0;
            const auto hi = line == to->line ? to->index : coordinates.glyphCount(line);
            if (hi <= lo)
                continue;
            const auto count = std::min({hi - lo, size_t(4096), size_t(65536 - budget)});
            result.truncated |= count != hi - lo;
            const auto &widths = widthsFor(line);
            if (lo + count > widths.size()) {
                result.truncated = true;
                continue;
            }
            result.lines[line].push_back(
                {lo, lo + count, kind, {widths.begin() + lo, widths.begin() + lo + count}});
            ++segments;
            budget += count;
            applied = true;
        }
        result.count += applied;
    }
    return result;
}
void drawOccurrenceAnnotations(TextEditor &editor, const OccurrenceAnnotations &model, ImGuiID childId) {
    if (model.lines.empty())
        return;
    ImGuiWindow *child = nullptr;
    for (auto *candidate : ImGui::GetCurrentWindow()->DC.ChildWindows)
        if (candidate->ChildId == childId) {
            child = candidate;
            break;
        }
    if (!child || child->Hidden || !child->Active || child->SkipItems ||
        child->InnerClipRect.GetWidth() <= 0 || child->InnerClipRect.GetHeight() <= 0)
        return;
    auto *draw = child->DrawList;
    const auto first = editor.GetFirstVisibleRow(), last = editor.GetLastVisibleRow();
    // Work only in visible visual rows; use the backend's layout for tabs, wide glyphs,
    // wrapping, folded lines and scrolling. Never infer display width from UTF-8 bytes.
    for (size_t row = first; row <= last; ++row) {
        const auto start = editor.VisPos2DocPos({row, 0});
        const auto found = model.lines.find(start.line);
        if (found == model.lines.end() || !editor.IsLineVisible(start.line))
            continue;
        const auto origin = editor.DocPos2ScreenPos(start);
        const auto originVis = editor.DocPos2VisPos(start);
        if (originVis.row != row)
            continue;
        const auto width = editor.GetGlyphWidth();
        const float base = origin.x - originVis.column * width;
        auto minimum = child->InnerClipRect.Min;
        minimum.x = std::max(minimum.x, base + child->Scroll.x);
        if (minimum.x >= child->InnerClipRect.Max.x)
            continue;
        draw->PushClipRect(minimum, child->InnerClipRect.Max, true);
        for (const auto &span : found->second) {
            const auto a = editor.DocPos2VisPos({start.line, span.first}),
                       b = editor.DocPos2VisPos({start.line, span.last});
            if (row < a.row || row > b.row)
                continue;
            const auto lo = row == a.row ? a.column : originVis.column;
            size_t hi = b.column;
            if (row != b.row) {
                const auto end = editor.VisPos2DocPos({row, std::numeric_limits<size_t>::max()});
                if (end.line != start.line || end.index <= span.first || end.index > span.last)
                    continue;
                const auto previous = editor.DocPos2VisPos({start.line, end.index - 1});
                if (previous.row != row)
                    continue;
                hi = previous.column + span.widths.at(end.index - 1 - span.first);
            }
            if (hi <= lo)
                continue;
            const auto &p = cyui::palette();
            const auto role = span.kind == 3 ? p.error : span.kind == 2 ? p.info : p.accent;
            auto color = ImGui::ColorConvertU32ToFloat4(role);
            color.w *= .65f;
            draw->AddRect({base + lo * width, origin.y + cyui::dp(.5f)},
                          {base + hi * width, origin.y + editor.GetLineHeight() - cyui::dp(.5f)},
                          ImGui::ColorConvertFloat4ToU32(color), cyui::dp(1), 0, cyui::dp(1));
        }
        draw->PopClipRect();
    }
}
void selectOccurrenceRanges(TextEditor &editor, const nlohmann::json &ranges) {
    if (!ranges.is_array() || ranges.empty() || ranges.size() > 128)
        throw std::runtime_error("Editor selection limit: 128 occurrences");
    const auto text = editor.GetText();
    EditorTextMap coordinates(text);
    std::vector<TextEditor::DocSelection> selections;
    for (const auto &range : ranges) {
        const auto start = coordinates.position(range.at("start")),
                   end = coordinates.position(range.at("end"));
        if (!start || !end || !(*start < *end) || (!selections.empty() && *start < selections.back().end))
            throw std::runtime_error("Occurrence selection requires nonempty, disjoint ranges");
        selections.push_back({*start, *end});
    }
    editor.SetSelections(selections);
}
} // namespace cycode
