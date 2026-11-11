#include "cycode/Application.h"
#include <cyui/Core.h>
#include <algorithm>
namespace cycode {
Json Application::codeActionParameters() const {
    for (const auto &tab : tabs_) if (tab->path==active_ && !tab->pagedProfile && !tab->readOnly && engine_->supportsLanguage(tab->path)) {
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
    if (!beginPanel("Code actions", "codeActions"))
        return;
    const auto current = codeActionParameters();
    const bool stale = current.is_null() || codeActionsKey_ != current.dump();
    {
        cyui::ScopedDisabled disabled(codeActionsFuture_.valid() || refactorFuture_.valid() ||
                                      current.is_null());
        if (cyui::iconButton(cyui::icon::refresh, tr("Refresh code actions")))
            requestCodeActions();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(cyui::dp(180));
        const char *filters[] = {tr("All context actions"), tr("Quick fixes"), tr("Refactorings"),
                                 tr("Organize imports")};
        if (ImGui::Combo("##code-action-kind", &codeActionsFilter_, filters, 4))
            requestCodeActions();
    }
    viewFilter("##code-actions-filter", codeActionsTextFilter_);
    if (!codeActionsError_.empty())
        cyui::callout(cyui::Tone::warning, codeActionsError_.c_str());
    if (codeActionsFuture_.valid()) {
        cyui::spinner("##loading-code-actions");
        ImGui::End();
        return;
    }
    if (!codeActionsState_.contains("actions")) {
        cyui::emptyState(tr("Code actions"), tr("Request fixes or refactorings at the cursor or selection."),
                         cyui::icon::code);
        ImGui::End();
        return;
    }
    const auto &actions = codeActionsState_.at("actions");
    cyui::textDim("%s", codeActionsState_.value("path", "").c_str());
    if (stale)
        cyui::callout(cyui::Tone::warning, tr("Code action context changed. Refresh the list."));
    if (codeActionsState_.value("truncated", false))
        cyui::badge(tr("Code action list truncated"), cyui::Tone::warning);
    if (actions.empty()) {
        cyui::emptyState(tr("Code actions"), tr("No code actions at this position."), cyui::icon::code);
        ImGui::End();
        return;
    }
    std::vector<size_t> matches;
    for (size_t i = 0; i < actions.size(); ++i) {
        const auto &action = actions[i];
        if (codeActionsTextFilter_.matches(action.at("title").get_ref<const std::string &>(),
                                           action.value("kind", "") + " " + action.value("reason", "")))
            matches.push_back(i);
    }
    auto selected = std::find(matches.begin(), matches.end(), static_cast<size_t>(codeActionSelected_));
    if (selected == matches.end()) {
        codeActionSelected_ = -1;
        for (const auto index : matches)
            if (actions[index].at("available") == true) {
                codeActionSelected_ = static_cast<int>(index);
                break;
            }
        if (codeActionSelected_ < 0 && !matches.empty())
            codeActionSelected_ = static_cast<int>(matches.front());
        selected = std::find(matches.begin(), matches.end(), static_cast<size_t>(codeActionSelected_));
    }
    cyui::textDim("%zu / %zu", matches.size(), actions.size());
    if (matches.empty()) {
        cyui::emptyState(tr("No matching code actions"), tr("Change or clear the filter to show context actions."),
                         cyui::icon::filter);
        ImGui::End();
        return;
    }
    // Search fields, combo popups and other active controls own their keystrokes.
    const bool keys =
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_NoPopupHierarchy) &&
        !ImGui::IsAnyItemActive() && !ImGui::GetIO().WantTextInput &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    bool moved = false;
    if (keys && (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow))) {
        auto index = static_cast<size_t>(selected - matches.begin());
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
            index = std::min(index + 1, matches.size() - 1);
        else if (index > 0)
            --index;
        codeActionSelected_ = static_cast<int>(matches[index]);
        selected = matches.begin() + static_cast<std::ptrdiff_t>(index);
        moved = true;
    }
    if (ImGui::BeginTable("##code-actions", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                          cyui::dp(0, 240))) {
        ImGui::TableSetupColumn(tr("Action"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(tr("Kind"), ImGuiTableColumnFlags_WidthFixed, cyui::dp(160));
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(matches.size()));
        if (moved)
            clipper.IncludeItemByIndex(static_cast<int>(selected - matches.begin()));
        while (clipper.Step())
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto index = matches[static_cast<size_t>(row)];
                const auto &action = actions[index];
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const auto pos = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##action", codeActionSelected_ == static_cast<int>(index),
                                      ImGuiSelectableFlags_SpanAllColumns))
                    codeActionSelected_ = static_cast<int>(index);
                ImGui::GetWindowDrawList()->AddText(pos, cyui::palette().text,
                                                   action.at("title").get_ref<const std::string &>().c_str());
                if (ImGui::IsItemHovered() && !action.at("reason").get_ref<const std::string &>().empty())
                    cyui::tooltip(action.at("reason").get_ref<const std::string &>().c_str());
                if (moved && codeActionSelected_ == static_cast<int>(index))
                    ImGui::SetScrollHereY();
                ImGui::TableNextColumn();
                cyui::textDim("%s", action.value("kind", "").c_str());
                ImGui::PopID();
            }
        ImGui::EndTable();
    }
    const bool hasSelection = codeActionSelected_ >= 0 &&
                              static_cast<size_t>(codeActionSelected_) < actions.size();
    const bool enabled = hasSelection && actions[codeActionSelected_].at("available") == true &&
                         !stale && !refactorFuture_.valid();
    if (hasSelection && !actions[codeActionSelected_].at("reason").get_ref<const std::string &>().empty())
        cyui::callout(cyui::Tone::warning, actions[codeActionSelected_].at("reason").get_ref<const std::string &>().c_str());
    auto prepare = [&](bool apply) {
        codeActionApply_ = apply;
        changeTask("language.codeActionPrepare",
                   {{"sessionId", codeActionsState_.at("sessionId")},
                    {"item", actions[codeActionSelected_].at("index")},
                    {"expectedRevision", codeActionsState_.at("revision")}});
    };
    {
        cyui::ScopedDisabled disabled(!enabled);
        const bool apply = cyui::primaryButton(tr("Apply correction"));
        ImGui::SameLine();
        const bool preview = cyui::button(tr("Preview changes"));
        const bool previewFocused = ImGui::IsItemFocused();
        // Activating Preview keeps its intent even when Enter was pressed.
        if (preview)
            prepare(false);
        else if (apply || (enabled && keys && !previewFocused &&
                           ImGui::IsKeyPressed(ImGuiKey_Enter, false)))
            prepare(true);
    }
    ImGui::End();
}
} // namespace cycode
