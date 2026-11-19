#include "cycode/Application.h"
#include "cycode/AssistantState.h"
#include "cycode/IdeAutomation.h"
#include <cycode/AtomicFile.h>
#include <cyui/Core.h>
#include <imgui_stdlib.h>
#include <fstream>
#include <cstdlib>
#include <algorithm>

namespace cycode {
namespace {
void saveAssistantPreferences(AssistantState& state) {
    const Json value={{"provider",state.provider},{"model",state.model},{"baseUrl",state.baseUrl},
        {"executable",state.executable},{"apiKeyEnvironment",state.apiKeyEnvironment},
        {"historyId",state.historyId},{"contextBytes",state.contextBytes},{"deferredTools",state.deferredTools}};
    AtomicFileOptions options;options.access=AtomicAccess::PrivateUser;options.publication=AtomicPublication::Replace;
    writeFileAtomic(state.preferences,value.dump(2),options);
}
}
void Application::startAssistant() {
    if(smoke_ || (testSession_ && !testAssistant_.is_object()))return;
    try {
        // The native bridge identifies this Engine, including unsaved buffers.
        const auto key=layoutKey_.empty()?std::to_string(std::hash<std::string>{}(pathUtf8(engine_->workspace()))):layoutKey_;
        const auto state=root_/"data/automation"/key;
        automation_=std::make_unique<IdeAutomation>(bridge_->descriptor(),root_/"app/IDEAutomationMethods.json",
            state,coreDescriptor_,settings_.value("automation",Json::object()));
        assistant_=std::make_unique<AssistantState>();
        auto& assistant=*assistant_;assistant.preferences=automation_->historyDirectory().parent_path()/"assistant-settings.json";
        assistant.history=automation_->historyDirectory();
        if(std::filesystem::is_regular_file(assistant.preferences) && std::filesystem::file_size(assistant.preferences)<=65536) {
            std::ifstream in(assistant.preferences);const auto value=Json::parse(in);
            assistant.provider=value.value("provider",assistant.provider);assistant.model=value.value("model","");
            assistant.baseUrl=value.value("baseUrl","");assistant.executable=value.value("executable","");
            assistant.apiKeyEnvironment=value.value("apiKeyEnvironment","");assistant.historyId=value.value("historyId","");
            assistant.contextBytes=std::clamp(value.value("contextBytes",0),0,16*1024*1024);
            assistant.deferredTools=value.value("deferredTools",true);
        }
        // SDK history IDs are opaque filenames, never workspace/provider paths.
        if(assistant.historyId.empty() || assistant.historyId.size()>64 ||
            assistant.historyId.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)
            assistant.historyId=cymcp::random_token(16);
        cymcp::ChatPanelOptions options;
        options.pick_attachments=[this,dialog=assistant.attachmentDialog]{
            auto* reference=new std::shared_ptr<AssistantState::AttachmentDialog>(dialog);
            SDL_ShowOpenFileDialog([](void* data,const char* const* paths,int){
                std::unique_ptr<std::shared_ptr<AssistantState::AttachmentDialog>> reference(
                    static_cast<std::shared_ptr<AssistantState::AttachmentDialog>*>(data));
                std::lock_guard lock((*reference)->mutex);(*reference)->paths.clear();
                if(paths)for(std::size_t i=0;paths[i] && i<16;++i)(*reference)->paths.push_back(utf8Path(paths[i]));
                (*reference)->ready=true;
            },reference,window_,nullptr,0,nullptr,true);
            return std::vector<std::filesystem::path>{};
        };
        options.open_settings=[this]{if(assistant_)assistant_->settingsOpen=true;};
        options.open_link=[](const std::string& url){
            if(url.starts_with("https://") || url.starts_with("http://"))SDL_OpenURL(url.c_str());
        };
        options.edit_message=[this](std::uint64_t id,const std::string& text){assistant_->chat.edit_message(id,text);};
        options.delete_message=[this](std::uint64_t id){assistant_->chat.delete_turn(id);};
        options.require_editable_history=true;
        assistant.panel=std::make_unique<cymcp::ChatPanel>(assistant.chat,std::move(options));
        mcpWrite_=false;automation_->permissions(false,false);
    }catch(const std::exception& exception){automationError_=exception.what();assistant_.reset();}
}
void Application::stopAssistant() {
    // Release queued UI calls before joining a provider worker that may be waiting on one.
    if(assistant_){assistant_->discoveryCancellation.cancel();assistant_->chat.cancel();}
    if(automation_)automation_->stop();
    assistant_.reset();automation_.reset();
}
void Application::updateAssistant() {
    if(automation_) {
        automation_->permissions(mcpWrite_,assistant_ && assistant_->settingsWrite);
        automation_->drain([this](const std::string& method,const Json& arguments){return automationUi(method,arguments);});
    }
    if(!assistant_)return;
    auto& assistant=*assistant_;assistant.chat.poll();
    {
        std::lock_guard lock(assistant.attachmentDialog->mutex);
        if(assistant.attachmentDialog->ready){assistant.panel->add_attachments(std::move(assistant.attachmentDialog->paths));assistant.attachmentDialog->ready=false;}
    }
    if(testAssistant_.is_object()) {
        panels_["assistant"]=true;
        auto& view=assistant.chat.state();
        if(assistantTestPhase_==0) {
            assistant.provider=testAssistant_.value("provider",std::string("openai-compatible"));
            assistant.baseUrl=testAssistant_.value("baseUrl",std::string());
            assistant.model=testAssistant_.value("model",std::string("fixture"));
            assistant.apiKeyEnvironment.clear();assistant.executable.clear();
            mcpWrite_=testAssistant_.value("edit",false);
            connectAssistant(testAssistant_.value("restore",false));
            assistantTestPhase_=1;panels_["assistant"]=true;
        }else if(assistantTestPhase_==1 && view.connected && !view.busy) {
            assistant.chat.send(testAssistant_.value("prompt",std::string("Inspect this IDE using its tools.")));
            assistantTestStarted_=ImGui::GetTime();assistantTestPhase_=2;
        }else if(assistantTestPhase_==2 && !view.busy)assistantTestPhase_=3;
        else if(assistantTestPhase_==2 && testAssistant_.contains("cancelAfterMs") &&
            (ImGui::GetTime()-assistantTestStarted_)*1000>testAssistant_.at("cancelAfterMs").get<double>())assistant.chat.cancel();
        if(!view.error_code.empty() || !assistant.error.empty())assistantTestPhase_=3;
    }
    if(assistant.discovery.valid() && assistant.discovery.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try{assistant.connections=assistant.discovery.get();assistant.error.clear();}
        catch(const std::exception& exception){assistant.error=cymcp::redact_error_message(exception.what());}
    }
}
void Application::connectAssistant(bool restore) {
    if(!assistant_ || !automation_)return;
    auto& assistant=*assistant_;
    try {
        if(!restore)assistant.historyId=cymcp::random_token(16);
        cymcp::ChatConnection connection;connection.provider_id=assistant.provider;connection.model=assistant.model;
        connection.system_prompt="You assist the user of CyCodeIDE. Discover application tools before calling them. "
            "Use the shared document/index/transaction services. Preview guarded edits and save only on explicit request. "
            "Never retry a mutation to recover an expired result. Report unavailable compiler context honestly.";
        for(const auto& detected:assistant.connections)if(detected.available() && detected.provider_id==assistant.provider){connection.provider=detected.config;break;}
        if(!assistant.baseUrl.empty())connection.provider.settings["base_url"]=assistant.baseUrl;
        if(!assistant.executable.empty())connection.provider.process.executable=utf8Path(assistant.executable);
        if(!assistant.apiKeyEnvironment.empty()){
            const auto key=std::getenv(assistant.apiKeyEnvironment.c_str());
            if(!key)throw cymcp::Error("AuthenticationRequired","The selected environment variable is unavailable");
            connection.provider.api_key=key;
        }
        connection.provider.settings["deferred_tools"]=assistant.deferredTools;
        connection.provider.tools=automation_->registry();
        // Native-only CLI transports use this application's authenticated MCP endpoint.
        if(assistant.provider=="claude-code" || assistant.provider=="claude") {
            connection.provider.tools.reset();
            const auto descriptor=automation_->descriptor();
            connection.provider.settings["mcp_servers"]={{"cycodeide",{{"type","http"},
                {"url",descriptor.at(mcpWrite_?"mcpEditUrl":"mcpReadUrl")},
                {"headers",{{"Authorization","Bearer "+descriptor.at(mcpWrite_?"editToken":"readToken").get<std::string>()}}}}}};
        }
        connection.history_directory=assistant.history;connection.history_id=assistant.historyId;
        connection.restore=restore;connection.automatic_model=assistant.model.empty();
        connection.context.max_bytes=assistant.contextBytes;
        automation_->permissions(mcpWrite_,assistant.settingsWrite);
        saveAssistantPreferences(assistant);assistant.chat.open(std::move(connection));assistant.settingsOpen=false;assistant.error.clear();
    }catch(const std::exception& exception){assistant.error=cymcp::redact_error_message(exception.what());}
}
void Application::assistantPanel() {
    if(!beginPanel("Assistant","assistant","mcp"))return;
    if(!assistant_ || !automation_) {
        cyui::callout(cyui::Tone::warning,tr("Assistant unavailable"));
        if(!automationError_.empty())ImGui::TextWrapped("%s",automationError_.c_str());
        ImGui::End();return;
    }
    auto& assistant=*assistant_;
    if(assistant.settingsOpen) {
        cyui::sectionTitle(tr("Assistant connection"));
        ImGui::Checkbox(tr("Enable editing, builds and plugin commands"),&mcpWrite_);
        ImGui::Checkbox(tr("Allow editor preference changes"),&assistant.settingsWrite);
        if(cyui::button(tr("Detect providers")) && !assistant.discovery.valid()) {
            const auto providers=assistant.providers;assistant.discoveryCancellation=cymcp::Cancellation{};
            const auto cancellation=assistant.discoveryCancellation;
            assistant.discovery=std::async(std::launch::async,[providers,cancellation]{
                cymcp::CallContext context;context.cancellation=cancellation;return providers->discover({},context);
            });
        }
        if(assistant.discovery.valid()){ImGui::SameLine();cyui::textDim("%s",tr("Detecting providers..."));}
        for(const auto& detected:assistant.connections) {
            if(!detected.available())continue;
            if(ImGui::Selectable((detected.name+"###provider-"+detected.provider_id).c_str(),assistant.provider==detected.provider_id)) {
                assistant.provider=detected.provider_id;assistant.model.clear();assistant.baseUrl.clear();
                assistant.executable.clear();assistant.apiKeyEnvironment=detected.api_key_env;
            }
        }
        if(cyui::beginProperties("##assistant-provider-properties")) {
        cyui::property(tr("Provider"));
        if(ImGui::BeginCombo("##assistant-field-0",assistant.provider.c_str())) {
            for(const auto& entry:assistant.providers->describe()) {
                const auto id=entry.at("id").get<std::string>();
                if(ImGui::Selectable(entry.value("name",id).c_str(),assistant.provider==id))assistant.provider=id;
            }
            ImGui::EndCombo();
        }
        cyui::property(tr("Model"));
        ImGui::InputText("##assistant-field-1",&assistant.model);
        cyui::property(tr("Provider URL"));
        ImGui::InputText("##assistant-field-2",&assistant.baseUrl);
        cyui::property(tr("Provider executable"));
        ImGui::InputText("##assistant-field-3",&assistant.executable);
        cyui::property(tr("API key environment variable"));
        ImGui::InputText("##assistant-field-4",&assistant.apiKeyEnvironment);
        cyui::property(tr("Discover tools on demand"));
        ImGui::Checkbox("##assistant-field-5",&assistant.deferredTools);
        cyui::property(tr("API context window (bytes, 0 = unlimited)"));
        ImGui::InputInt("##assistant-field-6",&assistant.contextBytes);
        assistant.contextBytes=std::clamp(assistant.contextBytes,0,16*1024*1024);
        cyui::endProperties();
        }
        cyui::textDim("%s",tr("Native agents keep their own context. Conversation history stays complete."));
        const bool busy=assistant.chat.state().busy;
        {cyui::ScopedDisabled disabled(busy);
            if(cyui::button(tr("Connect / New conversation")))connectAssistant(false);
            ImGui::SameLine();if(cyui::button(tr("Restore conversation")))connectAssistant(true);
        }
        if(assistant.chat.state().connected){ImGui::SameLine();if(cyui::ghostButton(tr("Back to conversation")))assistant.settingsOpen=false;}
        if(!assistant.error.empty())cyui::callout(cyui::Tone::warning,assistant.error.c_str());
    }else assistant.panel->draw("##assistant-chat");
    ImGui::End();
}
} // namespace cycode
