#include "cycode/Application.h"
#include "cycode/DiagnosticAnnotations.h"
namespace cycode {
void Application::updateDiagnostics() {
    const auto servers = languageState_.find("servers");
    for (const auto &tab : tabs_) {
        if (tab->image)
            continue;
        const Json *server = nullptr, *publication = nullptr, *entries = nullptr;
        if (servers != languageState_.end() && servers->is_object()) {
            const auto found = servers->find(engine_->languageBackendFor(tab->path));
            if (found != servers->end() && found->value("ready", false) && found->value("enabled", false) &&
                !(LanguageService::supportsCpp(tab->path) && unrealAnalysisState_ == "preparing")) {
                server = &*found;
                const auto revisions = server->find("diagnosticRevisions");
                if (revisions != server->end() && revisions->is_object()) {
                    const auto candidate = revisions->find(tab->path);
                    if (candidate != revisions->end() &&
                        candidate->value("revision", uint64_t(0)) == tab->revision &&
                        candidate->value("id", uint64_t(0)) == tab->identity)
                        publication = &*candidate;
                }
                const auto diagnostics = server->find("diagnostics");
                if (publication && diagnostics != server->end() && diagnostics->is_object()) {
                    const auto candidate = diagnostics->find(publication->at("uri").get<std::string>());
                    if (candidate != diagnostics->end())
                        entries = &*candidate;
                }
            }
        }
        if (!entries) {
            if (!tab->diagnosticKey.empty())
                clearDiagnosticAnnotations(tab->editor);
            tab->diagnosticKey.clear();
            tab->diagnosticCount = 0;
            tab->diagnosticTruncated = false;
            continue;
        }
        const auto &p = cyui::palette();
        const auto key = tab->path + ":" + std::to_string(tab->identity) + ":" + std::to_string(tab->revision) + ":" +
                         std::to_string(server->value("semanticGeneration", uint64_t(0))) + ":" +
                         std::to_string(server->value("diagnosticGeneration", uint64_t(0))) + ":" +
                         std::to_string(p.error) + ":" + std::to_string(p.warning) + ":" +
                         std::to_string(p.info) + ":" + std::to_string(p.textDim);
        if (tab->diagnosticKey == key)
            continue;
        const auto result = applyDiagnosticAnnotations(tab->editor, *entries);
        tab->diagnosticKey = key;
        tab->diagnosticCount = result.count;
        tab->diagnosticTruncated = result.truncated;
    }
}
} // namespace cycode
