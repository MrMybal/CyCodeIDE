#include "cycode/Application.h"
#include "cycode/EditorLanguage.h"
#include <algorithm>
#include <imgui.h>
namespace cycode {
void Application::updateHighlights() {
    const auto now=ImGui::GetTime(); size_t pending=0;
    const auto servers=languageState_.find("servers");
    if (servers==languageState_.end() || !servers->is_object()) return;
    for (const auto &tab : tabs_) {
        if(tab->pagedProfile)continue;
        const auto server=servers->find(engine_->languageBackendFor(tab->path));
        if (server==servers->end()) continue;
        const auto generation=server->value("semanticGeneration",uint64_t(0));
        const bool enabled=!smoke_ && server->value("ready",false) && server->value("semanticTokens",false);
        if (tab->highlightObservedGeneration!=generation || !enabled) {
            tab->editor.ClearSemanticTokens(); tab->highlightCount=0; tab->highlightRevision=0;
            if (tab->highlightObservedGeneration!=generation) tab->highlightRetryAt=now+.3;
            tab->highlightObservedGeneration=generation;
        }
        if (tab->highlighting.valid() && tab->highlighting.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {
                const auto result=tab->highlighting.get();
                if (enabled && result.at("revision")==tab->revision && result.at("generation")==generation) {
                    tab->highlightCount=applySemanticHighlights(tab->editor,result.at("tokens"),LanguageService::supportsCpp(tab->path));
                    tab->highlightRevision=tab->revision; tab->highlightGeneration=generation;
                    tab->highlightError.clear(); tab->highlightTruncated=result.at("truncated");
                }
            } catch (const std::exception &e) { tab->highlightError=e.what(); tab->highlightRetryAt=now+.5; }
        }
        if (tab->highlightRevision && tab->highlightRevision!=tab->revision) { tab->editor.ClearSemanticTokens(); tab->highlightCount=0; tab->highlightRevision=0; }
        pending+=tab->highlighting.valid();
    }
    auto launch=[&](const std::shared_ptr<Tab> &tab) {
        if (tab->pagedProfile || smoke_ || pending>=2 || tab->highlighting.valid() || !engine_->supportsLanguage(tab->path) ||
            now<tab->highlightRetryAt || (tab->highlightRevision==tab->revision && tab->highlightGeneration==tab->highlightObservedGeneration)) return;
        const auto server=servers->find(engine_->languageBackendFor(tab->path));
        if (server==servers->end() || !server->value("ready",false) || !server->value("semanticTokens",false) || unrealAnalysisState_=="preparing") return;
        const auto engine=engine_; const Json params={{"path",tab->path},{"expectedRevision",tab->revision}};
        tab->highlighting=std::async(std::launch::async,[engine,params] { return engine->dispatch("language.highlight",params); }); ++pending;
    };
    for (const auto &tab : tabs_) if (tab->path==active_) launch(tab);
    for (const auto &tab : tabs_) if (tab->visible && tab->path!=active_) launch(tab);
}
}
