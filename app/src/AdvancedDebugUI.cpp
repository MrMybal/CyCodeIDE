#include "cycode/Application.h"
#include <algorithm>
#include <cyui/Core.h>
#include <imgui_stdlib.h>
#include <iomanip>
#include <sstream>
#include "cycode/DebugMutations.h"

namespace cycode {
void Application::advancedDebugTabs() {
    debugExecutionTab();
    stepInTargetsTab();
    gotoTargetsTab();
    runToCursorTab();
    debugValueEditTab();
    dataBreakpointTab();
    instructionBreakpointTab();
    const auto caps = debugState_.value("capabilities", Json::object());
    const auto phase = debugState_.value("state", std::string("idle"));
    const bool stopped = phase == "stopped", busy = debugFuture_.valid();
    const bool live = phase == "running" || stopped || phase == "starting";
    if (ImGui::BeginTabItem(tr("Scopes / Registers"))) {
        cyui::textDim("%s", tr("Register scopes appear when provided by the selected debug adapter."));
        cyui::ScopedDisabled disabled(!stopped || busy);
        if (ImGui::BeginCombo("##inspection-scope", tr("Select a scope"))) {
            for (const auto &scope : debugState_.value("scopes", Json::array())) {
                const int reference = scope.value("variablesReference", 0);
                if (reference > 0 &&
                    ImGui::Selectable(scope.value("name", "").c_str(), reference == debugScopeReference_)) {
                    debugScopeReference_ = reference;
                    debugScopeStart_ = 0;
                    debugScopePage_ = Json::object();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::BeginCombo("##inspection-filter",
                              debugScopeFilter_.empty() ? tr("All variables") : debugScopeFilter_.c_str())) {
            for (const auto *filter : {"", "named", "indexed"})
                if (ImGui::Selectable(*filter ? filter : tr("All variables"), debugScopeFilter_ == filter)) {
                    debugScopeFilter_ = filter;
                    debugScopeStart_ = 0;
                    debugScopePage_ = Json::object();
                }
            ImGui::EndCombo();
        }
        auto fetch = [&] {
            if (debugScopeReference_ > 0) {
                debugVariableReference_ = 0;
                Json request = {{"variablesReference", debugScopeReference_},
                                {"start", debugScopeStart_},
                                {"count", 200}};
                if (!debugScopeFilter_.empty())
                    request["filter"] = debugScopeFilter_;
                debugTask("debug.variables", request);
            }
        };
        {
            cyui::ScopedDisabled noScope(debugScopeReference_ <= 0);
            if (cyui::button(tr("Read scope page")))
                fetch();
        }
        ImGui::SameLine();
        {
            cyui::ScopedDisabled first(debugScopeStart_ == 0);
            if (cyui::ghostButton(tr("Previous page"))) {
                debugScopeStart_ = std::max(0, debugScopeStart_ - 200);
                fetch();
            }
        }
        ImGui::SameLine();
        {
            cyui::ScopedDisabled last(!debugScopePage_.value("hasMore", false));
            if (cyui::ghostButton(tr("Next page"))) {
                debugScopeStart_ += 200;
                fetch();
            }
        }
        if (ImGui::BeginTable("##scope-page", 3,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
            for (const auto *name : {"Name", "Value", "Type"})
                ImGui::TableSetupColumn(tr(name));
            ImGui::TableHeadersRow();
            int variableIndex = 0;
            for (const auto &item : debugScopePage_.value("variables", Json::array())) {
                ImGui::PushID(variableIndex++);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(item.value("name", "").c_str());
                if (ImGui::BeginPopupContextItem("##scope-value-actions")) {
                    const auto editMethod = debugVariableEditMethod(item,caps);
                    const bool editable = !editMethod.empty() && (editMethod != "debug.setVariable" || debugScopeReference_ > 0);
                    if (ImGui::MenuItem(cyui::trLabel("Change value").c_str(),nullptr,false,editable)) beginDebugValueEdit(item,debugScopeReference_);
                    ImGui::EndPopup();
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(item.value("value", "").c_str());
                ImGui::TableNextColumn();
                cyui::textDim("%s", item.value("type", "").c_str());
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tr("Memory"))) {
        const bool supported = caps.value("supportsReadMemoryRequest", false);
        if (!supported)
            cyui::textDim("%s", tr("This debug adapter does not advertise memory reads."));
        ImGui::InputText(tr("Memory reference"), &debugMemoryReference_);
        {
            cyui::ScopedDisabled unavailable(debugEvaluationMemory_.empty());
            if (cyui::ghostButton(tr("Use evaluated memory reference")))
                debugMemoryReference_ = debugEvaluationMemory_;
        }
        ImGui::SetNextItemWidth(cyui::dp(180));
        ImGui::InputInt(tr("Byte offset"), &debugMemoryOffset_);
        ImGui::SetNextItemWidth(cyui::dp(180));
        ImGui::InputInt(tr("Bytes to read"), &debugMemoryCount_);
        {
            cyui::ScopedDisabled disabled(busy || !stopped || !supported || debugMemoryReference_.empty() ||
                                          debugMemoryCount_ < 1 || debugMemoryCount_ > 4096 ||
                                          debugMemoryOffset_ < -1048576 || debugMemoryOffset_ > 1048576);
            if (cyui::button(tr("Read memory")))
                debugTask("debug.memory", {{"memoryReference", debugMemoryReference_},
                                           {"offset", debugMemoryOffset_},
                                           {"count", debugMemoryCount_}});
        }
        debugMemoryWriteControls();
        if (debugMemory_.contains("address")) {
            cyui::textDim("%s: %s", tr("Address"),
                          debugMemory_.at("address").get_ref<const std::string &>().c_str());
            cyui::textDim("%s: %d", tr("Unreadable bytes"), debugMemory_.value("unreadableBytes", 0));
            const auto bytes = debugMemory_.value("bytes", Json::array());
            std::ostringstream out;
            for (size_t base = 0; base < bytes.size(); base += 16) {
                out << '+' << std::hex << std::setw(4) << std::setfill('0') << base << "  ";
                std::string ascii;
                for (size_t j = 0; j < 16; ++j)
                    if (base + j < bytes.size()) {
                        const int value = bytes[base + j];
                        out << std::setw(2) << value << ' ';
                        ascii += value >= 32 && value < 127 ? char(value) : '.';
                    } else
                        out << "   ";
                out << " |" << ascii << "|\n";
            }
            auto rendered = out.str();
            cyui::ScopedFont mono(cyui::monoFont());
            ImGui::InputTextMultiline("##memory-bytes", &rendered, {-1, -1}, ImGuiInputTextFlags_ReadOnly);
        }
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tr("Disassembly"))) {
        const bool supported = caps.value("supportsDisassembleRequest", false);
        if (!supported)
            cyui::textDim("%s", tr("This debug adapter does not advertise disassembly."));
        ImGui::InputText(tr("Instruction reference"), &debugInstructionReference_);
        {
            cyui::ScopedDisabled disabled(!stopped || busy);
            if (cyui::ghostButton(tr("Use current instruction")))
                for (const auto &frame : debugState_.value("frames", Json::array()))
                    if (frame.value("id", 0) == debugState_.value("frameId", 0))
                        debugInstructionReference_ =
                            frame.value("instructionPointerReference", std::string{});
        }
        ImGui::SetNextItemWidth(cyui::dp(180));
        ImGui::InputInt(tr("Instruction offset"), &debugInstructionOffset_);
        {
            cyui::ScopedDisabled disabled(busy || !stopped || !supported ||
                                          debugInstructionReference_.empty() ||
                                          debugInstructionOffset_ < -4096 || debugInstructionOffset_ > 4096);
            if (cyui::button(tr("Read instructions")))
                debugTask("debug.disassemble", {{"memoryReference", debugInstructionReference_},
                                                {"instructionOffset", debugInstructionOffset_},
                                                {"instructionCount", 64}});
        }
        if (ImGui::BeginTable("##instructions", 4,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
            for (const auto *name : {"Address", "Bytes", "Instruction", "Symbol"})
                ImGui::TableSetupColumn(tr(name));
            ImGui::TableHeadersRow();
            int index = 0;
            for (const auto &line : debugAssembly_.value("instructions", Json::array())) {
                ImGui::PushID(line.at("address").get_ref<const std::string &>().c_str());
                ImGui::PushID(index++);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(line.value("address", "").c_str());
                if (ImGui::BeginPopupContextItem("##instruction-breakpoint-actions")) {
                    const bool current = stopped && !busy && debugAssemblyRead_.value("ready",false) &&
                        debugAssemblyRead_.at("sessionGeneration") == debugState_.at("sessionGeneration") && debugAssemblyRead_.at("stopGeneration") == debugState_.at("stopGeneration");
                    const bool available = current && caps.value("supportsInstructionBreakpoints",false) && line.value("presentationHint","") != "invalid";
                    if (ImGui::MenuItem(cyui::trLabel("Add instruction breakpoint").c_str(),nullptr,false,available)) addInstructionBreakpoint({{"instructionReference",line.at("address")}});
                    if (ImGui::MenuItem(cyui::trLabel("Copy instruction address").c_str())) ImGui::SetClipboardText(line.at("address").get_ref<const std::string &>().c_str());
                    ImGui::EndPopup();
                }
                ImGui::TableNextColumn();
                cyui::textDim("%s", line.value("instructionBytes", "").c_str());
                ImGui::TableNextColumn();
                if (ImGui::Selectable(line.value("instruction", "").c_str()) && line.contains("location"))
                    debugFrame({{"source", line.at("location")},
                                {"line", line.value("line", 1)},
                                {"column", line.value("column", 1)}});
                ImGui::TableNextColumn();
                cyui::textDim("%s", line.value("symbol", "").c_str());
                ImGui::PopID();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tr("Function / Exception breakpoints"))) {
        const auto functions = debugState_.value("functionBreakpoints", Json::array());
        const bool functionSupport = !live || caps.value("supportsFunctionBreakpoints", false);
        if (!functionSupport)
            cyui::textDim("%s", tr("This debug adapter does not advertise function breakpoints."));
        if (cyui::beginSection(tr("Function breakpoints"))) {
            ImGui::InputText(tr("Function name"), &debugFunctionName_);
            ImGui::InputText(tr("Condition"), &debugFunctionCondition_);
            ImGui::InputText(tr("Hit condition"), &debugFunctionHit_);
            {
                cyui::ScopedDisabled disabled(busy || !functionSupport || debugFunctionName_.empty());
                if (cyui::button(tr("Add function breakpoint"))) {
                    auto next = functions;
                    next.push_back({{"name", debugFunctionName_},
                                    {"enabled", true},
                                    {"condition", debugFunctionCondition_},
                                    {"hitCondition", debugFunctionHit_}});
                    debugTask("debug.functionBreakpoints", {{"breakpoints", next}, {"expected", functions}});
                }
            }
            int index = 0, verified = 0;
            const auto confirmations = debugState_.value("verifiedFunctionBreakpoints", Json::array());
            for (const auto &point : functions) {
                ImGui::PushID(index);
                bool enabled = point.value("enabled", true);
                {
                    cyui::ScopedDisabled disabled(busy || !functionSupport);
                    if (ImGui::Checkbox(point.value("name", "").c_str(), &enabled)) {
                        auto next = functions;
                        next[index]["enabled"] = enabled;
                        debugTask("debug.functionBreakpoints",
                                  {{"breakpoints", next}, {"expected", functions}});
                    }
                    ImGui::SameLine();
                    if (cyui::ghostButton(tr("Remove"))) {
                        auto next = functions;
                        next.erase(next.begin() + index);
                        debugTask("debug.functionBreakpoints",
                                  {{"breakpoints", next}, {"expected", functions}});
                    }
                }
                if (point.value("enabled", true) && size_t(verified) < confirmations.size()) {
                    const auto &confirmation = confirmations[verified++];
                    ImGui::SameLine();
                    cyui::badge(tr(confirmation.value("verified", false) ? "Verified" : "Unverified"),
                                confirmation.value("verified", false) ? cyui::Tone::success
                                                                      : cyui::Tone::warning);
                    const auto message = confirmation.value("message", std::string{});
                    if (!message.empty())
                        ImGui::TextWrapped("%s", message.c_str());
                }
                ImGui::PopID();
                ++index;
            }
            cyui::endSection();
        }
        if (cyui::beginSection(tr("Exception breakpoints"))) {
            const auto filters = debugState_.value("exceptionFilters", Json(nullptr));
            auto selected = filters;
            const auto catalogue = caps.value("exceptionBreakpointFilters", Json::array());
            if (selected.is_null()) {
                selected = Json::array();
                for (const auto &filter : catalogue)
                    if (filter.value("default", false))
                        selected.push_back(filter.at("filter"));
            }
            if (catalogue.empty())
                cyui::textDim("%s", tr("Start a debug session to discover its exception filters."));
            for (const auto &filter : catalogue) {
                const auto id = filter.at("filter");
                auto found = std::find(selected.begin(), selected.end(), id);
                bool enabled = found != selected.end();
                cyui::ScopedDisabled disabled(busy);
                ImGui::PushID(id.get_ref<const std::string &>().c_str());
                if (ImGui::Checkbox(filter.value("label", id.get<std::string>()).c_str(), &enabled)) {
                    auto next = selected;
                    if (enabled)
                        next.push_back(id);
                    else
                        next.erase(next.begin() + std::distance(selected.begin(), found));
                    debugTask("debug.exceptionBreakpoints", {{"filters", next}, {"expected", filters}});
                }
                ImGui::PopID();
            }
            {
                cyui::ScopedDisabled disabled(busy || catalogue.empty());
                if (cyui::ghostButton(tr("Use adapter exception defaults")))
                    debugTask("debug.exceptionBreakpoints", {{"filters", nullptr}, {"expected", filters}});
            }
            {
                cyui::ScopedDisabled disabled(busy || !stopped ||
                                              !caps.value("supportsExceptionInfoRequest", false));
                if (cyui::button(tr("Inspect stopped exception")))
                    debugTask("debug.exceptionInfo");
            }
            if (!debugException_.empty()) {
                ImGui::TextWrapped("%s: %s", debugException_.value("exceptionId", "").c_str(),
                                   debugException_.value("description", "").c_str());
                auto details = debugException_.dump(2);
                ImGui::InputTextMultiline("##exception-info", &details, {-1, cyui::dp(180)},
                                          ImGuiInputTextFlags_ReadOnly);
            }
            cyui::endSection();
        }
        ImGui::EndTabItem();
    }
}
void Application::dataBreakpointTab() {
    if (!ImGui::BeginTabItem(cyui::trLabel("Data breakpoints").c_str())) return;
    const auto caps = debugState_.value("capabilities",Json::object());
    const bool supported = caps.value("supportsDataBreakpoints",false);
    const bool paused = debugState_.value("state","") == "stopped";
    const bool disabled = !supported || !paused || debugFuture_.valid();
    if (!supported) cyui::textDim("%s",tr("This debug adapter does not advertise data breakpoints."));
    if (!paused) cyui::textDim("%s",tr("Pause the program to inspect or replace data breakpoints."));
    ImGui::TextWrapped("%s",tr("A replacement changes the full list. Inspect every desired target at this pause; installed breakpoints remain active after resuming."));
    {
        cyui::ScopedDisabled guard(disabled);
        if (cyui::beginProperties("##data-breakpoint-inputs")) {
            cyui::property(tr("Variable or expression"));
            if (ImGui::InputText("##data-name",&debugDataName_)) debugDataCandidate_ = Json::object();
            cyui::property(tr("Variable context"));
            std::string context = tr("Selected stack frame");
            for (const auto &scope : debugState_.value("scopes",Json::array()))
                if (scope.value("variablesReference",0) == debugDataContainer_) context = scope.value("name",context);
            if (ImGui::BeginCombo("##data-context",context.c_str())) {
                if (ImGui::Selectable(tr("Selected stack frame"),debugDataContainer_ == 0)) { debugDataContainer_ = 0; debugDataCandidate_ = Json::object(); }
                for (const auto &scope : debugState_.value("scopes",Json::array())) {
                    const int reference = scope.value("variablesReference",0);
                    ImGui::PushID(reference);
                    if (reference > 0 && ImGui::Selectable(scope.value("name","").c_str(),reference == debugDataContainer_)) { debugDataContainer_ = reference; debugDataCandidate_ = Json::object(); }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            cyui::endProperties();
        }
        cyui::ScopedDisabled empty(debugDataName_.empty());
        if (cyui::button(cyui::trLabel("Inspect data breakpoint target").c_str())) {
            debugDataCandidate_ = Json::object(); debugDataAccess_.clear();
            Json params = {{"name",debugDataName_},{"sessionGeneration",debugState_.at("sessionGeneration")}};
            if (debugDataContainer_ > 0) params["variablesReference"] = debugDataContainer_;
            debugTask("debug.dataBreakpointInfo",std::move(params));
        }
    }
    if (debugDataCandidate_.contains("description")) {
        ImGui::TextWrapped("%s",debugDataCandidate_.at("description").get_ref<const std::string &>().c_str());
        if (debugDataCandidate_.value("canPersist",false)) cyui::textDim("%s",tr("The adapter reports persistence support; CyCode keeps data breakpoints within this session."));
    }
    {
        cyui::ScopedDisabled guard(disabled || !debugDataCandidate_.value("available",false));
        if (cyui::beginProperties("##data-breakpoint-options")) {
            cyui::property(tr("Break on access"));
            const auto accessLabel = [](const std::string &value) { return value == "read" ? "Read" : value == "write" ? "Write" : value == "readWrite" ? "Read or write" : "Adapter default"; };
            if (ImGui::BeginCombo("##data-access",tr(accessLabel(debugDataAccess_)))) {
                if (ImGui::Selectable(tr("Adapter default"),debugDataAccess_.empty())) debugDataAccess_.clear();
                for (const auto &value : debugDataCandidate_.value("accessTypes",Json::array())) {
                    const auto access = value.get<std::string>();
                    if (ImGui::Selectable(tr(accessLabel(access)),access == debugDataAccess_)) debugDataAccess_ = access;
                }
                ImGui::EndCombo();
            }
            cyui::property(tr("Condition"));
            { cyui::ScopedDisabled unsupported(!caps.value("supportsConditionalBreakpoints",false)); ImGui::InputText("##data-condition",&debugDataCondition_); }
            cyui::property(tr("Hit condition"));
            { cyui::ScopedDisabled unsupported(!caps.value("supportsHitConditionalBreakpoints",false)); ImGui::InputText("##data-hit",&debugDataHit_); }
            cyui::endProperties();
        }
        cyui::ScopedDisabled full(debugDataDraft_.size() >= 32);
        if (cyui::button(cyui::trLabel("Add target to replacement").c_str())) {
            if (!debugDataEditing_) { debugDataExpectedRevision_ = debugState_.at("dataBreakpointRevision"); debugDataEditing_ = true; }
            Json point = {{"id",debugDataCandidate_.at("id")},{"condition",caps.value("supportsConditionalBreakpoints",false) ? debugDataCondition_ : ""},{"hitCondition",caps.value("supportsHitConditionalBreakpoints",false) ? debugDataHit_ : ""}};
            if (!debugDataAccess_.empty()) point["accessType"] = debugDataAccess_;
            debugDataDraft_.push_back({{"request",point},{"name",debugDataCandidate_.at("name")}});
            debugDataCandidate_ = Json::object();
        }
    }
    cyui::sectionTitle(tr("Replacement list"));
    for (size_t index = 0; index < debugDataDraft_.size(); ++index) {
        ImGui::PushID(int(index));
        ImGui::TextUnformatted(debugDataDraft_[index].at("name").get_ref<const std::string &>().c_str()); ImGui::SameLine();
        bool remove = false;
        { cyui::ScopedDisabled guard(disabled); remove = cyui::ghostButton(cyui::trLabel("Remove").c_str()); }
        ImGui::PopID();
        if (remove) { debugDataDraft_.erase(index); break; }
    }
    {
        cyui::ScopedDisabled guard(disabled);
        if (cyui::ghostButton(cyui::trLabel("New replacement list").c_str())) { debugDataDraft_ = Json::array(); debugDataEditing_ = true; debugDataExpectedRevision_ = debugState_.at("dataBreakpointRevision"); }
        ImGui::SameLine();
        {
            cyui::ScopedDisabled noDraft(!debugDataEditing_);
            if (cyui::primaryButton(cyui::trLabel("Replace data breakpoints").c_str())) {
                Json points = Json::array(); for (const auto &entry : debugDataDraft_) points.push_back(entry.at("request"));
                debugTask("debug.dataBreakpoints",{{"breakpoints",points},{"expectedRevision",debugDataExpectedRevision_},{"sessionGeneration",debugState_.at("sessionGeneration")}});
            }
        }
        ImGui::SameLine();
        const auto active = debugState_.value("dataBreakpoints",Json::array());
        cyui::ScopedDisabled noPoints(active.empty() && debugState_.value("dataBreakpointsSynchronized",true));
        if (cyui::dangerButton(cyui::trLabel("Clear data breakpoints").c_str()))
            debugTask("debug.dataBreakpoints",{{"breakpoints",Json::array()},{"expectedRevision",debugState_.at("dataBreakpointRevision")},{"sessionGeneration",debugState_.at("sessionGeneration")}});
    }
    cyui::sectionTitle(tr("Data breakpoint status"));
    if (!debugState_.value("dataBreakpointsSynchronized",true)) cyui::textColoured(cyui::palette().warning,"%s",tr("Data breakpoint state is uncertain. Pause and replace or clear the full list."));
    if (ImGui::BeginTable("##installed-data-breakpoints",3,ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        for (const auto *name : {"Name","Break on access","Verification"}) ImGui::TableSetupColumn(tr(name));
        ImGui::TableHeadersRow();
        for (const auto &entry : debugState_.value("dataBreakpoints",Json::array())) {
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(entry.at("name").get_ref<const std::string &>().c_str());
            ImGui::TableNextColumn(); const auto access = entry.value("accessType","");
            ImGui::TextUnformatted(tr(access == "read" ? "Read" : access == "write" ? "Write" : access == "readWrite" ? "Read or write" : "Adapter default"));
            ImGui::TableNextColumn(); const auto verified = entry.at("verification");
            ImGui::TextUnformatted(tr(!debugState_.value("dataBreakpointsSynchronized",true) ? "State unknown" : verified.at("verified").get<bool>() ? "Verified" : "Not verified"));
            if (!verified.value("message","").empty()) ImGui::TextWrapped("%s",verified.at("message").get_ref<const std::string &>().c_str());
        }
        ImGui::EndTable();
    }
    ImGui::EndTabItem();
}
} // namespace cycode
