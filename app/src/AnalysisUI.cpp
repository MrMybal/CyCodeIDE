#include "cycode/Application.h"
#include "ApplicationHelp.h"
#include <algorithm>
#include <imgui_stdlib.h>

namespace cycode {
namespace {
bool analysisReady(std::future<Json> &future) {
    return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
const char *analysisStateLabel(const std::string &state) {
    if (state == "running") return "Running";
    if (state == "pending") return "Ready for the next batch";
    if (state == "completed") return "Finished";
    if (state == "cancelled") return "Cancelled";
    if (state == "failed") return "Failed";
    return "Idle";
}
} // namespace

void Application::stopAnalysis() {
    // Only cancel operations issued by this view. Other clients share this Engine.
    while (analysisView_.prepare.valid() && !analysisReady(analysisView_.prepare)) {
        engine_->dispatch("analysis.cancel", {{"operationId", analysisView_.operation}});
        analysisView_.prepare.wait_for(std::chrono::milliseconds(20));
    }
    while (analysisView_.inspection.valid() && !analysisReady(analysisView_.inspection)) {
        engine_->dispatch("build.cancel", {{"operationId", analysisView_.inspectionOperation}});
        analysisView_.inspection.wait_for(std::chrono::milliseconds(20));
    }
    for (auto *future : {&analysisView_.prepare, &analysisView_.monitor, &analysisView_.metadata,
                         &analysisView_.inspection})
        if (future->valid()) {
            try { future->get(); } catch (const std::exception &) {}
        }
}

void Application::prepareAnalysis(Json parameters) {
    auto &view = analysisView_;
    if (smoke_ || testSession_ || view.prepare.valid()) return;
    // operationId is the native host's cancellation correlation, like build operations;
    // it is not an invented MCP method or a document revision.
    view.operation = instance_ + "-analysis-" + std::to_string(++view.serial);
    parameters["operationId"] = view.operation;
    view.error.clear(); view.cancel = false; view.lastPoll = -1;
    auto engine = engine_;
    view.prepare = std::async(std::launch::async, [engine, parameters] {
        return engine->dispatch("analysis.prepare", parameters);
    });
}

void Application::analysisMetadata(const std::string &method, Json parameters) {
    auto &view = analysisView_;
    if (view.metadata.valid()) return;
    view.metadataMethod = method; view.metadataError.clear();
    auto engine = engine_;
    view.metadata = std::async(std::launch::async, [engine, method, parameters] {
        return engine->dispatch(method, parameters);
    });
}

void Application::updateAnalysis() {
    auto &view = analysisView_;
    const bool prepared = analysisReady(view.prepare);
    if (prepared) {
        try {
            view.status = view.prepare.get();
            view.results = {{"session", view.status.at("session")},
                            {"documents", view.status.at("documents")}, {"offset", 0},
                            {"nextOffset", view.status.at("nextOffset")},
                            {"hasMore", view.status.at("nextOffset").get<size_t>() <
                                            view.status.at("completed").get<size_t>()}};
        } catch (const std::exception &e) { view.error = e.what(); }
        view.cancel = false; view.lastPoll = -1;
    }
    if (analysisReady(view.inspection)) {
        try {
            view.inspected = view.inspection.get();
            auto output = view.inspected.value("output", "");
            view.inspectionOutput->synchronize(output);
        }
        catch (const std::exception &e) { view.inspectionError = e.what(); }
        view.cancelInspection = false; view.lastPoll = -1;
    }
    if (analysisReady(view.metadata)) {
        try {
            auto result = view.metadata.get();
            if (view.metadataMethod == "analysis.checkpoints") view.checkpoints = std::move(result);
            else if (result.value("session", "") == view.status.value("session", ""))
                view.results = std::move(result);
        } catch (const std::exception &e) { view.metadataError = e.what(); }
    }
    if (analysisReady(view.monitor)) {
        try {
            const auto result = view.monitor.get();
            if (result.at("ticket") != view.serial) return;
            if (!prepared) view.status = result.at("analysis");
            view.monitorError.clear();
            view.process = result.value("process", Json::object());
            view.service = result.value("service", Json::object());
            if (view.results.value("session", "") != view.status.value("session", ""))
                view.results = Json::object();
        } catch (const std::exception &e) { view.monitorError = e.what(); }
    }
    if ((!panels_["analysis"] && !view.prepare.valid() && !view.inspection.valid()) ||
        view.monitor.valid() || ImGui::GetTime() - view.lastPoll < .25) return;
    view.lastPoll = ImGui::GetTime();
    auto engine = engine_;
    const auto analysisId = view.prepare.valid() ? view.operation : std::string{};
    const auto inspectId = view.inspection.valid() ? view.inspectionOperation : std::string{};
    const bool cancel = view.cancel, cancelInspection = view.cancelInspection;
    const auto ticket = view.serial;
    // stopAnalysis joins this read before setWorkspace/destruction releases the Bridge.
    auto *bridge = bridge_.get();
    view.monitor = std::async(std::launch::async, [engine, bridge, analysisId, inspectId, cancel, cancelInspection, ticket] {
        if (cancel && !analysisId.empty())
            engine->dispatch("analysis.cancel", {{"operationId", analysisId}});
        if (cancelInspection && !inspectId.empty())
            engine->dispatch("build.cancel", {{"operationId", inspectId}});
        Json result{{"ticket", ticket}, {"analysis", engine->dispatch("analysis.status", analysisId.empty() ? Json::object() : Json{{"operationId",analysisId}})}};
        result["service"] = bridge ? bridge->serviceStatus() : Json::object();
        if (!inspectId.empty()) result["process"] = engine->dispatch("build.status");
        return result;
    });
}

void Application::analysisPanel() {
    if (!beginPanel("Analysis and inspections", "analysis")) return;
    auto &view = analysisView_;
    if (ImGui::BeginTabBar("##analysis-sections")) {
        if (ImGui::BeginTabItem(tr("Analysis progress"))) {
            cyui::sectionTitle(tr("Service and language backends"));
            cyui::textDim("%s", tr(engine_ ? "Native Engine attached" : "Native Engine unavailable"));
            const auto service = view.service.value("service", Json::object());
            cyui::textDim("%s: %s", tr("Shared native service"), tr(service.value("state", "") == "ready"
                ? "Ready" : service.value("state", "") == "draining" ? "Stopping" : "Status pending"));
            if (cyui::beginSection(tr("Service details"), false)) {
                const auto context = view.service.value("engine", Json::object());
                cyui::textDim("%s: %s", tr("Shared Engine context"), context.value("context", "").c_str());
                cyui::textDim("%s: %s | %.1f s", tr("Core runtime version"), view.service.value("version", "").c_str(),
                              service.value("uptimeMilliseconds", 0.) / 1000.);
                if (cyui::ghostButton(tr("Connect agents to this Engine"))) showPanel("MCP", "mcp");
                cyui::endSection();
            }
            cyui::textDim("%s: %zu | %s", tr("File inventory"), files_.size(),
                          tr(filesIndexing_ ? "Indexing files..." : "Inventory currently available"));
            const Json empty = Json::object();
            const auto foundServers = languageState_.find("servers");
            const auto &servers = foundServers != languageState_.end() && foundServers->is_object() ? *foundServers : empty;
            for (const auto &[id, server] : servers.items()) {
                cyui::textDim("%s: %s", id.c_str(), tr(!server.value("enabled", false) ? "Disabled" :
                    server.value("ready", false) ? "Ready" : "Waiting for language server"));
            }
            for (const auto &tab : tabs_) if (tab->path == active_ && !tab->image) {
                const auto backend = engine_->languageBackendFor(tab->path);
                const auto found = servers.find(backend);
                const auto &server = found != servers.end() ? *found : backend == "cpp" ? languageState_ : empty;
                const auto publications = server.find("diagnosticRevisions");
                const auto &revisions = publications != server.end() && publications->is_object() ? *publications : empty;
                const auto published = revisions.find(tab->path);
                const auto &publication = published != revisions.end() ? *published : empty;
                const bool current = publication.value("revision", uint64_t(0)) == tab->revision &&
                    publication.value("id", uint64_t(0)) == tab->identity && tab->revision;
                cyui::textDim("%s: %s | %s", tr("Active file diagnostics"), tab->path.c_str(),
                              tr(current ? "Current revision" : "Not observed at the current revision"));
            }
            cyui::callout(cyui::Tone::neutral, tr("Inventory, active-file analysis and whole-project semantic coverage are different. Index completeness remains unknown."));
            if (cyui::beginProperties("##analysis-scope")) {
                cyui::property(tr("Scope"));
                const char *scopes[] = {tr("Active C++ file"), tr("Compilation database")};
                ImGui::Combo("##scope", &view.scope, scopes, 2);
                if (view.scope == 1) {
                    cyui::property(tr("Path prefix")); ImGui::InputText("##analysis-prefix", &view.prefix);
                    cyui::property(tr("Persist traversal checkpoint")); ImGui::Checkbox("##persist-analysis", &view.persist);
                }
                cyui::property(tr("Sources per batch"));
                ImGui::InputInt("##analysis-batch", &view.batch);
                view.batch = std::clamp(view.batch, 0, 4096);
                cyui::tooltip(tr("Zero inventories without compiling. Each next batch requires an explicit request."));
                cyui::endProperties();
            }
            {
                cyui::ScopedDisabled disabled(view.prepare.valid() || smoke_ || testSession_ ||
                    (view.scope == 0 && (active_.empty() || active_.find("://") != std::string::npos || isUntitledDocument(active_) || !LanguageService::supportsCpp(active_) || std::any_of(tabs_.begin(),tabs_.end(),[&](const auto &tab){return tab->path==active_ && tab->pagedProfile;}))));
                if (cyui::primaryButton(tr("Prepare selected scope"))) {
                    Json parameters{{"batchSize", view.batch}};
                    if (view.scope == 0) parameters["paths"] = Json::array({active_});
                    else {
                        parameters["scope"] = "compilation-database";
                        parameters["persist"] = view.persist;
                        if (!view.prefix.empty()) parameters["pathPrefix"] = view.prefix;
                    }
                    prepareAnalysis(std::move(parameters));
                }
            }
            ImGui::SameLine();
            const auto state = view.status.value("state", "idle");
            const auto session = view.status.value("session", "");
            {
                cyui::ScopedDisabled disabled(view.prepare.valid() || session.empty() ||
                    (state != "pending" && state != "cancelled"));
                if (cyui::button(tr("Resume next batch")))
                    prepareAnalysis({{"continuation", session}, {"batchSize", view.batch}});
            }
            ImGui::SameLine();
            {
                cyui::ScopedDisabled disabled(!view.prepare.valid() || view.cancel);
                if (cyui::button(tr("Pause analysis"))) { view.cancel = true; view.lastPoll = -1; }
            }
            cyui::textDim("%s", tr(analysisStateLabel(state)));
            if (!view.status.empty()) {
                const bool hasTotal=view.status.value("total",uint64_t(0))!=0;
                const float fraction=hasTotal?view.status.value("progress",0.f)/100.f:state=="running"?-1.f:state=="completed"?1.f:0.f;
                cyui::progressBar(fraction, {}, tr("Source traversal"));
                const auto coverage = view.status.value("coverage", Json::object());
                for (const auto &[key, label] : std::initializer_list<std::pair<const char *, const char *>>{
                        {"diagnosticClean", "Clean diagnostics"}, {"diagnosticErrors", "Diagnostics with errors"},
                        {"diagnosticUnknown", "Unknown diagnostics"}, {"remaining", "Unvisited sources"}})
                    cyui::textDim("%s: %llu", tr(label), static_cast<unsigned long long>(coverage.value(key, uint64_t(0))));
                cyui::textDim("%s: %llu | %s: %llu", tr("Historical observations"),
                              static_cast<unsigned long long>(view.status.value("historicalObserved", uint64_t(0))), tr("Current-pass observations"),
                              static_cast<unsigned long long>(view.status.value("currentPassObserved", view.status.value("completed", uint64_t(0)))));
                if (!view.status.value("currentPath", "").empty())
                    cyui::textDim("%s | %.0f ms", view.status.at("currentPath").get_ref<const std::string &>().c_str(),
                                  view.status.value("currentFileMilliseconds", 0.));
                if (view.status.value("historicalObserved", uint64_t(0)))
                    cyui::callout(cyui::Tone::warning, tr("Restored observations are historical and unknown for this pass. They do not authorize current edits."));
                if (!view.status.value("checkpoint", "").empty())
                    cyui::textDim("%s: %s", tr("Checkpoint"), view.status.at("checkpoint").get_ref<const std::string &>().c_str());
                for (const auto *key : {"error", "checkpointError"})
                    if (!view.status.value(key, "").empty()) cyui::callout(cyui::Tone::warning, view.status.at(key).get_ref<const std::string &>().c_str());
            }
            for (const auto *message : {&view.error, &view.monitorError, &view.metadataError})
                if (!message->empty()) cyui::callout(cyui::Tone::warning, message->c_str());
            {
                cyui::ScopedDisabled disabled(view.metadata.valid() || session.empty());
                if (cyui::button(tr("Refresh result page"))) analysisMetadata("analysis.results", {{"session", session}, {"offset", 0}, {"limit", 128}});
            }
            const Json emptyRows = Json::array();
            const auto resultRows = view.results.find("documents");
            const auto &rows = resultRows != view.results.end() ? *resultRows : emptyRows;
            if (!rows.empty()) {
                viewFilter("##analysis-filter", view.filter);
                if (ImGui::BeginTable("##analysis-rows", 5, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, cyui::dp(0, 220))) {
                    for (const auto *label : {"File", "Diagnostics", "Evidence", "Elapsed (ms)", "Error"}) ImGui::TableSetupColumn(tr(label));
                    ImGui::TableHeadersRow();
                    for (const auto &row : rows) {
                        const auto path = row.value("path", ""), message = row.value("error", "");
                        if (!view.filter.matches(path, message)) continue;
                        ImGui::PushID(path.c_str()); ImGui::TableNextRow(); ImGui::TableNextColumn();
                        if (cyui::ghostButton(tr("Open"))) open(path);
                        ImGui::SameLine(); ImGui::TextUnformatted(path.c_str());
                        ImGui::TableNextColumn(); ImGui::Text("%s (%llu)", tr(row.value("diagnostics", "unknown") == "clean" ? "Clean diagnostics" : row.value("diagnostics", "unknown") == "errors" ? "Diagnostics with errors" : "Unknown diagnostics"), static_cast<unsigned long long>(row.value("errors",uint64_t(0))));
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(tr(row.value("evidence", "") == "historical-checkpoint" ? "Historical observation" : "Current-pass observation"));
                        ImGui::TableNextColumn();
                        if (row.contains("elapsedMilliseconds")) ImGui::Text("%.0f", row.at("elapsedMilliseconds").get<double>());
                        else ImGui::TextUnformatted("-");
                        ImGui::TableNextColumn(); ImGui::TextWrapped("%s", message.c_str()); ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
                const auto offset = view.results.value("offset", size_t(0));
                {
                    cyui::ScopedDisabled disabled(view.metadata.valid() || offset == 0);
                    if (cyui::button(tr("Previous page"))) analysisMetadata("analysis.results", {{"session", session}, {"offset", offset > 128 ? offset - 128 : 0}, {"limit", 128}});
                }
                ImGui::SameLine();
                {
                    cyui::ScopedDisabled disabled(view.metadata.valid() || !view.results.value("hasMore", false));
                    if (cyui::button(tr("Next page"))) analysisMetadata("analysis.results", {{"session", session}, {"offset", view.results.at("nextOffset")}, {"limit", 128}});
                }
            }
            cyui::divider();
            { cyui::ScopedDisabled disabled(view.metadata.valid());
              if (cyui::button(tr("Discover analysis checkpoints"))) analysisMetadata("analysis.checkpoints"); }
            const auto checkpointRows = view.checkpoints.find("checkpoints");
            const auto &checkpoints = checkpointRows != view.checkpoints.end() ? *checkpointRows : emptyRows;
            for (const auto &checkpoint : checkpoints) {
                const auto id = checkpoint.at("checkpoint").get<std::string>();
                ImGui::PushID(id.c_str());
                cyui::ScopedDisabled disabled(view.prepare.valid());
                if (cyui::button(tr("Restore traversal"))) prepareAnalysis({{"checkpoint", id}, {"batchSize", 0}});
                ImGui::SameLine(); ImGui::TextUnformatted(id.c_str()); ImGui::PopID();
            }
            if (view.checkpoints.value("invalid", size_t(0)) || view.checkpoints.value("truncated", false))
                cyui::callout(cyui::Tone::warning, tr("Checkpoint discovery was limited or contains invalid records. No checkpoint was deleted."));
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(tr("C++ inspections"))) {
            inspectionPanel(); ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void Application::inspectionPanel() {
    auto &view = analysisView_;
    std::shared_ptr<Tab> source;
    for (const auto &tab : tabs_) if (tab->path == active_ && !tab->image) source = tab;
    const bool eligible = source && source->path.find("://") == std::string::npos &&
        !isUntitledDocument(source->path) && LanguageService::supportsCpp(source->path) && source->identity && source->revision && !source->dirty && source->diskConflict.empty() && !source->pagedClosed && (!source->pagedProfile || (source->paged && !source->paged->status().busy && !source->paged->unsettled()));
    cyui::callout(cyui::Tone::neutral, tr("Inspect one saved C++ translation unit with its real compilation command. No fixes are applied or sources saved."));
    if (source) ImGui::TextUnformatted(source->path.c_str());
    if (!eligible) cyui::callout(cyui::Tone::warning, tr("Open a saved C++ source and resolve unsaved changes or disk conflicts before inspection."));
    if (cyui::beginProperties("##inspection-inputs")) {
        cyui::property(tr("Compilation database")); ImGui::InputText("##inspection-database", &view.database);
        cyui::property(tr("Inspection profile"));
        const char *profiles[] = {tr("Recommended"), tr("Security"), tr("Modernize"), tr("Performance"), tr("Portability")};
        ImGui::Combo("##inspection-profile", &view.profile, profiles, 5);
        cyui::property(tr("clang-tidy executable")); ImGui::InputText("##inspection-executable", &view.executable);
        cyui::tooltip(tr("Leave empty to use Core tool discovery. A missing tool is reported explicitly."));
        cyui::property(tr("Timeout (seconds)")); ImGui::InputInt("##inspection-timeout", &view.timeout);
        view.timeout = std::clamp(view.timeout, 1, 900); cyui::endProperties();
    }
    auto configuredDatabase = buildState_.value("compilationDatabase", Json::object());
    if (!configuredDatabase.value("available", false))
        configuredDatabase = unreal_.value("compilationDatabase", Json::object());
    { cyui::ScopedDisabled disabled(view.inspection.valid() || !configuredDatabase.value("available", false));
      if (cyui::ghostButton(tr("Use configured compilation database")))
          view.database = configuredDatabase.value("path", ""); }
    if (cyui::ghostButton(tr("Inspection documentation"))) {
        try { app_help::open(root_, "app/Documentation", locale_, "core-quality", "inspections", smoke_ || testSession_); }
        catch (const std::exception &e) { error(e.what()); }
    }
    { cyui::ScopedDisabled disabled(!eligible || view.database.empty() || view.inspection.valid() || smoke_ || testSession_);
      if (cyui::primaryButton(tr("Inspect saved source"))) {
          view.inspectionError.clear(); view.inspected = Json::object(); view.cancelInspection = false;
          view.inspectionOperation = instance_ + "-inspection-" + std::to_string(++view.serial);
          const char *profiles[] = {"recommended", "security", "modernize", "performance", "portability"};
          Json parameters{{"path", source->path}, {"expectedRevision", source->revision},
                          {"compilationDatabase", view.database}, {"profile", profiles[view.profile]},
                          {"timeoutSeconds", view.timeout}, {"operationId", view.inspectionOperation}};
          if (!view.executable.empty()) parameters["executable"] = view.executable;
          auto engine = engine_; view.inspectionStarted = ImGui::GetTime();
          view.inspection = std::async(std::launch::async, [engine, parameters] { return engine->dispatch("analysis.inspect", parameters); });
      }
    }
    ImGui::SameLine();
    { cyui::ScopedDisabled disabled(!view.inspection.valid() || view.cancelInspection);
      if (cyui::button(tr("Cancel inspection"))) { view.cancelInspection = true; view.lastPoll = -1; } }
    if (view.inspection.valid()) {
        cyui::spinner("##inspection-progress"); ImGui::SameLine();
        cyui::textDim("%s | %.1f s", tr(view.cancelInspection ? "Cancelling task..." : "Running"), ImGui::GetTime() - view.inspectionStarted);
        if (view.process.value("operationId", "") == view.inspectionOperation)
            cyui::textDim("%s", tr(view.process.value("running", false) ? "Native process running" : "Waiting for native process"));
    }
    if (!view.inspectionError.empty()) cyui::callout(cyui::Tone::warning, view.inspectionError.c_str());
    if (view.inspected.empty()) return;
    bool current = false;
    for (const auto &tab : tabs_) if (tab->path == view.inspected.value("path", ""))
        current = tab->identity == view.inspected.value("documentId", uint64_t(0)) &&
                  tab->revision == view.inspected.value("revision", uint64_t(0));
    const bool stale = !current || view.inspected.value("stale", false);
    const auto status = view.inspected.value("status", "");
    const char *statusLabel = status == "complete" ? "Complete" : status == "failed" ? "Failed" :
        status == "cancelled" ? "Cancelled" : status == "timed-out" ? "Timed out" :
        status == "truncated" ? "Truncated" : "Stale results";
    cyui::badge(tr(statusLabel), stale ? cyui::Tone::warning : cyui::Tone::neutral);
    cyui::textDim("%s: %llu", tr("Diagnostics outside the workspace"),
                  static_cast<unsigned long long>(view.inspected.value("externalDiagnostics", uint64_t(0))));
    if (stale) cyui::callout(cyui::Tone::warning, tr("Inspection results are stale. Inspect the current saved revision again."));
    if (view.inspected.value("truncated", false)) cyui::callout(cyui::Tone::warning, tr("Inspection output or diagnostics were truncated."));
    cyui::textDim("%s", tr("Compiler locations use one-based UTF-8 byte columns, not LSP UTF-16 positions."));
    const auto &diagnostics = view.inspected.at("diagnostics");
    if (diagnostics.empty()) cyui::textDim("%s", tr("No diagnostics in this result."));
    ImGui::BeginChild("##inspection-diagnostics", cyui::dp(0, 220));
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(diagnostics.size()), ImGui::GetFrameHeightWithSpacing());
    while (clipper.Step()) for (int i=clipper.DisplayStart; i<clipper.DisplayEnd; ++i) {
        const auto &diagnostic = diagnostics.at(i);
        const auto path = diagnostic.value("path", ""), message = diagnostic.value("message", "");
        const auto caption = path + ":" + std::to_string(diagnostic.value("line", size_t(1))) + ":" +
            std::to_string(diagnostic.value("byteColumn", size_t(1))) + "  " + message;
        ImGui::PushID(i);
        { cyui::ScopedDisabled disabled(stale);
          if (cyui::ghostButton(tr("Open location"))) open(path, diagnostic.value("line", size_t(1)), diagnostic.value("byteColumn", size_t(1)), false); }
        ImGui::SameLine(); ImGui::TextUnformatted(caption.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (cyui::button(tr("Copy inspection output"))) ImGui::SetClipboardText(view.inspected.value("output", "").c_str());
    cyui::logView("##inspection-output", view.inspectionOutput->state, view.inspectionOutput->buffer, cyui::dp(0, 180));
}
} // namespace cycode
