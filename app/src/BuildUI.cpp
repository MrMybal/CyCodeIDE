#include "cycode/Application.h"

namespace cycode {
void Application::cancelBuild() {
    if (unrealAnalysisCancellation_ && unrealAnalysisFuture_.valid())
        *unrealAnalysisCancellation_ = true;
    if (taskFuture_.valid() && BuildOperations::supports(taskMethod_))
        buildCancellationId_ = taskOperationId_;
    else
        buildCancellationId_ = engine_->dispatch("build.status").value("operationId", std::string{});
    updateBuildCancellation();
}
void Application::updateBuildCancellation() {
    if (buildCancellationId_.empty())
        return;
    // Retry the same ID while an async GUI task is entering its reservation.
    // Never cancel a different operation that happens to start afterwards.
    engine_->dispatch("build.cancel", {{"operationId", buildCancellationId_}});
    const auto status = engine_->dispatch("build.status");
    if (!(taskFuture_.valid() && taskOperationId_ == buildCancellationId_) &&
        (!status.value("running", false) || status.at("operationId") != buildCancellationId_))
        buildCancellationId_.clear();
}
void Application::liveBuildOutput() {
    bool changed = ImGui::RadioButton(tr("Native build output"), &liveBuildSource_, 0);
    ImGui::SameLine();
    changed = ImGui::RadioButton(tr("Project task output"), &liveBuildSource_, 1) || changed;
    if (changed) {
        liveBuildState_ = Json::object();
        liveBuildText_.clear();
        liveBuildLog_.synchronize(liveBuildText_);
        lastLiveBuildPoll_ = -1;
    }
    const auto now = ImGui::GetTime();
    if (now - lastLiveBuildPoll_ >= .25) {
        lastLiveBuildPoll_ = now;
        Json parameters = Json::object();
        if (!liveBuildState_.value("streamId", std::string{}).empty())
            parameters = {{"streamId", liveBuildState_.at("streamId")},
                          {"revision", liveBuildState_.at("revision")}};
        try {
            auto state = engine_->dispatch(liveBuildSource_ ? "project.output" : "build.output", parameters);
            if (state.at("changed").get<bool>()) {
                liveBuildText_ = state.at("output");
                liveBuildLog_.synchronize(liveBuildText_);
            }
            state.erase("output");
            liveBuildState_ = std::move(state);
            liveBuildError_.clear();
        } catch (const std::exception &e) {
            liveBuildError_ = e.what();
        }
    }
    if (liveBuildState_.value("streamId", std::string{}).empty() && liveBuildError_.empty())
        return;
    if (!ImGui::CollapsingHeader(tr("Live build output"), ImGuiTreeNodeFlags_DefaultOpen))
        return;
    if (!liveBuildError_.empty())
        ImGui::TextWrapped("%s", liveBuildError_.c_str());
    if (!liveBuildState_.empty()) {
        cyui::textDim("%s | %s | %s", liveBuildState_.at("method").get_ref<const std::string &>().c_str(),
                      liveBuildState_.at("operationId").get_ref<const std::string &>().c_str(),
                      tr(liveBuildState_.value("cancelling", false) ? "Cancelling task..."
                         : liveBuildState_.value("running", false)  ? "Running"
                                                                    : "Finished"));
        if (liveBuildState_.contains("context")) {
            const auto &context = liveBuildState_.at("context");
            cyui::textDim("%s | %s", context.value("system", std::string{}).c_str(),
                          context.value("directory", std::string{}).c_str());
        }
        if (liveBuildState_.value("droppedBytes", uint64_t(0)) || liveBuildState_.value("incomplete", false))
            cyui::textColoured(cyui::palette().warning, "%s",
                               tr("Only the latest build messages are retained here."));
    }
    if (cyui::ghostButton(tr("Copy live build output")))
        ImGui::SetClipboardText(liveBuildText_.c_str());
    ImGui::SameLine();
    cyui::textDim("%s", tr("The final result is returned to the build caller."));
    const auto context = liveBuildState_.value("context", Json::object());
    navigableOutput("##live-build-output", liveBuildLog_, context.value("directory", std::string(".")), {0, cyui::dp(190)});
}
void Application::navigableOutput(const char *id, OutputLog &log, const std::string &directory, ImVec2 size) {
    const auto clicked = cyui::logView(id, log.state, log.buffer, size);
    if (!clicked) return;
    for (const auto &row : log.state.rows) if (row.id == clicked) {
        if (row.text.size() > 16384) return;
        try {
            const auto location = engine_->dispatch("build.location", {{"text", row.text}, {"directory", directory}});
            if (location.at("mapped").get<bool>())
                open(location.at("path"), location.at("line"), location.at("column"));
        } catch (const std::exception &) { /* Selecting an ordinary log line stays a log selection. */ }
        break;
    }
}
void Application::cancelProjectTask() {
    projectCancellationId_ = taskFuture_.valid() && taskMethod_.starts_with("project.")
                                 ? taskOperationId_
                                 : engine_->dispatch("project.status").value("operationId", std::string{});
    updateProjectCancellation();
}
void Application::updateProjectCancellation() {
    if (projectCancellationId_.empty())
        return;
    engine_->dispatch("project.cancel", {{"operationId", projectCancellationId_}});
    const auto state = engine_->dispatch("project.status");
    if (!(taskFuture_.valid() && taskOperationId_ == projectCancellationId_) &&
        (!state.value("running", false) || state.at("operationId") != projectCancellationId_))
        projectCancellationId_.clear();
}
} // namespace cycode
