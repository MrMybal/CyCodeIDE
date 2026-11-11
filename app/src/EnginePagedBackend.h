#pragma once
#include <cycode/Engine.h>
#include <cycode/PagedTextBackend.h>
#include <memory>
#include <map>

namespace cycode {
// Translation of graphical intents only. Engine owns all native document bytes,
// revisions, publication, transactions and history. Called on the Reader worker.
class EnginePagedBackend final : public PagedTextBackend {
  public:
    EnginePagedBackend(std::shared_ptr<Engine>, cyui::ItemId, const std::string &);
    PagedTextStatus state(cyui::ItemId) override;
    uint64_t offset(cyui::ItemId, uint64_t, cyui::PagedCodePosition) override;
    PagedTextBytes readBytes(cyui::ItemId, uint64_t, uint64_t, size_t) override;
    std::vector<cyui::CodeToken> syntax(cyui::ItemId, uint64_t, cyui::CodeLanguage, uint64_t, size_t) override;
    PagedTextPreparedEdit prepare(const cyui::PagedCodeReplacement &) override;
    PagedTextCommit apply(const PagedTextPreparedEdit &) override;
    void discard(const PagedTextPreparedEdit &) noexcept override;
    PagedTextCommit undo(cyui::ItemId, uint64_t, bool) override;
    PagedTextStatus setReadOnly(cyui::ItemId, uint64_t, bool) override;
    std::string copy(cyui::ItemId, uint64_t, cyui::PagedCodeSelection, size_t) override;
    PagedTextSearch search(cyui::ItemId, uint64_t, const std::string &, bool, bool, size_t, const Cancelled &) override;
    PagedTextStatus save(cyui::ItemId, uint64_t, const std::filesystem::path &) override;
    PagedTextStatus checkpoint(cyui::ItemId, uint64_t) override;
  private:
    Json parameters(cyui::ItemId, uint64_t) const;
    PagedTextStatus describe(const Json &, bool withLines=true);
    Json history();
    PagedTextStatus committed(const Json &,std::optional<uint64_t> lines={});
    std::shared_ptr<Engine> engine_;
    cyui::ItemId identity_;
    std::string path_;
    PagedTextStatus current_;
    bool readOnly_=false;
    uint64_t syntaxRevision_=0, syntaxCatalogue_=0;
    cyui::CodeLanguage syntaxLanguage_=cyui::CodeLanguage::plain;
    std::vector<cyui::CodeToken> tokens_;
    struct Prepared {
        uint64_t revision,beforeLines,afterLines;
        cyui::PagedCodeSelection previous,selection;
    };
    std::map<std::string, Prepared> prepared_;
    // Optional caret/line presentation only; Core retains the complete history.
    std::map<std::string, Prepared> cursors_;
};
}
