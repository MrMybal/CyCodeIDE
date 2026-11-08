#include "cycode/Application.h"
#include "cycode/InstructionBreakpoints.h"
#include <cfloat>
#include <cyui/Core.h>
#include <imgui_stdlib.h>

namespace cycode {
void Application::beginInstructionBreakpointDraft() {
    if (debugState_.value("state", "") != "stopped" || debugFuture_.valid())
        return;
    debugInstructionDraft_ =
        InstructionBreakpoints::draft(debugState_.value("instructionBreakpoints", Json::array()));
    debugInstructionRowIds_.clear();
    for (size_t index = 0; index < debugInstructionDraft_.size(); ++index)
        debugInstructionRowIds_.push_back(++debugInstructionRowSerial_);
    debugInstructionContext_ = {
        {"sessionGeneration", debugState_.value("sessionGeneration", uint64_t(0))},
        {"stopGeneration", debugState_.value("stopGeneration", uint64_t(0))},
        {"expectedRevision", debugState_.value("instructionBreakpointRevision", uint64_t(0))}};
}
void Application::addInstructionBreakpoint(Json point) {
    if (debugState_.value("state", "") != "stopped" || debugFuture_.valid())
        return;
    try {
        if (debugInstructionContext_.empty())
            beginInstructionBreakpointDraft();
        if (debugInstructionContext_.at("sessionGeneration") != debugState_.at("sessionGeneration") ||
            debugInstructionContext_.at("stopGeneration") != debugState_.at("stopGeneration"))
            throw std::runtime_error(
                "StaleDebugReference: start a new instruction breakpoint replacement list");
        auto next = debugInstructionDraft_;
        next.push_back(std::move(point));
        const auto plan =
            InstructionBreakpoints::plan(next, debugState_.value("capabilities", Json::object()));
        debugInstructionDraft_ = InstructionBreakpoints::draft(plan.at("entries"));
        debugInstructionRowIds_.push_back(++debugInstructionRowSerial_);
        debugInstructionTabSelect_ = true;
    } catch (const std::exception &e) {
        error(e.what());
    }
}
void Application::instructionBreakpointTab() {
    const auto flags = debugInstructionTabSelect_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    debugInstructionTabSelect_ = false;
    if (!ImGui::BeginTabItem(cyui::trLabel("Instruction breakpoints").c_str(), nullptr, flags))
        return;
    ImGui::PushTextWrapPos(0.0f);
    const auto caps = debugState_.value("capabilities", Json::object());
    const bool paused = debugState_.value("state", "") == "stopped",
               supported = caps.value("supportsInstructionBreakpoints", false);
    const bool editable = paused && supported && !debugFuture_.valid();
    if (!supported)
        cyui::textDim("%s", tr("This debug adapter does not advertise instruction breakpoints."));
    if (!paused)
        cyui::textDim("%s", tr("Pause the program to edit instruction breakpoints."));
    cyui::textDim(
        "%s", tr("Right-click an instruction address in Disassembly, or enter an adapter reference below. "
                 "Changes replace the complete session list; review all entries before applying."));
    {
        cyui::ScopedDisabled disabled(!editable);
        if (cyui::ghostButton(cyui::trLabel("New instruction replacement list").c_str()))
            beginInstructionBreakpointDraft();
        ImGui::SameLine();
        if (cyui::ghostButton(cyui::trLabel("Clear instruction breakpoints").c_str()))
            debugTask("debug.instructionBreakpoints",
                      {{"sessionGeneration", debugState_.at("sessionGeneration")},
                       {"stopGeneration", debugState_.at("stopGeneration")},
                       {"expectedRevision", debugState_.at("instructionBreakpointRevision")},
                       {"breakpoints", Json::array()}});
    }
    {
        cyui::ScopedDisabled disabled(!editable);
        if (cyui::beginProperties("##instruction-breakpoint-entry")) {
            cyui::property(tr("Instruction reference"));
            ImGui::InputText("##instruction-point-reference", &debugInstructionPointReference_);
            cyui::property(tr("Byte offset"));
            ImGui::InputInt("##instruction-byte-offset", &debugInstructionByteOffset_);
            cyui::endProperties();
        }
        if (cyui::ghostButton(cyui::trLabel("Use selected frame instruction").c_str()))
            for (const auto &frame : debugState_.value("frames", Json::array()))
                if (frame.at("id") == debugState_.at("frameId")) {
                    debugInstructionPointReference_ = frame.value("instructionPointerReference", "");
                    debugInstructionByteOffset_ = 0;
                }
        ImGui::SameLine();
        {
            cyui::ScopedDisabled empty(debugInstructionPointReference_.empty());
            if (cyui::ghostButton(cyui::trLabel("Add instruction to replacement").c_str()))
                addInstructionBreakpoint({{"instructionReference", debugInstructionPointReference_},
                                          {"offset", debugInstructionByteOffset_}});
        }
    }
    const bool current =
        !debugInstructionContext_.empty() && paused &&
        debugInstructionContext_.at("sessionGeneration") == debugState_.at("sessionGeneration") &&
        debugInstructionContext_.at("stopGeneration") == debugState_.at("stopGeneration") &&
        debugInstructionContext_.at("expectedRevision") == debugState_.at("instructionBreakpointRevision");
    if (!debugInstructionContext_.empty()) {
        cyui::sectionTitle(tr("Instruction replacement draft"));
        if (!current)
            cyui::textColoured(
                cyui::palette().warning, "%s",
                tr("The instruction list changed. Start a new replacement list before applying."));
        int remove = -1;
        {
            cyui::ScopedDisabled disabled(!editable || !current);
            if (ImGui::BeginTable("##instruction-draft", 6,
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                                  cyui::dp(-1, 200))) {
                for (const auto *label : {"Enabled", "Instruction reference", "Byte offset", "Condition",
                                          "Hit condition", "Remove"})
                    ImGui::TableSetupColumn(tr(label));
                ImGui::TableHeadersRow();
                for (size_t index = 0; index < debugInstructionDraft_.size(); ++index) {
                    const auto rowId = std::to_string(debugInstructionRowIds_.at(index));
                    ImGui::PushID(rowId.c_str());
                    auto &point = debugInstructionDraft_[index];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    bool enabled = point.value("enabled", true);
                    if (ImGui::Checkbox("##instruction-enabled", &enabled))
                        point["enabled"] = enabled;
                    ImGui::TableNextColumn();
                    auto reference = point.at("instructionReference").get<std::string>();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::InputText("##instruction-reference", &reference))
                        point["instructionReference"] = reference;
                    ImGui::TableNextColumn();
                    int offset = point.value("offset", 0);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::InputInt("##instruction-offset", &offset))
                        point["offset"] = offset;
                    ImGui::TableNextColumn();
                    {
                        cyui::ScopedDisabled unsupported(
                            enabled && !caps.value("supportsConditionalBreakpoints", false));
                        auto condition = point.value("condition", std::string{});
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::InputText("##instruction-condition", &condition))
                            point["condition"] = condition;
                    }
                    ImGui::TableNextColumn();
                    {
                        cyui::ScopedDisabled unsupported(
                            enabled && !caps.value("supportsHitConditionalBreakpoints", false));
                        auto hit = point.value("hitCondition", std::string{});
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::InputText("##instruction-hit", &hit))
                            point["hitCondition"] = hit;
                    }
                    ImGui::TableNextColumn();
                    if (cyui::ghostButton(cyui::trLabel("Remove").c_str()))
                        remove = int(index);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (remove >= 0) {
                debugInstructionDraft_.erase(debugInstructionDraft_.begin() + remove);
                debugInstructionRowIds_.erase(debugInstructionRowIds_.begin() + remove);
            }
            if (cyui::primaryButton(cyui::trLabel("Apply instruction breakpoints").c_str())) {
                try {
                    (void)InstructionBreakpoints::plan(debugInstructionDraft_, caps);
                    auto request = debugInstructionContext_;
                    request["breakpoints"] = debugInstructionDraft_;
                    debugTask("debug.instructionBreakpoints", std::move(request));
                } catch (const std::exception &e) {
                    error(e.what());
                }
            }
        }
    }
    cyui::sectionTitle(tr("Instruction breakpoint status"));
    if (!debugState_.value("instructionBreakpointsSynchronized", true))
        cyui::textColoured(
            cyui::palette().warning, "%s",
            tr("Instruction breakpoint state is uncertain. Pause and replace or clear the full list."));
    const auto active = debugState_.value("instructionBreakpoints", Json::array());
    if (active.empty())
        cyui::textDim("%s", tr("No instruction breakpoints are confirmed in this session."));
    if (!active.empty() &&
        ImGui::BeginTable("##instruction-status", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        for (const auto *label : {"Instruction reference", "Byte offset", "Verification", "Message"})
            ImGui::TableSetupColumn(tr(label));
        ImGui::TableHeadersRow();
        for (const auto &point : active) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(point.at("instructionReference").get_ref<const std::string &>().c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", point.at("offset").get<int>());
            ImGui::TableNextColumn();
            const auto verified = point.at("verification");
            ImGui::TextUnformatted(tr(!point.at("enabled").get<bool>()    ? "Disabled"
                                      : verified.value("verified", false) ? "Verified"
                                                                          : "Not verified"));
            ImGui::TableNextColumn();
            if (verified.is_object()) {
                ImGui::TextWrapped("%s", verified.at("message").get_ref<const std::string &>().c_str());
                if (verified.contains("instructionReference"))
                    ImGui::TextWrapped(
                        "%s: %s", tr("Actual instruction location"),
                        verified.at("instructionReference").get_ref<const std::string &>().c_str());
                if (verified.contains("offset"))
                    ImGui::Text("%s: %lld", tr("Actual byte offset"),
                                static_cast<long long>(verified.at("offset").get<int64_t>()));
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopTextWrapPos();
    ImGui::EndTabItem();
}
} // namespace cycode
