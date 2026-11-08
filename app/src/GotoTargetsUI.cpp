#include "cycode/Application.h"
#include "cycode/DebugTargets.h"
#include <algorithm>
#include <cyui/Core.h>

namespace cycode {
void Application::requestGotoTargets(bool frameLine) {
    if (smoke_ || debugFuture_.valid())
        return;
    try {
        auto parameters = DebugTargets::context(debugState_, DebugTargets::Kind::goTo);
        if (frameLine) {
            const auto &frames = debugState_.at("frames");
            const auto frame = std::find_if(frames.begin(), frames.end(), [&](const auto &value) {
                return value.at("id") == parameters.at("frameId");
            });
            if (frame == frames.end() || !frame->contains("line") || !frame->at("line").is_number_integer() ||
                frame->at("line") < 1 || frame->at("line") > 10000000)
                throw std::runtime_error("Selected frame has no supported source line");
            debugGotoLine_ = frame->at("line").get<int>();
            debugGotoColumn_ = 0;
        }
        parameters["line"] = debugGotoLine_;
        if (debugGotoColumn_ != 0)
            parameters["column"] = debugGotoColumn_;
        (void)DebugTargets::query(parameters, debugState_, DebugTargets::Kind::goTo);
        debugGotoTargets_ = Json::object();
        debugGotoTarget_.clear();
        debugGotoTargetsTabSelect_ = true;
        debugTask("debug.gotoTargets", std::move(parameters));
    } catch (const std::exception &failure) {
        error(failure.what());
    }
}
void Application::gotoTargetsTab() {
    const auto flags = debugGotoTargetsTabSelect_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (!ImGui::BeginTabItem(cyui::trLabel("Next statement").c_str(), nullptr, flags))
        return;
    debugGotoTargetsTabSelect_ = false;
    ImGui::PushTextWrapPos(0.0f);
    const bool busy = debugFuture_.valid(), available = commandEnabled("debug.gotoTargets");
    cyui::textColoured(cyui::palette().warning, "%s",
                       tr("Moving the next statement can skip or repeat code. It does not execute the "
                          "intervening code and has no automatic Undo."));
    if (debugState_.value("pendingStop", false))
        cyui::textDim("%s", tr("Waiting for the debugger to report the relocated stack."));
    else if (!debugState_.value("capabilities", Json::object()).value("supportsGotoTargetsRequest", false))
        cyui::textDim("%s", tr("This debug adapter does not advertise next-statement targets."));
    else if (!available && !busy)
        cyui::textDim("%s", tr("Pause and select a stack frame with a source before reading destinations."));
    if (DebugTargets::available(debugState_, DebugTargets::Kind::goTo)) {
        const auto source = DebugTargets::source(debugState_);
        cyui::sectionTitle(tr("Selected frame source"));
        if (source.contains("path"))
            ImGui::TextUnformatted(source.at("path").get_ref<const std::string &>().c_str());
        else if (source.contains("name"))
            ImGui::TextUnformatted(source.at("name").get_ref<const std::string &>().c_str());
        if (source.contains("sourceReference"))
            ImGui::Text("%s %d", tr("Source reference"), source.at("sourceReference").get<int>());
    }
    bool edited = false;
    if (cyui::beginProperties("##goto-location")) {
        cyui::ScopedDisabled disabled(!available);
        cyui::property(tr("Destination line"));
        edited |= ImGui::InputInt("##goto-line", &debugGotoLine_);
        cyui::property(tr("Destination column (0 = unspecified)"));
        edited |= ImGui::InputInt("##goto-column", &debugGotoColumn_);
        cyui::endProperties();
    }
    if (edited) {
        debugGotoTargets_ = Json::object();
        debugGotoTarget_.clear();
    }
    cyui::textDim(
        "%s", tr("Coordinates use one-based source lines and UTF-16 columns in the selected frame source."));
    {
        cyui::ScopedDisabled disabled(!available || debugGotoLine_ < 1 || debugGotoLine_ > 10000000 ||
                                      debugGotoColumn_ < 0 || debugGotoColumn_ > 10000000);
        if (cyui::ghostButton(cyui::trLabel("Read next-statement targets").c_str()))
            requestGotoTargets();
    }
    if (!debugGotoTargets_.empty() &&
        DebugTargets::current(debugGotoTargets_, debugState_, DebugTargets::Kind::goTo)) {
        const auto &targets = debugGotoTargets_.at("targets");
        if (targets.empty())
            cyui::textDim("%s", tr("The debugger reported no valid destination for this location."));
        debugTargetTable(targets, debugGotoTarget_, "##goto-targets", "Destination");
        cyui::ScopedDisabled disabled(busy || debugGotoTarget_.empty());
        if (cyui::primaryButton(cyui::trLabel("Set next statement here").c_str())) {
            Json parameters = Json::object();
            for (const auto *key : {"sessionGeneration", "stopGeneration", "threadId", "frameId"})
                parameters[key] = debugGotoTargets_.at(key);
            parameters["target"] = debugGotoTarget_;
            debugTask("debug.gotoTarget", std::move(parameters));
        }
    }
    ImGui::PopTextWrapPos();
    ImGui::EndTabItem();
}
} // namespace cycode
