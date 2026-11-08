#include "cycode/Application.h"
#include "cycode/SymbolCatalogue.h"
#include <functional>

namespace cycode {
bool Application::symbolBackendSelector() {
    const auto servers = languageState_.find("servers");
    if (servers == languageState_.end())
        return false;
    auto automatic = engine_->languageBackendFor(active_);
    if (automatic.empty())
        automatic = "cpp";
    const auto title = symbolBackend_.empty()
                           ? std::string(tr("Automatic (active document)")) + " | " + automatic
                       : servers->contains(symbolBackend_)
                           ? servers->at(symbolBackend_).value("name", symbolBackend_)
                           : std::string(tr("Missing language server")) + ": " + symbolBackend_;
    cyui::textDim("%s", tr("Symbol language server"));
    ImGui::SetNextItemWidth(-1);
    bool changed = false;
    if (ImGui::BeginCombo("##symbol-language-server", title.c_str())) {
        if (ImGui::Selectable(tr("Automatic (active document)"), symbolBackend_.empty())) {
            symbolBackend_.clear();
            changed = true;
        }
        for (const auto &[id, server] : servers->items()) {
            const auto name = server.value("name", id) + " | " + id;
            const bool supported = !server.value("ready", false) ||
                                   server.value("methods", Json::object()).value("symbols", false);
            const bool usable =
                server.value("enabled", false) && server.value("available", false) && supported;
            cyui::ScopedDisabled disabled(!usable);
            if (ImGui::Selectable(name.c_str(), symbolBackend_ == id)) {
                symbolBackend_ = id;
                changed = true;
            }
            if (!usable)
                cyui::tooltip(tr("Configure and enable a server with workspace symbol support."));
        }
        ImGui::EndCombo();
    }
    return changed;
}
bool Application::symbolResultCurrent(const Json &result) const {
    if (!result.contains("backend") || !result.contains("epoch"))
        return false;
    const auto servers = languageState_.find("servers");
    if (servers == languageState_.end())
        return false;
    const auto server = servers->find(result.at("backend").get<std::string>());
    if (server == servers->end() || !server->value("enabled", false) || !server->value("ready", false) ||
        server->value("epoch", uint64_t(0)) != result.at("epoch"))
        return false;
    if (result.contains("documentId")) {
        if (result.at("path") != active_)
            return false;
        for (const auto &tab : tabs_)
            if (tab->path == active_ && !tab->image)
                return tab->identity == result.at("documentId") && tab->revision == result.at("revision");
        return false;
    }
    return true;
}
void Application::updateStructure() {
    if (!structure_.future.valid() ||
        structure_.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    try {
        auto reply = structure_.future.get();
        if (reply.at("key") != structure_.wanted)
            return;
        structure_.sent = structure_.wanted;
        if (reply.contains("error")) {
            structure_.error = reply.at("error");
            return;
        }
        structure_.report = std::move(reply.at("result"));
        structure_.model.set(structure_.report.at("symbols"));
        structure_.selected = structure_.reveal = 0;
        structure_.error.clear();
    } catch (const std::exception &e) {
        structure_.error = e.what();
    }
}
void Application::structurePanel() {
    panels_.try_emplace("structure", false);
    if (!beginPanel("Structure", "structure")) {
        structure_.wanted.clear();
        structure_.sent.clear();
        return;
    }
    const auto finish = [] { ImGui::End(); };
    std::shared_ptr<Tab> tab;
    for (const auto &candidate : tabs_)
        if (candidate->path == active_ && !candidate->image)
            tab = candidate;
    const auto backend = tab ? engine_->languageBackendFor(tab->path) : std::string{};
    const auto servers = languageState_.find("servers");
    if (!tab || tab->path.starts_with("debug://") || backend.empty() || servers == languageState_.end() ||
        !servers->contains(backend)) {
        structure_.wanted.clear();
        structure_.sent.clear();
        cyui::emptyState(tr("No document structure"), tr("Open a source file with a language server."),
                         cyui::icon::code);
        finish();
        return;
    }
    const auto &server = servers->at(backend);
    const bool preparing = LanguageService::supportsCpp(tab->path) && unrealAnalysisState_ == "preparing";
    if (!server.value("enabled", false) || preparing ||
        (server.value("ready", false) &&
         !server.value("methods", Json::object()).value("documentSymbols", false))) {
        structure_.wanted.clear();
        structure_.sent.clear();
        cyui::emptyState(tr("No document structure"),
                         tr(preparing ? "Analysis configuration is changing"
                                      : "Enable a language server that provides document symbols."),
                         cyui::icon::code);
        finish();
        return;
    }
    Json parameters = {{"path", tab->path},
                       {"expectedRevision", tab->revision},
                       {"expectedDocumentId", tab->identity},
                       {"expectedEpoch", server.value("epoch", uint64_t(0))},
                       {"includeExternal", tab->readOnly},
                       {"limit", 1000}};
    const auto key = parameters.dump();
    if (key != structure_.wanted) {
        structure_.wanted = key;
        structure_.report = Json::object();
        structure_.model.set(Json::array());
        structure_.error.clear();
        structure_.selected = structure_.reveal = 0;
        structure_.changed = ImGui::GetTime();
    }
    {
        cyui::ScopedDisabled disabled(structure_.future.valid());
        if (cyui::iconButton(cyui::icon::refresh, tr("Refresh structure")))
            structure_.sent.clear();
    }
    ImGui::SameLine();
    const bool locate = cyui::iconButton(cyui::icon::search, tr("Locate current symbol"));
    ImGui::SameLine();
    ImGui::Checkbox(tr("Follow caret"), &structure_.follow);
    ImGui::SameLine();
    ImGui::Checkbox(tr("Alphabetical order"), &structure_.alphabetic);
    viewFilter("##structure-filter", structure_.filter);
    cyui::textDim("%s", tab->displayPath.empty() ? tab->path.c_str() : tab->displayPath.c_str());
    if (!structure_.future.valid() && structure_.sent != key && ImGui::GetTime() - structure_.changed >= .2) {
        auto engine = engine_;
        structure_.sent = key;
        structure_.future = std::async(std::launch::async, [engine, parameters, key] {
            try {
                return Json{{"key", key},
                            {"result", engine->dispatch("language.documentSymbols", parameters)}};
            } catch (const std::exception &e) {
                return Json{{"key", key}, {"error", e.what()}};
            }
        });
    }
    if (structure_.future.valid()) {
        cyui::spinner("##structure-loading");
        ImGui::SameLine();
        cyui::textDim("%s", tr("Reading document structure..."));
    }
    if (!structure_.error.empty())
        cyui::textColoured(cyui::palette().warning, "%s", structure_.error.c_str());
    if (!symbolResultCurrent(structure_.report)) {
        finish();
        return;
    }
    if (structure_.report.value("truncated", false))
        cyui::textColoured(cyui::palette().warning, "%s",
                           tr("Structure limited to the reported symbol budget."));
    if (structure_.report.value("invalid", 0))
        cyui::textDim("%s: %d", tr("Invalid symbols ignored"), structure_.report.at("invalid").get<int>());
    if (structure_.model.symbols().empty()) {
        cyui::emptyState(tr("No document symbols"), tr("The language server returned no document symbols."),
                         cyui::icon::code);
        finish();
        return;
    }
    const auto cursor = tab->editor.GetMainCursorPosition();
    const auto current = enclosingSymbol(
        structure_.model.symbols(), tab->path,
        {{"line", cursor.line},
         {"character", LanguageClient::utf16Length(tab->editor.GetSectionText({cursor.line, 0}, cursor))}});
    if (locate) {
        structure_.filter.query.clear();
        structure_.selected = current;
        structure_.reveal = current;
    } else if (structure_.follow && structure_.selected != current) {
        structure_.selected = current;
        structure_.reveal = current;
    }
    std::set<uint64_t> reveal;
    for (auto node = structure_.reveal; node; node = structure_.model.at(node).at("parent").get<uint64_t>())
        reveal.insert(node);
    const auto &visible = structure_.model.visible(structure_.filter);
    if (visible.empty())
        cyui::textDim("%s", tr("No matching symbols"));
    if (current) {
        const auto &scope = structure_.model.at(current);
        cyui::textDim("%s: %s%s%s", tr("Current symbol"),
                      scope.at("container").get_ref<const std::string &>().c_str(),
                      scope.at("container").get_ref<const std::string &>().empty() ? "" : "::",
                      scope.at("name").get_ref<const std::string &>().c_str());
    }
    std::string destination;
    Json position;
    ImGui::BeginChild("##structure-tree", {}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::PushID(key.c_str());
    std::function<void(uint64_t)> draw = [&](uint64_t parent) {
        for (const auto node : structure_.model.children(parent, structure_.alphabetic)) {
            if (!visible.contains(node))
                continue;
            const auto &row = structure_.model.at(node);
            const bool leaf = structure_.model.children(node, false).empty();
            cyui::ItemScope id(node);
            if (!leaf && (structure_.filter.active() || reveal.contains(node)))
                ImGui::SetNextItemOpen(true);
            const auto flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               (leaf ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen : 0) |
                               (structure_.selected == node ? ImGuiTreeNodeFlags_Selected : 0);
            const bool expanded = ImGui::TreeNodeEx("##symbol", flags, "%s",
                                                    row.at("name").get_ref<const std::string &>().c_str());
            if ((ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) ||
                (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
                structure_.selected = node;
                structure_.follow = false;
                destination = row.at("path");
                position = row.at("range").at("start");
            }
            if (structure_.reveal == node)
                ImGui::SetScrollHereY(.5f);
            const auto kind = tr(symbolKindName(row.at("kind")));
            const auto tip = std::string(kind) + "\n" + row.at("detail").get<std::string>();
            cyui::tooltip(tip.c_str());
            ImGui::SameLine();
            cyui::textDim("%s%s", kind, row.value("deprecated", false) ? tr(" (deprecated)") : "");
            if (!leaf && expanded) {
                draw(node);
                ImGui::TreePop();
            }
        }
    };
    draw(0);
    ImGui::PopID();
    ImGui::EndChild();
    structure_.reveal = 0;
    finish();
    if (!destination.empty())
        open(destination, position.at("line").get<size_t>() + 1, position.at("character").get<size_t>() + 1,
             true);
}
} // namespace cycode
