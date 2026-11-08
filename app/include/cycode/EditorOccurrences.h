#pragma once
#include <TextEditor.h>
#include <map>
#include <nlohmann/json.hpp>
#include <vector>
namespace cycode {
struct OccurrenceSpan {
    size_t first, last;
    unsigned kind;
    std::vector<uint8_t> widths;
};
struct OccurrenceAnnotations {
    std::map<size_t, std::vector<OccurrenceSpan>> lines;
    size_t count = 0;
    bool truncated = false;
};
// Geometry model owned by the host, independent of the backend's single glyph annotation.
OccurrenceAnnotations occurrenceAnnotations(TextEditor &, const nlohmann::json &, unsigned kinds);
// Call after CodeEditor::draw, in its parent window; no source/annotation mutation.
void drawOccurrenceAnnotations(TextEditor &, const OccurrenceAnnotations &, ImGuiID childId);
// Converts every range before one atomic SetSelections; source and Undo stay untouched.
void selectOccurrenceRanges(TextEditor &, const nlohmann::json &ranges);
} // namespace cycode
