#pragma once
#include <TextEditor.h>
#include <nlohmann/json.hpp>
namespace cycode {
struct DiagnosticAnnotationResult {
    size_t count = 0;
    bool truncated = false;
};
// Own only this annotation family; search/plugin annotations remain intact.
void clearDiagnosticAnnotations(TextEditor &);
DiagnosticAnnotationResult applyDiagnosticAnnotations(TextEditor &, const nlohmann::json &);
} // namespace cycode
