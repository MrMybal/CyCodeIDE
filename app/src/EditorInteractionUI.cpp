#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_internal.h>
#include <algorithm>

namespace cycode {
void Application::openFromProject(const std::string &path, bool keep) {
    std::string canonical;
    try { canonical = path.find("://") != std::string::npos ? path : pathUtf8(engine_->resolve(path).lexically_relative(engine_->workspace())); }
    catch (const std::exception &e) { error(e.what()); return; }
    const bool existed = std::any_of(tabs_.begin(), tabs_.end(), [&](const auto &tab) { return tab->path == canonical; });
    const auto previous = std::find_if(tabs_.begin(), tabs_.end(), [](const auto &tab) { return tab->preview; });
    const auto preview = previous == tabs_.end() ? std::shared_ptr<Tab>{} : *previous;
    // Opening/permission checks finish before the previous preview can be closed.
    open(path);
    const auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const auto &tab) { return tab->path == active_; });
    if (found == tabs_.end() || active_ != canonical) return;
    const auto selected = *found;
    if (keep) selected->preview = false;
    else if (!selected->dirty && !existed) selected->preview = true;
    if (!keep && preview && preview != selected) {
        if (!preview->dirty && !preview->completionAccept.valid() &&
            !preview->pagedOpening.valid() && !preview->pagedNavigation.valid() &&
            (!preview->paged || (!preview->paged->status().busy && !preview->paged->unsettled())))
            closeTab(preview->path);
        preview->preview = false; // Also promote it if a concurrent native edit prevented close.
    }
    active_ = focusTab_ = selected->path;
    rightmostTab_ = selected->path;
    // A user opening a file takes priority over queued session restoration.
    sessionRestoreFocus_ = false;
}

void Application::focusDocumentTab(const std::shared_ptr<Tab> &tab, const std::string &title) {
    if (focusTab_ != tab->path && rightmostTab_ != tab->path) return;
    if (focusTab_ == tab->path) { ImGui::SetNextWindowFocus(); focusTab_.clear(); }
    const auto window = ImGui::FindWindowByName(title.c_str());
    if (!window || !window->DockNode || !window->DockNode->TabBar) return;
    auto bar = window->DockNode->TabBar;
    auto item = ImGui::TabBarFindTabByID(bar, window->TabId);
    if (!item) return;
    ImGui::TabBarQueueFocus(bar, item); // Dear ImGui also scrolls the selected tab into view.
    if (rightmostTab_ == tab->path) {
        const int index = static_cast<int>(item - bar->Tabs.Data);
        const int offset = bar->Tabs.Size - index - 1;
        if (offset > 0) {
            if (!bar->ReorderRequestTabId) ImGui::TabBarQueueReorder(bar, item, offset);
        } else rightmostTab_.clear();
    }
}

void Application::editorContextMenu(const std::shared_ptr<Tab> &tab, TextEditor::DocPos position) {
    active_ = tab->path;
    if (ImGui::IsWindowAppearing()) {
        const auto selection=tab->editor.GetMainCursorSelection();
        const auto before=[](auto a,auto b){return a.line<b.line || (a.line==b.line && a.index<b.index);};
        const auto begin=before(selection.start,selection.end)?selection.start:selection.end;
        const auto end=before(selection.start,selection.end)?selection.end:selection.start;
        if(!tab->editor.MainCursorHasSelection() || before(position,begin) || before(end,position))tab->editor.SetCursor(position);
    }
    const auto catalogue = commandCatalogue();
    std::vector<cyui::FilterMenuItem> rows;
    auto add = [&](std::vector<cyui::FilterMenuItem> &to, const std::string &id) {
        const auto at = std::find_if(catalogue.begin(), catalogue.end(), [&](const auto &c) { return c.id == id; });
        if (at != catalogue.end()) to.push_back({uiId("command/" + id), at->title, keymap_.hint(id), id, commandEnabled(id), false, {}});
    };
    for (const auto *id : {"navigate.definition", "navigate.references", "code.actions"}) add(rows, id);
    rows.push_back({});
    for (const auto *id : {"edit.cut", "edit.copy", "edit.paste", "edit.undo", "edit.redo"}) add(rows, id);
    rows.push_back({});
    for (const auto *category : {"Navigate", "Refactor", "Edit", "Search", "File", "View", "Version control", "Run"}) {
        cyui::FilterMenuItem group;
        group.label = category;
        for (const auto &command : catalogue) if (command.category == category && command.context != CommandContext::global)
            add(group.children, command.id);
        if (!group.children.empty()) rows.push_back(std::move(group));
    }
    rows.push_back({});
    add(rows, "file.keepOpen");
    rows.push_back({uiId("editor/copy-path"), "Copy file path"});
    const auto request = commandMenu("##editor-actions",editorMenuFilter_,rows);
    if (request == uiId("editor/copy-path")) SDL_SetClipboardText(tab->displayPath.c_str());
    else if (request) for (const auto &command : catalogue)
        if (request == uiId("command/" + command.id)) { editorCommand_=command.id;editorCommandPath_=tab->path;break; }
    extensionMenu("editor");
}

void Application::editorMouseNavigation(const std::shared_ptr<Tab> &tab) {
    auto &io = ImGui::GetIO();
    if (!io.KeyCtrl || io.KeyAlt || io.KeyShift ||
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) ||
        !tab->editor.IsMousePosOverGlyph(io.MousePos)) return;
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
    const auto position = tab->editor.GetDocPosAtMousePos(io.MousePos);
    // Replace the widget's Ctrl-click multicursor gesture with IDE navigation.
    tab->editor.SetSelections({{position, position}});
    active_ = tab->path;
    const auto line = tab->editor.GetLineText(position.line);
    if (line.find("#include") != std::string::npos || line.find("# include") != std::string::npos)
        executeCommand("navigate.include");
    else {
        const auto column=tab->editor.GetSectionText({position.line,0},position).size();
        for(const char quote:{'"','\''}) {
            const auto begin=line.rfind(quote,column);
            if(begin==std::string::npos)continue;
            const auto end=line.find(quote,begin+1);
            if(end==std::string::npos || column<=begin || column>=end || end-begin>4097)continue;
            try {
                auto candidate=line.substr(begin+1,end-begin-1);std::replace(candidate.begin(),candidate.end(),'\\','/');
                const auto full=engine_->resolve(pathUtf8(utf8Path(tab->path).parent_path()/utf8Path(candidate)));
                if(std::filesystem::is_regular_file(full)) {
                    editorCommand_="ui.openPath";editorCommandPath_=pathUtf8(full.lexically_relative(engine_->workspace()));return;
                }
            }catch(const std::exception &) { /* Definition lookup remains authoritative for other code. */ }
        }
        if (commandEnabled("navigate.definition")) executeCommand("navigate.definition");
        else cyui::notify(tr("Code navigation requires a configured language server."), cyui::Tone::warning);
    }
}
} // namespace cycode
