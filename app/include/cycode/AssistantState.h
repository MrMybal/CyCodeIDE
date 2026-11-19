#pragma once
#include <cymcp/chat_ui.hpp>
#include <cymcp/mcp.hpp>

namespace cycode {
struct AssistantState {
    struct AttachmentDialog {std::mutex mutex;std::vector<std::filesystem::path> paths;bool ready=false;};
    std::shared_ptr<AttachmentDialog> attachmentDialog=std::make_shared<AttachmentDialog>();
    std::shared_ptr<cymcp::ProviderRegistry> providers=std::make_shared<cymcp::ProviderRegistry>();
    cymcp::ChatController chat{providers};
    std::unique_ptr<cymcp::ChatPanel> panel;
    std::vector<cymcp::DiscoveredConnection> connections;
    std::future<std::vector<cymcp::DiscoveredConnection>> discovery;
    cymcp::Cancellation discoveryCancellation;
    std::filesystem::path preferences,history;
    std::string provider="codex",model,baseUrl,executable,apiKeyEnvironment,historyId;
    std::string error;
    int contextBytes=0;
    bool settingsOpen=true,deferredTools=true;
    bool edit=false,settingsWrite=false;
};
} // namespace cycode
