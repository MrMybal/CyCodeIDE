#include "cycode/Application.h"
#include "EnginePagedBackend.h"
#include "ApplicationHelp.h"
#include <cycode/TextFile.h>
#include <cycode/Error.h>
#include <algorithm>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace cycode {
namespace {
bool pagedReady(std::future<Json> &value) {
    return value.valid() && value.wait_for(std::chrono::seconds(0))==std::future_status::ready;
}
}
bool Application::openPaged(const std::string &path,size_t line,size_t column,bool utf16,bool activate,bool force) {
    if (isUntitledDocument(path) || path.find("://")!=std::string::npos) return false;
    const auto full=engine_->resolve(path);
    const auto canonical=pathUtf8(full.lexically_relative(engine_->workspace()));
    std::shared_ptr<Tab> tab;
    for (const auto &candidate:tabs_) if (candidate->path==canonical) { tab=candidate;break; }
    bool paged=tab && tab->pagedProfile;
    if (!paged && !force) {
        const auto docs=engine_->dispatch("documents.list").at("documents");
        for (const auto &doc:docs) if (doc.at("path")==canonical && doc.value("storage","")=="paged") paged=true;
        if (!paged && std::filesystem::is_regular_file(full)) paged=std::filesystem::file_size(full)>maxTextFileBytes;
    }
    if (!paged && !force) return false;
    if (tab && !tab->pagedProfile) {
        if (tab->dirty || tab->completionAccept.valid()) throw std::runtime_error(tr("Save or close modified documents before changing text profiles."));
        closeTab(canonical);
        if (std::find(tabs_.begin(),tabs_.end(),tab)!=tabs_.end()) return true;
        tab.reset();
    }
    if (!tab) {
        tab=std::make_shared<Tab>();tab->path=tab->displayPath=canonical;tab->pagedProfile=true;tab->readOnly=true;
        auto owner=engine_;
        tab->pagedOpening=std::async(std::launch::async,[owner,canonical] {
            return owner->dispatch("documents.readPage",{{"path",canonical},{"length",4}});
        });
        tabs_.push_back(tab);
    }
    if (line) { tab->pagedLocation={{"line",line},{"column",column},{"utf16",utf16}}; }
    if (activate) {
        if (active_!=canonical && !historyMoving_) rememberNavigation();
        active_=focusTab_=canonical;
        std::erase(recentFiles_,canonical);recentFiles_.insert(recentFiles_.begin(),canonical);
        if (recentFiles_.size()>100) recentFiles_.resize(100);
    }
    return true;
}
void Application::updatePaged() {
    const auto views=tabs_;
    for (const auto &tab:views) if (tab->pagedProfile) {
        if (pagedReady(tab->pagedOpening)) {
            try {
                const auto info=tab->pagedOpening.get();tab->identity=info.at("id");tab->revision=info.at("revision");tab->dirty=info.at("dirty");
                tab->pagedBackend=std::make_shared<EnginePagedBackend>(engine_,tab->identity,tab->path);
                tab->paged=std::make_unique<PagedTextEditor>(tab->identity,tab->pagedBackend);
                tab->paged->setLanguage(cyui::codeLanguageForPath(tab->path));
                auto options=tab->paged->view().options();options.keyboardShortcuts=false;tab->paged->view().setOptions(options);
            } catch (const std::exception &e) { tab->pagedError=e.what(); }
        }
        if (!tab->paged) continue;
        tab->paged->poll();const auto &status=tab->paged->status();
        if (!status.ready) continue;
        const auto nextPath=pathUtf8(status.path);
        if (nextPath!=tab->path) {
            resourceTabPath(tab,nextPath);tab->paged->setLanguage(cyui::codeLanguageForPath(nextPath));
            refreshFiles(false);refreshGit();
        }
        tab->revision=status.revision;tab->readOnly=status.readOnly;
        tab->dirty=status.dirty || tab->paged->unsettled();
        if(tab->pagedScale) {auto options=tab->paged->view().options();options.textScale=codeZoom_;tab->paged->view().setOptions(options);tab->pagedScale.reset();}
        if (!status.busy && tab->pagedLocation.is_object() && !tab->pagedNavigation.valid()) {
            const auto location=tab->pagedLocation;tab->pagedLocation=nullptr;
            const auto scroll=tab->pagedScroll,scrollColumn=tab->pagedScrollColumn;
            tab->pagedScroll.reset();tab->pagedScrollColumn.reset();
            const auto owner=engine_;const auto path=tab->path;const auto revision=tab->revision,id=tab->identity;
            tab->pagedNavigation=std::async(std::launch::async,[owner,path,revision,id,location,scroll,scrollColumn] {
                const auto lineInfo=owner->dispatch("documents.readLines",{{"path",path},{"expectedRevision",revision},{"expectedDocumentId",id},{"line",0},{"length",4}});
                const auto line=std::min(location.at("line").get<uint64_t>()-1,lineInfo.at("totalLines").get<uint64_t>()-1);
                auto args=Json{{"path",path},{"expectedRevision",revision},{"expectedDocumentId",id},{"line",line},{"length",4}};
                uint64_t characters=location.value("column",uint64_t(1));if(characters)--characters;
                if(location.value("utf16",false)) {
                    args["character"]=characters;
                    try{owner->dispatch("documents.readLines",args);}
                    catch(const NativeError &error){if(error.code()!=ErrorCode::InvalidParams)throw;characters=0;}
                } else {
                    const auto start=owner->dispatch("documents.readLines",args).at("offset").get<uint64_t>();
                    auto remaining=characters;characters=0;uint64_t at=start;bool done=false;
                    while(remaining && !done) {
                        const auto part=owner->dispatch("documents.readPage",{{"path",path},{"expectedRevision",revision},{"expectedDocumentId",id},{"offset",at},{"length",65536}});
                        const auto &text=part.at("text").get_ref<const std::string &>();if(text.empty())break;
                        size_t take=0;
                        while(take<text.size() && remaining) {
                            const auto c=static_cast<unsigned char>(text[take]);if(c=='\r'||c=='\n'){done=true;break;}
                            const size_t width=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
                            if(width>remaining){done=true;break;}take+=width;remaining-=width;characters+=width==4?2:1;
                        }
                        at+=take;if(take<text.size())break;
                    }
                }
                Json result={{"revision",revision},{"line",line},{"character",characters}};
                if(scroll)result.update({{"scrollLine",*scroll},{"scrollColumn",scrollColumn.value_or(0)}});
                return result;
            });
        }
        if (pagedReady(tab->pagedNavigation)) {
            try { const auto pos=tab->pagedNavigation.get();
                if (pos.at("revision")==tab->revision && !tab->pagedLocation.is_object()) {
                    cyui::PagedCodePosition p{pos.at("line").get<uint64_t>(),pos.at("character").get<uint64_t>()};tab->paged->view().setSelection({p,p});
                    if(pos.contains("scrollLine"))tab->paged->view().scrollTo(std::min(pos.at("scrollLine").get<uint64_t>(),status.lines-1),pos.at("scrollColumn").get<uint64_t>());
                }
            } catch (const std::exception &e) { tab->pagedError=e.what(); }
        }
        if (tab->pagedCloseAfterSave && !status.busy && !tab->dirty) {
            tab->pagedCloseAfterSave=false;closeTab(tab->path);
        }
    }
    if (pagedReady(pagedRecoveryFuture_)) {
        try {
            auto result=pagedRecoveryFuture_.get();
            if (pagedRecoveryMethod_=="documents.pagedCheckpoints") pagedCheckpoints_=std::move(result);
            else if (pagedRecoveryMethod_=="documents.previewPagedCheckpoint") {
                pagedCheckpoint_=std::move(result);pagedCheckpointCode_.setText(pagedCheckpoint_.at("text"),cyui::CodeLanguage::plain);pagedCheckpointView_={};
            } else if (pagedRecoveryMethod_=="documents.restorePagedCheckpoint") {
                refactorPlan_=std::move(result);refactorPreview_=nullptr;
                showPanel("Refactoring","refactoring");selectChangeFile(refactorPlan_.at("files")[0].at("path"));
            }
        } catch (const std::exception &e) { pagedRecoveryError_=e.what(); }
    }
}
bool Application::pagedCommand(const std::string &id,const std::shared_ptr<Tab> &tab) {
    if (!tab || !tab->paged) return false;
    using C=cyui::PagedCodeCommand;
    for (const auto &[name,command]:std::initializer_list<std::pair<const char*,C>>{
        {"edit.undo",C::undo},{"edit.redo",C::redo},{"edit.copy",C::copy},{"edit.cut",C::cut}})
        if (id==name) { tab->paged->command(command);return true; }
    if (id=="edit.paste") { tab->paged->paste();return true; }
    if (id=="edit.selectAll") { tab->paged->selectAll();return true; }
    if (id=="search.document" || id=="search.replace") { tab->pagedFind=true;return true; }
    if (id=="search.next" || id=="search.previous") { tab->paged->find(id=="search.previous");return true; }
    if (id=="navigate.line") { tab->pagedFind=true;return true; }
    if (id=="file.checkpointPaged") { tab->paged->checkpoint();return true; }
    return false;
}
void Application::pagedPanel(const std::shared_ptr<Tab> &tab) {
    if (!tab->pagedError.empty()) cyui::callout(cyui::Tone::warning,tab->pagedError.c_str());
    if (!tab->paged) { cyui::spinner("##paged-opening");ImGui::SameLine();ImGui::TextUnformatted(tr("Opening native paged text..."));return; }
    auto &editor=*tab->paged;
    cyui::textDim("%s",tr("Paged UTF-8. No LSP overlay; save or close unsaved paged edits before compiler queries."));
    if(cyui::ghostButton(tr("Paged text documentation")))try{app_help::open(root_,"app/Documentation",locale_,"large-projects","storage",smoke_||testSession_);}catch(const std::exception &e){error(e.what());}
    ImGui::SameLine();if(cyui::ghostButton(tr("Find in paged text")))tab->pagedFind=!tab->pagedFind;
    ImGui::SameLine();if(cyui::ghostButton(tr("Refresh paged state"))){editor.refresh();tab->pagedError.clear();}
    ImGui::SameLine();if(cyui::ghostButton(tr("Paged checkpoints"))){showPanel("Recovery","recovery");pagedRecoveryTask("documents.pagedCheckpoints");}
    if (!editor.cancelledInput().empty() || !editor.cancelledEdits().empty()) {
        cyui::callout(cyui::Tone::warning,tr("Pending input was cancelled. Recover it before closing or saving."));
        if(cyui::button(tr("Copy pending input")))ImGui::SetClipboardText(editor.cancelledText().c_str());
        ImGui::SameLine();if(cyui::button(tr("Acknowledge pending input")))editor.acknowledgeCancelledInput();
    }
    if(tab->pagedFind) {
        ImGui::PushID("paged-find");
        ImGui::SetNextItemWidth(cyui::dp(240));const bool enter=ImGui::InputText(tr("Find"),&tab->pagedQuery,ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();ImGui::Checkbox(tr("Case sensitive"),&tab->pagedSensitive);
        ImGui::SameLine();ImGui::Checkbox(tr("Whole words"),&tab->pagedWords);
        if(cyui::button(tr("Search")) || enter)editor.search(tab->pagedQuery,tab->pagedSensitive,tab->pagedWords);
        ImGui::SameLine();if(cyui::button(tr("Previous")))editor.find(true);
        ImGui::SameLine();if(cyui::button(tr("Next")))editor.find(false);
        ImGui::SetNextItemWidth(cyui::dp(240));ImGui::InputText(tr("Replace"),&tab->pagedReplacement);
        ImGui::SameLine();if(cyui::button(tr("Replace current match"))) {
            const auto &s=editor.status();const auto selected=editor.view().selection();
            const auto hit=std::find_if(s.matches.begin(),s.matches.end(),[&](const auto &m){return m.range.first()==selected.first() && m.range.last()==selected.last();});
            if(s.searchedRevision==s.revision && hit!=s.matches.end())editor.replace(tab->pagedReplacement);
            else editor.find(false);
        }
        ImGui::SetNextItemWidth(cyui::dp(150));ImGui::InputScalar(tr("Line"),ImGuiDataType_U64,&tab->pagedLine);
        ImGui::SameLine();if(cyui::button(tr("Go to line")))editor.goTo(tab->pagedLine?tab->pagedLine-1:0);
        const auto &s=editor.status();
        cyui::textDim("%zu %s%s",s.matches.size(),tr("matches"),s.truncated?tr("Results limited"):"");
        if(s.searching)cyui::spinner("##paged-search");
        ImGui::PopID();
    }
    using C=cyui::PagedCodeCommand;
    for(const auto action:drawPaged(editor,("##paged-"+std::to_string(tab->identity)).c_str())) {
        if(action==C::save)save(tab);else if(action==C::find)tab->pagedFind=true;
        else if(action==C::findNext)editor.find(false);else if(action==C::findPrevious)editor.find(true);
    }
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !(ImGui::GetActiveID() && ImGui::GetCurrentContext()->InputTextState.ID==ImGui::GetActiveID()))editorFocused_=true;
}
void Application::pagedRecoveryTask(const std::string &method,Json args) {
    if(pagedRecoveryFuture_.valid())return;
    pagedRecoveryMethod_=method;pagedRecoveryError_.clear();auto owner=engine_;
    pagedRecoveryFuture_=std::async(std::launch::async,[owner,method,args]{return owner->dispatch(method,args);});
}
void Application::pagedRecoveryControls() {
    if(!cyui::beginSection(tr("Explicit paged recovery")))return;
    cyui::callout(cyui::Tone::neutral,tr("Paged checkpoints are explicit. Publishing one does not save the source; restore is a reviewed native change with Undo."));
    {cyui::ScopedDisabled disabled(pagedRecoveryFuture_.valid());if(cyui::button(tr("Discover paged checkpoints")))pagedRecoveryTask("documents.pagedCheckpoints");}
    for(const auto &tab:tabs_)if(tab->path==active_ && tab->paged) {
        ImGui::SameLine();cyui::ScopedDisabled disabled(tab->paged->status().busy || tab->paged->unsettled());
        if(cyui::button(tr("Checkpoint active paged text")))tab->paged->checkpoint();
    }
    if(!pagedRecoveryError_.empty())cyui::callout(cyui::Tone::warning,pagedRecoveryError_.c_str());
    if(pagedRecoveryFuture_.valid())cyui::spinner("##paged-recovery");
    if(pagedCheckpoints_.value("truncated",false)||pagedCheckpoints_.value("invalid",size_t(0)))cyui::callout(cyui::Tone::warning,tr("Checkpoint discovery was limited or contains invalid records. No checkpoint was deleted."));
    const Json empty=Json::array();const auto found=pagedCheckpoints_.find("checkpoints");
    const auto &checkpoints=found!=pagedCheckpoints_.end()?*found:empty;
    for(const auto &row:checkpoints) {
        const auto id=row.at("checkpoint").get<std::string>(),path=row.at("path").get<std::string>();ImGui::PushID(id.c_str());
        {cyui::ScopedDisabled disabled(pagedRecoveryFuture_.valid());if(cyui::button(tr("Preview")))pagedRecoveryTask("documents.previewPagedCheckpoint",{{"checkpoint",id},{"length",65536}});}
        ImGui::SameLine();if(cyui::ghostButton(tr("Open paged source")))try{openPaged(path,0,0,false,true,true);}catch(const std::exception &e){pagedRecoveryError_=e.what();}
        ImGui::SameLine();ImGui::TextUnformatted(path.c_str());ImGui::PopID();
    }
    if(pagedCheckpoint_.contains("text")) {
        const auto id=pagedCheckpoint_.at("checkpoint").get<std::string>(),path=pagedCheckpoint_.at("path").get<std::string>();
        cyui::textDim("%s | %s | %llu / %llu",path.c_str(),id.c_str(),static_cast<unsigned long long>(pagedCheckpoint_.at("offset").get<uint64_t>()),static_cast<unsigned long long>(pagedCheckpoint_.at("bytes").get<uint64_t>()));
        if(!pagedCheckpoint_.value("diskMatchesBaseline",false))cyui::callout(cyui::Tone::warning,tr("Checkpoint baseline differs from disk. Reconcile the source before restoration."));
        drawCodeView("##paged-checkpoint-preview",pagedCheckpointView_,pagedCheckpointCode_,cyui::dp(0,180));
        {cyui::ScopedDisabled disabled(pagedRecoveryFuture_.valid());
          if(cyui::button(tr("First preview page")))pagedRecoveryTask("documents.previewPagedCheckpoint",{{"checkpoint",id},{"length",65536}});
          ImGui::SameLine();{cyui::ScopedDisabled more(!pagedCheckpoint_.value("hasMore",false));if(cyui::button(tr("Next preview page")))pagedRecoveryTask("documents.previewPagedCheckpoint",{{"checkpoint",id},{"offset",pagedCheckpoint_.at("nextOffset")},{"length",65536}});}}
        for(const auto &tab:tabs_)if(tab->path==path && tab->paged) {
            cyui::ScopedDisabled disabled(pagedRecoveryFuture_.valid()||refactorFuture_.valid()||tab->paged->status().busy||tab->paged->unsettled()||!pagedCheckpoint_.value("diskMatchesBaseline",false));
            if(cyui::primaryButton(tr("Prepare checkpoint restoration")))pagedRecoveryTask("documents.restorePagedCheckpoint",{{"path",path},{"checkpoint",id},{"expectedRevision",tab->revision}});
        }
    }
    cyui::endSection();
}
}
