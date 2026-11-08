#pragma once
#include "cycode/Paths.h"
#include <nlohmann/json.hpp>
#include <cyui/Core.h>
#include <cyui/modules/CodeEditor.h>
#include <cyui/modules/Commands.h>
#include <cyui/modules/Diagnostics.h>
#include <cyui/modules/Diff.h>
#include <cyui/modules/DocumentViews.h>
#include <cyui/modules/Inspector.h>
#include <map>
#include <set>

namespace cycode {
cyui::ItemId uiId(const std::string &);
std::string uiTimestamp(uint64_t milliseconds);
void cancelPropertyDrafts(cyui::InspectorState &);
struct ProblemLocation {
    std::string path;
    int line = 1, column = 1;
    bool utf16 = false;
    bool navigable = true;
    std::string provenance;
};
// The application owns navigation permissions and coordinate conversion.
struct ProblemModel {
    std::vector<cyui::Diagnostic> rows;
    std::map<cyui::ItemId, ProblemLocation> locations;
    void add(const std::string &key, cyui::LogLevel, const std::string &source,
             const std::string &message, ProblemLocation);
    const std::vector<cyui::Diagnostic> &matching(const cyui::TextFilter &, unsigned levels = 15) const;
    cyui::ItemId adjacent(const cyui::TextFilter &, cyui::ItemId selected, bool previous,
                         const std::string &path = {}, int line = 0, unsigned levels = 15) const;
  private:
    uint64_t revision_ = 0;
    mutable unsigned filteredLevels_ = 0;
    mutable uint64_t filteredRevision_ = ~uint64_t(0);
    mutable std::string filteredQuery_, filteredLocale_;
    mutable std::vector<cyui::Diagnostic> filteredRows_;
};
class LocationModel {
  public:
    void set(const nlohmann::json &locations);
    const std::string &label(size_t index) const { return labels_.at(index); }
    const std::vector<size_t> &matching(const cyui::TextFilter &) const;
    int adjacent(const cyui::TextFilter &, int selected, bool previous) const;
  private:
    std::vector<std::string> labels_;
    mutable std::vector<size_t> matching_;
    mutable std::string query_,locale_;
    mutable bool cached_=false;
};
class OutlineModel {
  public:
    void set(nlohmann::json symbols);
    const nlohmann::json &at(uint64_t node) const;
    const std::vector<uint64_t> &children(uint64_t parent, bool alphabetic) const;
    const std::set<uint64_t> &visible(const cyui::TextFilter &) const;
    const nlohmann::json &symbols() const { return symbols_; }
  private:
    nlohmann::json symbols_ = nlohmann::json::array();
    std::map<uint64_t, size_t> indices_;
    std::map<uint64_t, std::vector<uint64_t>> source_, alphabetic_;
    mutable std::set<uint64_t> visible_;
    mutable std::string query_, locale_;
    mutable bool cached_ = false;
};
class OutputLog {
  public:
    void synchronize(std::string &);
    cyui::LogBuffer buffer;
    cyui::LogState state;
  private:
    std::string seen_;
};
struct InformationDocument {
    cyui::Document document;
    cyui::DocumentState state;
    cyui::CodeDocument code;
    cyui::CodeViewState codeState;
};
} // namespace cycode
