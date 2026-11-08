#include "cycode/ModuleModels.h"
#include <algorithm>
#include <ctime>
#include <stdexcept>
#include <tuple>

namespace cycode {
std::string uiTimestamp(uint64_t milliseconds) {
    const std::time_t seconds = static_cast<std::time_t>(milliseconds / 1000);
    std::tm value{};
#ifdef _WIN32
    localtime_s(&value, &seconds);
#else
    localtime_r(&seconds, &value);
#endif
    char text[32]{};
    std::strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &value);
    return text;
}
cyui::ItemId uiId(const std::string &key) {
    uint64_t id = 14695981039346656037ull;
    for (unsigned char c : key) {
        id ^= c;
        id *= 1099511628211ull;
    }
    return id ? id : 1;
}
void cancelPropertyDrafts(cyui::InspectorState &state) {
    for (auto &[id, draft] : state.drafts)
        if (draft.active) {
            draft.value = draft.before;
            draft.active = false;
            draft.cancelled = true;
            draft.error.clear();
        }
}
void ProblemModel::add(const std::string &key, cyui::LogLevel level, const std::string &source,
                       const std::string &message, ProblemLocation location) {
    auto id = uiId(key);
    // Duplicate backend reports are separate rows; changing list order does not
    // change the identity of unrelated diagnostics.
    for (size_t duplicate = 1; locations.contains(id); ++duplicate)
        id = uiId(key + "/duplicate/" + std::to_string(duplicate));
    rows.push_back({id, level, source, message, location.line, location.column});
    locations.emplace(id, std::move(location));
    ++revision_;
}
const std::vector<cyui::Diagnostic> &ProblemModel::matching(const cyui::TextFilter &filter,
                                                            unsigned levels) const {
    if (!filter.active() && (levels & 15) == 15)
        return rows;
    if (filteredRevision_ == revision_ && filteredQuery_ == filter.query &&
        filteredLocale_ == cyui::locale() && filteredLevels_ == levels)
        return filteredRows_;
    filteredRows_.clear();
    for (const auto &row : rows)
        if ((levels & (1u << static_cast<unsigned>(row.level))) &&
            filter.matches(row.source, row.source + " " + row.message + " " + std::to_string(row.line) + " " +
                                           std::to_string(row.column)))
            filteredRows_.push_back(row);
    filteredLevels_ = levels;
    filteredRevision_ = revision_;
    filteredQuery_ = filter.query;
    filteredLocale_ = cyui::locale();
    return filteredRows_;
}
cyui::ItemId ProblemModel::adjacent(const cyui::TextFilter &filter, cyui::ItemId selected, bool previous,
                                    const std::string &path, int line, unsigned levels) const {
    std::vector<cyui::ItemId> candidates;
    for (const auto &row : matching(filter, levels)) {
        const auto found = locations.find(row.id);
        if (found != locations.end() && found->second.navigable && !found->second.path.empty() &&
            (path.empty() || found->second.path == path))
            candidates.push_back(row.id);
    }
    if (candidates.empty())
        return 0;
    if (!path.empty())
        std::stable_sort(candidates.begin(), candidates.end(), [&](auto a, auto b) {
            const auto &left = locations.at(a), &right = locations.at(b);
            return std::tie(left.line, left.column, a) < std::tie(right.line, right.column, b);
        });
    const auto at = std::find(candidates.begin(), candidates.end(), selected);
    if (at != candidates.end()) {
        const auto index = static_cast<size_t>(at - candidates.begin());
        return candidates[previous ? (index + candidates.size() - 1) % candidates.size()
                                   : (index + 1) % candidates.size()];
    }
    if (!path.empty() && line > 0) {
        if (previous) {
            for (auto it = candidates.rbegin(); it != candidates.rend(); ++it)
                if (locations.at(*it).line <= line)
                    return *it;
        } else {
            for (auto id : candidates)
                if (locations.at(id).line >= line)
                    return id;
        }
    }
    return previous ? candidates.back() : candidates.front();
}
void LocationModel::set(const nlohmann::json &locations) {
    labels_.clear(); matching_.clear(); cached_=false;
    for (const auto &row:locations) {
        const auto &start=row.at("range").at("start");
        labels_.push_back(row.value("displayPath",row.at("path").get<std::string>())+":"+
            std::to_string(start.at("line").get<uint64_t>()+1)+":"+
            std::to_string(start.at("character").get<uint64_t>()+1));
    }
}
const std::vector<size_t> &LocationModel::matching(const cyui::TextFilter &filter) const {
    const auto locale=std::string(cyui::locale());
    if (cached_ && query_==filter.query && locale_==locale) return matching_;
    query_=filter.query; locale_=locale; cached_=true; matching_.clear();
    for (size_t i=0;i<labels_.size();++i) if (filter.matches(labels_[i])) matching_.push_back(i);
    return matching_;
}
int LocationModel::adjacent(const cyui::TextFilter &filter,int selected,bool previous) const {
    const auto &rows=matching(filter);
    if (rows.empty()) return -1;
    const auto at=selected<0 ? rows.end() : std::find(rows.begin(),rows.end(),size_t(selected));
    if (at==rows.end()) return static_cast<int>(previous ? rows.back() : rows.front());
    const auto index=size_t(at-rows.begin());
    return static_cast<int>(rows[previous ? (index+rows.size()-1)%rows.size() : (index+1)%rows.size()]);
}
void OutlineModel::set(nlohmann::json symbols) {
    symbols_ = std::move(symbols);
    indices_.clear();
    source_.clear();
    alphabetic_.clear();
    cached_ = false;
    std::map<uint64_t, std::string> sortKeys;
    for (size_t i = 0; i < symbols_.size(); ++i) {
        const auto &row = symbols_.at(i);
        const auto id = row.at("node").get<uint64_t>();
        sortKeys[id] = cyui::foldFilterText(row.at("name").get_ref<const std::string &>());
        indices_[id] = i;
        source_[row.at("parent").get<uint64_t>()].push_back(id);
    }
    for (auto &[parent, children] : source_)
        std::stable_sort(children.begin(), children.end(), [&](uint64_t a, uint64_t b) {
            const auto &left = at(a).at("fullRange").at("start"), &right = at(b).at("fullRange").at("start");
            return std::pair{left.at("line").get<int>(), left.at("character").get<int>()} <
                   std::pair{right.at("line").get<int>(), right.at("character").get<int>()};
        });
    alphabetic_ = source_;
    for (auto &[parent, children] : alphabetic_)
        std::stable_sort(children.begin(), children.end(),
                         [&](uint64_t a, uint64_t b) { return sortKeys.at(a) < sortKeys.at(b); });
}
const nlohmann::json &OutlineModel::at(uint64_t node) const {
    return symbols_.at(indices_.at(node));
}
const std::vector<uint64_t> &OutlineModel::children(uint64_t parent, bool alphabetic) const {
    static const std::vector<uint64_t> empty;
    const auto &rows = alphabetic ? alphabetic_ : source_;
    const auto found = rows.find(parent);
    return found == rows.end() ? empty : found->second;
}
const std::set<uint64_t> &OutlineModel::visible(const cyui::TextFilter &filter) const {
    if (cached_ && query_ == filter.query && locale_ == cyui::locale())
        return visible_;
    visible_.clear();
    for (const auto &row : symbols_)
        if (filter.matches(row.at("name").get_ref<const std::string &>(),
                           row.at("name").get<std::string>() + " " + row.at("container").get<std::string>() +
                               " " + row.at("detail").get<std::string>())) {
            auto node = row.at("node").get<uint64_t>();
            for (size_t depth = 0; node && depth <= 64; ++depth) {
                visible_.insert(node);
                node = at(node).at("parent").get<uint64_t>();
            }
        }
    query_ = filter.query;
    locale_ = cyui::locale();
    cached_ = true;
    return visible_;
}
void OutputLog::synchronize(std::string &text) {
    constexpr size_t maximum = 8 * 1024 * 1024;
    if (text.size() > maximum) {
        auto cut = text.find('\n', text.size() - maximum);
        if (cut == std::string::npos) {
            cut = text.size() - maximum;
            while (cut < text.size() && (static_cast<unsigned char>(text[cut]) & 0xc0) == 0x80)
                ++cut;
        } else
            ++cut;
        text.erase(0, cut);
    }
    if (text == seen_)
        return;
    size_t at = 0;
    if (!seen_.empty() && seen_.back() == '\n' && text.starts_with(seen_))
        at = seen_.size();
    else
        buffer.clear();
    while (at < text.size()) {
        const auto end = text.find('\n', at);
        auto line = text.substr(at, end == std::string::npos ? end : end - at);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto level = cyui::LogLevel::info;
        for (const auto &span : cyui::highlightCode(line, cyui::CodeLanguage::log)) {
            if (span.role == cyui::TokenRole::error) {
                level = cyui::LogLevel::error;
                break;
            }
            if (span.role == cyui::TokenRole::warning)
                level = cyui::LogLevel::warning;
            if (span.role == cyui::TokenRole::comment)
                level = cyui::LogLevel::debug;
        }
        buffer.append(level, line);
        if (end == std::string::npos)
            break;
        at = end + 1;
    }
    seen_ = text;
}
} // namespace cycode
