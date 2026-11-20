#include "cycode/Application.h"
#include "cycode/EditorLanguage.h"
#include <algorithm>
#include <cyui/Core.h>

namespace cycode {
cyui::CodeLanguage Application::codeLanguageForDocument(const std::string &path) const {
    for (const auto &tab : tabs_)
        if (tab->path == path || tab->displayPath == path)
            return tab->syntaxLanguageId.empty() ? cyui::codeLanguageForPath(tab->displayPath)
                                                 : cyui::codeLanguageFromName(tab->syntaxLanguageId);
    return cyui::codeLanguageForPath(path);
}
void Application::updateEditorSyntax(const std::shared_ptr<Tab> &tab, const std::string &text) {
    if (tab->legacySyntax) tab->syntax.update(tab->editor, tab->path, text);
    else tab->codeEditor.refreshSyntax();
}
void Application::applyEditorLanguage(const std::shared_ptr<Tab> &tab) {
    if (tab->image) return;
    const auto previousCatalogue = tab->syntaxCatalogue;
    if (tab->syntaxLanguageId.empty()) tab->codeEditor.setLanguageForPath(tab->displayPath);
    else tab->codeEditor.setLanguage(cyui::codeLanguageFromName(tab->syntaxLanguageId));
    const auto language = tab->codeEditor.language();
    tab->syntaxCatalogue = cyui::codeLanguageRevision();
    if (tab->pagedProfile) {
        if (tab->paged) {
            tab->paged->setLanguage(language);
            if (previousCatalogue && previousCatalogue != tab->syntaxCatalogue) tab->paged->refresh();
        }
        return;
    }
    tab->legacySyntax = tab->syntaxLanguageId.empty() && cyui::codeLanguageInfo(language).builtin &&
                        languageUsesLegacyLexer(tab->displayPath);
    tab->editor.ClearSyntaxTokens();
    tab->codeEditor.setSyntaxEnabled(!tab->legacySyntax);
    if (tab->legacySyntax)
        tab->editor.SetLanguage(unreal_.value("available", false) && LanguageService::supportsCpp(tab->path)
                                    ? &unrealLanguage_ : editorLanguage(tab->displayPath));
    if (language != cyui::codeLanguageForPath(tab->displayPath)) {
        tab->editor.ClearSemanticTokens(); tab->highlightRevision = 0; tab->highlightCount = 0;
    }
    updateEditorSyntax(tab, tab->editor.GetText());
}
void Application::configureCodeLanguage(cyui::CodeEditor &view, const std::string &path) {
    view.setLanguage(codeLanguageForDocument(path));
    view.native().ClearSyntaxTokens();
    view.setSyntaxEnabled(true);
}
void Application::selectSyntaxLanguage(const std::shared_ptr<Tab> &tab, const std::string &id) {
    if (tab->syntaxLanguageId == id) return;
    tab->syntaxLanguageId = id;
    tab->preview = false;
    if (tab->pendingSession.is_object()) tab->pendingSession["syntaxLanguage"] = id;
    applyEditorLanguage(tab);
    // Refresh related previews on the next update, including before/after views.
    syntaxCatalogue_ = 0;
}
void Application::updateCodeLanguages() {
    const auto revision = cyui::codeLanguageRevision();
    if (syntaxCatalogue_ == revision) return;
    syntaxCatalogue_ = revision;
    syntaxLanguageCatalogue_ = cyui::codeLanguages();
    for (const auto &tab : tabs_)
        if (tab->syntaxCatalogue != revision) applyEditorLanguage(tab);
    if (!searchPreviewLoaded_.empty()) configureCodeLanguage(searchPreview_, searchPreviewPath_);
    if (refactorPreview_.is_object() && refactorPreview_.contains("before"))
        for (auto *view : {&beforePreviewCode_, &afterPreviewCode_}) configureCodeLanguage(*view, refactorFile_);
    if (vcsConflict_.contains("path"))
        for (auto *view : {&vcsBaseCode_, &vcsOursCode_, &vcsTheirsCode_}) configureCodeLanguage(*view, vcsConflict_.at("path"));
    if (recoveryPreview_.contains("path"))
        recoveryCode_.setText(recoveryCode_.text(), codeLanguageForDocument(recoveryPreview_.at("path")), recoveryCode_.revision());
    if (templatePlan_.contains("files") && templateFile_ < templatePlan_.at("files").size()) {
        const auto &file = templatePlan_.at("files").at(templateFile_);
        templateCode_.setText(templateCode_.text(), cyui::codeLanguageForPath(file.at("path").get<std::string>()), templateCode_.revision());
    }
}
void Application::syntaxLanguageMenu(const std::shared_ptr<Tab> &tab) {
    ImGui::PushID(tab.get());
    if (syntaxLanguageCatalogue_.empty()) syntaxLanguageCatalogue_ = cyui::codeLanguages();
    const auto &catalogue = syntaxLanguageCatalogue_;
    const auto selected = std::find_if(catalogue.begin(), catalogue.end(), [&](const auto &info) {
        return info.id == tab->syntaxLanguageId;
    });
    const auto name = tab->syntaxLanguageId.empty() ? cyui::codeLanguageInfo(codeLanguageForDocument(tab->path)).name
                    : selected != catalogue.end() ? selected->name : tab->syntaxLanguageId + " (" + tr("Unavailable") + ")";
    const auto label = name + (tab->syntaxLanguageId.empty() ? " (" + std::string(tr("Automatic")) + ")" : "") + "###syntax-language";
    if (cyui::ghostButton(label.c_str())) {
        syntaxLanguageFilter_.query.clear(); ImGui::OpenPopup("##syntax-languages");
    }
    cyui::tooltip(tr("Syntax language: click to choose automatic detection or a manual mode."));
    ImGui::SetNextWindowSizeConstraints(cyui::dp(240, 0), cyui::dp(440, 460));
    if (ImGui::BeginPopup("##syntax-languages")) {
        ImGui::TextUnformatted(tr("Syntax language"));
        if (ImGui::Selectable(tr("Automatic (file association)"), tab->syntaxLanguageId.empty())) {
            selectSyntaxLanguage(tab, ""); ImGui::CloseCurrentPopup();
        }
        if (!tab->syntaxLanguageId.empty() && selected == catalogue.end())
            cyui::textDim("%s: %s", tr("Unavailable"), tab->syntaxLanguageId.c_str());
        cyui::filterBox("##syntax-filter", syntaxLanguageFilter_);
        ImGui::Separator();
        if (ImGui::BeginChild("##syntax-catalogue", cyui::dp(0, 320)))
            for (const auto &info : catalogue) {
                if (!syntaxLanguageFilter_.matches(info.name, info.id)) continue;
                ImGui::PushID(info.id.c_str());
                if (ImGui::Selectable(info.name.c_str(), tab->syntaxLanguageId == info.id)) {
                    selectSyntaxLanguage(tab, info.id); ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    ImGui::PopID();
}
} // namespace cycode
