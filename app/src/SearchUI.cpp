#include "cycode/Application.h"
#include "cycode/EditorTheme.h"
#include "cycode/text/Search.h"
#include "cycode/TextFile.h"
#include <cyui/Core.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <map>

namespace cycode {
namespace {
// Display coordinates only. Matching and source document ownership remain Core's.
TextEditor::DocPos displayPosition(std::string_view text, size_t offset) {
    offset=std::min(offset,text.size());size_t line=0,start=0;
    for(size_t i=0;i<offset;++i)if(text[i]=='\n'){++line;start=i+1;}
    size_t glyph=0;for(size_t i=start;i<offset;++i)if((static_cast<unsigned char>(text[i])&0xc0)!=0x80)++glyph;
    return {line,glyph};
}
size_t bytePosition(std::string_view text,TextEditor::DocPos pos) {
    size_t at=0;
    for(size_t line=0;line<pos.line && at<text.size();++line){const auto n=text.find('\n',at);at=n==std::string::npos?text.size():n+1;}
    for(size_t i=0;i<pos.index && at<text.size() && text[at]!='\n';++i){++at;while(at<text.size() && (static_cast<unsigned char>(text[at])&0xc0)==0x80)++at;}
    return at;
}
std::vector<TextEditor::DocSelection> displayMatches(std::string_view source,const text::TextMatches &matches) {
    std::vector<TextEditor::DocSelection> ranges;size_t at=0,line=0,glyph=0;
    auto advance=[&](size_t target) {
        while(at<std::min(target,source.size())) {
            const auto c=static_cast<unsigned char>(source[at++]);
            if(c=='\n'){++line;glyph=0;}else if((c&0xc0)!=0x80)++glyph;
        }
        return TextEditor::DocPos{line,glyph};
    };
    for(const auto &m:matches.matches){const auto begin=advance(m.offset);ranges.push_back({begin,advance(m.offset+m.length)});}
    return ranges;
}
}

void Application::openDocumentFind(const std::shared_ptr<Tab> &tab,bool replace) {
    if(!tab || tab->image)return;
    if(tab->paged){tab->pagedFind=true;return;}
    const auto selected=tab->editor.GetCursorText(0);
    if(!selected.empty() && selected.size()<=4096 && selected.find('\n')==std::string::npos)tab->findQuery=selected;
    tab->findOpen=tab->findFocus=true;tab->findReplace=replace;tab->findMessage.clear();
}

void Application::findInDocument(const std::shared_ptr<Tab> &tab,bool previous,bool replace,bool all) {
    if(!tab || tab->image)return;
    if(tab->paged){if(replace)tab->paged->replace(tab->pagedReplacement);else tab->paged->find(previous);return;}
    try {
        const auto query=text::expandedSearchText(tab->findQuery,tab->findExtended);
        if(query.empty() || query.size()>4096)return;
        const auto source=tab->editor.GetText();
        const auto matches=text::findTextMatches(source,query,tab->findSensitive,tab->findWords,2000);
        if(matches.matches.empty()){tab->findMessage="No matches";return;}
        const auto selection=tab->editor.GetMainCursorSelection();
        const auto start=bytePosition(source,selection.start),end=bytePosition(source,selection.end);
        if(replace) {
            if(tab->readOnly)return;
            const auto replacement=normalizeEditorText(text::expandedSearchText(tab->findReplacement,tab->findExtended));
            if(all) {
                if(matches.truncated){tab->findMessage="Too many replacements. Narrow the search.";return;}
                auto next=source;
                for(auto i=matches.matches.rbegin();i!=matches.matches.rend();++i)next.replace(i->offset,i->length,replacement);
                tab->editor.SelectAll();tab->editor.ReplaceTextPreservingTabs(next);changed(tab);
                tab->findMessage=std::to_string(matches.matches.size())+" "+tr("replacements");return;
            }
            const auto selected=std::find_if(matches.matches.begin(),matches.matches.end(),[&](const auto &m){return m.offset==std::min(start,end) && m.offset+m.length==std::max(start,end);});
            if(selected!=matches.matches.end()) {tab->editor.ReplaceTextPreservingTabs(replacement);changed(tab);}
            findInDocument(tab,false,false,false);return;
        }
        const text::TextMatch *hit=nullptr;
        for(const auto &m:matches.matches) {
            if(previous){if(m.offset<std::min(start,end))hit=&m;}
            else if(m.offset>=std::max(start,end)){hit=&m;break;}
        }
        if(!hit && tab->findWrap)hit=previous?&matches.matches.back():&matches.matches.front();
        if(!hit){tab->findMessage=previous?"Start of document reached":"End of document reached";return;}
        tab->editor.SelectRegion(displayPosition(source,hit->offset),displayPosition(source,hit->offset+hit->length));
        tab->editor.ScrollToLine(hit->line?hit->line-1:0);
        tab->findMessage.clear();
    }catch(const std::exception &e){error(e.what());}
}

void Application::documentFindBar(const std::shared_ptr<Tab> &tab) {
    if(!tab->findOpen) {
        if(!tab->findKey.empty()){tab->editor.SetReaderSearchMatches({},cyui::palette().warning);tab->findKey.clear();}
        return;
    }
    ImGui::PushID("document-find");
    const auto &p=cyui::palette();
    cyui::ScopedColor bg(ImGuiCol_ChildBg,cyui::vec(p.bg2));
    cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding,cyui::dp(8,6));
    const float height=ImGui::GetFrameHeightWithSpacing()*(tab->findReplace?5:4)+ImGui::GetStyle().WindowPadding.y*2;
    ImGui::BeginChild("##find-bar",{0,height},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar);
    if(tab->findFocus){ImGui::SetKeyboardFocusHere();tab->findFocus=false;}
    ImGui::SetNextItemWidth(std::max(cyui::dp(60),ImGui::GetContentRegionAvail().x-cyui::dp(90)-ImGui::GetStyle().ItemSpacing.x*3));
    const bool enter=ImGui::InputTextWithHint("##query",tr("Find in file"),&tab->findQuery,ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();if(cyui::iconButton(cyui::icon::chevronUp,tr("Previous"),false,0,cyui::dp(26,24)))findInDocument(tab,true);
    ImGui::SameLine();if(cyui::iconButton(cyui::icon::chevronDown,tr("Next"),false,0,cyui::dp(26,24)) || enter)findInDocument(tab,ImGui::GetIO().KeyShift);
    ImGui::SameLine();if(cyui::iconButton(cyui::icon::close,tr("Close search"),false,0,cyui::dp(26,24))) {tab->findOpen=false;tab->editor.SetFocus();}
    if(tab->findReplace) {
        const float controls=ImGui::CalcTextSize(tr("Replace")).x+ImGui::CalcTextSize(tr("Replace all")).x+ImGui::GetStyle().FramePadding.x*4+ImGui::GetStyle().ItemSpacing.x*2;
        ImGui::SetNextItemWidth(std::max(cyui::dp(60),ImGui::GetContentRegionAvail().x-controls));
        ImGui::InputTextWithHint("##replacement",tr("Replace with"),&tab->findReplacement);
        ImGui::SameLine();cyui::ScopedDisabled ro(tab->readOnly);
        if(cyui::button(tr("Replace")))findInDocument(tab,false,true);
        ImGui::SameLine();if(cyui::button(tr("Replace all")))findInDocument(tab,false,true,true);
    }
    ImGui::Checkbox(tr("Match case"),&tab->findSensitive);ImGui::SameLine();
    ImGui::Checkbox(tr("Whole word"),&tab->findWords);
    ImGui::Checkbox(tr("Wrap around"),&tab->findWrap);ImGui::SameLine();
    ImGui::Checkbox(tr("Extended escapes"),&tab->findExtended);
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Escape)) {tab->findOpen=false;tab->editor.SetFocus();}
    const auto key=tab->findQuery+":"+std::to_string(tab->editor.GetTextRevision())+":"+std::to_string(tab->findSensitive)+std::to_string(tab->findWords)+std::to_string(tab->findExtended)+":"+std::to_string(p.warning);
    if(key!=tab->findKey) {
        tab->findKey=key;const auto source=tab->editor.GetText();const auto query=text::expandedSearchText(tab->findQuery,tab->findExtended);
        const auto found=query.size()<=4096?text::findTextMatches(source,query,tab->findSensitive,tab->findWords,2000):text::TextMatches{};
        tab->findCount=found.matches.size();tab->findTruncated=found.truncated;
        tab->editor.SetReaderSearchMatches(displayMatches(source,found),p.warning);
    }
    ImGui::SameLine();cyui::textDim("%zu%s",tab->findCount,tab->findTruncated?"+":"");
    if(!tab->findMessage.empty())cyui::textDim("%s",tr(tab->findMessage.c_str()));
    ImGui::EndChild();ImGui::PopID();
}

void Application::updateSearchPreview(const Json &hit) {
    const auto wanted=std::to_string(searchGeneration_)+":"+hit.dump();
    if(searchPreviewFuture_.valid() && searchPreviewFuture_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            const auto result=searchPreviewFuture_.get();
            if(searchPreviewRunning_==wanted) {
                configureCodeEditor(searchPreview_);searchPreview_.setLanguageForPath(hit.at("path").get<std::string>());
                searchPreview_.setReadOnly(true);searchPreview_.native().SetFindReplaceEnabled(false);
                searchPreview_.native().SetShowMiniMapEnabled(false);
                searchPreview_.setText(result.at("text").get<std::string>());
                if(!regex_) {
                    const auto source=searchPreview_.native().GetText();
                    searchPreview_.native().SetReaderSearchMatches(displayMatches(source,text::findTextMatches(source,searchQuery_,caseSensitive_,false,2000)),cyui::palette().warning);
                }
                const auto line=hit.at("line").get<size_t>();
                const auto value=searchPreview_.native().GetLineText(line?line-1:0);
                const auto byte=std::min(value.size(),hit.at("column").get<size_t>()-1);
                const auto point=displayPosition(value,byte);
                searchPreview_.native().SetCursor({line?line-1:0,point.index});
                searchPreview_.native().ScrollToLine(line>5?line-5:0);
                searchPreviewLoaded_=wanted;searchPreviewError_.clear();
            }
        }catch(const std::exception &e){if(searchPreviewRunning_==wanted){searchPreviewLoaded_=wanted;searchPreviewError_=e.what();}}
    }
    if(!searchPreviewFuture_.valid() && searchPreviewLoaded_!=wanted) {
        const auto owner=engine_;const auto path=hit.at("path").get<std::string>();
        searchPreviewRunning_=wanted;
        searchPreviewFuture_=std::async(std::launch::async,[owner,path] {
            const auto documents=owner->dispatch("documents.list").at("documents");
            for(const auto &document:documents)if(document.at("path")==path) {
                if(document.value("storage","")=="paged")throw std::runtime_error("Paged results open in the source view; no full-text preview copy.");
                return owner->dispatch("documents.read",{{"path",path}});
            }
            // Bounded read-only display snapshot through Core's codecs. Merely
            // previewing a disk hit must not open or retain a native document.
            const auto bytes=readTextFileBytes(owner->resolve(path));
            const auto document=text::decodeBuffer(bytes,bytes.starts_with("\xef\xbb\xbf")?"utf-8-bom":"utf-8");
            return Json{{"path",path},{"text",normalizeEditorText(document.text)}};
        });
    }
}

void Application::searchPanel() {
    auto [state,inserted]=panels_.try_emplace("search",false);
    if(!state->second)return;
    ImGui::SetNextWindowSize(cyui::dp(1050,650),ImGuiCond_FirstUseEver);
    if(focusSearch_)ImGui::SetNextWindowFocus();
    if(!ImGui::Begin(label("Find in workspace...","search").c_str(),&state->second,ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoCollapse)){ImGui::End();return;}
    if(focusSearch_){ImGui::SetKeyboardFocusHere();focusSearch_=false;focusPanel_.clear();}
    const float controls=ImGui::CalcTextSize(tr("Case sensitive")).x+ImGui::CalcTextSize(tr("Regex")).x+ImGui::GetFrameHeight()*3+ImGui::GetStyle().ItemInnerSpacing.x*2+ImGui::GetStyle().ItemSpacing.x*3;
    ImGui::SetNextItemWidth(std::max(cyui::dp(80),ImGui::GetContentRegionAvail().x-controls));
    bool changed=ImGui::InputTextWithHint("##workspace-query",tr("Find in workspace..."),&searchQuery_);
    ImGui::SameLine();changed|=ImGui::Checkbox(tr("Case sensitive"),&caseSensitive_);
    ImGui::SameLine();changed|=ImGui::Checkbox(tr("Regex"),&regex_);
    ImGui::SameLine();
    if(cyui::iconButton(searchSideBySide_?cyui::icon::dockBottom:cyui::icon::dockRight,tr(searchSideBySide_?"Preview below results":"Preview beside results"))) {searchSideBySide_=!searchSideBySide_;saveSettings();}
    if(changed){if(searchCancellation_)*searchCancellation_=true;searchChanged_=ImGui::GetTime();++searchGeneration_;searchQueued_=searchQuery_.size()<=4096;selectedSearch_=size_t(-1);searchPreviewLoaded_.clear();searchPreviewError_.clear();}
    if(searchQuery_.size()>4096)cyui::callout(cyui::Tone::warning,tr("Search query must contain 1 to 4096 bytes"));
    cyui::filterBox("##search-results-filter",searchResultsFilter_);
    const auto matches=search_.value("matches",Json::array());
    const bool current=displayedSearchGeneration_==searchGeneration_;
    cyui::textDim("%zu %s",matches.size(),tr("matches"));
    if(searchFuture_.valid() || searchQueued_){ImGui::SameLine();cyui::spinner("##searching");}
    if(search_.value("truncated",false)){ImGui::SameLine();cyui::badge(tr("Results limited"),cyui::Tone::warning);}
    if(search_.value("unsavedRegexExcluded",false))cyui::callout(cyui::Tone::warning,tr("Regex excludes modified buffers."));
    bool openResult=false;
    const float height=std::max(cyui::dp(180),ImGui::GetContentRegionAvail().y);
    const bool columns=searchSideBySide_;
    const bool shown=!columns || ImGui::BeginTable("##search-split",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV,{0,height});
    if(shown) {
        if(columns){ImGui::TableSetupColumn("Results",ImGuiTableColumnFlags_WidthStretch,.46f);ImGui::TableSetupColumn("Preview",ImGuiTableColumnFlags_WidthStretch,.54f);ImGui::TableNextColumn();}
        const float handle=cyui::dp(6);
        float top=(height-handle)*searchResultsFraction_,bottom=height-handle-top;
        ImGui::BeginChild("##search-results",{0,columns?0:top},ImGuiChildFlags_Borders);
        std::map<std::string,std::vector<size_t>> groups;
        for(size_t i=0;i<matches.size();++i)if(searchResultsFilter_.matches(matches[i].value("text",""),matches[i].at("path").get_ref<const std::string &>()))groups[matches[i].at("path").get<std::string>()].push_back(i);
        bool selectedVisible=false;
        for(const auto &[path,indices]:groups)if(std::find(indices.begin(),indices.end(),selectedSearch_)!=indices.end()){selectedVisible=true;break;}
        if(!selectedVisible)selectedSearch_=groups.empty()?size_t(-1):groups.begin()->second.front();
        const auto selectionKey=ImGui::GetID(("selection/"+std::to_string(displayedSearchGeneration_)).c_str());
        auto storage=ImGui::GetStateStorage();
        const bool revealSelection=storage->GetInt(selectionKey,-1)!=static_cast<int>(selectedSearch_);
        storage->SetInt(selectionKey,static_cast<int>(selectedSearch_));
        std::vector<size_t> visible;
        for(const auto &[path,indices]:groups) {
            ImGui::PushID(path.c_str());
            const auto title=path+" ("+std::to_string(indices.size())+")";
            const bool expanded=ImGui::TreeNodeEx("##file",ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_SpanAvailWidth);
            ImGui::SameLine();ImGui::TextUnformatted(title.c_str());cyui::tooltip(path.c_str());
            if(expanded) {
                ImGuiListClipper clip;clip.Begin(static_cast<int>(indices.size()));
                if(revealSelection)if(const auto at=std::find(indices.begin(),indices.end(),selectedSearch_);at!=indices.end())clip.IncludeItemByIndex(static_cast<int>(at-indices.begin()));
                while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
                    const auto i=indices[static_cast<size_t>(row)];const auto &hit=matches[i];ImGui::PushID(static_cast<int>(i));
                    const auto title=std::to_string(hit.at("line").get<size_t>())+"  "+hit.at("text").get<std::string>();
                    const auto origin=ImGui::GetCursorScreenPos();
                    if(ImGui::Selectable("##match",i==selectedSearch_,ImGuiSelectableFlags_AllowDoubleClick)){selectedSearch_=i;openResult=ImGui::IsMouseDoubleClicked(0);}
                    if(revealSelection && i==selectedSearch_)ImGui::SetScrollHereY(.5f);
                    auto draw=ImGui::GetWindowDrawList();draw->PushClipRect(origin,{origin.x+ImGui::GetContentRegionAvail().x,origin.y+ImGui::GetTextLineHeight()},true);
                    if(!regex_)for(const auto &m:text::findTextMatches(title,searchQuery_,caseSensitive_,false,64).matches) {
                        const auto a=ImGui::CalcTextSize(title.data(),title.data()+m.offset).x;
                        const auto b=ImGui::CalcTextSize(title.data(),title.data()+m.offset+m.length).x;
                        draw->AddRectFilled({origin.x+a,origin.y},{origin.x+b,origin.y+ImGui::GetTextLineHeight()},cyui::alpha(cyui::palette().warning,.35f));
                    }
                    draw->AddText(origin,cyui::palette().text,title.c_str());draw->PopClipRect();
                    cyui::tooltip(path.c_str());ImGui::PopID();
                }
                ImGui::TreePop();visible.insert(visible.end(),indices.begin(),indices.end());
            }ImGui::PopID();
        }
        if(groups.empty())cyui::emptyState(tr(searchQuery_.empty()?"Search your project":"No matches"),tr("Results include unsaved documents."),cyui::icon::search);
        ImGui::EndChild();
        if(columns)ImGui::TableNextColumn();
        else {
            const auto pos=ImGui::GetCursorScreenPos();const float minSize=std::min(cyui::dp(110),(height-handle)*.3f);
            const ImRect rect(pos,{pos.x+ImGui::GetContentRegionAvail().x,pos.y+handle});
            if(ImGui::SplitterBehavior(rect,ImGui::GetID("##results-preview-split"),ImGuiAxis_Y,&top,&bottom,minSize,minSize,0))searchResultsFraction_=top/(height-handle);
            ImGui::GetWindowDrawList()->AddLine({rect.Min.x,rect.Min.y+handle*.5f},{rect.Max.x,rect.Min.y+handle*.5f},cyui::palette().lineStrong,cyui::dp(1));
            ImGui::Dummy({0,handle});
        }
        ImGui::BeginChild("##search-preview",{0,0});
        if(current && selectedSearch_<matches.size()) {
            const auto &hit=matches[selectedSearch_];updateSearchPreview(hit);
            if(cyui::iconButton(cyui::icon::openFile,tr("Open result")))openResult=true;
            ImGui::SameLine();cyui::textDim("%s:%zu",hit.at("path").get_ref<const std::string &>().c_str(),hit.at("line").get<size_t>());
            if(searchPreviewFuture_.valid())cyui::spinner("##loading-preview");
            else if(!searchPreviewError_.empty())cyui::callout(cyui::Tone::warning,tr(searchPreviewError_.c_str()));
            else drawCode(searchPreview_,"##workspace-search-preview");
        }else cyui::emptyState(tr("Preview"),tr("Select a result to preview it."),cyui::icon::document);
        ImGui::EndChild();if(columns)ImGui::EndTable();
        if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !visible.empty()) {
            const int direction=ImGui::IsKeyPressed(ImGuiKey_DownArrow,false)?1:ImGui::IsKeyPressed(ImGuiKey_UpArrow,false)?-1:0;
            if(direction){const auto at=std::find(visible.begin(),visible.end(),selectedSearch_);const auto index=at==visible.end()?(direction>0?-1:0):int(at-visible.begin());selectedSearch_=visible[static_cast<size_t>((index+int(visible.size())+direction)%int(visible.size()))];}
        }
    }
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        if(ImGui::IsKeyPressed(ImGuiKey_Enter,false))openResult=true;
        if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){state->second=false;if(!active_.empty())focusTab_=active_;}
    }
    if(openResult && current && selectedSearch_<matches.size()) {
        const auto hit=matches[selectedSearch_];state->second=false;
        openFromProject(hit.at("path"),ImGui::GetIO().KeyCtrl);
        open(hit.at("path"),hit.at("line"),hit.at("column"));
    }
    ImGui::End();
}
} // namespace cycode
