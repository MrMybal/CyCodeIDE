#include "../src/EnginePagedBackend.h"
#include <cycode/TextFile.h>
#include <algorithm>
#include <fstream>
#include <iostream>

using namespace cycode;
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
template<class F> static void refused(F operation,const char *message){bool failed=false;try{operation();}catch(const std::exception &){failed=true;}require(failed,message);}
static std::string disk(const std::filesystem::path &file,uint64_t at,size_t bytes){
    std::ifstream input(file,std::ios::binary);input.seekg(std::streamoff(at));std::string text(bytes,'\0');input.read(text.data(),std::streamsize(bytes));text.resize(size_t(input.gcount()));return text;
}
static PagedTextCommit edit(EnginePagedBackend &backend,cyui::ItemId id,uint64_t revision,
    cyui::PagedCodeSelection selection,const std::string &text){
    auto prepared=backend.prepare({1,id,revision,selection,text});
    auto result=backend.apply(prepared);backend.discard(prepared);return result;
}
int main(int argc,char **argv){
    try{
        if(argc!=2)throw std::runtime_error("Pass a new, empty synthetic fixture directory");
        const auto folder=std::filesystem::absolute(utf8Path(argv[1]));
        require(!std::filesystem::exists(folder),"Preserve existing fixture data; use a new directory");
        std::filesystem::create_directories(folder);
        const auto file=folder/"Huge.log";
        constexpr uint64_t longLine=128*1024,total=96*1024*1024;
        const auto prefix=std::string("\xef\xbb\xbf")+std::string(longLine,'a')+"\xf0\x9f\x9b\xa0Z\r\nbefore\r\n";
        {std::ofstream output(file,std::ios::binary);output.write(prefix.data(),std::streamsize(prefix.size()));
            uint64_t written=prefix.size();const auto block=std::string(65534,'x')+"\r\n";
            while(written<total-6){const auto count=size_t(std::min<uint64_t>(block.size(),total-6-written));output.write(block.data(),std::streamsize(count));written+=count;}
            output<<"\r\nTAIL";require(bool(output),"Cannot create synthetic 96 MiB source");}
        auto engine=std::make_shared<Engine>(folder);
        const auto opened=engine->dispatch("documents.readPage",{{"path","Huge.log"},{"length",4}});
        const auto id=opened.at("id").get<uint64_t>();
        EnginePagedBackend backend(engine,id,"Huge.log");
        auto state=backend.state(id);
        require(state.bytes==total && state.newline=="\r\n" && !state.dirty,"Long-first-line CRLF import metadata is wrong");
        require(backend.offset(id,state.revision,{0,0})==3,"BOM became a UTF-16 cursor character");
        require(backend.offset(id,state.revision,{0,longLine+2})==3+longLine+4,"Supplementary UTF-16 mapping is wrong");
        const auto original=state.revision;
        auto changed=edit(backend,id,state.revision,{{0,longLine},{1,3}},"\xc3\xa9\n\xce\xa9");
        state=changed.status;
        require(state.dirty && state.bytes==total-4 && changed.selection.caret==cyui::PagedCodePosition{1,1},"Cross-page edit was not committed once with native metadata");
        require(std::filesystem::file_size(file)==total,"Native edit implicitly saved the source");
        refused([&]{backend.readBytes(id,original,0,4);},"Stale native page was accepted");
        changed=edit(backend,id,state.revision,{{1,1},{1,1}},"!");state=changed.status;
        require(engine->dispatch("documents.readPage",{{"path","Huge.log"},{"offset",3+longLine},{"length",32},{"expectedRevision",state.revision},{"expectedDocumentId",id}}).at("text").get<std::string>().starts_with("\xc3\xa9\r\n\xce\xa9!ore\r\n"),"Agent does not observe GUI backend edits in the same Engine");
        state=backend.undo(id,state.revision,false).status;
        state=backend.undo(id,state.revision,false).status;
        require(backend.readBytes(id,state.revision,3+longLine,32).text.starts_with("\xf0\x9f\x9b\xa0Z\r\nbefore"),"Multi-step native Undo lost the original bytes");
        state=backend.undo(id,state.revision,true).status;
        state=backend.undo(id,state.revision,true).status;
        const auto matches=backend.search(id,state.revision,"ore",true,true,2000,[]{return false;});
        require(!matches.matches.empty() && matches.matches[0].range.first()==cyui::PagedCodePosition{1,2},"Search byte/UTF-16 coordinates are inconsistent");
        require(backend.copy(id,state.revision,{{1,0},{1,5}},4*1024*1024)=="\xce\xa9!ore","Clipboard selection did not preserve UTF-8");
        state=backend.save(id,state.revision,{});
        require(!state.dirty && std::filesystem::file_size(file)==state.bytes,"Save published only a page");
        require(disk(file,0,3)=="\xef\xbb\xbf" && disk(file,state.bytes-4,4)=="TAIL","Full save lost BOM or distant tail");
        backend.checkpoint(id,state.revision);
        const auto records=engine->dispatch("documents.pagedCheckpoints").at("checkpoints");
        require(records.size()==1,"Explicit native checkpoint was not discoverable");
        const auto checkpointId=records[0].at("checkpoint");
        state=edit(backend,id,state.revision,{{1,0},{1,1}},"Q").status;
        const auto restored=engine->dispatch("documents.restorePagedCheckpoint",{{"path","Huge.log"},{"checkpoint",checkpointId},{"expectedRevision",state.revision}});
        const auto preview=engine->dispatch("changes.preview",{{"id",restored.at("id")},{"path","Huge.log"},{"length",65536}});
        require(preview.at("hasMore")==true && preview.at("after").get<std::string>().size()<=65536,"Recovery preview materialized the entire source");
        engine->dispatch("changes.apply",{{"id",restored.at("id")}});state=backend.state(id);
        require(backend.copy(id,state.revision,{{1,0},{1,1}},32)=="\xce\xa9","Recovery did not restore the native checkpoint");
        state=backend.undo(id,state.revision,false).status;
        require(backend.copy(id,state.revision,{{1,0},{1,1}},32)=="Q","Recovery Undo lost the live preimage");
        const auto beforeCopy=std::filesystem::file_size(file);
        state=backend.save(id,state.revision,utf8Path("Copy.log"));
        require(pathUtf8(state.path)=="Copy.log" && std::filesystem::file_size(folder/"Copy.log")==state.bytes,"Save As did not rebind the complete native document");
        require(std::filesystem::file_size(file)==beforeCopy,"Save As changed the original source");
        refused([&]{backend.save(id,state.revision,utf8Path("Huge.log"));},"Save As overwrote an existing source");
        const auto current=engine->dispatch("documents.readPage",{{"path","Copy.log"},{"length",4},{"expectedDocumentId",id}});
        require(current.at("id")==id,"Save As replaced native identity");
        {std::ofstream output(folder/"Copy.log",std::ios::binary|std::ios::app);output<<"external";}
        refused([&]{edit(backend,id,state.revision,{{1,0},{1,1}},"R");},"Disk conflict allowed a GUI edit");
        require(backend.readBytes(id,state.revision,3+longLine,16).text.starts_with("\xc3\xa9\r\nQ!ore"),"Rejected edit changed live bytes");
        engine->dispatch("documents.closePaged",{{"path","Copy.log"},{"expectedRevision",state.revision},{"discard",true}});
        refused([&]{backend.state(id);},"Closed native source was silently reopened by its view");
        const auto reopened=engine->dispatch("documents.readPage",{{"path","Copy.log"},{"length",4}});
        require(reopened.at("id")!=id,"Reopened native source reused an old identity");
        refused([&]{backend.readBytes(id,state.revision,0,4);},"Old view accepted a replacement source");
        std::cout<<"Engine paged backend integration passed\n";return 0;
    }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
