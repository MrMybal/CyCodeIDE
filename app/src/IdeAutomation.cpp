#include "cycode/IdeAutomation.h"
#include <cycode/AtomicFile.h>
#include <cycode/NativeFileLease.h>
#include <algorithm>
#include <fstream>

namespace cycode {
struct IdeAutomation::Impl {
    struct Request {
        std::string method;
        Json arguments;
        cymcp::CallContext context;
        std::promise<Json> result;
    };
    std::mutex mutex;
    std::deque<std::shared_ptr<Request>> requests;
    std::atomic_bool stopping{false};
    std::filesystem::path file, history;
    std::unique_ptr<NativeFileLease> slotLease;
    std::unique_ptr<automation::Host> host;
    std::unique_ptr<cymcp::McpServer> mcp;
    std::unique_ptr<cymcp::LocalHttpServer> reader, editor;
    Json connection;
    bool published=false;

    Json enqueue(const std::string& method, const Json& arguments, const cymcp::CallContext& context) {
        auto request=std::make_shared<Request>();
        request->method=method;request->arguments=arguments;request->context=context;
        auto result=request->result.get_future();
        {
            std::lock_guard lock(mutex);
            if(stopping)throw cymcp::Error("Unavailable","The IDE instance is closing");
            if(requests.size()>=128)throw cymcp::Error("Busy","The IDE command queue is full");
            requests.push_back(request);
        }
        // Cancellation is checked by the UI immediately before execution. Once
        // execution starts, return its actual receipt, even if cancellation races.
        return result.get();
    }
    void stop() {
        if(stopping.exchange(true))return;
        std::deque<std::shared_ptr<Request>> pending;
        {std::lock_guard lock(mutex);pending.swap(requests);}
        for(auto& request:pending)request->result.set_exception(std::make_exception_ptr(
            cymcp::Error("Unavailable","The IDE instance is closing; the action was not executed")));
        if(mcp)mcp->cancel_all();
        if(host)host->stop();
        if(reader)reader->stop();
        if(editor)editor->stop();
        if(published)try {
            std::error_code ignored;
            if(!std::filesystem::is_symlink(file,ignored) && std::filesystem::is_regular_file(file,ignored) && std::filesystem::file_size(file,ignored)<=65536) {
                std::ifstream input(file,std::ios::binary);
                const auto existing=Json::parse(input,nullptr,false);
                if(existing==connection)std::filesystem::remove(file,ignored);
            }
        }catch(...){/* Preserve a changed descriptor rather than removing another owner's file. */}
    }
    ~Impl(){stop();}
};

IdeAutomation::IdeAutomation(const std::filesystem::path& nativeDescriptor,
    const std::filesystem::path& catalogue,const std::filesystem::path& state,
    const std::filesystem::path& descriptor,const Json& preferences):impl_(std::make_unique<Impl>()) {
    auto& runtime=*impl_;runtime.file=descriptor;
    cymcp::private_directory(state);
    // A stable free slot preserves completed jobs/history across restarts while
    // independent windows never share a live scheduler or conversation file.
    std::filesystem::path selected;
    for(unsigned slot=0;slot<64;++slot) {
        auto candidate=state/"slots"/std::to_string(slot);cymcp::private_directory(candidate);
        auto lease=std::make_unique<NativeFileLease>(candidate/"window.lock");
        if(lease->acquired()){selected=candidate;runtime.slotLease=std::move(lease);break;}
    }
    if(selected.empty())throw cymcp::Error("Busy","All IDE automation slots have a live owner");
    runtime.history=selected/"conversations";
    automation::Options options;options.state=selected/"jobs";options.catalogue=catalogue;
    options.profile="compact";
    options.gateway.inline_bytes=std::clamp(preferences.value("inlineBytes",16384),1024,1024*1024);
    options.gateway.resources.result_ttl=std::chrono::seconds(std::clamp(preferences.value("resultTtlSeconds",300),30,86400));
    options.cacheTtl=std::chrono::milliseconds(std::clamp(preferences.value("cacheTtlMs",30000),0,300000));
    auto size=[&](const char* key,std::size_t value,std::size_t low,std::size_t high){return std::clamp(preferences.value(key,value),low,high);};
    options.gateway.resources.chunk_bytes=size("chunkBytes",32768,1024,1024*1024);
    options.gateway.resources.max_stored_bytes=size("resultMemoryBytes",64ULL*1024*1024,1024*1024,1024ULL*1024*1024);
    options.gateway.resources.max_disk_bytes=size("resultDiskBytes",1024ULL*1024*1024,16ULL*1024*1024,8ULL*1024*1024*1024);
    options.jobDiskBytes=size("jobDiskBytes",1024ULL*1024*1024,16ULL*1024*1024,8ULL*1024*1024*1024);
    options.jobRamBytes=size("jobRamBytes",512ULL*1024*1024,16ULL*1024*1024,8ULL*1024*1024*1024);
    options.workers=size("workers",4,1,8);options.queueCapacity=size("queueCapacity",128,1,1024);
    options.notifications=preferences.value("notifications",false);
    if(preferences.value("diskResults",false))options.spoolDirectory=selected/"results";
    if(preferences.value("telemetry",false))options.telemetryFile=selected/"performance.jsonl";
    auto native=automation::nativeEndpoint(nativeDescriptor);
    runtime.host=std::make_unique<automation::Host>([this,native](const std::string& method,const Json& p,const cymcp::CallContext& c){
        if(method.starts_with("ide."))return impl_->enqueue(method,p,c);
        return native(method,p,c);
    },options);
    runtime.host->addPrincipal("ide-read",{});
    permissions(false,false);
    runtime.mcp=runtime.host->mcpServer();
    auto endpoint=[this](const std::string& role){
        return [this,role](const Json& input,const cymcp::CallContext& incoming)->Json {
            auto actor=incoming;actor.principal=role;
            if(input.contains("jsonrpc"))return impl_->mcp->handle(input,actor);
            try {
                return {{"protocolVersion","1.0"},{"id",input.value("id",Json(nullptr))},
                    {"result",impl_->host->legacy(input.at("method"),input.value("params",Json::object()),actor)}};
            }catch(const cymcp::Error& error){
                auto failure=Json::parse(error.what(),nullptr,false);
                if(!failure.is_object() || !failure.contains("code") || !failure.contains("message"))
                    failure={{"code",error.code},{"message",error.what()}};
                return {{"id",input.value("id",Json(nullptr))},{"error",failure}};
            }catch(const std::exception&){
                return {{"id",input.value("id",Json(nullptr))},{"error",{{"code","InvalidRequest"},{"message","The automation request failed"}}}};
            }
        };
    };
    const auto readToken=cymcp::random_token(32),editToken=cymcp::random_token(32);
    runtime.reader=std::make_unique<cymcp::LocalHttpServer>(readToken,endpoint("ide-read"),4);
    runtime.editor=std::make_unique<cymcp::LocalHttpServer>(editToken,endpoint("ide-edit"),4);
    const auto read="http://127.0.0.1:"+std::to_string(runtime.reader->start());
    const auto edit="http://127.0.0.1:"+std::to_string(runtime.editor->start());
    runtime.connection={{"protocol","cycode-core/1"},{"sdk","CyMCPSDK"},{"application","CyCodeIDE"},
        {"url",read+"/v1"},{"readUrl",read+"/v1"},{"editUrl",edit+"/v1"},
        {"mcpReadUrl",read+"/mcp"},{"mcpEditUrl",edit+"/mcp"},{"readToken",readToken},{"editToken",editToken}};
    AtomicFileOptions privateFile;privateFile.access=AtomicAccess::PrivateUser;
    writeFileAtomic(descriptor,runtime.connection.dump(2),privateFile);
    runtime.published=true;
}
IdeAutomation::~IdeAutomation()=default;
void IdeAutomation::stop(){impl_->stop();}
void IdeAutomation::permissions(bool edit,bool settings) {
    std::set<std::string> rights;
    if(edit)rights={"workspace.write","vcs.write","process.execute","plugins.execute"};
    if(settings)rights.insert("settings.write");
    impl_->host->addPrincipal("ide-edit",rights);impl_->host->addPrincipal("local",rights);
}
void IdeAutomation::drain(const std::function<Json(const std::string&,const Json&)>& handler) {
    for(unsigned count=0;count<8;++count) {
        std::shared_ptr<Impl::Request> request;
        {std::lock_guard lock(impl_->mutex);if(impl_->requests.empty())break;request=impl_->requests.front();impl_->requests.pop_front();}
        try{request->context.cancellation.check();request->result.set_value(handler(request->method,request->arguments));}
        catch(...){request->result.set_exception(std::current_exception());}
    }
}
std::shared_ptr<cymcp::Registry> IdeAutomation::registry() const{return impl_->host->registry();}
std::shared_ptr<cymcp::ResourceRegistry> IdeAutomation::resources() const{return impl_->host->resources();}
Json IdeAutomation::descriptor() const{return impl_->connection;}
const std::filesystem::path& IdeAutomation::historyDirectory() const{return impl_->history;}
} // namespace cycode
