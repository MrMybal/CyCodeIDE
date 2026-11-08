#include "cycode/Application.h"
#include "cycode/DebugExecution.h"
#include <cyui/Core.h>

namespace cycode {
Json Application::debugExecutionParameters(const std::string &method, Json parameters) const {
    if (method == "debug.runToCursor")
        return debugExecutionParameters("debug.continue",std::move(parameters));
    if (!debugExecutionOperation(method))
        return parameters;
    if (!parameters.contains("sessionGeneration"))
        parameters["sessionGeneration"] = debugState_.value("sessionGeneration", uint64_t(0));
    if (!parameters.contains("stopGeneration"))
        parameters["stopGeneration"] = debugState_.value("stopGeneration", uint64_t(0));
    if (method == "debug.restartFrame") {
        if (!parameters.contains("frameId"))
            parameters["frameId"] = debugState_.value("frameId", 0);
        return parameters;
    }
    if (!parameters.contains("threadId"))
        parameters["threadId"] = debugState_.value("threadId", 0);
    if ((method == "debug.stepIntoTarget" || method == "debug.gotoTarget") && !parameters.contains("frameId"))
        parameters["frameId"] = debugState_.value("frameId", 0);
    if (method == "debug.gotoTarget")
        return parameters;
    if (debugSingleThread_ && !parameters.contains("singleThread"))
        parameters["singleThread"] = true;
    if (method != "debug.continue" && method != "debug.reverseContinue" &&
        !parameters.contains("granularity")) {
        static const char *values[] = {"", "statement", "line", "instruction"};
        if (debugStepGranularity_ > 0 && debugStepGranularity_ < 4)
            parameters["granularity"] = values[debugStepGranularity_];
    }
    return parameters;
}
void Application::debugExecutionTab() {
    if (!ImGui::BeginTabItem(cyui::trLabel("Execution controls").c_str()))
        return;
    ImGui::PushTextWrapPos(0.0f);
    const auto caps = debugState_.value("capabilities", Json::object());
    const bool paused = debugState_.value("state", "") == "stopped" && !debugState_.value("pendingStop", false), busy = debugFuture_.valid();
    if (debugState_.value("pendingStop", false))
        cyui::textDim("%s", tr("Waiting for the debugger to report the relocated stack."));
    else if (!paused)
        cyui::textDim("%s", tr("Pause the program to change its execution position."));
    if (cyui::beginProperties("##debug-execution-options")) {
        cyui::property(tr("Stepping granularity"));
        {
            cyui::ScopedDisabled disabled(!paused || busy ||
                                          !caps.value("supportsSteppingGranularity", false));
            const char *labels[] = {"Adapter default", "Statement", "Source line", "Machine instruction"};
            if (ImGui::BeginCombo("##step-granularity", tr(labels[debugStepGranularity_]))) {
                for (int index = 0; index < 4; ++index)
                    if (ImGui::Selectable(cyui::trLabel(labels[index]).c_str(),
                                          debugStepGranularity_ == index))
                        debugStepGranularity_ = index;
                ImGui::EndCombo();
            }
        }
        cyui::property(tr("Thread execution"));
        {
            cyui::ScopedDisabled disabled(!paused || busy ||
                                          !caps.value("supportsSingleThreadExecutionRequests", false));
            cyui::toggleSwitch(cyui::trLabel("Only selected thread").c_str(), &debugSingleThread_);
        }
        cyui::endProperties();
    }
    cyui::textDim("%s", tr("Stepping shortcuts use these options. The thread option also applies to "
                           "Continue. Unavailable options reset to the adapter default."));
    const auto action = [&](const char *method, const char *label) {
        cyui::ScopedDisabled disabled(!commandEnabled(method));
        if (cyui::ghostButton(cyui::trLabel(label).c_str()))
            executeCommand(method);
    };
    cyui::sectionTitle(tr("Forward execution"));
    action("debug.next", "Step over");
    ImGui::SameLine();
    action("debug.stepIn", "Step into");
    ImGui::SameLine();
    action("debug.stepOut", "Step out");
    action("debug.stepInTargets", "Choose step-in target");
    action("debug.gotoTargets", "Set next statement");
    action("debug.runToCursor", "Run to cursor");
    cyui::sectionTitle(tr("Reverse execution"));
    if (!caps.value("supportsStepBack", false))
        cyui::textDim("%s", tr("Reverse execution requires a debugger with recorded execution support."));
    action("debug.stepBack", "Step back");
    ImGui::SameLine();
    action("debug.reverseContinue", "Continue backward");
    cyui::sectionTitle(tr("Restart stack frame"));
    cyui::textDim("%s",
                  tr("Restarting a frame can repeat program side effects. It is separate from source Undo."));
    action("debug.restartFrame", "Restart selected frame");
    const auto last = debugState_.value("lastExecution", Json::object());
    if (!last.empty()) {
        cyui::sectionTitle(tr("Last execution request"));
        const auto status = last.value("status", "");
        const auto *label = status == "applying"   ? "Sending execution request"
                            : status == "accepted" ? "Execution request accepted"
                                                   : "Execution outcome unconfirmed";
        cyui::textColoured(status == "unconfirmed" ? cyui::palette().warning : cyui::palette().text, "%s",
                           tr(label));
        cyui::textDim("%s", tr("Acceptance acknowledges the request. The next stopped event supplies the "
                               "actual stack and values."));
        if (!last.value("message", "").empty())
            ImGui::TextWrapped("%s", last.at("message").get_ref<const std::string &>().c_str());
        const auto result = last.value("result", Json::object());
        if (result.contains("allThreadsContinued"))
            cyui::textDim("%s", tr(result.at("allThreadsContinued").get<bool>()
                                       ? "The debugger reports all threads resumed."
                                       : "The debugger reports that some threads remain suspended."));
    }
    ImGui::PopTextWrapPos();
    ImGui::EndTabItem();
}
} // namespace cycode
