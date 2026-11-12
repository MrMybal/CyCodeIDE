#include "cycode/DiagnosticAnnotations.h"
#include "cycode/EditorTextMap.h"
#include <algorithm>
#include <cyui/Core.h>
#include <map>
#include <optional>
#include <string_view>
#include <imgui_internal.h>
#include <limits>
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
    for (auto it = annotations.rbegin(); it != annotations.rend(); ++it) {
        // Keep the native glyph anchors and tooltip; the host paints a thin line
        // below the text instead of the widget's large wave through descenders.
        editor.AddSquiggle(it->start,it->end,annotationType,cyui::alpha(it->color,0.f),it->tooltip,TextEditor::SquiggleStyle::background);
        result.underlines.push_back({it->start,it->end,cyui::alpha(it->color,.7f)});
    }
    return result;
}

void drawDiagnosticUnderlines(TextEditor &editor,const std::vector<DiagnosticUnderline> &spans,ImGuiID childId) {
    if(spans.empty())return;
    ImGuiWindow *child=nullptr;
    for(auto candidate:ImGui::GetCurrentWindow()->DC.ChildWindows)if(candidate->ChildId==childId){child=candidate;break;}
    if(!child || child->Hidden || !child->Active || child->SkipItems)return;
    const float unit=cyui::dp(1),width=editor.GetGlyphWidth();
    auto draw=child->DrawList;
    for(size_t row=editor.GetFirstVisibleRow();row<=editor.GetLastVisibleRow();++row) {
        const auto start=editor.VisPos2DocPos({row,0});
        if(!editor.IsLineVisible(start.line))continue;
        const auto origin=editor.DocPos2ScreenPos(start);
        const auto visual=editor.DocPos2VisPos(start);
        if(visual.row!=row)continue;
        const float base=origin.x-visual.column*width;
        auto minimum=child->InnerClipRect.Min;minimum.x=std::max(minimum.x,base+child->Scroll.x);
        if(minimum.x>=child->InnerClipRect.Max.x)continue;
        draw->PushClipRect(minimum,child->InnerClipRect.Max,true);
        for(const auto &span:spans) {
            if(span.start.line!=start.line)continue;
            const auto a=editor.DocPos2VisPos(span.start),b=editor.DocPos2VisPos(span.end);
            if(row<a.row || row>b.row)continue;
            const auto lo=row==a.row?a.column:visual.column;
            size_t hi=b.column;
            if(row!=b.row) {
                const auto end=editor.VisPos2DocPos({row,std::numeric_limits<size_t>::max()});
                if(end.line!=start.line || !end.index)continue;
                const auto previous=editor.DocPos2VisPos({start.line,end.index-1});
                if(previous.row!=row)continue;
                const auto text=editor.GetSectionText({start.line,end.index-1},end);
                const std::string_view glyph(text);
                ImWchar cp=0;TextEditor::CodePoint::read(glyph.begin(),glyph.end(),&cp);
                hi=previous.column+(cp=='\t'?editor.GetTabSize()-previous.column%editor.GetTabSize():TextEditor::CodePoint::getGlyphWidth(cp));
            }
            if(hi<=lo)continue;
            const float left=std::max(base+lo*width,minimum.x),right=std::min(base+hi*width,child->InnerClipRect.Max.x);
            const float y=origin.y+editor.GetLineHeight()-unit*1.5f;
            for(float x=left;x<right;x+=unit*4)draw->AddLine({x,y},{std::min(x+unit*2,right),y},span.color,unit*.8f);
        }
        draw->PopClipRect();
    }
}
} // namespace cycode
