#include "cycode/Application.h"
#include "cycode/EditorTheme.h"
#include <algorithm>
#include <cmath>
#include <cyui/Core.h>
#include <imgui_internal.h>

namespace cycode {
void Application::newTerminal() {
    if (terminalFuture_.valid() || smoke_) return;
    showPanel("Terminal", "terminal"); terminalOperation_ = "terminal.create";
    auto engine = engine_; const auto profile = terminalProfile_;
    terminalFuture_ = std::async(std::launch::async, [engine, profile] { return engine->dispatch("terminal.create", {{"profile", profile}}); });
}
void Application::updateTerminal() {
    if (!terminalFuture_.valid() || terminalFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        auto result = terminalFuture_.get();
        if (terminalOperation_ == "terminal.create") { terminalId_ = result.at("id"); focusTerminal_ = true; terminalScrollBottom_ = true; terminalSelectionStart_ = terminalSelectionEnd_ = {-1, 0}; }
    } catch (const std::exception &e) { error(e.what()); }
}
static ImU32 terminalColor(const TerminalColor &color, bool background, const std::string &theme) {
    const auto &p = cyui::palette();
    if (color.index == -1) return background ? p.bg0 : p.text;
    if (color.index == -2) return ImGui::ColorConvertFloat4ToU32({color.red / 255.f, color.green / 255.f, color.blue / 255.f, 1});
    if (color.index < 16) {
        const ImU32 base[] = {p.bg0, p.error, p.success, p.warning, p.info, syntaxColours(theme).type, p.accent, p.text};
        return color.index < 8 ? base[color.index] : (color.index == 8 ? p.textDim : cyui::mix(base[color.index-8], p.text, .2f));
    }
    // Standard xterm-256 indexed colours are terminal content, not UI theme literals.
    if (color.index < 232) {
        int value = color.index - 16;
        auto channel = [](int n) { return (n ? 55 + n*40 : 0) / 255.f; };
        return ImGui::ColorConvertFloat4ToU32({channel(value / 36), channel((value / 6) % 6), channel(value % 6), 1});
    }
    float grey = (8 + (color.index-232)*10) / 255.f; return ImGui::ColorConvertFloat4ToU32({grey, grey, grey, 1});
}
void Application::terminalPanel() {
    terminalFocused_ = false;
    if (!beginPanel("Terminal", "terminal")) return;
    bool childOpen = false, popupOpen = false;
    try {
        auto &terminals = engine_->terminals(); const auto sessions = terminals.list().at("sessions");
        if (!sessions.empty() && std::none_of(sessions.begin(), sessions.end(), [&](const Json &s) { return s.at("id") == terminalId_; })) terminalId_ = sessions[0].at("id");
        if (sessions.empty()) terminalId_.clear();
        ImGui::BeginDisabled(terminalFuture_.valid() || smoke_);
        if (cyui::iconButton(cyui::icon::add, tr("New terminal (Find action)"))) newTerminal();
        ImGui::SameLine(); ImGui::SetNextItemWidth(cyui::dp(155));
        if (ImGui::BeginCombo("##shell-profile", terminalProfile_.c_str())) {
            for (const auto *profile : {
#ifdef _WIN32
                "default", "powershell", "cmd"
#else
                "default", "bash", "zsh"
#endif
            }) if (ImGui::Selectable(profile, terminalProfile_ == profile)) terminalProfile_ = profile;
            ImGui::EndCombo();
        }
        ImGui::SameLine(); ImGui::SetNextItemWidth(cyui::dp(220));
        if (ImGui::BeginCombo("##terminal-session", terminalId_.c_str())) {
            for (const auto &session : sessions) {
                const auto id = session.at("id").get<std::string>();
                auto title = id + "  " + pathUtf8(utf8Path(session.at("executable").get<std::string>()).filename());
                if (ImGui::Selectable(title.c_str(), terminalId_ == id)) { terminalId_ = id; focusTerminal_ = true; terminalScrollBottom_ = true; terminalSelectionStart_ = terminalSelectionEnd_ = {-1, 0}; }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine(); ImGui::BeginDisabled(terminalId_.empty());
        if (cyui::iconButton(cyui::icon::trash, tr("Close terminal and its processes"))) {
            auto engine = engine_; const auto id = terminalId_; terminalOperation_ = "terminal.close";
            terminalFuture_ = std::async(std::launch::async, [engine, id] { return engine->dispatch("terminal.close", {{"id", id}}); });
        }
        ImGui::SameLine();
        if (cyui::ghostButton(tr("Clear scrollback"))) { terminals.clear(terminalId_); terminalSelectionStart_ = terminalSelectionEnd_ = {-1, 0}; }
        ImGui::EndDisabled(); ImGui::EndDisabled();
        if (terminalFuture_.valid()) { ImGui::SameLine(); cyui::spinner("##terminal-starting"); }
        if (terminalId_.empty()) {
            cyui::emptyState(tr("Terminal"), tr("Open a terminal to run commands in this workspace. Alt+F12 focuses the terminal."), cyui::icon::code);
            ImGui::End(); return;
        }
        cyui::ScopedFont mono(cyui::monoFont());
        auto size = ImGui::GetContentRegionAvail();
        const float cellWidth = ImGui::CalcTextSize("M").x, cellHeight = ImGui::GetTextLineHeight() * 1.12f;
        const int columns = std::clamp(int((size.x - ImGui::GetStyle().ScrollbarSize - cyui::dp(8)) / cellWidth), 10, 400);
        const int rows = std::clamp(int((size.y - cyui::dp(8)) / cellHeight), 2, 120);
        terminals.resize(terminalId_, columns, rows);
        auto metadata = terminals.snapshot(terminalId_, 0, 0);
        cyui::ScopedColor bg(ImGuiCol_ChildBg, cyui::vec(cyui::palette().bg0));
        ImGui::BeginChild("##terminal-canvas", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_NoNavInputs);
        childOpen = true;
        if (focusTerminal_) { ImGui::SetWindowFocus(); focusTerminal_ = false; }
        terminalFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        auto origin = ImGui::GetCursorScreenPos(); origin.x += cyui::dp(4);
        const auto total = metadata.total;
        const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - cellHeight;
        auto point = [&](ImVec2 mouse) { return std::pair{std::clamp(int((mouse.y-origin.y) / cellHeight), 0, std::max(0,total-1)), std::clamp(int((mouse.x-origin.x) / cellWidth), 0, columns)}; };
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImGui::SetWindowFocus(); terminalSelectionStart_ = terminalSelectionEnd_ = point(ImGui::GetIO().MousePos); terminalSelecting_ = true;
        }
        if (terminalSelecting_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) terminalSelectionEnd_ = point(ImGui::GetIO().MousePos);
        else terminalSelecting_ = false;
        auto selectionStart = std::min(terminalSelectionStart_, terminalSelectionEnd_), selectionEnd = std::max(terminalSelectionStart_, terminalSelectionEnd_);
        const bool selected = selectionStart.first >= 0 && selectionStart != selectionEnd;
        auto copy = [&] {
            if (!selected) return;
            std::string text; auto snapshot = terminals.snapshot(terminalId_, selectionStart.first, selectionEnd.first-selectionStart.first+1);
            for (size_t row = 0; row < snapshot.lines.size(); ++row) {
                auto line = snapshot.lines[row]; const int first = row == 0 ? selectionStart.second : 0;
                const int end = int(row) + selectionStart.first == selectionEnd.first ? selectionEnd.second : int(line.size());
                line.resize(std::clamp(end, 0, int(line.size()))); if (first < int(line.size())) line.erase(line.begin(), line.begin()+first); else line.clear();
                if (row) text += '\n'; text += Terminals::text(line);
            }
            ImGui::SetClipboardText(text.c_str());
        };
        auto paste = [&] { auto clipboard = ImGui::GetClipboardText(); if (clipboard) terminals.paste(terminalId_, clipboard); terminalScrollBottom_ = true; };
        if (ImGui::BeginPopupContextWindow("##terminal-menu")) {
            popupOpen = true;
            if (ImGui::MenuItem(tr("Copy"), "Ctrl+Shift+C", false, selected)) copy();
            if (ImGui::MenuItem(tr("Paste"), "Ctrl+Shift+V", false, metadata.running)) paste();
            if (ImGui::MenuItem(tr("Select all"))) { terminalSelectionStart_ = {0, 0}; terminalSelectionEnd_ = {total-1, columns}; }
            ImGui::EndPopup();
            popupOpen = false;
        }
        auto draw = ImGui::GetWindowDrawList();
        std::map<std::pair<ImU32,ImU32>,ImU32> contrastCache;
        ImGuiListClipper clipper; clipper.Begin(total, cellHeight);
        while (clipper.Step()) {
            auto snapshot = terminals.snapshot(terminalId_, clipper.DisplayStart, clipper.DisplayEnd-clipper.DisplayStart);
            for (size_t index = 0; index < snapshot.lines.size(); ++index) {
                const int row = snapshot.first + int(index); float y = origin.y + row*cellHeight;
                for (size_t col = 0; col < snapshot.lines[index].size(); ++col) {
                    const auto &cell = snapshot.lines[index][col]; if (!cell.width || cell.chars[0] == UINT32_MAX) continue;
                    auto foreground = terminalColor(cell.foreground, false, theme_), background = terminalColor(cell.background, true, theme_);
                    if (cell.reverse) std::swap(foreground, background);
                    if (selected && std::pair{row,int(col)} >= selectionStart && std::pair{row,int(col)} < selectionEnd) background = cyui::palette().selection;
                    const ImVec2 left{origin.x+col*cellWidth,y}, right{left.x + std::max(1,cell.width)*cellWidth,y+cellHeight};
                    if (background != cyui::palette().bg0) draw->AddRectFilled(left, right, background);
                    auto text = Terminals::cellText(cell);
                    auto [color, inserted] = contrastCache.try_emplace({foreground,background});
                    if (inserted) color->second = readableText(foreground,{background});
                    foreground = color->second;
                    if (!text.empty()) { draw->AddText(left, foreground, text.c_str()); if (cell.bold) draw->AddText({left.x+cyui::dp(.4f),left.y}, foreground, text.c_str()); }
                    if (cell.underline) draw->AddLine({left.x,right.y-cyui::dp(2)}, {right.x,right.y-cyui::dp(2)}, foreground);
                    if (cell.strike) draw->AddLine({left.x,y+cellHeight*.5f}, {right.x,y+cellHeight*.5f}, foreground);
                }
            }
        }
        if (metadata.running && metadata.cursorVisible && terminalFocused_ && std::fmod(ImGui::GetTime(),1.1) < .7) {
            ImVec2 cursor{origin.x+metadata.cursorColumn*cellWidth,origin.y+metadata.cursorRow*cellHeight};
            draw->AddRect(cursor, {cursor.x+cellWidth,cursor.y+cellHeight}, cyui::palette().text);
        }
        if (terminalScrollBottom_ || atBottom || metadata.alternate) {
            ImGui::SetScrollY(std::max(0.f, total*cellHeight-ImGui::GetWindowHeight()+ImGui::GetStyle().WindowPadding.y*2)); terminalScrollBottom_ = false;
        }
        if (terminalFocused_ && !ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
            auto &ime = ImGui::GetCurrentContext()->PlatformImeData;
            ime.WantTextInput = true; ime.WantVisible = true; ime.ViewportId = ImGui::GetWindowViewport()->ID;
            ime.InputPos = {origin.x + metadata.cursorColumn*cellWidth, origin.y + metadata.cursorRow*cellHeight};
            ime.InputLineHeight = cellHeight;
            auto &io = ImGui::GetIO();
            const int modifiers = (io.KeyShift ? 1 : 0) | (io.KeyAlt ? 2 : 0) | (io.KeyCtrl ? 4 : 0);
            if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_C,false)) copy();
            else if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_V,false)) paste();
            else if (metadata.running) {
                for (const auto &[key,name] : std::initializer_list<std::pair<ImGuiKey,const char *>>{{ImGuiKey_Enter,"Enter"},{ImGuiKey_KeypadEnter,"Enter"},{ImGuiKey_Tab,"Tab"},{ImGuiKey_Backspace,"Backspace"},{ImGuiKey_Escape,"Escape"},{ImGuiKey_LeftArrow,"Left"},{ImGuiKey_RightArrow,"Right"},{ImGuiKey_UpArrow,"Up"},{ImGuiKey_DownArrow,"Down"},{ImGuiKey_Home,"Home"},{ImGuiKey_End,"End"},{ImGuiKey_Delete,"Delete"},{ImGuiKey_Insert,"Insert"},{ImGuiKey_PageUp,"PageUp"},{ImGuiKey_PageDown,"PageDown"}})
                    if (ImGui::IsKeyPressed(key)) { terminals.key(terminalId_, name, modifiers); terminalScrollBottom_ = true; }
                for (int i = 0; i < 12; ++i) if (ImGui::IsKeyPressed(ImGuiKey(ImGuiKey_F1+i))) terminals.key(terminalId_, "F"+std::to_string(i+1), modifiers);
                if (io.KeyCtrl && !io.KeyAlt) for (int i = 0; i < 26; ++i) if (ImGui::IsKeyPressed(ImGuiKey(ImGuiKey_A+i))) terminals.character(terminalId_, 'a'+i, modifiers);
                if (io.KeyCtrl && !io.KeyAlt) for (const auto &[key,character] : std::initializer_list<std::pair<ImGuiKey,uint32_t>>{{ImGuiKey_Space,' '},{ImGuiKey_LeftBracket,'['},{ImGuiKey_RightBracket,']'},{ImGuiKey_Backslash,'\\'},{ImGuiKey_Minus,'_'},{ImGuiKey_Slash,'?'}})
                    if (ImGui::IsKeyPressed(key)) terminals.character(terminalId_, character, modifiers);
                if (!io.KeyCtrl || io.KeyAlt) for (auto character : io.InputQueueCharacters) if (character >= 32) { terminals.character(terminalId_, character, io.KeyAlt && !io.KeyCtrl ? 2 : 0); terminalScrollBottom_ = true; }
            }
            io.InputQueueCharacters.resize(0);
        }
        ImGui::EndChild();
        childOpen = false;
    } catch (const std::exception &e) {
        if (popupOpen) ImGui::EndPopup(); if (childOpen) ImGui::EndChild();
        if (std::string_view(e.what()) == "Unknown terminal session") { terminalId_.clear(); terminalFocused_ = false; }
        else error(e.what());
    }
    ImGui::End();
}
} // namespace cycode
