#pragma once
#include <cyui/modules/Diff.h>
#include <string>
#include <utility>

namespace cycode {
// Retain source input so a language/catalogue change can refresh syntax once,
// without replacing the separate DiffState (selection, scroll, search and zoom).
class CodeComparison {
  public:
    void setTexts(std::string before, std::string after, std::string path, cyui::CodeLanguage language) {
        before_ = std::move(before); after_ = std::move(after); path_ = std::move(path);
        unified_ = false; prepared_ = false; refresh(language);
    }
    void setUnified(std::string patch, std::string path, cyui::CodeLanguage language) {
        before_ = std::move(patch); after_.clear(); path_ = std::move(path);
        unified_ = true; prepared_ = false; refresh(language);
    }
    void refresh(cyui::CodeLanguage language) {
        const auto revision = cyui::codeLanguageRevision();
        if (prepared_ && language_ == language && catalogue_ == revision) return;
        if (unified_) document_.setUnified(before_, language);
        else document_.setTexts(before_, after_, language);
        language_ = language; catalogue_ = revision; prepared_ = true;
    }
    const std::string &path() const { return path_; }
    const cyui::DiffDocument &document() const { return document_; }
    const std::vector<int> &hunks() const { return document_.hunks(); }
  private:
    cyui::DiffDocument document_;
    std::string before_, after_, path_;
    cyui::CodeLanguage language_ = cyui::CodeLanguage::plain;
    std::uint64_t catalogue_ = 0;
    bool unified_ = false, prepared_ = false;
};
} // namespace cycode
