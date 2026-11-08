#include "cycode/Application.h"
#include "cycode/DebugMutations.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
#include <iomanip>
#include <sstream>

namespace cycode {
void Application::beginDebugValueEdit(const Json &variable, int container) {
    const auto method = debugVariableEditMethod(variable, debugState_.value("capabilities", Json::object()));
    if (method.empty() || debugFuture_.valid() || debugState_.value("state", "") != "stopped")
        return;
    Json parameters = {{"sessionGeneration", debugState_.at("sessionGeneration")},
                       {"stopGeneration", debugState_.at("stopGeneration")}};
    if (method == "debug.setExpression") {
        parameters["expression"] = variable.at("evaluateName");
        parameters["frameId"] = debugState_.at("frameId");
    } else {
        if (container <= 0)
            return;
        parameters["name"] = variable.at("name");
        parameters["variablesReference"] = container;
    }
    debugValueEdit_ = {{"method", method},
                       {"parameters", parameters},
                       {"name", variable.value("name", "")},
                       {"oldValue", variable.value("value", "")}};
    debugNewValue_ = variable.value("value", "");
    debugValueEditSelect_ = true;
}
void Application::debugValueEditTab() {
    const auto flags = debugValueEditSelect_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    debugValueEditSelect_ = false;
    if (!ImGui::BeginTabItem(cyui::trLabel("Edit value").c_str(), nullptr, flags))
        return;
    const auto caps = debugState_.value("capabilities", Json::object());
    const bool paused = debugState_.value("state", "") == "stopped", busy = debugFuture_.valid();
    ImGui::TextWrapped("%s", tr("Right-click a variable or scope value to edit it. A watched expression can "
                                "also be assigned when the adapter supports it."));
    {
        cyui::ScopedDisabled unavailable(!paused || busy || !caps.value("supportsSetExpression", false) ||
                                         debugWatch_.empty());
        if (cyui::ghostButton(cyui::trLabel("Edit watched expression").c_str())) {
            debugValueEdit_ = {{"method", "debug.setExpression"},
                               {"name", debugWatch_},
                               {"oldValue", tr("Not evaluated for assignment")},
                               {"parameters",
                                {{"expression", debugWatch_},
                                 {"frameId", debugState_.at("frameId")},
                                 {"sessionGeneration", debugState_.at("sessionGeneration")},
                                 {"stopGeneration", debugState_.at("stopGeneration")}}}};
            debugNewValue_.clear();
        }
    }
    if (debugValueEdit_.contains("parameters")) {
        const auto parameters = debugValueEdit_.at("parameters");
        const bool current = paused &&
                             parameters.at("sessionGeneration") == debugState_.at("sessionGeneration") &&
                             parameters.at("stopGeneration") == debugState_.at("stopGeneration");
        if (cyui::beginProperties("##debug-value-edit")) {
            cyui::property(tr("Target"));
            ImGui::TextUnformatted(debugValueEdit_.at("name").get_ref<const std::string &>().c_str());
            cyui::property(tr("Previous displayed value"));
            ImGui::TextWrapped("%s", debugValueEdit_.at("oldValue").get_ref<const std::string &>().c_str());
            cyui::property(tr("New value"));
            {
                cyui::ScopedDisabled disabled(!current || busy);
                ImGui::InputText("##new-debug-value", &debugNewValue_);
            }
            cyui::endProperties();
        }
        cyui::textDim("%s", tr("This changes the paused target. There is no automatic Undo."));
        cyui::ScopedDisabled disabled(!current || busy || debugNewValue_.size() > 8192);
        if (cyui::primaryButton(cyui::trLabel("Apply value change").c_str())) {
            auto request = parameters;
            request["value"] = debugNewValue_;
            debugTask(debugValueEdit_.at("method"), std::move(request));
        }
    } else
        cyui::textDim("%s", tr("Select a variable or watched expression first."));
    const auto mutation = debugState_.value("lastMutation", Json::object());
    if (!mutation.empty()) {
        const auto status = mutation.value("status", "");
        const auto label = status == "checking"   ? "Checking target bytes"
                           : status == "applying" ? "Applying target change"
                           : status == "applied"  ? "Target change confirmed"
                           : status == "partial"  ? "Partial target change"
                           : status == "rejected" ? "Target change rejected"
                                                  : "Target change unconfirmed";
        cyui::sectionTitle(tr("Last target change"));
        cyui::textColoured(status == "applied" ? cyui::palette().success : cyui::palette().warning, "%s",
                           tr(label));
        if (!mutation.value("message", "").empty())
            ImGui::TextWrapped("%s", mutation.at("message").get_ref<const std::string &>().c_str());
        const auto result = mutation.value("result", Json::object());
        if (result.contains("value"))
            ImGui::TextWrapped("%s", result.at("value").get_ref<const std::string &>().c_str());
    }
    ImGui::EndTabItem();
}
void Application::debugMemoryWriteControls() {
    const auto caps = debugState_.value("capabilities", Json::object());
    const bool supported =
        caps.value("supportsWriteMemoryRequest", false) && caps.value("supportsReadMemoryRequest", false);
    if (!supported) {
        cyui::textDim("%s", tr("Memory writes require adapter support for both reading and writing."));
        return;
    }
    const bool paused = debugState_.value("state", "") == "stopped";
    const bool current = paused && debugMemoryRead_.value("ready", false) &&
                         debugMemoryRead_.at("sessionGeneration") == debugState_.at("sessionGeneration") &&
                         debugMemoryRead_.at("stopGeneration") == debugState_.at("stopGeneration") &&
                         debugMemoryRead_.at("memoryReference") == debugMemoryReference_ &&
                         debugMemoryRead_.value("offset", 0) == debugMemoryOffset_;
    cyui::sectionTitle(tr("Write target memory"));
    cyui::textDim("%s", tr("Read the target first, then enter replacement hex bytes. The expected bytes are "
                           "checked again before writing."));
    Json replacement = Json::array();
    std::string problem;
    {
        cyui::ScopedDisabled guard(!current || debugFuture_.valid());
        ImGui::InputTextMultiline("##memory-hex-replacement", &debugMemoryHex_, cyui::dp(-1, 64));
    }
    if (!debugMemoryHex_.empty())
        try {
            replacement = debugHexBytes(debugMemoryHex_);
        } catch (const std::exception &error) {
            problem = error.what();
        }
    if (problem.empty() && !replacement.empty() &&
        replacement.size() > debugMemory_.value("bytes", Json::array()).size())
        problem = tr("Read enough target bytes for this replacement.");
    if (!problem.empty())
        ImGui::TextWrapped("%s", problem.c_str());
    if (current && problem.empty() && !replacement.empty()) {
        const auto expected = debugMemory_.at("bytes");
        std::ostringstream preview;
        for (size_t index = 0; index < replacement.size(); ++index) {
            preview << '+' << std::dec << index << "  " << std::hex << std::setfill('0') << std::setw(2)
                    << expected[index].get<int>() << " -> " << std::setw(2) << replacement[index].get<int>()
                    << '\n';
        }
        auto text = preview.str();
        cyui::ScopedFont mono(cyui::monoFont());
        ImGui::InputTextMultiline("##memory-write-preview", &text, cyui::dp(-1, 96),
                                  ImGuiInputTextFlags_ReadOnly);
    }
    {
        cyui::ScopedDisabled guard(!current || debugFuture_.valid() || replacement.empty() ||
                                   !problem.empty());
        if (cyui::primaryButton(cyui::trLabel("Write these bytes").c_str())) {
            Json expected = Json::array();
            for (size_t index = 0; index < replacement.size(); ++index)
                expected.push_back(debugMemory_.at("bytes")[index]);
            debugTask("debug.writeMemory", {{"memoryReference", debugMemoryRead_.at("memoryReference")},
                                            {"offset", debugMemoryRead_.value("offset", 0)},
                                            {"bytes", replacement},
                                            {"expectedBytes", expected},
                                            {"sessionGeneration", debugMemoryRead_.at("sessionGeneration")},
                                            {"stopGeneration", debugMemoryRead_.at("stopGeneration")}});
        }
    }
    const auto mutation = debugState_.value("lastMutation", Json::object());
    if (mutation.value("method", "") == "debug.writeMemory") {
        const auto status = mutation.value("status", "");
        cyui::textColoured(status == "applied" ? cyui::palette().success : cyui::palette().warning, "%s",
                           tr(status == "applied"    ? "Target change confirmed"
                              : status == "partial"  ? "Partial target change"
                              : status == "checking" ? "Checking target bytes"
                              : status == "applying" ? "Applying target change"
                              : status == "rejected" ? "Target change rejected"
                                                     : "Target change unconfirmed"));
        if (!mutation.value("message", "").empty())
            ImGui::TextWrapped("%s", mutation.at("message").get_ref<const std::string &>().c_str());
        const auto result = mutation.value("result", Json::object());
        if (result.contains("bytesWritten"))
            cyui::textDim("%s: %d", tr("Adapter-reported bytes written"),
                          result.at("bytesWritten").get<int>());
    }
}
} // namespace cycode
