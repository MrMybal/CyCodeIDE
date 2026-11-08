#include "cycode/Application.h"
#include "cycode/StepInTargets.h"
#include <cyui/Core.h>

namespace cycode {
void Application::debugTargetTable(const Json &targets, std::string &selected, const char *id,
                                   const char *heading) {
    if (targets.empty() ||
        !ImGui::BeginTable(id, 2,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY,
                           ImVec2(0, cyui::dp(220))))
        return;
    ImGui::TableSetupColumn(tr(heading), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(tr("Reported location"), ImGuiTableColumnFlags_WidthFixed, cyui::dp(160));
    ImGui::TableHeadersRow();
    for (const auto &target : targets) {
        const auto &handle = target.at("id").get_ref<const std::string &>();
        ImGui::PushID(handle.c_str());
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        const auto position = ImGui::GetCursorScreenPos();
        const auto &label = target.at("label").get_ref<const std::string &>();
        bool hovered = false;
        {
            cyui::ScopedDisabled disabled(debugFuture_.valid());
            if (ImGui::Selectable("##target", selected == handle, ImGuiSelectableFlags_SpanAllColumns))
                selected = handle;
            hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal);
        }
        ImGui::SetCursorScreenPos(position);
        const auto newline = label.find_first_of("\r\n");
        ImGui::TextUnformatted(label.data(),
                               label.data() + (newline == std::string::npos ? label.size() : newline));
        if (hovered) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(cyui::dp(500));
            ImGui::TextUnformatted(label.c_str());
            if (target.contains("instructionPointerReference"))
                ImGui::TextUnformatted(
                    target.at("instructionPointerReference").get_ref<const std::string &>().c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::TableNextColumn();
        if (target.value("line", uint64_t(0)) > 0)
            ImGui::Text("%s %llu", tr("Line"), target.at("line").get<unsigned long long>());
        if (target.value("column", uint64_t(0)) > 0)
            cyui::textDim("%s %llu", tr("Column"), target.at("column").get<unsigned long long>());
        ImGui::PopID();
    }
    ImGui::EndTable();
}
void Application::requestStepInTargets() {
    if (smoke_ || debugFuture_.valid())
        return;
    const auto parameters = StepInTargets::context(debugState_);
    debugStepTargets_ = Json::object();
    debugStepTarget_.clear();
    debugStepTargetsTabSelect_ = true;
    debugTask("debug.stepInTargets", parameters);
}
void Application::stepInTargetsTab() {
    const auto flags = debugStepTargetsTabSelect_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (!ImGui::BeginTabItem(cyui::trLabel("Step-in targets").c_str(), nullptr, flags))
        return;
    debugStepTargetsTabSelect_ = false;
    ImGui::PushTextWrapPos(0.0f);
    const bool busy = debugFuture_.valid();
    if (!debugState_.value("capabilities", Json::object()).value("supportsStepInTargetsRequest", false))
        cyui::textDim("%s", tr("This debug adapter does not advertise step-in targets."));
    else
        cyui::textDim("%s", tr("Pause on a line containing calls, read the available targets, then select "
                               "the function to enter. Targets expire when the debug context changes."));
    {
        cyui::ScopedDisabled disabled(!commandEnabled("debug.stepInTargets"));
        if (cyui::ghostButton(cyui::trLabel("Read step-in targets").c_str()))
            requestStepInTargets();
    }
    if (!debugStepTargets_.empty() && StepInTargets::current(debugStepTargets_, debugState_)) {
        const auto &targets = debugStepTargets_.at("targets");
        if (targets.empty())
            cyui::textDim("%s", tr("The debugger reported no step-in targets for this frame."));
        debugTargetTable(targets, debugStepTarget_, "##step-in-targets", "Function");
        {
            cyui::ScopedDisabled disabled(busy || debugStepTarget_.empty());
            if (cyui::primaryButton(cyui::trLabel("Step into selected target").c_str())) {
                Json parameters = debugStepTargets_;
                parameters.erase("targets");
                parameters.erase("catalogId");
                parameters["target"] = debugStepTarget_;
                debugTask("debug.stepIntoTarget", std::move(parameters));
            }
        }
        cyui::textDim("%s", tr("The stepping options in Execution controls also apply to this action."));
    } else if (!busy)
        cyui::textDim("%s", tr("Read targets again after changing the selected frame or thread."));
    ImGui::PopTextWrapPos();
    ImGui::EndTabItem();
}
} // namespace cycode
