#pragma once
#include <TextEditor.h>
#include <nlohmann/json.hpp>
#include <vector>
namespace cycode {
struct DiagnosticUnderline {
    TextEditor::DocPos start,end;
    ImU32 color;
};
struct DiagnosticAnnotationResult {
    size_t count = 0;
    bool truncated = false;
    std::vector<DiagnosticUnderline> underlines;
};
// Own only this annotation family; search/plugin annotations remain intact.
void clearDiagnosticAnnotations(TextEditor &);
DiagnosticAnnotationResult applyDiagnosticAnnotations(TextEditor &, const nlohmann::json &);
// Paint after the shared editor has laid out its child; never covers the glyphs.
void drawDiagnosticUnderlines(TextEditor &,const std::vector<DiagnosticUnderline> &,ImGuiID childId);
} // namespace cycode
