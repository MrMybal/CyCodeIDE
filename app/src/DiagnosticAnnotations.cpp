#include "cycode/DiagnosticAnnotations.h"
#include "cycode/EditorTextMap.h"
#include <algorithm>
#include <cyui/Core.h>
#include <map>
#include <optional>
#include <string_view>
namespace cycode {
namespace {
using Json = nlohmann::json;
constexpr size_t annotationType = 0x43594449; // CYDI: host language diagnostics
std::optional<size_t> offset(const Json &position, const char *field) {
    if (!position.is_object() || !position.contains(field))
        return {};
    const auto &value = position.at(field);
    if (!value.is_number_integer() || value < 0 || value > 10000000)
        return {};
    return value.get<size_t>();
}
std::string bounded(std::string text) {
    if (text.size() > 4096) {
        size_t cut = 4096;
        while (cut && (static_cast<unsigned char>(text[cut]) & 0xc0) == 0x80)
            --cut;
        text.resize(cut);
        text += "…";
    }
    return text;
}
} // namespace
void clearDiagnosticAnnotations(TextEditor &editor) {
    if (editor.HasSquiggles())
        editor.ClearSquiggles(annotationType);
}
DiagnosticAnnotationResult applyDiagnosticAnnotations(TextEditor &editor, const Json &entries) {
    clearDiagnosticAnnotations(editor);
    DiagnosticAnnotationResult result;
    if (!entries.is_array() || entries.empty())
        return result;
    // Copy only pointers and sort so the highest severity owns overlapping glyphs.
    std::vector<const Json *> ordered;
    for (const auto &entry : entries) {
        if (ordered.size() == 512) {
            result.truncated = true;
            break;
        }
        if (entry.is_object())
            ordered.push_back(&entry);
    }
    auto severity = [](const Json &entry) {
        const auto level = entry.value("severity", Json(1));
        return level.is_number_integer() && level >= 1 && level <= 4 ? level.get<int>() : 1;
    };
    std::stable_sort(ordered.begin(), ordered.end(),
                     [&](auto a, auto b) { return severity(*a) < severity(*b); });
    const auto text = editor.GetText();
    EditorTextMap coordinates(text);
    const auto &lines = coordinates.lines();
    auto glyph = [&](size_t line, size_t units) { return coordinates.glyph(line, units); };
    struct Annotation {
        TextEditor::DocPos start, end;
        ImU32 color;
        std::string tooltip;
    };
    std::vector<Annotation> annotations;
    size_t segments = 0, budget = 0;
    for (const auto *entry : ordered)
        try {
            const auto &range = entry->at("range");
            const auto &start = range.at("start"), &end = range.at("end");
            const auto a = offset(start, "line"), b = offset(start, "character"), c = offset(end, "line"),
                       d = offset(end, "character");
            if (!a || !b || !c || !d || *a >= lines.size() || *c >= lines.size() || *a > *c ||
                (*a == *c && *b > *d))
                continue;
            const auto from = glyph(*a, *b), to = glyph(*c, *d);
            if (!from || !to)
                continue;
            std::string tooltip = entry->value("message", std::string{});
            if (entry->contains("source") && entry->at("source").is_string())
                tooltip = entry->at("source").get<std::string>() + ": " + tooltip;
            tooltip = bounded(std::move(tooltip));
            const auto level = severity(*entry);
            const auto &palette = cyui::palette();
            const auto color = level == 1   ? palette.error
                               : level == 2 ? palette.warning
                               : level == 3 ? palette.info
                                            : palette.textDim;
            bool applied = false;
            const auto last = std::min(*c, *a + 63);
            if (last != *c)
                result.truncated = true;
            for (size_t line = *a; line <= last; ++line) {
                if (segments >= 2048 || budget >= 131072) {
                    result.truncated = true;
                    break;
                }
                // Populate the map for intermediate lines using its zero boundary.
                (void)glyph(line, 0);
                const auto length = coordinates.glyphCount(line);
                size_t lo = line == *a ? *from : 0, hi = line == *c ? *to : length;
                if (*a == *c && lo == hi && length) {
                    if (lo == length)
                        --lo;
                    hi = lo + 1;
                }
                if (hi <= lo)
                    continue;
                const auto count = std::min({hi - lo, size_t(4096), size_t(131072 - budget)});
                if (count < hi - lo)
                    result.truncated = true;
                annotations.push_back({{line, lo}, {line, lo + count}, color, tooltip});
                ++segments;
                budget += count;
                applied = true;
            }
            result.count += applied;
        } catch (const std::exception &) { /* Malformed provider entries stay out of the editor. */
        }
    // Bound work in severity order, then paint in reverse order so errors retain
    // their tooltip/colour when a provider reports overlapping hints.
    for (auto it = annotations.rbegin(); it != annotations.rend(); ++it)
        editor.AddSquiggle(it->start, it->end, annotationType, it->color, it->tooltip);
    return result;
}
} // namespace cycode
