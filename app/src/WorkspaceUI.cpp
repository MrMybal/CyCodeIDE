#include "cycode/Application.h"
namespace cycode {
void Application::updateWorkspace() {
    if (extensionsRefreshFuture_.valid() &&
        extensionsRefreshFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            pluginInfo_ = extensionsRefreshFuture_.get();
            synchronizeExtensions();
            refreshGit();
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (workspacePollFuture_.valid() &&
        workspacePollFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            const auto state = workspacePollFuture_.get();
            recoveryInfo_["status"]=state.at("recovery");
            const auto recoveryError=state.at("recovery").at("errors").dump();
            if(recoveryError!="[]"&&recoveryError!=recoveryHealthError_) {
                error(tr("Recovery checkpoints could not be saved. Open Recovery for details."));
                showPanel("Recovery","recovery");
            }
            recoveryHealthError_=recoveryError;
            if (!extensionsRefreshFuture_.valid() &&
                state.at("pluginsRevision") != pluginInfo_.value("revision", uint64_t(0))) {
                auto engine = engine_;
                extensionsRefreshFuture_ =
                    std::async(std::launch::async, [engine] { return engine->dispatch("plugins.list"); });
            }
            if (state.at("indexRevision") != workspaceIndexRevision_ && !filesFuture_.valid())
                refreshFiles(false);
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (!smoke_ && !testSession_ && !workspacePollFuture_.valid() &&
        ImGui::GetTime() - lastWorkspaceCheck_ > .5) {
        lastWorkspaceCheck_ = ImGui::GetTime();
        auto engine = engine_;
        workspacePollFuture_ =
            std::async(std::launch::async, [engine] { return engine->dispatch("workspace.poll"); });
    }
}
} // namespace cycode
