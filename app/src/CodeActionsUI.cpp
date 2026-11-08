#include "cycode/Application.h"
#include <cyui/Core.h>
#include <algorithm>
namespace cycode {
Json Application::codeActionParameters() const {
    for (const auto &tab : tabs_) if (tab->path==active_ && !tab->readOnly && engine_->supportsLanguage(tab->path)) {
        const auto selection=tab->editor.GetMainCursorSelection();
        auto position=[&](TextEditor::DocPos p) { return Json{{"line",p.line},{"character",LanguageClient::utf16Length(tab->editor.GetSectionText({p.line,0},p))}}; };
        Json params={{"path",tab->path},{"expectedRevision",tab->revision},{"range",{{"start",position(selection.start)},{"end",position(selection.end)}}}};
        if (codeActionsFilter_) params["only"]=Json::array({codeActionsFilter_==1 ? "quickfix" : codeActionsFilter_==2 ? "refactor" : "source.organizeImports"});
        return params;
    }
    return nullptr;
}
void Application::requestCodeActions() {
    if (codeActionsFuture_.valid() || refactorFuture_.valid()) return;
    for (const auto &tab : tabs_) if (tab->path==active_) tab->snippet.cancel();
    const auto params=codeActionParameters();if (params.is_null()) return;
    codeActionsKey_=params.dump();codeActionsState_=Json::object();codeActionsError_.clear();codeActionSelected_=-1;
    showPanel("Code actions","codeActions");auto engine=engine_;
    codeActionsFuture_=std::async(std::launch::async,[engine,params] { return engine->dispatch("language.codeActions",params); });
}
void Application::updateCodeActions() {
    if (!codeActionsFuture_.valid() || codeActionsFuture_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
    try {
        auto result=codeActionsFuture_.get();const auto current=codeActionParameters();
        if (current.is_null() || current.dump()!=codeActionsKey_) { codeActionsError_=tr("Code action context changed. Refresh the list.");return; }
        codeActionsState_=std::move(result);
        for (size_t i=0;i<codeActionsState_.at("actions").size();++i) {
            const auto &action=codeActionsState_.at("actions")[i];
            if (action.at("available")==true && (codeActionSelected_<0 || action.at("preferred")==true)) { codeActionSelected_=static_cast<int>(i);if (action.at("preferred")==true) break; }
        }
    } catch (const std::exception &e) { codeActionsError_=e.what(); }
}
void Application::codeActionsPanel() {
    if (!beginPanel("Code actions","codeActions")) return;
    const auto current=codeActionParameters();const bool stale=current.is_null() || codeActionsKey_!=current.dump();
    ImGui::BeginDisabled(codeActionsFuture_.valid() || refactorFuture_.valid() || current.is_null());
    if (cyui::iconButton(cyui::icon::refresh,tr("Refresh code actions"))) requestCodeActions();
    ImGui::SameLine();ImGui::SetNextItemWidth(cyui::dp(180));
    const char *filters[]={tr("All context actions"),tr("Quick fixes"),tr("Refactorings"),tr("Organize imports")};
    if (ImGui::Combo("##code-action-kind",&codeActionsFilter_,filters,4)) requestCodeActions();
    ImGui::EndDisabled();
    if (!codeActionsError_.empty()) cyui::callout(cyui::Tone::warning,codeActionsError_.c_str());
    if (codeActionsFuture_.valid()) { cyui::spinner(tr("Loading code actions"));ImGui::End();return; }
    if (!codeActionsState_.contains("actions")) { cyui::emptyState(tr("Code actions"),tr("Request fixes or refactorings at the cursor or selection."),cyui::icon::code);ImGui::End();return; }
    const auto &actions=codeActionsState_.at("actions");
    cyui::textDim("%s",codeActionsState_.value("path","").c_str());
    if (stale) cyui::callout(cyui::Tone::warning,tr("Code action context changed. Refresh the list."));
    if (actions.empty()) { cyui::emptyState(tr("Code actions"),tr("No code actions at this position."),cyui::icon::code);ImGui::End();return; }
    if (ImGui::BeginTable("##code-actions",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable,ImVec2(0,cyui::dp(240)))) {
        ImGui::TableSetupColumn(tr("Action"),ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn(tr("Kind"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(160));ImGui::TableHeadersRow();
        for (size_t i=0;i<actions.size();++i) {
            const auto &action=actions[i];ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();ImGui::TableNextColumn();
            if (ImGui::Selectable(action.at("title").get_ref<const std::string&>().c_str(),codeActionSelected_==i,ImGuiSelectableFlags_SpanAllColumns)) codeActionSelected_=static_cast<int>(i);
            if (ImGui::IsItemHovered() && !action.at("reason").get_ref<const std::string&>().empty()) cyui::tooltip(action.at("reason").get_ref<const std::string&>().c_str());
            ImGui::TableNextColumn();cyui::textDim("%s",action.value("kind","").c_str());ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !actions.empty()) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) codeActionSelected_=std::min(static_cast<int>(actions.size())-1,codeActionSelected_+1);
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) codeActionSelected_=std::max(0,codeActionSelected_-1);
    }
    const bool selected=codeActionSelected_>=0 && codeActionSelected_<actions.size();
    const bool enabled=selected && actions[codeActionSelected_].at("available")==true && !stale && !refactorFuture_.valid();
    if (selected && !actions[codeActionSelected_].at("reason").get_ref<const std::string&>().empty()) cyui::callout(cyui::Tone::warning,actions[codeActionSelected_].at("reason").get_ref<const std::string&>().c_str());
    auto prepare=[&](bool apply) {
        codeActionApply_=apply;
        changeTask("language.codeActionPrepare",{{"sessionId",codeActionsState_.at("sessionId")},{"item",actions[codeActionSelected_].at("index")},{"expectedRevision",codeActionsState_.at("revision")}});
    };
    ImGui::BeginDisabled(!enabled);
    if (cyui::primaryButton(tr("Apply correction")) || (enabled && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Enter,false))) prepare(true);
    ImGui::SameLine();if (cyui::button(tr("Preview changes"))) prepare(false);
    ImGui::EndDisabled();
    if (codeActionsState_.value("truncated",false)) cyui::badge(tr("Code action list truncated"),cyui::Tone::warning);
    ImGui::End();
}
}
