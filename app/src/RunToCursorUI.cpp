#include "cycode/Application.h"
#include <cyui/Core.h>

namespace cycode {
void Application::requestRunToCursor() {
    if (!commandEnabled("debug.runToCursor"))
        return;
    for (const auto &tab : tabs_)
        if (tab->path == active_) {
            const auto points =
                debugState_.value("breakpoints", Json::object()).value(tab->path, Json::array());
            debugRunToTabSelect_ = true;
            debugTask("debug.runToCursor",
                      {{"path", tab->path},
                       {"line", tab->editor.GetMainCursorPosition().line + 1},
                       {"expectedDocumentId", tab->identity},
                       {"expectedRevision", tab->revision},
                       {"expectedBreakpoints", points},
                       {"sessionGeneration", debugState_.value("sessionGeneration", uint64_t(0))},
                       {"stopGeneration", debugState_.value("stopGeneration", uint64_t(0))},
                       {"threadId", debugState_.value("threadId", 0)}});
            return;
        }
}
void Application::runToCursorTab() {
    const auto flags = debugRunToTabSelect_ ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (!ImGui::BeginTabItem(cyui::trLabel("Run to cursor").c_str(), nullptr, flags))
        return;
    debugRunToTabSelect_ = false;
    ImGui::PushTextWrapPos(0.0f);
    cyui::textDim("%s", tr("Resume with a verified temporary breakpoint at the cursor. Existing breakpoints "
                           "and exceptions can stop the program first."));
    {
        cyui::ScopedDisabled disabled(!commandEnabled("debug.runToCursor"));
        if (cyui::primaryButton(cyui::trLabel("Run to cursor").c_str()))
            requestRunToCursor();
    }
    const auto run = debugState_.value("runToCursor", Json::object());
    const auto status = run.value("status", "idle");
    if (status != "idle") {
        const char *label = status == "arming"      ? "Verifying destination breakpoint"
                            : status == "running"   ? "Running to cursor"
                            : status == "restoring" ? "Restoring source breakpoints"
                            : status == "stopped"   ? "Run-to-cursor stop"
                                                    : "Run-to-cursor outcome unconfirmed";
        cyui::sectionTitle(tr(label));
        ImGui::TextUnformatted(run.value("path", "").c_str());
        ImGui::Text("%s %d", tr("Line"), run.value("line", 0));
        if (!run.value("message", "").empty())
            cyui::textColoured(cyui::palette().warning, "%s",
                               run.at("message").get_ref<const std::string &>().c_str());
        if (run.value("restorePending", false)) {
            if (status == "unconfirmed")
                cyui::textColoured(
                    cyui::palette().warning, "%s",
                    tr("The original source breakpoint list still needs restoration. Pause the program and "
                       "wait for outstanding replies before restoring, or stop the debug session."));
            else
                cyui::textDim("%s", tr("The temporary breakpoint will be removed at the next stop."));
            {
                cyui::ScopedDisabled disabled(!commandEnabled("debug.pause"));
                if (cyui::ghostButton(cyui::trLabel("Pause").c_str()))
                    executeCommand("debug.pause");
            }
            ImGui::SameLine();
            {
                cyui::ScopedDisabled disabled(!commandEnabled("debug.restoreRunToCursor"));
                if (cyui::ghostButton(cyui::trLabel("Restore source breakpoints").c_str()))
                    executeCommand("debug.restoreRunToCursor");
            }
        }
        if (run.contains("targetHit") && !run.at("targetHit").is_null())
            cyui::textDim("%s", tr(run.at("targetHit").get<bool>()
                                       ? "The debugger reported the destination breakpoint hit."
                                       : "Reported breakpoint IDs do not include the destination."));
        else if (!run.value("stopReason", "").empty())
            cyui::textDim("%s",
                          tr("The debugger did not report whether the destination breakpoint was hit."));
        if (!run.value("stopReason", "").empty())
            ImGui::TextUnformatted(run.at("stopReason").get_ref<const std::string &>().c_str());
    } else
        cyui::textDim("%s", tr("Pause and select a saved workspace source document. The destination line "
                               "must be verified without relocation."));
    ImGui::PopTextWrapPos();
    ImGui::EndTabItem();
}
} // namespace cycode
