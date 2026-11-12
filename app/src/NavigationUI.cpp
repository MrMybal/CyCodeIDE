#include "cycode/Application.h"
#include "cycode/NavigationLocations.h"
#include "cycode/RelatedFiles.h"
#include "ApplicationHelp.h"
#include <algorithm>

namespace cycode {
namespace {
std::string navigationCommand(const std::string &method) {
    return "navigate." + method.substr(9);
}
std::string relatedHelpHint(const Keymap &keymap) {
    for(const auto &binding:keymap.bindings())if(binding.command=="help.context" && binding.strokes.size()==1)return binding.shortcut;
    return {};
}
} // namespace
void Application::relatedHelp() {
    app_help::open(root_,"app/Documentation",locale_,"navigation",relatedHelpAnchor(counterpartInclude_,counterpartChoose_),smoke_ || testSession_);
}
void Application::beginCounterparts(bool included,bool choose,bool refresh) {
    if(!commandEnabled(included?"navigate.include":choose?"navigate.relatedFiles":"navigate.counterpart"))return;
    if(refresh && filesFuture_.valid()) {
        cyui::notify(tr("File index refresh is already in progress. Retry when it is ready."),cyui::Tone::neutral);return;
    }
    for(const auto &tab:tabs_)if(tab->path==active_) {
        counterpartTab_=tab;counterpartEngine_=engine_;
        counterpartRevision_=tab->revision;counterpartIdentity_=tab->identity;
        counterpartInclude_=included;counterpartPosition_=tab->editor.GetMainCursorPosition();
        counterpartChoose_=choose;
        counterpartReport_=Json::object();counterpartEntries_.clear();counterpartPaletteState_={};counterpartOpen_=true;
        counterpartEntries_.push_back({uiId("counterpart/status"),refresh?"Refreshing lookup index...":
            included?"Loading included files...":"Loading related C/C++ files...","","",false});
        counterpartEntries_.push_back({uiId("counterpart/help"),"Help for this navigation",relatedHelpHint(keymap_),"Actions",true});
        Json parameters={{"path",tab->path},{"expectedRevision",tab->revision},{"expectedDocumentId",tab->identity}};
        if(included)parameters["position"]={{"line",counterpartPosition_.line},{"character",LanguageClient::utf16Length(
            tab->editor.GetSectionText({counterpartPosition_.line,0},counterpartPosition_))}};
        auto owner=engine_;
        counterpartFuture_=std::async(std::launch::async,[owner,parameters,included,refresh] {
            if(refresh)(void)owner->dispatch("workspace.files",{{"refresh",true},{"limit",1}});
            return owner->dispatch(included?"workspace.include":"workspace.counterparts",parameters);
        });
        return;
    }
}
bool Application::counterpartsPalette() {
    auto tab=counterpartTab_.lock();
    const auto current=[&] {return tab && counterpartEngine_.lock()==engine_ && tab->path==active_ &&
        tab->revision==counterpartRevision_ && tab->identity==counterpartIdentity_ &&
        (!counterpartInclude_ || tab->editor.GetMainCursorPosition()==counterpartPosition_);};
    std::string target;
    if(counterpartFuture_.valid() && counterpartFuture_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        counterpartEntries_.clear();
        try {
            counterpartReport_=counterpartFuture_.get();
            const auto &files=counterpartReport_.at("candidates");
            const bool partial=counterpartReport_.at("truncated").get<bool>() || counterpartReport_.at("unreadable").get<size_t>()!=0;
            if(counterpartOpen_ && current() && relatedAutoOpen(files.size(),counterpartReport_.at("unreadable").get<size_t>(),
                                                               counterpartReport_.at("truncated").get<bool>(),counterpartChoose_)) {
                target=files[0].at("path");counterpartOpen_=false;
            } else {
                for(size_t i=0;i<files.size();++i) {
                    const auto relation=files[i].at("relation").get<std::string>();
                    const auto kind=relation=="sibling"?"Sibling":relation=="paired-root"?"Paired root":
                        relation=="workspace-index"?"Workspace index":relation=="source-relative"?"Source-relative":"Paired directory";
                    counterpartEntries_.push_back({uiId("counterpart/"+std::to_string(i)),files[i].at("path"),tr(kind),kind,true});
                }
                if(files.empty())counterpartEntries_.push_back({uiId("counterpart/status"),counterpartInclude_?
                    "No matching included file found.":"No related C/C++ files found.","","",false});
                if(counterpartInclude_ && files.empty())counterpartEntries_.push_back({uiId("counterpart/scope"),
                    "For compiler or SDK headers, use Go to definition.","","",false});
                if(partial)counterpartEntries_.push_back({uiId("counterpart/limit"),"Related-file search was limited or some paths were unreadable.","","",false});
                const auto indexState=counterpartReport_.at("index").at("state").get<std::string>();
                const char *indexNotice=nullptr;
                if(indexState=="missing")indexNotice="Workspace file index is unavailable. Refresh the project files, then retry.";
                else if(indexState=="busy")indexNotice="Workspace file index is being refreshed. Retry when it is ready.";
                else if(indexState=="stale" || indexState=="changed")indexNotice="Workspace file index changed. Refresh the project files, then retry.";
                else if(indexState=="partial")indexNotice="Workspace file index is incomplete. Other matching paths may be missing.";
                if(indexNotice)counterpartEntries_.push_back({uiId("counterpart/index"),indexNotice,"","",false});
            }
        } catch(const std::exception &exception) {
            counterpartEntries_.push_back({uiId("counterpart/error"),exception.what(),"","",false});
        }
        if(counterpartOpen_) {
            const bool report=counterpartReport_.contains("candidates");
            counterpartEntries_.push_back({uiId("counterpart/retry"),"Retry this lookup","","Actions",true});
            counterpartEntries_.push_back({uiId("counterpart/refresh"),"Refresh lookup index and retry","","Actions",report && !filesFuture_.valid()});
            counterpartEntries_.push_back({uiId("counterpart/copy"),"Copy all result paths","","Actions",
                report && !counterpartReport_.at("candidates").empty()});
            counterpartEntries_.push_back({uiId("counterpart/report"),"Copy lookup report (JSON)","","Actions",report});
            counterpartEntries_.push_back({uiId("counterpart/help"),"Help for this navigation",relatedHelpHint(keymap_),"Actions",true});
        }
    }
    if(counterpartOpen_ && !current()) {
        counterpartOpen_=false;cyui::notify(tr(counterpartInclude_?"Include navigation context changed. Run the command again.":
                                             "Header/source context changed. Run the command again."),cyui::Tone::warning);
    }
    const auto chosen=cyui::commandPalette(counterpartInclude_?"Open included file":counterpartChoose_?"Related C/C++ files":"Switch header/source",
        counterpartOpen_,counterpartPaletteState_,counterpartEntries_,cyui::dp(850,420));
    if(chosen==uiId("counterpart/help")) {
        try{relatedHelp();}catch(const std::exception &exception){error(exception.what());}
    }
    if(chosen && current() && (chosen==uiId("counterpart/retry") || chosen==uiId("counterpart/refresh"))) {
        beginCounterparts(counterpartInclude_,counterpartChoose_,chosen==uiId("counterpart/refresh"));
        return counterpartOpen_;
    }
    if(chosen && current() && counterpartReport_.contains("candidates")) {
        const auto &files=counterpartReport_.at("candidates");
        if(chosen==uiId("counterpart/copy")) {
            std::string paths;for(const auto &file:files){if(!paths.empty())paths+='\n';paths+=file.at("path").get_ref<const std::string &>();}
            if(!SDL_SetClipboardText(paths.c_str()))error(SDL_GetError());
        }
        if(chosen==uiId("counterpart/report")) {
            const auto report=counterpartReport_.dump(2);
            if(!SDL_SetClipboardText(report.c_str()))error(SDL_GetError());
        }
        for(size_t i=0;i<files.size();++i)if(chosen==uiId("counterpart/"+std::to_string(i)))target=files[i].at("path");
    }
    if(!target.empty() && current()) {
        try {open(target);}
        catch(const std::exception &exception){error(exception.what());}
    }
    return counterpartOpen_;
}
bool Application::navigationCurrent(const Json &report) const {
    if (!report.contains("path") || !report.contains("documentId") || !report.contains("backend") ||
        !report.contains("epoch") || !report.contains("revision"))
        return false;
    const auto servers = languageState_.find("servers");
    if (servers == languageState_.end())
        return false;
    const auto server = servers->find(report.at("backend").get_ref<const std::string &>());
    if (server == servers->end() || !server->value("enabled", false) || !server->value("ready", false) ||
        server->value("epoch", uint64_t(0)) != report.at("epoch"))
        return false;
    for (const auto &tab : tabs_)
        if (tab->path == report.at("path").get_ref<const std::string &>() && !tab->image && !tab->pagedProfile)
            return tab->identity == report.at("documentId") && tab->revision == report.at("revision");
    return false;
}
void Application::navigate(bool references) {
    requestNavigation(references ? "language.references" : "language.definition");
}
void Application::requestNavigation(const std::string &method, bool refresh) {
    if (navigationFuture_.valid() || !locationRequest(method))
        return;
    for (const auto &tab : tabs_) {
        if (tab->path != active_ || tab->image || tab->pagedProfile || !engine_->supportsLanguage(tab->path) ||
            tab->path.starts_with("debug://"))
            continue;
        navigationPosition_ = tab->editor.GetMainCursorPosition();
        navigationPath_ = tab->path;
        navigationReferences_ = method == "language.references";
        navigationMethod_ = method;
        navigation_ = Json::object();
        navigationModel_.set(Json::array());
        navigationSelected_ = -1;
        navigationFilter_.query.clear();
        navigationError_.clear();
        const auto prefix = tab->editor.GetSectionText({navigationPosition_.line, 0}, navigationPosition_);
        Json parameters = {
            {"path", tab->path},
            {"includeExternal", true},
            {"expectedRevision", tab->revision},
            {"expectedDocumentId", tab->identity},
            {"position",
             {{"line", navigationPosition_.line}, {"character", LanguageClient::utf16Length(prefix)}}}};
        if (refresh) parameters["refresh"] = true;
        const auto servers = languageState_.find("servers");
        if (servers != languageState_.end()) {
            const auto server = servers->find(engine_->languageBackendFor(tab->path));
            if (server != servers->end() && server->contains("epoch"))
                parameters["expectedEpoch"] = server->at("epoch");
        }
        const auto ticket = ++navigationTicket_;
        auto engine = engine_;
        navigationFuture_ = std::async(std::launch::async, [engine, parameters, method, ticket] {
            try {
                return Json{{"ticket", ticket}, {"result", engine->dispatch(method, parameters)}};
            } catch (const std::exception &e) {
                return Json{{"ticket", ticket}, {"error", e.what()}};
            }
        });
        // Like Rider, only Find Usages opens the results window up front. Go to
        // definition/declaration/type/implementation jumps straight to a tab and
        // shows the list only when the symbol has several targets.
        if (navigationReferences_) openToolWindow("References", "references");
        return;
    }
    cyui::notify(tr("Open a source file with a language server."), cyui::Tone::neutral);
}
void Application::updateNavigation() {
    if (!navigationFuture_.valid() ||
        navigationFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    try {
        auto reply = navigationFuture_.get();
        if (reply.at("ticket") != navigationTicket_)
            return;
        if (reply.contains("error")) {
            navigationError_ = reply.at("error");
            if (!navigationReferences_) cyui::notify(navigationError_.c_str(), cyui::Tone::warning);
            return;
        }
        languageState_ = engine_->languageStatus();
        if (!navigationCurrent(reply.at("result"))) {
            navigationError_ = tr("Navigation context changed. Run the query again.");
            if (!navigationReferences_) cyui::notify(navigationError_.c_str(), cyui::Tone::neutral);
            return;
        }
        navigation_ = std::move(reply.at("result"));
        navigationError_.clear();
        navigationModel_.set(navigation_.at("locations"));
        const auto &locations = navigation_.at("locations");
        bool sameCaret = false;
        for (const auto &tab : tabs_)
            if (tab->path == navigationPath_ && active_ == navigationPath_)
                sameCaret = tab->editor.GetMainCursorPosition() == navigationPosition_;
        if (locations.empty())
            cyui::notify(tr("No locations found"), cyui::Tone::neutral);
        if (!navigationReferences_ && locations.size() == 1 && sameCaret) {
            const auto &row = locations.front();
            const auto &start = row.at("range").at("start");
            open(row.at("path"), start.at("line").get<size_t>() + 1, start.at("character").get<size_t>() + 1,
                 true);
        } else if (!navigationReferences_ && locations.size() > 1)
            openToolWindow("References", "references");
    } catch (const std::exception &e) {
        navigationError_ = e.what();
    }
}
void Application::referencesPanel() {
    if (!beginPanel("References", "references"))
        return;
    const auto command = findCommand(navigationCommand(navigationMethod_));
    cyui::textDim("%s", tr(command ? command->title.c_str() : "Symbol navigation"));
    if (navigationFuture_.valid()) {
        ImGui::SameLine();
        cyui::spinner("##navigation");
    }
    ImGui::BeginDisabled(!commandEnabled(navigationCommand(navigationMethod_)));
    if (cyui::button(tr("Refresh at caret")))
        requestNavigation(navigationMethod_, true);
    cyui::tooltip(tr("Request a fresh backend result. Index progress may leave cached results."));
    ImGui::EndDisabled();
    if (!navigationError_.empty())
        cyui::textColoured(cyui::palette().warning, "%s", navigationError_.c_str());
    if (!navigation_.contains("locations")) {
        cyui::emptyState(
            tr("Symbol navigation"),
            tr("Use Navigate to find definitions, declarations, types, implementations or references."),
            cyui::icon::search);
        ImGui::End();
        return;
    }
    const bool current = navigationCurrent(navigation_);
    if (!current)
        cyui::textColoured(cyui::palette().warning, "%s",
                           tr("Navigation context changed. Run the query again."));
    const auto &source = navigation_.at("position");
    cyui::textDim("%s:%llu:%llu", navigation_.at("path").get_ref<const std::string &>().c_str(),
                  static_cast<unsigned long long>(source.at("line").get<uint64_t>() + 1),
                  static_cast<unsigned long long>(source.at("character").get<uint64_t>() + 1));
    viewFilter("##navigation-filter", navigationFilter_);
    const auto &matches = navigationModel_.matching(navigationFilter_);
    const auto &locations = navigation_.at("locations");
    cyui::textDim("%zu / %zu %s", matches.size(), locations.size(), tr("matches"));
    if (navigation_.value("invalid", size_t(0)))
        cyui::textDim("%s: %zu", tr("Invalid locations ignored"), navigation_.at("invalid").get<size_t>());
    if (navigation_.value("outsideWorkspace", size_t(0)))
        cyui::textDim("%s: %zu", tr("Unavailable locations"),
                      navigation_.at("outsideWorkspace").get<size_t>());
    if (navigation_.value("truncated", false))
        cyui::textDim("%s", tr("Results limited"));
    bool moved = false;
    ImGui::BeginDisabled(matches.empty());
    if (cyui::button(tr("Previous location"))) {
        navigationSelected_ = navigationModel_.adjacent(navigationFilter_, navigationSelected_, true);
        moved = true;
    }
    ImGui::SameLine();
    if (cyui::button(tr("Next location"))) {
        navigationSelected_ = navigationModel_.adjacent(navigationFilter_, navigationSelected_, false);
        moved = true;
    }
    ImGui::SameLine();
    if (cyui::button(tr("Copy locations"))) {
        std::string text;
        for (const auto index : matches) {
            text += navigationModel_.label(index);
            text += '\n';
        }
        ImGui::SetClipboardText(text.c_str());
    }
    ImGui::EndDisabled();
    const bool keys =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_NoPopupHierarchy) &&
        !ImGui::IsAnyItemActive();
    if (keys && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) {
        navigationSelected_ = navigationModel_.adjacent(navigationFilter_, navigationSelected_,
                                                        ImGui::IsKeyPressed(ImGuiKey_UpArrow));
        moved = true;
    }
    const bool enter = keys && ImGui::IsKeyPressed(ImGuiKey_Enter);
    if (std::find(matches.begin(), matches.end(), static_cast<size_t>(navigationSelected_)) == matches.end())
        navigationSelected_ = matches.empty() ? -1 : static_cast<int>(matches.front());
    int chosen = -1;
    ImGui::BeginChild("##navigation-locations", {}, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(matches.size()));
    const auto selected = std::find(matches.begin(), matches.end(), static_cast<size_t>(navigationSelected_));
    if ((moved || enter) && selected != matches.end())
        clipper.IncludeItemByIndex(static_cast<int>(selected - matches.begin()));
    while (clipper.Step())
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const auto index = matches.at(static_cast<size_t>(row));
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Selectable(navigationModel_.label(index).c_str(),
                                  navigationSelected_ == static_cast<int>(index))) {
                navigationSelected_ = static_cast<int>(index);
                if (current)
                    chosen = static_cast<int>(index);
            }
            if (enter && current && navigationSelected_ == static_cast<int>(index))
                chosen = static_cast<int>(index);
            if (moved && navigationSelected_ == static_cast<int>(index))
                ImGui::SetScrollHereY();
            ImGui::PopID();
        }
    if (matches.empty())
        cyui::textDim("%s", tr("No matching locations"));
    ImGui::EndChild();
    ImGui::End();
    if (chosen >= 0) {
        const auto &row = locations.at(static_cast<size_t>(chosen));
        const auto &start = row.at("range").at("start");
        open(row.at("path"), start.at("line").get<size_t>() + 1, start.at("character").get<size_t>() + 1,
             true);
    }
}
} // namespace cycode
