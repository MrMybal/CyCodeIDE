#include "cycode/Application.h"
#include "cycode/EditorLanguage.h"
#include <cyui/modules/Code.h>
namespace cycode {
namespace {
struct Registered {
    Json descriptor;
    std::set<const Application *> owners;
};
std::map<std::string, Registered> registry;
std::mutex registryMutex;
cyui::CodeLanguageDefinition definition(const Json &entry) {
    cyui::CodeLanguageDefinition result;
    result.id = entry.at("id");
    result.name = entry.at("name");
    result.extensions = entry.value("extensions", std::vector<std::string>{});
    result.filenames = entry.value("filenames", std::vector<std::string>{});
    result.wordLists = entry.value("wordLists", std::vector<std::string>{});
    const auto base = entry.value("base", std::string("plain"));
    bool found = false;
    for (const auto &info : cyui::codeLanguages())
        if (info.builtin && (info.id == base || info.name == base)) {
            result.base = info.language;
            found = true;
            break;
        }
    if (!found)
        throw std::runtime_error("Unknown built-in syntax base: " + base);
    if (entry.contains("rules")) {
        const auto &source = entry.at("rules");
        cyui::CodeSyntaxRules rules;
        rules.keywords = source.value("keywords", std::vector<std::string>{});
        rules.types = source.value("types", std::vector<std::string>{});
        rules.functions = source.value("functions", std::vector<std::string>{});
        rules.lineComment = source.value("lineComment", std::string{});
        rules.blockCommentStart = source.value("blockCommentStart", std::string{});
        rules.blockCommentEnd = source.value("blockCommentEnd", std::string{});
        rules.quotes = source.value("quotes", rules.quotes);
        rules.escape = source.value("escape", std::string(1, rules.escape)).at(0);
        rules.caseSensitive = source.value("caseSensitive", true);
        result.rules = std::move(rules);
    }
    return result;
}
} // namespace
void Application::synchronizeSyntax(const Json &syntaxes) {
    std::lock_guard lock(registryMutex);
    std::set<std::string> desired;
    for (const auto &entry : syntaxes) {
        const std::string id = entry.at("id");
        try {
            auto existing = registry.find(id);
            if (existing != registry.end() && existing->second.descriptor != entry &&
                existing->second.owners != std::set<const Application *>{this})
                throw std::runtime_error("Another workspace uses a different definition of syntax: " + id);
            if (existing == registry.end() || existing->second.descriptor != entry) {
                cyui::registerCodeLanguage(definition(entry));
                registry[id].descriptor = entry;
            }
            registry.at(id).owners.insert(this);
            desired.insert(id);
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    for (const auto &id : extensionSyntaxIds_)
        if (!desired.contains(id)) {
            auto found = registry.find(id);
            if (found == registry.end())
                continue;
            found->second.owners.erase(this);
            if (found->second.owners.empty()) {
                cyui::unregisterCodeLanguage(id);
                registry.erase(found);
            }
        }
    extensionSyntaxIds_ = std::move(desired);
    for (const auto &tab : tabs_)
        if (!tab->image) {
            tab->codeEditor.setLanguageForPath(tab->displayPath);
            const bool custom = !cyui::codeLanguageInfo(tab->codeEditor.language()).builtin;
            tab->codeEditor.setSyntaxEnabled(custom || !languageUsesLegacyLexer(tab->displayPath));
            if (custom || !languageUsesLegacyLexer(tab->displayPath))
                tab->codeEditor.refreshSyntax();
            else {
                tab->editor.SetLanguage(unreal_.value("available", false) &&
                                                LanguageService::supportsCpp(tab->displayPath)
                                            ? &unrealLanguage_
                                            : editorLanguage(tab->displayPath));
                tab->syntax.update(tab->editor, tab->path, tab->editor.GetText());
            }
        }
}
} // namespace cycode
