#include "cycode/Application.h"
#include "cycode/RelatedFiles.h"
#include "ApplicationHelp.h"
#include "cycode/FileIcons.h"
#include "cycode/DebugExecution.h"
#include "cycode/StepInTargets.h"
#include "cycode/SnippetCatalogue.h"
#include <algorithm>
#include <cctype>
#include <cyui/Core.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <sstream>

namespace cycode {
static std::string keyName(ImGuiKey key) {
    if (key >= ImGuiKey_A && key <= ImGuiKey_Z) return std::string(1, 'a' + key - ImGuiKey_A);
    if (key >= ImGuiKey_0 && key <= ImGuiKey_9) return std::string(1, '0' + key - ImGuiKey_0);
    if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12) return "f" + std::to_string(key - ImGuiKey_F1 + 1);
    for (auto [value, name] : std::initializer_list<std::pair<ImGuiKey, const char *>>{
             {ImGuiKey_Tab, "tab"}, {ImGuiKey_Enter, "enter"}, {ImGuiKey_Escape, "escape"},
             {ImGuiKey_Space, "space"}, {ImGuiKey_Delete, "delete"}, {ImGuiKey_Backspace, "backspace"},
             {ImGuiKey_Insert, "insert"}, {ImGuiKey_Home, "home"}, {ImGuiKey_End, "end"},
             {ImGuiKey_PageUp, "pageup"}, {ImGuiKey_PageDown, "pagedown"},
             {ImGuiKey_LeftArrow, "left"}, {ImGuiKey_RightArrow, "right"}, {ImGuiKey_UpArrow, "up"}, {ImGuiKey_DownArrow, "down"},
             {ImGuiKey_Slash, "slash"}, {ImGuiKey_LeftBracket, "leftbracket"}, {ImGuiKey_RightBracket, "rightbracket"},
             {ImGuiKey_Comma, "comma"}, {ImGuiKey_Period, "period"}, {ImGuiKey_Minus, "minus"}, {ImGuiKey_Equal, "equal"}})
        if (key == value) return name;
    return "";
}
static std::string commentPrefix(const std::string &path) {
    auto ext = utf8Path(path).extension().string();
    for (auto &c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".py" || ext == ".sh" || ext == ".cmake" || ext == ".yml" || ext == ".yaml") return "#";
    if (ext == ".lua" || ext == ".sql") return "--";
    for (const auto *value : {".c", ".cpp", ".cc", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl", ".cs", ".rs", ".java", ".js", ".ts", ".tsx", ".jsx", ".glsl", ".vert", ".frag", ".comp", ".hlsl", ".hlsli"}) if (ext == value) return "//";
    return "";
}
void Application::shortcuts() {
    auto &io = ImGui::GetIO();
    const auto window = ImGui::GetCurrentContext()->NavWindow;
    const auto focused = window ? window->ID : 0;
    if (keymapFocus_ != focused) { keymap_.cancel(); keymapFocus_ = focused; lastShiftTap_ = -1; }
    if (ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        // The related-file modal permits only a configured single-stroke Context help action.
        // Other global commands and chords continue to belong to the modal's search field.
        if(counterpartOpen_)for(int i=ImGuiKey_NamedKey_BEGIN;i<ImGuiKey_NamedKey_END;++i) {
            const auto key=static_cast<ImGuiKey>(i);
            if(!ImGui::IsKeyPressed(key,ImGuiInputFlags_None,ImGuiKeyOwner_Any))continue;
            auto name=keyName(key);if(name.empty())continue;
            std::string stroke;
            if(io.KeyCtrl)stroke+="ctrl+";if(io.KeyShift)stroke+="shift+";
            if(io.KeyAlt)stroke+="alt+";if(io.KeySuper)stroke+="super+";
            stroke+=name;
            for(const auto &binding:keymap_.bindings())if(binding.command=="help.context" &&
                binding.strokes.size()==1 && binding.strokes[0]==stroke) {
                ImGui::SetKeyOwner(key,ImHashStr("##cycode-related-help"),ImGuiInputFlags_LockThisFrame);
                io.InputQueueCharacters.resize(0);executeCommand("help.context");
                keymap_.cancel();lastShiftTap_=-1;return;
            }
        }
        keymap_.cancel(); lastShiftTap_ = -1; return;
    }
    // Core view filters own Ctrl/Cmd+F in their panel. Dear ImGui maps the
    // primary macOS modifier to Ctrl when ConfigMacOSXBehaviors is enabled.
    const bool input = ImGui::GetActiveID() != 0 && ImGui::GetCurrentContext()->InputTextState.ID == ImGui::GetActiveID();
    if (window && viewFilters_.contains(window->RootWindow->ID) && io.KeyCtrl && !io.KeyShift && !io.KeyAlt &&
        ImGui::IsKeyPressed(ImGuiKey_F, ImGuiInputFlags_None, ImGuiKeyOwner_Any)) {
        if (!input) viewFilters_.at(window->RootWindow->ID)->focus();
        return;
    }
    if (terminalFocused_ && !input) {
        if (io.KeyAlt && !io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F12, false)) executeCommand("view.terminal");
        return; // The focused PTY owns Ctrl+C, Ctrl+F, Tab and shell editing keys.
    }
    const bool editor = editorFocused_ && !input;
    const bool resourcesWindow=window&&std::string(window->RootWindow->Name).ends_with("###resources");
    const bool projectWindow=window&&std::string(window->RootWindow->Name).ends_with("###project");
    if(resourcesWindow&&io.KeyCtrl&&!io.KeyShift&&!io.KeyAlt&&ImGui::IsKeyPressed(ImGuiKey_F,false)) {
        if(resourceTrashView_)resourceTrashFilter_.focus();else resourceSearchFocus_=true;
        return;
    }
    if((resourcesWindow||projectWindow)&&!input&&!io.KeyCtrl&&!io.KeyShift&&!io.KeyAlt) {
        if(ImGui::IsKeyPressed(ImGuiKey_F2,false)){executeCommand("project.moveFile");return;}
        if(ImGui::IsKeyPressed(ImGuiKey_Delete,false)){executeCommand("project.deleteFile");return;}
    }
    const auto owner = ImHashStr("##cycode-command-routing");
    for (int i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_NamedKey_END; ++i) {
        auto key = static_cast<ImGuiKey>(i);
        if (!ImGui::IsKeyPressed(key, ImGuiInputFlags_None, ImGuiKeyOwner_Any)) continue;
        auto name = keyName(key);
        if (name.empty()) continue;
        lastShiftTap_ = -1;
        std::string stroke;
        if (io.KeyCtrl) stroke += "ctrl+";
        if (io.KeyShift) stroke += "shift+";
        if (io.KeyAlt) stroke += "alt+";
        if (io.KeySuper) stroke += "super+";
        stroke += name;
        auto result = keymap_.press(stroke, ImGui::GetTime(), editor, !active_.empty());
        if (result.consumed) {
            ImGui::SetKeyOwner(key, owner, ImGuiInputFlags_LockThisFrame);
            io.InputQueueCharacters.resize(0);
            if (!result.command.empty()) executeCommand(result.command);
        } else if (editor) {
            // The widget supplies caret movement/typing. Its fixed editing shortcuts
            // must not bypass the selected profile or a removed custom binding.
            static const std::vector<std::string> widgetActions = {
                "ctrl+a", "ctrl+d", "ctrl+alt+d", "ctrl+alt+l", "ctrl+shift+d", "ctrl+shift+alt+d",
                "ctrl+x", "ctrl+delete", "ctrl+c", "ctrl+insert", "ctrl+v", "ctrl+z", "ctrl+shift+z", "ctrl+y",
                "ctrl+shift+k", "ctrl+leftbracket", "ctrl+rightbracket", "alt+up", "alt+down", "ctrl+slash", "ctrl+l",
                "ctrl+f", "ctrl+shift+f", "ctrl+g"};
            if (std::find(widgetActions.begin(), widgetActions.end(), stroke) != widgetActions.end())
                ImGui::SetKeyOwner(key, owner, ImGuiInputFlags_LockThisFrame);
        }
    }
    const bool shiftDown = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
    const bool shiftReleased = ImGui::IsKeyReleased(ImGuiKey_LeftShift) || ImGui::IsKeyReleased(ImGuiKey_RightShift);
    if (ImGui::IsKeyPressed(ImGuiKey_LeftShift, false) || ImGui::IsKeyPressed(ImGuiKey_RightShift, false)) shiftChordUsed_ = false;
    if (shiftDown || shiftReleased) {
        for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key)
            if (key != ImGuiKey_LeftShift && key != ImGuiKey_RightShift && key != ImGuiKey_ReservedForModShift && ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), ImGuiInputFlags_None, ImGuiKeyOwner_Any)) shiftChordUsed_ = true;
        if (io.KeyCtrl || io.KeyAlt || io.KeySuper) shiftChordUsed_ = true;
    }
    if (shiftReleased && !shiftDown) {
        if (!shiftChordUsed_ && lastShiftTap_ >= 0 && ImGui::GetTime() - lastShiftTap_ < .4) {
            auto result = keymap_.press("DoubleShift", ImGui::GetTime(), editor, !active_.empty());
            if (!result.command.empty()) executeCommand(result.command);
            lastShiftTap_ = -1;
        } else lastShiftTap_ = shiftChordUsed_ ? -1 : ImGui::GetTime();
    }
}
bool Application::commandEnabled(const std::string &id) const {
    if(id=="navigate.counterpart" || id=="navigate.include" || id=="navigate.relatedFiles") {
        if(smoke_ || testSession_ || counterpartFuture_.valid() || counterpartOpen_)return false;
        for(const auto &tab:tabs_)if(tab->path==active_) {
            const auto file=utf8Path(tab->path);
            return !tab->image && !isUntitledDocument(tab->path) && tab->path.find("://")==tab->path.npos &&
                cppFileKind(pathUtf8(file.extension()))!=CppFileKind::none &&
                (id=="navigate.include" || !relatedGeneratedStem(pathUtf8(file.stem())));
        }
        return false;
    }
    if(id=="edit.userSnippets")return !smoke_ && !testSession_ && !userSnippetsOpen_ && !userSnippetsFuture_.valid();
    if(id=="edit.captureSnippet") {
        if(!commandEnabled("edit.userSnippets"))return false;
        for(const auto &tab:tabs_)if(tab->path==active_) {
            const auto selected=tab->editor.GetMainCursorSelection();
            return !tab->image && !tab->readOnly && !isUntitledDocument(tab->path) && tab->path.find("://")==tab->path.npos &&
                tab->editor.GetNumberOfCursors()==1 && selected.start!=selected.end;
        }
        return false;
    }
    if (id=="edit.snippetCatalogue")return !smoke_ && !snippetFuture_.valid() && !snippetOpen_;
    if (id=="edit.snippet" || id=="edit.surroundSnippet") {
        if(smoke_ || snippetFuture_.valid() || snippetOpen_)return false;
        for(const auto &tab:tabs_)if(tab->path==active_)return !tab->readOnly && !tab->image &&
            !isUntitledDocument(tab->path) && tab->path.find("://")==tab->path.npos &&
            !tab->completion.valid() && !tab->completionAccept.valid() &&
            tab->editor.GetNumberOfCursors()==1 &&
            (id!="edit.surroundSnippet" || tab->editor.MainCursorHasSelection());
        return false;
    }
    if (id == "build.cancel") return engine_ &&
        ((taskFuture_.valid() && BuildOperations::supports(taskMethod_)) || unrealAnalysisFuture_.valid() ||
         engine_->dispatch("build.status").value("running", false));
    if(id=="tests.discover")return !testsFuture_.valid()&&!taskFuture_.valid();
    if(id=="tests.run")return !testsFuture_.valid()&&!taskFuture_.valid()&&testsCatalog_.contains("id")&&!testsSelection_.empty();
    if(id=="tests.cancel")return testsFuture_.valid();
    if(id=="file.newProject")return !templateFuture_.valid();
    if(id=="file.saveAs") {
        for (const auto &tab:tabs_) if(tab->path==active_) return !tab->readOnly&&!tab->image;
        return false;
    }
    if(id=="project.moveFile"||id=="project.deleteFile")return !resourceFuture_.valid()&&!selectedResource_.empty();
    if(id=="project.newFolder")return !resourceFuture_.valid();
    if(id=="project.undoFile")return !resourceFuture_.valid()&&resourceLast_.value("state",std::string{})=="applied"&&!resourceLast_.contains("undone");
    if (id=="code.actions") {
        if (smoke_ || codeActionsFuture_.valid() || refactorFuture_.valid() || codeActionParameters().is_null()) return false;
        const auto server=languageState_.value("servers",Json::object()).value(engine_->languageBackendFor(active_),Json::object());
        return !server.value("ready",false) || server.value("methods",Json::object()).value("codeActions",false);
    }
    if(id=="app.checkUpdates")return !smoke_ && !testSession_ && !applicationUpdatePending() && updates::platform()!="unsupported";
    const auto command = findCommand(id);
    if (!command) return extensionCommandEnabled(id);
    if (command->context != CommandContext::global && active_.empty()) return false;
    for (const auto &tab : tabs_) if (tab->path==active_ && tab->image && command->context!=CommandContext::global &&
        id!="file.close" && id!="navigate.nextTab" && id!="navigate.previousTab" && id!="vcs.diff") return false;
    static const std::map<std::string,std::string> languageCommands={{"navigate.definition","definition"},{"navigate.references","references"},
        {"navigate.declaration","declaration"},{"navigate.typeDefinition","typeDefinition"},{"navigate.implementation","implementation"},
        {"navigate.callHierarchy","callHierarchy"},{"navigate.typeHierarchy","typeHierarchy"},
        {"code.occurrences","documentHighlights"},{"code.documentation","hover"},{"code.parameters","signature"},
        {"navigate.members","documentSymbols"},{"refactor.rename","rename"},{"refactor.format","format"},{"refactor.formatSelection","rangeFormat"}};
    if (languageCommands.contains(id)) {
        const auto servers=languageState_.find("servers");
        if (servers==languageState_.end()) return false;
        const auto found=servers->find(engine_->languageBackendFor(active_));
        if (found==servers->end()) return false;
        const auto &server=*found;
        const auto methods=server.find("methods");
        if (server.value("ready",false) && (methods==server.end() || !methods->value(languageCommands.at(id),false))) return false;
        if (!server.value("enabled",false)) return false;
    }
    if (id == "build.run" || id == "build.configure") return !taskFuture_.valid() && !unrealAnalysisFuture_.valid() &&
        !engine_->dispatch("build.status").value("running",false);
    if (id=="code.occurrences") return !smoke_ && !occurrences_.future.valid() && !occurrenceParameters().is_null();
    if (id=="code.clearOccurrences") return !occurrences_.wanted.empty();
    if (id=="code.nextOccurrence" || id=="code.previousOccurrence" || id=="code.selectOccurrences")
        return !occurrences_.future.valid() && occurrencesCurrent() && !occurrences_.report.at("occurrences").empty();
    if (id=="code.documentation" || id=="code.parameters") return !smoke_ && engine_->supportsLanguage(active_);
    if (id == "project.cancel") return taskFuture_.valid() && taskMethod_.starts_with("project.");
    if (id == "project.test") return projectActive_ && projectState_.value("kind","")!="java" && !taskFuture_.valid();
    if (id == "project.install" || id == "project.script") return projectActive_ && projectState_.value("kind","")=="npm" && !taskFuture_.valid();
    if (id.starts_with("msbuild.")) return msbuildActive_ && !taskFuture_.valid();
    if (id == "unreal.prepare") return unreal_.value("available", false) && !unrealAnalysisFuture_.valid() && !taskFuture_.valid();
    if (id == "navigate.back") return !historyBack_.empty();
    if (id == "navigate.forward") return !historyForward_.empty();
    if (id == "file.reopen") return !closedFiles_.empty();
    if (id.starts_with("vcs.") && id != "vcs.refresh") {
        if (vcsFuture_.valid() || taskFuture_.valid()) return false;
        if ((id == "vcs.diff" || id == "vcs.blame") && (isUntitledDocument(active_) || active_.starts_with("sdk://") || active_.starts_with("debug://"))) return false;
        if (id == "vcs.diff" || id == "vcs.blame") return vcsSupports(id.substr(4));
        if (id != "vcs.changes" && provider_ != "git") return false;
    }
    if (id.starts_with("debug.") || id == "run.start") {
        if (id == "debug.cancelReload") {
            const auto reload = debugState_.value("javaReload",Json::object());
            return !smoke_ && reload.value("status","") == "building" && !reload.value("cancelRequested",false);
        }
        if (smoke_ || debugFuture_.valid() || pendingDebugBuild_) return false;
        if (debugState_.value("pendingStop",false) && id != "debug.stop" && id != "debug.terminal") return false;
        const auto runTo = debugState_.value("runToCursor",Json::object());
        if (id == "debug.restoreRunToCursor") return debugState_.value("state","") == "stopped" && runTo.value("active",false) && runTo.value("restorePending",false) && runTo.value("stopConfirmed",false) && !runTo.value("replyPending",true);
        if (runTo.value("active",false)) {
            if (id == "debug.pause") return debugState_.value("state","") == "running" || !runTo.value("stopConfirmed",false);
            if (id != "debug.stop" && id != "debug.terminal") return false;
        }
        if (id == "debug.runToCursor") {
            if (runTo.value("replyPending",false) || debugState_.value("state","") != "stopped" || debugState_.value("threadId",0) <= 0) return false;
            for (const auto &tab : tabs_) if (tab->path == active_) return !tab->image && !tab->readOnly && !tab->dirty && tab->diskConflict.empty() && tab->identity > 0 && tab->revision > 0 &&
                !isUntitledDocument(active_) && !active_.starts_with("sdk://") && !active_.starts_with("debug://");
            return false;
        }
        if (projectActive_ && (id=="run.start" || id=="debug.start") && taskFuture_.valid()) return false;
        const auto phase = debugState_.value("state","idle");
        if (projectActive_ && debugUnrealLaunch_.is_null() && id=="debug.start" && phase!="stopped" && projectState_.value("kind","")!="cargo" && debugAdapterId_=="builtin") return false;
        if (id == "debug.start") {
            if(phase!="stopped"&&debugAdapterId_!="builtin") {
                const auto selected=std::find_if(debugAdapters_.begin(),debugAdapters_.end(),[&](const Json &entry){return entry.at("id")==debugAdapterId_;});
                if(selected==debugAdapters_.end()||std::find(selected->at("requests").begin(),selected->at("requests").end(),"launch")==selected->at("requests").end())return false;
            }
            return phase != "starting" && phase != "running";
        }
        if (id == "debug.stop") return phase == "stopped" || phase == "running" || phase == "starting";
        if (id == "debug.pause") return phase == "running";
        if (id == "debug.hotReload") return (phase == "running" || phase == "stopped") && debugState_.value("runtime","") == "java" && !taskFuture_.valid();
        if (id == "debug.terminal") return !debugState_.value("terminalId","").empty();
        if (id == "debug.next" || id == "debug.stepIn" || id == "debug.stepOut") return phase == "stopped" && debugState_.value("threadId",0) > 0;
        if (id == "debug.stepBack" || id == "debug.reverseContinue") return phase == "stopped" && debugState_.value("threadId",0) > 0 && debugState_.value("capabilities",Json::object()).value("supportsStepBack",false);
        if (id == "debug.restartFrame") return debugFrameRestartable(debugState_);
        if (id == "debug.stepInTargets") return StepInTargets::available(debugState_);
        if (id == "debug.gotoTargets") return DebugTargets::available(debugState_,DebugTargets::Kind::goTo);
        if (id == "debug.breakpoint") { for (const auto &tab : tabs_) if (tab->path == active_) return !tab->readOnly && !isUntitledDocument(active_); return false; }
    }
    if (id == "refactor.undo") return !refactorFuture_.valid() && refactorPlan_.is_object() && refactorPlan_.value("state", "") == "applied";
    if (id == "refactor.apply") return !refactorFuture_.valid() && refactorPlan_.is_object() && refactorPlan_.value("state", "") == "preview" && !refactorPlan_.at("files").empty();
    if (id.starts_with("refactor.") || id == "navigate.members") {
        if (smoke_ || !engine_->supportsLanguage(active_) || refactorFuture_.valid()) return false;
        for (const auto &tab : tabs_) if (tab->path == active_ && ((id.starts_with("refactor.") && tab->readOnly) || (id == "refactor.formatSelection" && !tab->editor.MainCursorHasSelection()))) return false;
    }
    if (id == "navigate.callHierarchy" || id == "navigate.typeHierarchy")
        return !smoke_ && engine_->supportsLanguage(active_) && active_.find("://")==std::string::npos && !hierarchyFuture_.valid();
    if (id == "navigate.definition" || id == "navigate.references" || id == "navigate.declaration" || id == "navigate.typeDefinition" || id == "navigate.implementation") return engine_->supportsLanguage(active_) && !navigationFuture_.valid();
    if (id == "edit.comment" || id == "edit.addComment" || id == "edit.removeComment") return !commentPrefix(active_).empty();
    for (const auto &tab : tabs_) if (tab->path == active_) {
        if (tab->readOnly && (id == "file.save" || (id.starts_with("edit.") && id != "edit.copy" && id != "edit.selectAll" && id != "edit.grow" && id != "edit.shrink" && id != "edit.nextOccurrence" && id != "edit.allOccurrences"))) return false;
        if (id == "edit.undo") return tab->editor.CanUndo();
        if (id == "edit.redo") return tab->editor.CanRedo();
    }
    return true;
}
void Application::rememberNavigation() {
    for (const auto &tab : tabs_) if (tab->path == active_) {
        auto position = tab->editor.GetMainCursorPosition();
        HistoryPosition item{active_, position.line, LanguageClient::utf16Length(tab->editor.GetSectionText({position.line, 0}, position))};
        if (historyBack_.empty() || historyBack_.back().path != item.path || historyBack_.back().line != item.line || historyBack_.back().character != item.character) historyBack_.push_back(item);
        if (historyBack_.size() > 200) historyBack_.erase(historyBack_.begin());
        historyForward_.clear();
    }
}
void Application::navigationHistory(bool forward) {
    auto &from = forward ? historyForward_ : historyBack_;
    auto &to = forward ? historyBack_ : historyForward_;
    if (from.empty()) return;
    for (const auto &tab : tabs_) if (tab->path == active_) {
        auto pos = tab->editor.GetMainCursorPosition();
        to.push_back({active_, pos.line, LanguageClient::utf16Length(tab->editor.GetSectionText({pos.line, 0}, pos))});
    }
    auto destination = from.back(); from.pop_back();
    historyMoving_ = true;
    open(destination.path, destination.line + 1, destination.character + 1, true);
    historyMoving_ = false;
}
void Application::executeCommand(const std::string &id) {
    if (!commandEnabled(id)) return;
    lastShortcut_ = id;
    if (testSession_) { executedCommands_.push_back(id); if (executedCommands_.size() > 100) executedCommands_.erase(executedCommands_.begin()); }
    std::shared_ptr<Tab> tab;
    for (const auto &item : tabs_) if (item->path == active_) tab = item;
    try {
        if (executeExtensionCommand(id)) return;
        if (id.starts_with("help.")) {
            std::string page = id.substr(5);
            std::string anchor;
            if (page == "documentation") page = "overview";
            else if (page == "context") {
                if(counterpartOpen_){relatedHelp();return;}
                page = tab ? "editing" : "overview";
                for (auto *window = ImGui::GetCurrentContext()->NavWindow; window; window = window->ParentWindow) {
                    const std::string_view name(window->Name);
                    const auto marker = name.find("###");
                    if (marker != std::string_view::npos) {
                        const auto topic = app_help::context(name.substr(marker + 3));
                        page = topic.page; anchor = topic.anchor; break;
                    }
                }
            }
            app_help::open(root_, "app/Documentation", locale_, page, page == "tour" ? "interface" : anchor, smoke_ || testSession_, page == "tour" ? "interface-image" : "");
            return;
        }
        if(id.starts_with("problem.")) {navigateProblem(id=="problem.previous"||id=="problem.previousInFile",id.ends_with("InFile"));return;}
        if(id=="build.presetTest" || id=="build.presetPackage" || id=="build.presetWorkflow") {
            presetKind_=id=="build.presetTest" ? 0 : id=="build.presetPackage" ? 1 : 2;
            presetName_.clear();presetPlan_=nullptr;showPanel("CMake preset actions","cmakePresets");
        }
        else if (id == "debug.start") { if (debugState_.value("state","") == "stopped") debugTask("debug.continue"); else startDebug(); }
        else if (id == "debug.breakpoint") toggleBreakpoint(active_,tab->editor.GetMainCursorPosition().line+1);
        else if (id == "debug.terminal") { terminalId_ = debugState_.at("terminalId"); showPanel("Terminal","terminal"); focusTerminal_ = true; terminalScrollBottom_ = true; }
        else if (id == "debug.hotReload") {
            saveAll();
            if (std::none_of(tabs_.begin(),tabs_.end(),[](const auto &tab){return tab->dirty;}))
                debugTask(id,{{"sessionGeneration",debugState_.value("sessionGeneration",uint64_t(0))}});
        }
        else if (id == "debug.stepInTargets") requestStepInTargets();
        else if (id == "debug.gotoTargets") requestGotoTargets(true);
        else if (id == "debug.runToCursor") requestRunToCursor();
        else if (id == "debug.restoreRunToCursor") debugTask(id,{{"sessionGeneration",debugState_.at("sessionGeneration")},{"stopGeneration",debugState_.at("stopGeneration")},{"expectedRevision",debugState_.at("runToCursor").at("revision")}});
        else if (id.starts_with("debug.")) debugTask(id);
        else if (id == "run.start") { if (projectActive_ && debugUnrealLaunch_.is_null()) task("project.launch"); else debugTask("debug.run",debugParameters()); }
        else if (id == "view.terminal") {
            if (terminalFocused_) { panels_["terminal"] = false; terminalFocused_ = false; focusTab_ = active_; }
            else { showPanel("Terminal", "terminal"); focusTerminal_ = true; if (engine_->terminals().list().at("sessions").empty()) newTerminal(); }
        }
        else if (id == "terminal.new") newTerminal();
        else if (id == "edit.snippet" || id == "edit.surroundSnippet") beginSnippets();
        else if(id=="edit.userSnippets")beginUserSnippets();
        else if(id=="edit.captureSnippet")beginUserSnippets(true);
        else if(id=="navigate.counterpart")beginCounterparts();
        else if(id=="navigate.include")beginCounterparts(true);
        else if(id=="navigate.relatedFiles")beginCounterparts(false,true);
        else if (id == "edit.snippetCatalogue") {
            const std::string path(snippetCataloguePath);
            if (!std::filesystem::exists(engine_->resolve(path))) {
                const auto created=engine_->dispatch("documents.create",{{"path",path}});
                engine_->dispatch("documents.replace",{{"path",path},{"expectedRevision",created.at("revision")},{"text","{\n}\n"}});
            }
            open(path);
        }
        else if (id == "file.new") newFile_ = true;
        else if (id == "file.newUntitled") newUntitled();
        else if (id == "file.newProject") beginProjectTemplate();
        else if (id == "file.open") chooseSolution();
        else if (id == "file.openImage") chooseImage();
        else if (id == "file.folder") chooseFolder();
        else if (id == "file.window") newWindow(entry_);
        else if (id == "file.openWindow") chooseSolution(true);
        else if (id == "file.save") save(tab);
        else if (id == "file.saveAs") beginSaveAs(tab);
        else if (id == "file.saveAll") saveAll();
        else if (id == "file.close") closeTab(active_);
        else if (id == "file.closeAll") closeAll_ = true;
        else if (id == "file.reopen") { auto path = closedFiles_.back(); closedFiles_.pop_back(); open(path); }
        else if (id == "file.exit") requestQuit();
        else if (id == "search.workspace") showPanel("Search", "search");
        else if (id == "search.document" || id == "search.replace") { focusTab_ = active_; tab->editor.SetFocus(); tab->editor.OpenFindReplaceWindow(); }
        else if (id == "search.next") { focusTab_ = active_; tab->editor.FindNext(); tab->editor.SetFocus(); }
        else if (id == "search.files" || id == "search.actions" || id == "search.everywhere" || id == "navigate.recent" || id == "search.types" || id == "search.symbols" || id == "navigate.members") {
            paletteMode_ = id == "search.actions" ? "actions" : id == "search.everywhere" ? "everywhere" : id == "navigate.recent" ? "recent" : id == "search.types" ? "types" : id == "search.symbols" ? "symbols" : id == "navigate.members" ? "members" : "files";
            paletteOpen_ = true;
        } else if (id == "refactor.rename") beginRename();
        else if (id == "refactor.format" || id == "refactor.formatSelection") beginFormat(id == "refactor.formatSelection");
        else if (id == "refactor.undo") changeTask("changes.undo", {{"id", refactorPlan_.at("id")}});
        else if (id == "refactor.apply") changeTask("changes.apply", {{"id", refactorPlan_.at("id")}});
        else if (id == "navigate.definition") navigate(false);
        else if (id == "navigate.references") navigate(true);
        else if (id == "navigate.declaration" || id == "navigate.typeDefinition" || id == "navigate.implementation") requestNavigation("language."+id.substr(9));
        else if (id == "navigate.callHierarchy" || id == "navigate.typeHierarchy") requestHierarchy(id=="navigate.callHierarchy" ? "call" : "type");
        else if (id == "code.occurrences" || id == "code.nextOccurrence" || id == "code.previousOccurrence" || id == "code.selectOccurrences" || id == "code.clearOccurrences") occurrenceAction(id);
        else if (id == "code.documentation" || id == "code.parameters") requestInformation(id=="code.parameters");
        else if (id=="code.actions") requestCodeActions();
        else if (id == "navigate.back" || id == "navigate.forward") navigationHistory(id == "navigate.forward");
        else if (id == "navigate.line") { linePopup_ = true; lineNumber_ = static_cast<int>(tab->editor.GetMainCursorPosition().line + 1); }
        else if (id == "navigate.nextTab" || id == "navigate.previousTab") {
            auto at = std::find(tabs_.begin(), tabs_.end(), tab) - tabs_.begin();
            auto next = (at + tabs_.size() + (id == "navigate.nextTab" ? 1 : -1)) % tabs_.size();
            open(tabs_[next]->path);
        } else if(id=="tests.discover"){showPanel("Tests","tests");testTask("tests.discover",{{"buildDirectory",buildDirectory_},{"configuration",configuration_}});}
        else if(id=="tests.run"){Json selected=Json::array();for(auto i:testsSelection_)selected.push_back(i);showPanel("Tests","tests");testTask("tests.run",{{"id",testsCatalog_.at("id")},{"tests",selected}});}
        else if(id=="tests.cancel")engine_->dispatch("tests.cancel");
        else if (id == "build.run") task("build.run");
        else if (id == "build.configure") task("build.configure");
        else if (id == "build.cancel") cancelBuild();
        else if (id.starts_with("msbuild.")) { showPanel("MSBuild","msbuild"); task(id); }
        else if (id == "unreal.prepare") prepareUnreal();
        else if (id == "project.refresh") refreshFiles();
        else if(id=="project.newFolder")beginResource("createDirectory",resourceFolder_=="."?"":resourceFolder_+"/");
        else if(id=="project.moveFile")beginResource("move",selectedResource_,selectedResource_);
        else if(id=="project.deleteFile")beginResource("delete",selectedResource_);
        else if(id=="project.undoFile")resourceTask("resources.undo",{{"id",resourceLast_.at("id")}});
        else if (id == "project.cancel") engine_->dispatch("project.cancel");
        else if (id == "project.test" || id == "project.install" || id == "project.script") task(id);
        else if (id == "vcs.refresh") refreshGit();
        else if (id == "vcs.diff" || id == "vcs.blame" || id=="vcs.hunks") { showPanel("Version control","vcs"); vcsTask(id,{{"path",active_}}); }
        else if (id.starts_with("vcs.")) {
            showPanel("Version control","vcs");
            for (const auto &[command,section] : std::initializer_list<std::pair<const char *,const char *>>{{"changes","Changes"},{"branches","Branches"},{"history","History"},{"remotes","Remotes"},{"stashes","Stashes"},{"conflicts","Conflicts"}})
                if (id == std::string("vcs.")+command) vcsSection_ = section;
            if (id == "vcs.conflicts") {
                std::string conflict;
                for (const auto &file : git_.value("files",Json::array())) {
                    const auto status = file.at("status").get<std::string>();
                    if (status.find('U') != std::string::npos || status == "AA" || status == "DD") { conflict = file.at("path"); if (conflict == active_) break; }
                }
                // The status read may still be starting when invoked from Find Action.
                if (conflict.empty() && gitFuture_.valid() && !active_.starts_with("sdk://") && !active_.starts_with("debug://")) conflict = active_;
                if (!conflict.empty()) vcsTask("vcs.conflicts",{{"path",conflict}});
            }
            refreshGit();
        }
        else if (id == "app.checkUpdates") checkApplicationUpdates(true);
        else if (id == "view.reset") resetLayout_ = true;
        else if (id == "view.recovery") { showPanel("Recovery", "recovery");recoveryTask("recovery.list"); }
        else if (id == "view.options" || id == "view.keymap") { options_ = true; focusKeymap_ = id == "view.keymap"; }
        else if (id.starts_with("view.") && findCommand(id)->context == CommandContext::global) {
            auto panel = id.substr(5); showPanel(findCommand(id)->title.c_str(), panel.c_str());
        } else if (tab) {
            tab->pendingSession = nullptr;
            auto &e = tab->editor;
            const auto undo = e.GetUndoIndex();
            const auto pos = e.GetMainCursorPosition();
            if (id == "edit.undo") e.Undo();
            else if (id == "edit.redo") e.Redo();
            else if (id == "edit.cut") e.Cut();
            else if (id == "edit.copy") e.Copy();
            else if (id == "edit.paste") e.Paste();
            else if (id == "edit.selectAll") e.SelectAll();
            else if (id == "edit.indent") e.IndentLines();
            else if (id == "edit.unindent") e.DeindentLines();
            else if (id == "edit.moveUp") e.MoveUpLines();
            else if (id == "edit.moveDown") e.MoveDownLines();
            else if (id == "edit.grow") e.GrowSelections();
            else if (id == "edit.shrink") e.ShrinkSelections();
            else if (id == "edit.nextOccurrence") e.AddNextOccurrence();
            else if (id == "edit.allOccurrences") e.SelectAllOccurrences();
            else if (id == "edit.upper") e.SelectionToUpperCase();
            else if (id == "edit.lower") e.SelectionToLowerCase();
            else if (id == "edit.trim") e.StripTrailingWhitespaces();
            else if (id == "edit.duplicate") {
                if (e.MainCursorHasSelection()) { auto s = e.GetMainCursorSelection(); auto text = e.GetSectionText(s); e.ReplaceSectionText(s, text + text); }
                else { auto text = e.GetLineText(pos.line); e.ReplaceSectionText({pos.line, 0}, {pos.line, 0}, text + "\n"); e.SetCursor({pos.line + 1, pos.index}); }
            } else if (id == "edit.deleteLine") {
                auto selection = e.GetMainCursorSelection();
                auto a = std::min(selection.start.line, selection.end.line), b = std::max(selection.start.line, selection.end.line);
                if (b > a && selection.end.index == 0) --b;
                if (b + 1 < e.GetLineCount()) e.ReplaceSectionText({a, 0}, {b + 1, 0}, "");
                else if (a > 0) e.ReplaceSectionText({a-1, LanguageClient::utf16Length(e.GetLineText(a-1))}, {b, e.GetLineText(b).size()}, "");
                else e.ReplaceSectionText({0, 0}, {b, e.GetLineText(b).size()}, "");
                e.SetCursor({std::min(a, e.GetLineCount()-1), 0});
            } else if (id == "edit.comment" || id == "edit.addComment" || id == "edit.removeComment") {
                const auto prefix = commentPrefix(active_);
                auto selection = e.GetMainCursorSelection();
                auto first = selection.start.line, last = selection.end.line;
                if (first > last) std::swap(first, last);
                if (last > first && selection.end.index == 0) --last;
                bool allCommented = true;
                for (size_t line = first; line <= last; ++line) { auto text = e.GetLineText(line); auto at = text.find_first_not_of(" \t"); if (at != std::string::npos && text.compare(at, prefix.size(), prefix)) allCommented = false; }
                const bool remove = id == "edit.removeComment" || (id == "edit.comment" && allCommented);
                std::string replacement;
                for (size_t line = first; line <= last; ++line) {
                    auto text = e.GetLineText(line); auto at = text.find_first_not_of(" \t");
                    if (at != std::string::npos) {
                        if (remove && text.compare(at, prefix.size(), prefix) == 0) { text.erase(at, prefix.size()); if (at < text.size() && text[at] == ' ') text.erase(at, 1); }
                        else if (!remove && text.compare(at, prefix.size(), prefix) != 0) text.insert(at, prefix + " ");
                    }
                    if (line > first) replacement += '\n';
                    replacement += text;
                }
                e.ReplaceSectionText({first, 0}, {last, e.GetLineText(last).size()}, replacement);
            } else if (id == "view.wrap") e.SetWordWrapEnabled(!e.IsWordWrapEnabled());
            else if (id == "view.lineNumbers") e.SetShowLineNumbersEnabled(!e.IsShowLineNumbersEnabled());
            else if (id == "view.whitespace") e.SetShowWhitespacesEnabled(!e.IsShowWhitespacesEnabled());
            else if (id == "view.minimap") e.SetShowMiniMapEnabled(!e.IsShowMiniMapEnabled());
            else if (id == "view.fold") { if (keymapProfile_ == "visualstudio") e.ToggleAtLine(pos.line); else e.FoldAroundLine(pos.line); }
            else if (id == "view.unfold") e.UnfoldAroundLine(pos.line);
            else if (id == "view.unfoldAll") e.UnfoldAll();
            if (undo != e.GetUndoIndex()) changed(tab);
            focusTab_ = active_; e.SetFocus();
        }
    } catch (const std::exception &e) { error(e.what()); }
}
void Application::goToLine() {
    if (linePopup_) { ImGui::OpenPopup(tr("Go to line...")); linePopup_ = false; }
    if (ImGui::BeginPopup(tr("Go to line..."))) {
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(cyui::dp(220));
        const bool enter = ImGui::InputInt("##line", &lineNumber_, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
        if (cyui::primaryButton(tr("Go")) || enter) {
            rememberNavigation();
            for (auto &tab : tabs_) if (tab->path == active_) { auto line = std::clamp<size_t>(std::max(lineNumber_, 1) - 1, 0, tab->editor.GetLineCount() - 1); tab->editor.SetCursor({line, 0}); tab->editor.ScrollToLine(line); tab->editor.SetFocus(); focusTab_ = active_; }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void Application::keymapOptions() {
    if (focusKeymap_) { ImGui::SetNextItemOpen(true); focusKeymap_ = false; }
    if (!cyui::beginSection(tr("Keyboard shortcuts"))) return;
    if (cyui::beginProperties("##keymap-properties", 160)) {
        cyui::property(tr("Keymap profile"));
        const char *profiles[] = {"rider", "visualstudio", "vscode"};
        const char *names[] = {"Rider / ReSharper", "Visual Studio", "Visual Studio Code"};
        int selected = keymapProfile_ == "rider" ? 0 : keymapProfile_ == "visualstudio" ? 1 : 2;
        if (ImGui::Combo("##keymap", &selected, names, 3)) {
            try { applyKeymap(profiles[selected], keymapOverrides_.value(profiles[selected], Json::object())); keymapProfile_ = profiles[selected]; keymapEditId_.clear(); }
            catch (const std::exception &e) { error(e.what()); }
        }
        cyui::endProperties();
    }
    ImGui::TextWrapped("%s", tr("Choose a command to change its shortcuts. Separate alternatives with ; and chord steps with a comma. An empty value disables the shortcut."));
    ImGui::SetNextItemWidth(-1);
    cyui::filterBox("##keymap-filter", shortcutsFilter_, {"Filter (Ctrl/Cmd+F)...", 0, false});
    if (ImGui::BeginTable("##commands", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable, ImVec2(0, cyui::dp(210)))) {
        ImGui::TableSetupColumn(tr("Command")); ImGui::TableSetupColumn(tr("Shortcut")); ImGui::TableSetupColumn(tr("Context")); ImGui::TableHeadersRow();
        for (const auto &command : commandCatalogue()) {
            if (!shortcutsFilter_.matches(command.title, command.id + " " + keymap_.hint(command.id))) continue;
            ImGui::PushID(command.id.c_str()); ImGui::TableNextRow(); ImGui::TableNextColumn();
            if (ImGui::Selectable(tr(command.title.c_str()), command.id == keymapEditId_, ImGuiSelectableFlags_SpanAllColumns)) {
                keymapEditId_ = command.id; keymapEditText_.clear();
                for (const auto &binding : keymap_.bindings()) if (binding.command == command.id) { if (!keymapEditText_.empty()) keymapEditText_ += "; "; keymapEditText_ += binding.shortcut; }
            }
            ImGui::TableNextColumn(); ImGui::TextUnformatted(keymap_.hint(command.id).c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(tr(command.context == CommandContext::global ? "Global" : command.context == CommandContext::editor ? "Editor" : "Document"));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (!keymapEditId_.empty()) {
        const auto catalogue=commandCatalogue();const auto selected=std::find_if(catalogue.begin(),catalogue.end(),[&](const auto &c) { return c.id==keymapEditId_; });
        cyui::textDim("%s",selected==catalogue.end()?keymapEditId_.c_str():tr(selected->title.c_str()));
        ImGui::SetNextItemWidth(-1); ImGui::InputTextWithHint("##binding-edit", "Ctrl+K, Ctrl+C; Ctrl+Slash", &keymapEditText_);
        if (cyui::button(tr("Apply shortcut"))) {
            try {
                Json bindings = Json::array(); std::istringstream input(keymapEditText_); std::string text;
                while (std::getline(input, text, ';')) { auto start = text.find_first_not_of(" \t"); if (start != std::string::npos) bindings.push_back(text.substr(start, text.find_last_not_of(" \t") - start + 1)); }
                auto overrides = keymapOverrides_.value(keymapProfile_, Json::object()); overrides[keymapEditId_] = bindings;
                applyKeymap(keymapProfile_, overrides); keymapOverrides_[keymapProfile_] = overrides;
            } catch (const std::exception &e) { error(e.what()); }
        }
        ImGui::SameLine();
        if (cyui::button(tr("Reset command"))) {
            auto overrides = keymapOverrides_.value(keymapProfile_, Json::object()); overrides.erase(keymapEditId_);
            try { applyKeymap(keymapProfile_, overrides); keymapOverrides_[keymapProfile_] = overrides; keymapEditId_.clear(); }
            catch (const std::exception &e) { error(e.what()); }
        }
    }
    if (cyui::button(tr("Reset profile"))) { keymap_.load(keymapProfile_); keymapOverrides_.erase(keymapProfile_); keymapEditId_.clear(); }
    cyui::endSection();
}
} // namespace cycode
