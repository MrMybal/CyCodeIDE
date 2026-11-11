#include "EnginePagedBackend.h"
#include <cycode/TextFile.h>
#include <cycode/text/Search.h>
#include <algorithm>
#include <stdexcept>

namespace cycode {
namespace {
constexpr size_t chunkBytes=65536, syntaxBytes=32*1024*1024;
struct Cursor {
    cyui::PagedCodePosition position{};
    bool cr=false;
    void advance(std::string_view text) {
        for (size_t at=0; at<text.size();) {
            const auto c=static_cast<unsigned char>(text[at]);
            if (c=='\r') { ++position.line; position.character=0; cr=true; ++at; }
            else if (c=='\n') { if (!cr) ++position.line; position.character=0; cr=false; ++at; }
            else { const auto width=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
                position.character+=width==4?2:1; at+=width; cr=false; }
        }
    }
};
}
EnginePagedBackend::EnginePagedBackend(std::shared_ptr<Engine> engine, cyui::ItemId id, const std::string &path)
    :engine_(std::move(engine)),identity_(id),path_(path) {}
Json EnginePagedBackend::parameters(cyui::ItemId id, uint64_t revision) const {
    if (id!=identity_ || !revision) throw std::runtime_error("Paged document identity or revision changed");
    return {{"path",path_},{"expectedDocumentId",identity_},{"expectedRevision",revision}};
}
PagedTextStatus EnginePagedBackend::describe(const Json &info, bool withLines) {
    if (info.at("id")!=identity_ || info.value("storage","")!="paged")
        throw std::runtime_error("Native paged document was replaced or closed");
    path_=info.at("path");
    auto result=current_;
    result.path=utf8Path(path_); result.ready=true; result.busy=false;
    result.revision=info.at("revision"); result.bytes=info.at("bytes"); result.dirty=info.at("dirty");
    result.readOnly=readOnly_; result.message.clear();
    if (withLines) {
        auto args=parameters(identity_,result.revision); args.update({{"line",0},{"length",4}});
        result.lines=engine_->dispatch("documents.readLines",args).at("totalLines");
    }
    result.syntaxMessage=result.bytes>syntaxBytes ?
        "Syntax unavailable above 32 MiB: shared streaming lexer required." : "";
    return result;
}
PagedTextStatus EnginePagedBackend::state(cyui::ItemId id) {
    if (id!=identity_) throw std::runtime_error("Wrong native paged document identity");
    const auto rows=engine_->dispatch("documents.list").at("documents");
    for (const auto &info:rows) if (info.value("id",uint64_t(0))==id) {
        auto result=describe(info);
        // Establish the document's insertion policy from its first physical
        // newline, even when the first line exceeds one page. Keep that policy
        // through edits, as for ordinary buffers; this is bounded worker I/O.
        if(!current_.ready) for(uint64_t offset=0;offset<result.bytes;) {
            auto page=readBytes(id,result.revision,offset,chunkBytes);
            if(const auto at=page.text.find_first_of("\r\n");at!=std::string::npos) {
                if(page.text[at]=='\n')result.newline="\n";
                else {
                    const bool lf=at+1<page.text.size()?page.text[at+1]=='\n':
                        page.nextOffset<result.bytes && readBytes(id,result.revision,page.nextOffset,4).text.starts_with("\n");
                    result.newline=lf?"\r\n":"\r";
                }
                break;
            }
            offset=page.nextOffset;
        }
        current_=std::move(result); history(); return current_;
    }
    throw std::runtime_error("Native paged document was closed");
}
uint64_t EnginePagedBackend::offset(cyui::ItemId id, uint64_t revision, cyui::PagedCodePosition position) {
    auto args=parameters(id,revision);
    args.update({{"line",position.line},{"character",position.character},{"length",4}});
    return engine_->dispatch("documents.readLines",args).at("offset");
}
PagedTextBytes EnginePagedBackend::readBytes(cyui::ItemId id, uint64_t revision, uint64_t at, size_t length) {
    auto args=parameters(id,revision); args.update({{"offset",at},{"length",std::clamp<size_t>(length,4,262144)}});
    auto page=engine_->dispatch("documents.readPage",args);
    return {page.at("text").get<std::string>(), page.at("nextOffset").get<uint64_t>()};
}
std::vector<cyui::CodeToken> EnginePagedBackend::syntax(cyui::ItemId id, uint64_t revision,
    cyui::CodeLanguage language, uint64_t at, size_t length) {
    if (language==cyui::CodeLanguage::plain || current_.bytes>syntaxBytes) return {};
    if (syntaxRevision_!=revision || syntaxLanguage_!=language || syntaxCatalogue_!=cyui::codeLanguageRevision()) {
        std::string full; full.reserve(static_cast<size_t>(current_.bytes));
        for (uint64_t offset=0; offset<current_.bytes;) {
            auto page=readBytes(id,revision,offset,chunkBytes);
            if (page.nextOffset<=offset) throw std::runtime_error("Native syntax read did not advance");
            full+=page.text; offset=page.nextOffset;
        }
        // Complete bounded snapshot supplies all preceding multiline lexical context.
        auto tokens=cyui::highlightCode(full,language);
        tokens_=std::move(tokens); syntaxRevision_=revision; syntaxLanguage_=language;
        syntaxCatalogue_=cyui::codeLanguageRevision();
    }
    std::vector<cyui::CodeToken> result;
    auto token=std::lower_bound(tokens_.begin(),tokens_.end(),at,[](const auto &t,auto a){return t.start+t.length<=a;});
    for (;token!=tokens_.end() && token->start<at+length;++token) {
        const auto a=std::max<uint64_t>(at,token->start), b=std::min<uint64_t>(at+length,token->start+token->length);
        if (a<b) result.push_back({size_t(a-at),size_t(b-a),token->role});
    }
    return result;
}
PagedTextPreparedEdit EnginePagedBackend::prepare(const cyui::PagedCodeReplacement &proposal) {
    if (readOnly_) throw std::runtime_error("Paged view is read-only");
    const auto begin=offset(proposal.document,proposal.revision,proposal.selection.first());
    const auto end=offset(proposal.document,proposal.revision,proposal.selection.last());
    const auto logical=normalizeEditorText(proposal.text);
    const auto text=encodeEditorText(logical,false,current_.newline=="\r\n",current_.newline=="\r");
    if (text.size()>1024*1024) throw std::runtime_error("Insertion exceeds the 1 MiB operation budget");
    auto args=parameters(proposal.document,proposal.revision);
    args.update({{"offset",begin},{"length",end-begin},{"text",text}});
    const auto plan=engine_->dispatch("documents.prepareByteEdit",args);
    Cursor next{proposal.selection.first()}; next.advance(text);
    const auto token=plan.at("id").get<std::string>();
    const auto afterLines=current_.lines-(proposal.selection.last().line-proposal.selection.first().line)+
        uint64_t(std::count(logical.begin(),logical.end(),'\n'));
    prepared_.emplace(token,Prepared{proposal.revision,current_.lines,afterLines,proposal.selection,{next.position,next.position}});
    return {token,identity_,proposal.revision};
}
void EnginePagedBackend::discard(const PagedTextPreparedEdit &edit) noexcept { prepared_.erase(edit.token); }
Json EnginePagedBackend::history() {
    auto result=engine_->dispatch("changes.historyStatus");
    auto supports=[&](const char *direction) {
        const auto &head=result.at(direction);
        return head.is_object() && head.at("files").size()==1 && head.at("files")[0]==path_;
    };
    current_.canUndo=supports("undo");current_.canRedo=supports("redo");return result;
}
PagedTextStatus EnginePagedBackend::committed(const Json &result,std::optional<uint64_t> lines) {
    const auto &receipt=result.at("commit");
    for(const auto &info:receipt.at("documents"))if(info.at("id")==identity_) {
        current_=describe(info,false);
        if(lines)current_.lines=*lines;
        else try {
            auto args=parameters(identity_,current_.revision);args.update({{"line",0},{"length",4}});
            current_.lines=engine_->dispatch("documents.readLines",args).at("totalLines");
        } catch(const std::exception &) {
            current_.readOnly=true;current_.message="Committed. Refresh the native paged state before editing.";
        }
        try {history();} catch(const std::exception &){current_.canUndo=current_.canRedo=false;}
        if(!receipt.value("notificationsComplete",true))current_.message="Native edit committed; some notifications failed. Refresh the native paged state.";
        return current_;
    }
    // A validated native receipt must include the prepared document. This branch
    // is a protocol error, never a reason to retry the already-issued operation.
    throw std::runtime_error("Native transaction committed without its paged document receipt; do not retry");
}
PagedTextCommit EnginePagedBackend::apply(const PagedTextPreparedEdit &edit) {
    const auto found=prepared_.find(edit.token);
    if(edit.document!=identity_ || found==prepared_.end() || edit.revision!=found->second.revision)
        throw std::runtime_error("Prepared paged edit identity changed");
    const auto presentation=found->second;
    // Allocate presentation state before the native commit. There is no private
    // text snapshot or Undo stack here; eviction only forgets a caret position.
    cursors_.insert_or_assign(edit.token,presentation);
    if(cursors_.size()>256)cursors_.erase(cursors_.begin());
    const auto result=engine_->dispatch("changes.apply",{{"id",edit.token}});
    return {committed(result,presentation.afterLines),presentation.selection};
}
PagedTextCommit EnginePagedBackend::undo(cyui::ItemId id,uint64_t revision,bool redo) {
    const auto result=history();const auto &head=result.at(redo?"redo":"undo");
    if(id!=identity_ || head.is_null() || head.at("files").size()!=1 || head.at("files")[0]!=path_)
        throw std::runtime_error("Review multi-file native changes in Refactoring before Undo or Redo.");
    const auto source=head.at("id").get<std::string>();
    const auto plan=engine_->dispatch("changes.historyPrepare",{{"direction",redo?"redo":"undo"},{"id",source},
        {"expectedHistoryRevision",result.at("revision")},{"documents",Json::array({parameters(id,revision)})}});
    if(!plan.value("annotations",Json::array()).empty())throw std::runtime_error("Review annotated native changes in Refactoring before Redo.");
    const auto applied=engine_->dispatch("changes.apply",{{"id",plan.at("id")}});
    const auto cursor=cursors_.find(source);
    const auto lines=cursor==cursors_.end()?std::optional<uint64_t>{}:std::optional(redo?cursor->second.afterLines:cursor->second.beforeLines);
    const auto selection=cursor==cursors_.end()?cyui::PagedCodeSelection{}:redo?cursor->second.selection:cursor->second.previous;
    return {committed(applied,lines),selection};
}
PagedTextStatus EnginePagedBackend::setReadOnly(cyui::ItemId id,uint64_t revision,bool value) {
    auto args=parameters(id,revision); args["length"]=4;
    engine_->dispatch("documents.readPage",args);
    readOnly_=value; current_.readOnly=value; return current_;
}
std::string EnginePagedBackend::copy(cyui::ItemId id,uint64_t revision,cyui::PagedCodeSelection selection,size_t budget) {
    const auto first=offset(id,revision,selection.first()),last=offset(id,revision,selection.last());
    if (last-first>budget) throw std::runtime_error("Clipboard selection exceeds the 4 MiB export budget.");
    std::string text; text.reserve(size_t(last-first));
    for (auto at=first;at<last;) {
        auto page=readBytes(id,revision,at,size_t(std::min<uint64_t>(chunkBytes,last-at)));
        const auto take=std::min<uint64_t>(page.text.size(),last-at);
        if (!take) throw std::runtime_error("Native clipboard read did not advance");
        text.append(page.text,0,size_t(take)); at+=take;
    }
    return text;
}
PagedTextSearch EnginePagedBackend::search(cyui::ItemId id,uint64_t revision,const std::string &query,
    bool sensitive,bool words,size_t limit,const Cancelled &cancelled) {
    PagedTextSearch result;
    if (query.empty() || query.size()>4096) return result;
    uint64_t at=0,base=0,processed=0,lastMatchEnd=0;
    std::string window; Cursor cursor;
    const auto head=readBytes(id,revision,0,4).text;
    if (head.starts_with("\xef\xbb\xbf")) at=base=processed=3;
    while (at<current_.bytes || !window.empty()) {
        if (cancelled()) return {};
        if (at<current_.bytes) { auto page=readBytes(id,revision,at,chunkBytes); window+=page.text;at=page.nextOffset; }
        const bool final=at==current_.bytes;
        size_t safe=final?window.size():window.size()>query.size()+8?window.size()-query.size()-8:0;
        while (safe && safe<window.size() && (static_cast<unsigned char>(window[safe])&0xc0)==0x80) --safe;
        if (!safe) { if (final) break; continue; }
        const auto matches=text::findTextMatches(window,query,sensitive,words,limit+1,size_t(std::max(processed,lastMatchEnd)-base));
        for (const auto &match:matches.matches) {
            if (base+match.offset<processed || match.offset>=safe) continue;
            const auto finish=match.offset+match.length;
            if((match.offset && window[match.offset]=='\n' && window[match.offset-1]=='\r') ||
               (finish<window.size() && finish && window[finish-1]=='\r' && window[finish]=='\n'))
                throw std::runtime_error("Search must include the complete CRLF pair.");
            if (result.matches.size()==limit) { result.truncated=true; break; }
            Cursor begin=cursor; begin.advance(std::string_view(window).substr(0,match.offset));
            Cursor end=begin; end.advance(std::string_view(window).substr(match.offset,match.length));
            auto length=std::min<size_t>(512,window.size()-match.offset);
            while (length && match.offset+length<window.size() && (static_cast<unsigned char>(window[match.offset+length])&0xc0)==0x80) --length;
            auto preview=window.substr(match.offset,length);
            if (const auto eol=preview.find_first_of("\r\n");eol!=std::string::npos) preview.resize(eol);
            result.matches.push_back({base+match.offset,match.length,{begin.position,end.position},std::move(preview)});
            lastMatchEnd=base+match.offset+match.length;
        }
        if (result.truncated || final) break;
        processed=base+safe; auto drop=safe>4?safe-4:0;
        while (drop && (static_cast<unsigned char>(window[drop])&0xc0)==0x80) --drop;
        cursor.advance(std::string_view(window).substr(0,drop)); window.erase(0,drop); base+=drop;
    }
    // Validate even an empty result against the live revision before publication.
    auto args=parameters(id,revision);args["length"]=4;engine_->dispatch("documents.readPage",args);
    return result;
}
PagedTextStatus EnginePagedBackend::save(cyui::ItemId id,uint64_t revision,const std::filesystem::path &destination) {
    auto args=parameters(id,revision);
    if (!destination.empty()) args["destination"]=pathUtf8(destination);
    const auto info=engine_->dispatch(destination.empty()?"documents.savePaged":"documents.savePagedAs",args);
    // Never turn a completed publication into a rejected edit on a metadata refresh error.
    current_=describe(info,false);
    try{history();}catch(const std::exception &){current_.canUndo=current_.canRedo=false;}
    return current_;
}
PagedTextStatus EnginePagedBackend::checkpoint(cyui::ItemId id,uint64_t revision) {
    const auto result=engine_->dispatch("documents.checkpointPaged",parameters(id,revision));
    current_.message="Checkpoint published: "+result.at("checkpoint").get<std::string>(); return current_;
}
}
