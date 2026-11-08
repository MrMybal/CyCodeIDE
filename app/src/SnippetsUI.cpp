#include "cycode/Application.h"
#include <cyui/modules/Commands.h>
#include <algorithm>

namespace cycode {
void Application::beginSnippets() {
    if (!commandEnabled("edit.snippet")) return;
    for (const auto &tab : tabs_) if (tab->path == active_) {
        tab->snippet.cancel();tab->queuedCompletion=nullptr;
        tab->completionSession.clear();tab->completionItems.clear();tab->editor.SetAutoCompleteSuggestions({});
        ++tab->completionGeneration;
        snippetTab_=tab;snippetRevision_=tab->revision;snippetGeneration_=tab->completionGeneration;
        snippetPosition_=tab->editor.GetMainCursorPosition();
        snippetSelection_=tab->editor.GetMainCursorSelection();
        const auto prefix=tab->editor.GetSectionText({snippetPosition_.line,0},snippetPosition_);
        Json params={{"path",tab->path},{"expectedRevision",tab->revision},
            {"position",{{"line",snippetPosition_.line},{"character",LanguageClient::utf16Length(prefix)}}}};
        if (snippetSelection_.start!=snippetSelection_.end) {
            auto range=snippetSelection_;
            if (range.end<range.start)std::swap(range.start,range.end);
            const auto point=[&](TextEditor::DocPos at) {
                return Json{{"line",at.line},{"character",LanguageClient::utf16Length(tab->editor.GetSectionText({at.line,0},at))}};
            };
            params["selection"]={{"start",point(range.start)},{"end",point(range.end)}};
        }
        snippetCatalogue_=Json::object();snippetError_.clear();snippetPaletteState_={};snippetOpen_=true;
        auto engine=engine_;
        snippetFuture_=std::async(std::launch::async,[engine,params] { return engine->dispatch("snippets.complete",params); });
        break;
    }
}
bool Application::snippetsPalette() {
    if (snippetFuture_.valid() && snippetFuture_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try { snippetCatalogue_=snippetFuture_.get(); }
        catch (const std::exception &exception) { snippetError_=exception.what(); }
    }
    auto tab=snippetTab_.lock();
    const auto current=[&] {
        const auto selection=tab ? tab->editor.GetMainCursorSelection() : TextEditor::DocSelection{};
        const auto registry=snippetCatalogue_.find("result");
        return tab && tab->path==active_ && !tab->readOnly && tab->revision==snippetRevision_ &&
            (registry==snippetCatalogue_.end() || !registry->contains("cycodePluginRevision") || registry->at("cycodePluginRevision")==engine_->plugins().revision()) &&
            (registry==snippetCatalogue_.end() || !registry->contains("cycodeUserGeneration") || registry->at("cycodeUserGeneration")==engine_->userSnippetGeneration()) &&
            tab->completionGeneration==snippetGeneration_ && tab->editor.GetMainCursorPosition()==snippetPosition_ &&
            tab->editor.GetNumberOfCursors()==1 && selection.start==snippetSelection_.start && selection.end==snippetSelection_.end;
    };
    if (snippetOpen_ && !current()) {
        snippetOpen_=false;snippetCatalogue_=Json::object();error(tr("Snippet context changed. Request snippets again."));
    }
    std::vector<cyui::Command> entries;
    static const Json emptyItems=Json::array();
    const auto result=snippetCatalogue_.find("result");
    const auto &items=result!=snippetCatalogue_.end() ? result->at("items") : emptyItems;
    if (snippetOpen_) {
        for (size_t i=0;i<items.size();++i) {
            const auto available=items[i].value("cycodeSnippetAvailable",true);
            const auto reason=items[i].value("cycodeSnippetReason",std::string{});
            const auto name=items[i].at("label").get<std::string>();
            // Commands renders labels but uses categories only for ranking.
            // Keep disabled reasons visible without losing description searches.
            const auto label=available ? name : std::string(tr(reason.c_str()))+" — "+name;
            entries.push_back({uiId("snippet/"+std::to_string(i)),label,"",
                items[i].value("detail",std::string{}),available});
        }
        if (entries.empty()) entries.push_back({uiId("snippet/status"),
            snippetFuture_.valid() ? tr("Loading snippets...") : !snippetError_.empty() ? snippetError_ :
            tr("No matching snippets. Save project definitions or enable a snippet extension."),"","",false});
        if (snippetCatalogue_.value("truncated",false) || (result!=snippetCatalogue_.end() && result->value("isIncomplete",false)))
            entries.push_back({uiId("snippet/limit"),tr("Snippet catalogue limit reached; narrow the language scopes."),"","",false});
    }
    const auto title=snippetSelection_.start==snippetSelection_.end ? "Insert snippet..." : "Surround with snippet...";
    const auto selected=cyui::commandPalette(title,snippetOpen_,snippetPaletteState_,entries,cyui::dp(850,570));
    if (selected && current()) for (size_t i=0;i<items.size();++i) if (selected==uiId("snippet/"+std::to_string(i))) {
        if (tab->completionAccept.valid() || !items[i].value("cycodeSnippetAvailable",true)) break;
        tab->acceptedRevision=tab->revision;tab->acceptedGeneration=tab->completionGeneration;tab->acceptedPosition=snippetPosition_;
        tab->acceptedSelection=snippetSelection_;
        tab->acceptedSnippetPluginRevision=snippetCatalogue_.at("result").at("cycodePluginRevision").get<uint64_t>();
        tab->acceptedUserSnippetGeneration=snippetCatalogue_.at("result").at("cycodeUserGeneration").get<uint64_t>();
        const Json params={{"sessionId",snippetCatalogue_.at("sessionId")},{"item",i},{"expectedRevision",tab->revision}};
        auto engine=engine_;
        tab->completionAccept=std::async(std::launch::async,[engine,params] { return engine->dispatch("language.completionPrepare",params); });
        focusTab_=tab->path;tab->editor.SetFocus();
        break;
    }
    return snippetOpen_;
}
} // namespace cycode
