#include "cycode/Application.h"
#include "cycode/ImageView.h"
#include "cycode/UiFonts.h"
#include <cyui/Core.h>
#include <imgui_internal.h>
#include <algorithm>
#include <functional>
#include <fstream>
#include <cmath>

namespace cycode {
namespace {
class CodeFontScope {
    float saved_=ImGui::GetStyle().FontScaleMain;
public:
    explicit CodeFontScope(float size) {
        ImGui::GetStyle().FontScaleMain=size/ImGui::GetStyle().FontSizeBase;
        ImGui::PushFont(cyui::monoFont(),ImGui::GetStyle().FontSizeBase);
    }
    ~CodeFontScope(){ImGui::GetStyle().FontScaleMain=saved_;ImGui::PopFont();}
};
const char *menuGroup(const std::string &category, const std::string &id) {
    if (category == "File") {
        if (id.starts_with("file.new")) return "New";
        if (id.starts_with("file.save") || id == "file.checkpointPaged") return "Save";
        if (id.starts_with("file.close") || id == "file.reopen" || id == "file.keepOpen") return "Tabs";
        if (id == "file.exit") return "Application";
        return "Open";
    }
    if (category == "Edit") {
        if (id == "edit.undo" || id == "edit.redo") return "History";
        if (id == "edit.cut" || id == "edit.copy" || id == "edit.paste") return "Clipboard";
        if (id.find("Snippet") != std::string::npos || id.find("snippet") != std::string::npos) return "Snippets";
        if (id == "edit.selectAll" || id == "edit.grow" || id == "edit.shrink" || id.ends_with("Occurrence") || id.ends_with("Occurrences")) return "Selection";
        return "Lines and formatting";
    }
    if (category == "Navigate") {
        if (id.starts_with("problem.")) return "Diagnostics";
        if (id == "navigate.counterpart" || id == "navigate.include" || id == "navigate.relatedFiles") return "Related files";
        if (id.find("ccurrence") != std::string::npos) return "Occurrences";
        if (id == "code.documentation" || id == "code.parameters") return "Code information";
        if (id == "navigate.back" || id == "navigate.forward" || id == "navigate.recent" || id.ends_with("Tab")) return "Navigation history";
        return "Symbols";
    }
    if (category == "Search") return id == "search.document" || id == "search.replace" || id == "search.next" || id == "search.previous" ? "Current file" : "Project search";
    if (category == "View") {
        if (id == "view.toolWindows" || id == "view.backgroundTasks") return "Workspace";
        if (id == "view.options" || id == "view.keymap" || id == "view.reset") return "Settings and layout";
        if (id == "view.wrap" || id == "view.lineNumbers" || id == "view.whitespace" || id == "view.minimap" || id.starts_with("view.fold") || id.starts_with("view.unfold")) return "Editor display";
        return "Tool windows";
    }
    if (category == "Build") return id.starts_with("tests.") ? "Tests" : id.starts_with("unreal.") ? "Unreal Engine" : id.starts_with("msbuild.") ? "MSBuild" : id.starts_with("cmake.") ? "CMake" : "Build tasks";
    if (category == "Run") {
        if(id.find("breakpoint")!=std::string::npos || id.find("Breakpoint")!=std::string::npos)return "Breakpoints";
        if(id=="debug.next" || id=="debug.stepIn" || id=="debug.stepOut" || id=="debug.stepBack" || id=="debug.reverseContinue" || id=="debug.restartFrame" || id=="debug.stepInTargets")return "Stepping";
        return id=="run.start" || id=="debug.start" || id=="debug.pause" || id=="debug.stop"?"Execution":"Run configurations";
    }
    if (category == "Refactor") return id.find("format")!=std::string::npos?"Formatting":id.find("ndo")!=std::string::npos?"History":"Code changes";
    if (category == "Project") return id.starts_with("project.")?"Project files":"Project tools";
    if (category == "Version control") return id=="vcs.diff" || id=="vcs.blame" || id=="vcs.hunks"?"Current file":id=="vcs.refresh" || id=="vcs.changes"?"Changes":"Repository";
    if (category == "Help") return id=="help.context" || id=="help.documentation" || id=="help.tour"?"Documentation":"Reference guides";
    return "";
}
bool nestedMenu(const std::string &category,const std::string &group,size_t count) {
    if(count<4)return false;
    return (category=="View" && group=="Tool windows") ||
        (category=="Build" && (group=="CMake" || group=="MSBuild" || group=="Tests")) ||
        (category=="Run" && (group=="Breakpoints" || group=="Stepping")) ||
        (category=="Edit" && group=="Snippets") ||
        (category=="Navigate" && (group=="Occurrences" || group=="Diagnostics"));
}
const char *commandIcon(const std::string &id) {
    static const std::map<std::string,const char *> icons={
        {"file.new",cyui::icon::add},{"file.open",cyui::icon::folder},{"file.save",cyui::icon::save},{"file.exit",cyui::icon::power},
        {"edit.undo",cyui::icon::undo},{"edit.redo",cyui::icon::redo},{"edit.cut",cyui::icon::cut},{"edit.copy",cyui::icon::copy},{"edit.paste",cyui::icon::paste},
        {"search.document",cyui::icon::search},{"search.workspace",cyui::icon::search},{"navigate.definition",cyui::icon::forward},{"navigate.references",cyui::icon::link},
        {"refactor.rename",cyui::icon::rename},{"code.actions",cyui::icon::lightbulb},{"build.run",cyui::icon::repair},{"build.cancel",cyui::icon::stop},
        {"run.start",cyui::icon::play},{"debug.start",cyui::icon::bug},{"debug.stop",cyui::icon::stop},{"debug.breakpoint",cyui::icon::record},
        {"view.options",cyui::icon::settings},{"help.documentation",cyui::icon::help}};
    const auto at=icons.find(id);return at==icons.end()?nullptr:at->second;
}
bool sidePanel(const std::string &id) {
    return id == "project" || id == "resources" || id == "search";
}
std::string menuTitle(std::string title) {
    if (ImGui::CalcTextSize(title.c_str()).x <= cyui::dp(70)) return title;
    do {
        auto end=title.size()-1;
        while(end && (static_cast<unsigned char>(title[end])&0xc0)==0x80) --end;
        title.resize(end);
    } while(!title.empty() && ImGui::CalcTextSize((title+"...").c_str()).x > cyui::dp(70));
    return title+"...";
}
bool beginToolRail(const char *id, ImGuiDir direction) {
    cyui::ScopedColor colour(ImGuiCol_WindowBg,cyui::palette().bg0);
    cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding,cyui::dp(5,8));
    padding.push(ImGuiStyleVar_WindowBorderSize,0);
    const auto shown=ImGui::BeginViewportSideBar(id,ImGui::GetMainViewport(),direction,cyui::dp(40),
        ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoSavedSettings);
    auto window=ImGui::GetCurrentWindow();
    const float x=direction==ImGuiDir_Left?window->Pos.x+window->Size.x:window->Pos.x;
    window->DrawList->AddLine({x,window->Pos.y},{x,window->Pos.y+window->Size.y},cyui::palette().lineStrong,cyui::dp(1));
    return shown;
}
}

void Application::configureTitleBar() {
#ifdef _WIN32
    if(!window_)return;
    titleDragEnabled_=false;
    if(integratedTitleBar_ && SDL_SetWindowHitTest(window_,windowHitTest,this)) {
        titleBarActive_=SDL_SetWindowBordered(window_,false);
        if(!titleBarActive_)SDL_SetWindowHitTest(window_,nullptr,nullptr);
    } else {
        SDL_SetWindowHitTest(window_,nullptr,nullptr);
        SDL_SetWindowBordered(window_,true);
        titleBarActive_=false;
    }
#endif
}

SDL_HitTestResult SDLCALL Application::windowHitTest(SDL_Window *window,const SDL_Point *point,void *data) {
    const auto self=static_cast<Application *>(data);
    const auto flags=SDL_GetWindowFlags(window);
    if(flags&SDL_WINDOW_FULLSCREEN)return SDL_HITTEST_NORMAL;
    if(!(flags&SDL_WINDOW_MAXIMIZED)) {
        int width=0,height=0;SDL_GetWindowSize(window,&width,&height);
        const int edge=std::max(4,int(5*SDL_GetWindowDisplayScale(window)));
        const bool left=point->x<edge,right=point->x>=width-edge,top=point->y<edge,bottom=point->y>=height-edge;
        if(top)return left?SDL_HITTEST_RESIZE_TOPLEFT:right?SDL_HITTEST_RESIZE_TOPRIGHT:SDL_HITTEST_RESIZE_TOP;
        if(bottom)return left?SDL_HITTEST_RESIZE_BOTTOMLEFT:right?SDL_HITTEST_RESIZE_BOTTOMRIGHT:SDL_HITTEST_RESIZE_BOTTOM;
        if(left)return SDL_HITTEST_RESIZE_LEFT;
        if(right)return SDL_HITTEST_RESIZE_RIGHT;
    }
    return self->titleDragEnabled_ && SDL_PointInRect(point,&self->titleDrag_)?SDL_HITTEST_DRAGGABLE:SDL_HITTEST_NORMAL;
}

void Application::dockToolPanel(const char *text,const char *id,ImGuiDir direction) {
    if(!workspaceDock_ || direction==ImGuiDir_Left){showPanel(text,id);return;}
    auto &destination=direction==ImGuiDir_Right?rightDock_:bottomDock_;
    if(!ImGui::DockBuilderGetNode(destination)) {
        destination=0;
        const auto host=ImGui::FindWindowByName("##editors");
        // Reuse the user's existing side area when restoring an older layout.
        if(host)for(const auto &[panel,visible]:panels_) {
            if(panel==id || !visible)continue;
            for(auto window:ImGui::GetCurrentContext()->Windows)if(window->DockNode &&
                std::string(window->Name).ends_with("###"+panel) && window->DockNode->HostWindow &&
                ImGui::DockNodeGetRootNode(window->DockNode)->ID==workspaceDock_) {
                const bool onSide=direction==ImGuiDir_Right?window->Pos.x>=host->Pos.x+host->Size.x-cyui::dp(8):window->Pos.y>=host->Pos.y+host->Size.y-cyui::dp(8);
                if(onSide){destination=window->DockId;break;}
            }
            if(destination)break;
        }
        if(!destination)if(auto center=ImGui::DockBuilderGetCentralNode(workspaceDock_))
            destination=ImGui::DockBuilderSplitNode(center->ID,direction,.29f,nullptr,nullptr);
    }
    if(destination) {
        ImGui::DockBuilderDockWindow(label(text,id).c_str(),destination);
        ImGui::DockBuilderFinish(workspaceDock_);
    }
    showPanel(text,id);
}

void Application::interfaceStyle() {
    auto &style = ImGui::GetStyle();
    const auto &p = cyui::palette();
    style.PopupBorderSize = cyui::dp(1);
    style.WindowBorderSize = cyui::dp(1);
    style.PopupRounding = cyui::dp(5);
    style.IndentSpacing = cyui::dp(22);
    style.Colors[ImGuiCol_PopupBg] = cyui::vec(cyui::mix(p.bg2, p.bg3, .5f));
    style.Colors[ImGuiCol_Border] = cyui::vec(cyui::mix(p.lineStrong, p.textDim, .4f));
    style.Colors[ImGuiCol_Separator] = cyui::vec(cyui::mix(p.line, p.textDim, .32f));
}

void Application::popupElevation() {
    const auto &p = cyui::palette();
    for (auto window : ImGui::GetCurrentContext()->Windows) {
        if (!window->Active || window->Hidden || window->DockNode || window->DockIsActive || window->IsFallbackWindow ||
            (window->Flags & ImGuiWindowFlags_ChildWindow) ||
            (!(window->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) && (window->Flags & ImGuiWindowFlags_NoTitleBar))) continue;
        const auto viewport = window->Viewport;
        auto draw = window->DrawList;
        draw->PushClipRect(viewport->Pos, {viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y}, false);
        // Only outline rings are drawn: no overlay covers the window's contents.
        for (int ring = 8; ring > 0; --ring) {
            const float pad = cyui::dp(static_cast<float>(ring));
            draw->AddRect({window->Pos.x-pad,window->Pos.y-pad},
                          {window->Pos.x+window->Size.x+pad,window->Pos.y+window->Size.y+pad},
                          cyui::alpha(p.bg0,.22f*(9-ring)/8.f),cyui::dp(5.f+ring),0,cyui::dp(1.5f));
        }
        draw->AddRect(window->Pos, {window->Pos.x+window->Size.x,window->Pos.y+window->Size.y},
                      cyui::mix(p.lineStrong,p.textDim,.5f),cyui::dp(5),0,cyui::dp(1));
        draw->PopClipRect();
    }
}

uint64_t Application::commandMenu(const char *id,cyui::TextFilter &filter,const std::vector<cyui::FilterMenuItem> &rows) {
    ImGui::PushID(id);
    cyui::FilterOptions options;options.hint="Filter menu...";options.width=cyui::dp(350);
    options.focusOnAppearing=true;options.resetOnAppearing=true;
    cyui::filterBox("##filter",filter,options);ImGui::Separator();
    std::map<uint64_t,const char *> icons;
    std::function<void(const std::vector<cyui::FilterMenuItem> &)> collect=[&](const auto &entries) {
        for(const auto &entry:entries){if(entry.id)icons[entry.id]=commandIcon(entry.keywords);collect(entry.children);}
    };
    collect(rows);
    uint64_t request=0;
    auto action=[&](uint64_t key,const std::string &title,const std::string &shortcut,bool checked,bool enabled) {
        ImGui::PushID(static_cast<int>(key>>32));ImGui::PushID(static_cast<int>(key));
        const auto icon=icons[key];
        // ImGui's native icon column aligns commands, shortcuts and check marks.
        if(ImGui::MenuItemEx((title+"###action").c_str(),icon && applicationIconAvailable(icon)?icon:nullptr,shortcut.c_str(),checked,enabled))request=key;
        ImGui::PopID();ImGui::PopID();
    };
    std::function<void(const std::vector<cyui::FilterMenuItem> &)> draw=[&](const auto &entries) {
        for(size_t i=0;i<entries.size();++i) {
            const auto &entry=entries[i];ImGui::PushID(static_cast<int>(i));
            if(!entry.children.empty()) {
                if(ImGui::BeginMenu(tr(entry.label.c_str()),entry.enabled)){draw(entry.children);ImGui::EndMenu();}
            } else if(entry.id)action(entry.id,tr(entry.label.c_str()),entry.shortcut,entry.checked,entry.enabled);
            else if(entry.label.empty())ImGui::Separator();
            ImGui::PopID();
        }
    };
    if(!filter.active())draw(rows);
    else {
        const auto matches=cyui::filterMenuMatches(rows,filter);
        if(matches.empty())cyui::textDim("%s",tr("No matching items"));
        std::string previous;
        for(const auto &match:matches) {
            if(match.breadcrumb!=previous){previous=match.breadcrumb;if(!previous.empty())cyui::sectionTitle(previous.c_str());}
            action(match.id,match.label,match.shortcut,match.checked,match.enabled);
        }
    }
    ImGui::PopID();return request;
}

bool Application::drawCode(cyui::CodeEditor &editor,const char *id) {
    CodeFontScope font(codeFontSize_);
    editor.setTextScale(codeZoom_);
    editor.native().SetLineSpacing(codeLineSpacing_);
    const bool changed=editor.draw(id);
    if(editor.textScale()!=codeZoom_)setCodeZoom(editor.textScale());
    return changed;
}

cyui::CodeViewResult Application::drawCodeView(const char *id,cyui::CodeViewState &state,const cyui::CodeDocument &document,ImVec2 size) {
    CodeFontScope font(codeFontSize_);state.textScale=codeZoom_;
    const auto result=cyui::codeViewer(id,state,document,size);
    if(state.textScale!=codeZoom_)setCodeZoom(state.textScale);
    return result;
}

cyui::DiffResult Application::drawDiff(const char *id,cyui::DiffState &state,const cyui::DiffDocument &document,ImVec2 size) {
    CodeFontScope font(codeFontSize_);state.textScale=codeZoom_;
    const auto result=cyui::diffViewer(id,state,document,size);
    if(state.textScale!=codeZoom_)setCodeZoom(state.textScale);
    return result;
}

std::vector<cyui::PagedCodeCommand> Application::drawPaged(PagedTextEditor &editor,const char *id) {
    CodeFontScope font(codeFontSize_);
    auto options=editor.view().options();options.textScale=codeZoom_;editor.view().setOptions(options);
    auto result=editor.draw(id);
    if(editor.view().options().textScale!=codeZoom_)setCodeZoom(editor.view().options().textScale);
    return result;
}

void Application::setCodeZoom(float value,bool persist) {
    if(!std::isfinite(value))return;
    codeZoom_=std::clamp(value,.5f,3.f);
    for(const auto &tab:tabs_) {
        if(tab->image)continue;
        tab->codeEditor.setTextScale(codeZoom_);
        if(tab->paged){auto options=tab->paged->view().options();options.textScale=codeZoom_;tab->paged->view().setOptions(options);}
    }
    for(auto editor:{&searchPreview_,&vcsPreviewCode_,&beforePreviewCode_,&afterPreviewCode_})editor->setTextScale(codeZoom_);
    if(persist){codePreferencesDirty_=true;codePreferencesChanged_=ImGui::GetTime();}
}

void Application::setMinimap(bool value,bool persist) {
    minimap_=value;
    for(const auto &tab:tabs_)if(!tab->image && !tab->pagedProfile)tab->editor.SetShowMiniMapEnabled(value);
    if(persist){codePreferencesDirty_=true;codePreferencesChanged_=ImGui::GetTime();}
}

void Application::updateCodePreferences() {
    const auto now=ImGui::GetTime();
    const auto path=root_/"data/settings/ui-preferences.json";
    std::error_code errorCode;
    if(codePreferencesDirty_) {
        if(now-codePreferencesChanged_<.25)return;
        try{saveSettings();codePreferencesStamp_=std::filesystem::last_write_time(path,errorCode);}
        catch(const std::exception &e){error(e.what());}
        codePreferencesDirty_=false;return;
    }
    if(now-lastCodePreferencesPoll_<1.)return;
    lastCodePreferencesPoll_=now;
    const auto stamp=std::filesystem::last_write_time(path,errorCode);
    if(errorCode || stamp==codePreferencesStamp_)return;
    codePreferencesStamp_=stamp;
    if(std::filesystem::file_size(path,errorCode)>65536 || errorCode)return;
    try {
        std::ifstream input(path);const auto values=Json::parse(input);
        codeFontSize_=std::clamp(values.value("codeFontSize",codeFontSize_),8.f,32.f);
        codeLineSpacing_=std::clamp(values.value("codeLineSpacing",codeLineSpacing_),1.f,2.f);
        setCodeZoom(values.value("codeZoom",codeZoom_),false);
        setMinimap(values.value("minimap",minimap_),false);
    }catch(const std::exception &){/* A complete atomic preference publication will be read on its next timestamp. */}
}

void Application::menu() {
    titleDragEnabled_=false;
    const auto normalPadding=ImGui::GetStyle().FramePadding;
    if(titleBarActive_)ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{normalPadding.x,cyui::dp(6)});
    const bool shown=ImGui::BeginMainMenuBar();
    if (!shown) {if(titleBarActive_)ImGui::PopStyleVar();return;}
    if (brandingLogo_) {
        const float size = ImGui::GetTextLineHeight();
        ImGui::Image(brandingLogo_->data.texture, {size*brandingLogo_->data.width/brandingLogo_->data.height,size});
    } else ImGui::TextUnformatted("CyCodeIDE");
    ImGui::SameLine();
    cyui::badge(CYCODE_IDE_VERSION, cyui::Tone::neutral);
    const auto catalogue = commandCatalogue();
    const auto bar=ImGui::GetCurrentWindow();
    const float controls=cyui::dp(titleBarActive_?228.f:98.f);
    float menusWidth=0;
    for(const auto *name:{"File","Edit","Search","Navigate","Refactor","Build","Run","View","Project","Version control","Extensions","Help"})
        menusWidth+=ImGui::CalcTextSize(menuTitle(tr(name)).c_str()).x+ImGui::GetStyle().ItemSpacing.x*2;
    const bool compact=bar->Size.x-ImGui::GetCursorPosX()<menusWidth+controls+cyui::dp(60);
    const bool expand=!compact || ImGui::BeginMenu(tr("Menu"));
    if(expand)
    for (const auto *category : {"File", "Edit", "Search", "Navigate", "Refactor", "Build", "Run", "View", "Project", "Version control", "Extensions", "Help"}) {
        const auto caption=(compact?std::string(tr(category)):menuTitle(tr(category)))+"###menu-"+category;
        const bool open=ImGui::BeginMenu(caption.c_str());
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))ImGui::SetTooltip("%s",tr(category));
        if (!open) continue;
        cyui::ScopedVar popupPadding(ImGuiStyleVar_FramePadding,normalPadding);
        std::vector<cyui::FilterMenuItem> items;
        auto add = [&](const Command &command, const std::string &title) {
            cyui::FilterMenuItem row{uiId("command/"+command.id),title,keymap_.hint(command.id),command.id,commandEnabled(command.id),false,{}};
            if (command.id == "view.minimap") row.checked = minimap_;
            if (command.id.starts_with("view.")) {
                const auto panel = panels_.find(command.id.substr(5));
                if (panel != panels_.end()) row.checked = panel->second;
            }
            const std::string groupName = menuGroup(category, command.id);
            if (groupName.empty()) items.push_back(std::move(row));
            else {
                auto group = std::find_if(items.begin(),items.end(),[&](const auto &v){return v.label==groupName && !v.id;});
                if (group==items.end()) { if(!items.empty())items.push_back({});items.push_back({0,groupName});group=std::prev(items.end()); }
                group->children.push_back(std::move(row));
            }
        };
        const std::map<std::string,std::string> locations={{"File","file"},{"Edit","edit"},{"View","view"},{"Navigate","navigate"},{"Build","build"},{"Run","run"},{"Project","project"},{"Version control","vcs"},{"Extensions","extensions"}};
        for (const auto &command : catalogue) if (command.category==category &&
            !(std::string(category)=="Version control" && provider_!="git" && command.id.starts_with("vcs.") && command.id!="vcs.changes" && command.id!="vcs.refresh" && command.id!="vcs.diff" && command.id!="vcs.blame")) {
            std::string title = command.title;
            if (locations.contains(category)) for (const auto &entry : pluginInfo_.value("menus",Json::array()))
                if (entry.at("location")==locations.at(category) && entry.at("command")==command.id && extensionVisible(entry)) title=entry.value("title",title);
            add(command,title);
        }
        std::vector<cyui::FilterMenuItem> grouped;
        for(auto &item:items) {
            if(item.children.empty())continue; // Group boundaries below provide the separators.
            if(!grouped.empty())grouped.push_back({});
            if(nestedMenu(category,item.label,item.children.size()))grouped.push_back(std::move(item));
            else for(auto &child:item.children)grouped.push_back(std::move(child));
        }
        // Categories without grouping (notably extension contributions) stay direct.
        bool raw=false;
        for(auto &item:items)if(item.id){if(!raw && !grouped.empty())grouped.push_back({});grouped.push_back(std::move(item));raw=true;}
        const auto request = commandMenu(category,menuFilters_[category],grouped);
        if (request) for (const auto &command : catalogue) if (uiId("command/"+command.id)==request) { executeCommand(command.id);break; }
        ImGui::EndMenu();
    }
    if(compact && expand)ImGui::EndMenu();
    const float dragStart=ImGui::GetCursorPosX()+cyui::dp(5);
    const float buttonsStart=std::max(dragStart,bar->Size.x-controls);
    if(titleBarActive_) {
        const auto viewport=ImGui::GetMainViewport();
        titleDrag_={int(bar->Pos.x-viewport->Pos.x+dragStart),int(bar->Pos.y-viewport->Pos.y),
            std::max(0,int(buttonsStart-dragStart-cyui::dp(4))),int(bar->Size.y)};
        titleDragEnabled_=titleDrag_.w>0 && !ImGui::IsAnyItemActive() && ImGui::GetCurrentContext()->OpenPopupStack.empty();
    }
    float buttonX=buttonsStart;
    const float buttonHeight=std::max(ImGui::GetTextLineHeight(),bar->Size.y-cyui::dp(4));
    auto place=[&](float width) {
        ImGui::SetCursorPos({buttonX,(bar->Size.y-buttonHeight)*.5f});
        buttonX+=cyui::dp(width+2);
    };
    auto action=[&](const char *icon,const char *id,const char *hint) {
        place(28);
        ImGui::PushID(id);
        cyui::ScopedDisabled disabled(!commandEnabled(id));
        const auto tip=std::string(tr(hint))+" ("+keymap_.hint(id)+")";
        if(cyui::iconButton(icon,tip.c_str(),false,0,{cyui::dp(28),buttonHeight}))executeCommand(id);
        ImGui::PopID();
    };
    action(cyui::icon::play,"run.start","Run without debugging");
    action(cyui::icon::bug,"debug.start","Debug / Continue");
    action(cyui::icon::stop,"debug.stop","Stop debugging");
    if(titleBarActive_) {
        buttonX+=cyui::dp(8);place(38);
        if(cyui::iconButton(cyui::icon::remove,tr("Minimize"),false,0,{cyui::dp(38),buttonHeight}))SDL_MinimizeWindow(window_);
        const bool maximized=(SDL_GetWindowFlags(window_)&SDL_WINDOW_MAXIMIZED)!=0;
        place(38);
        if(cyui::iconButton(maximized?cyui::icon::backToWindow:cyui::icon::newWindow,tr(maximized?"Restore window":"Maximize"),false,0,{cyui::dp(38),buttonHeight})) {
            if(maximized)SDL_RestoreWindow(window_);else SDL_MaximizeWindow(window_);
        }
        place(38);
        if(cyui::iconButton(cyui::icon::close,tr("Close window"),false,0,{cyui::dp(38),buttonHeight}))requestQuit();
    }
    ImGui::EndMainMenuBar();
    if(titleBarActive_)ImGui::PopStyleVar();
}

void Application::toggleToolWindows() {
    const bool any = std::any_of(panels_.begin(),panels_.end(),[](const auto &p){return !sidePanel(p.first) && p.second;});
    if (any) {
        hiddenToolPanels_.clear();
        for (auto &[id,visible] : panels_) if (!sidePanel(id) && visible) { hiddenToolPanels_.insert(id);visible=false; }
        if (!active_.empty()) focusTab_=active_;
    } else if (hiddenToolPanels_.empty()) showPanel("Build","build");
    else { for(const auto &id:hiddenToolPanels_)panels_[id]=true;hiddenToolPanels_.clear(); }
}

std::vector<std::string> Application::backgroundTasks() const {
    std::vector<std::string> result;
    if (filesFuture_.valid() || filesIndexing_) result.push_back("Indexing files...");
    if (unrealAnalysisFuture_.valid()) result.push_back("Preparing Unreal code analysis...");
    if (searchFuture_.valid()) result.push_back("Searching workspace...");
    if (searchPreviewFuture_.valid() && searchPreviewFuture_.wait_for(std::chrono::seconds(0))!=std::future_status::ready) result.push_back("Loading search preview...");
    if (taskFuture_.valid()) result.push_back(taskMethod_);
    if (analysisView_.prepare.valid()) result.push_back("Project analysis");
    if (analysisView_.inspection.valid()) result.push_back("C++ inspections");
    if (buildFuture_.valid() || detectionFuture_.valid()) result.push_back("Detecting project tools...");
    if (debugFuture_.valid()) result.push_back("Debugger");
    if (updateDownloadFuture_.valid()) result.push_back("Downloading update...");
    const auto servers=languageState_.value("servers",Json::object());
    for (const auto &[key,server] : servers.items())
        if (server.value("running",false) && !server.value("ready",false)) result.push_back(server.value("name",key));
    return result;
}

void Application::statusBar() {
    cyui::beginStatusBar("##status");
    cyui::textDim("%s",git_.value("repository",false)?git_.value("branch",std::string{}).c_str():tr("No repository"));
    for (const auto &tab : tabs_) if (tab->path==active_) {
        ImGui::SameLine();cyui::textFaint(" | ");ImGui::SameLine();
        if(tab->image)cyui::textDim("%s",tr("Image preview"));
        else if(tab->pagedProfile) {
            const auto pos=tab->paged?tab->paged->view().selection().caret:cyui::PagedCodePosition{};
            cyui::textDim("%s | %llu:%llu",tr("Paged UTF-8"),static_cast<unsigned long long>(pos.line+1),static_cast<unsigned long long>(pos.character+1));
        } else {const auto pos=tab->editor.GetMainCursorPosition();cyui::textDim("%s | Ln %zu, Col %zu | UTF-8",tab->editor.GetLanguageName().c_str(),pos.line+1,pos.index+1);}
    }
    if (auto pending=keymap_.pending(ImGui::GetTime());!pending.empty()){ImGui::SameLine();cyui::textDim("%s",pending.c_str());}
    const auto tasks=backgroundTasks();
    ImGui::SameLine();cyui::alignRight(cyui::dp(230));
    const auto title=tasks.empty()?std::string(tr("Ready")):std::to_string(tasks.size())+" "+tr("background tasks");
    if(cyui::ghostButton((title+"###background-tasks").c_str()))ImGui::OpenPopup("##background-tasks");
    if(ImGui::BeginPopup("##background-tasks")) {
        cyui::heading(tr("Background tasks"),3);
        if(tasks.empty())cyui::textDim("%s",tr("No background tasks"));
        for(const auto &task:tasks) {
            ImGui::TextUnformatted(tr(task.c_str()));
            const bool measured=task=="Project analysis" && analysisView_.status.value("total",uint64_t(0))>0;
            cyui::progressBar(measured?std::clamp(analysisView_.status.value("progress",0.f)/100.f,0.f,1.f):-1.f,cyui::dp(290,5));
            cyui::space(5);
        }
        if(taskFuture_.valid() && commandEnabled("build.cancel") && cyui::button(tr("Cancel build")))executeCommand("build.cancel");
        if(searchFuture_.valid() && cyui::button(tr("Cancel search"))) {if(searchCancellation_)*searchCancellation_=true;++searchGeneration_;searchQueued_=false;}
        if(unrealAnalysisFuture_.valid() && cyui::button(tr("Cancel Unreal analysis")) && unrealAnalysisCancellation_)*unrealAnalysisCancellation_=true;
        if(analysisView_.prepare.valid() && cyui::button(tr("Cancel project analysis")))engine_->dispatch("analysis.cancel",{{"operationId",analysisView_.operation}});
        if(analysisView_.inspection.valid() && cyui::button(tr("Cancel inspection")))engine_->dispatch("build.cancel",{{"operationId",analysisView_.inspectionOperation}});
        ImGui::EndPopup();
    }
    if (backgroundTasksOpen_) { ImGui::OpenPopup("##background-tasks");backgroundTasksOpen_=false; }
    cyui::endStatusBar();
}

void Application::rail() {
    auto button=[&](const char *icon,const char *text,const char *id,ImGuiDir direction) {
        const auto window=ImGui::FindWindowByName(label(text,id).c_str());
        const bool visible=panels_[id] && window && window->WasActive && !window->Hidden &&
                           (!window->DockNode || window->DockNode->VisibleWindow==window);
        ImGui::PushID(id);
        if(cyui::iconButton(icon,tr(text),visible,0,cyui::dp(30,32))) {
            if(visible)panels_[id]=false;
            else {if(std::string_view(id)=="gitCommits")vcsSection_="History";dockToolPanel(text,id,direction);}
        }
        ImGui::PopID();cyui::space(3);
    };
    if(beginToolRail("##rail",ImGuiDir_Left)) {
        // The workspace search shortcut opens its full results/preview window.
        button(cyui::icon::search,"Find in workspace...","search",ImGuiDir_Left);
        const bool tall=ImGui::GetWindowHeight()>cyui::dp(500);
        const auto spacing=ImGui::GetStyle().ItemSpacing.y;
        const float toolsHeight=tall?cyui::dp(5*32+4*3+2*5+8)+spacing*13:cyui::dp(2*32+2*5+8)+spacing*5;
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY()+cyui::dp(10),ImGui::GetWindowHeight()-toolsHeight));
        ImGui::Separator();cyui::space(5);
        if(tall) {
            button(cyui::icon::repair,"Build","build",ImGuiDir_Down);
            button(cyui::icon::play,"Debugger","debugger",ImGuiDir_Down);
            button(cyui::icon::code,"Terminal","terminal",ImGuiDir_Down);
            button(cyui::icon::check,"Git commits","gitCommits",ImGuiDir_Down);
        } else {
            if(cyui::iconButton(cyui::icon::dockBottom,tr("Bottom tool windows"),false,0,cyui::dp(30,32)))ImGui::OpenPopup("##bottom-tools");
            if(ImGui::BeginPopup("##bottom-tools")) {
                for(const auto &[text,id]:std::vector<std::pair<const char *,const char *>>{{"Build","build"},{"Debugger","debugger"},{"Terminal","terminal"},{"Git commits","gitCommits"}})
                    if(ImGui::MenuItem(tr(text)))dockToolPanel(text,id,ImGuiDir_Down);
                ImGui::EndPopup();
            }
        }
        ImGui::Separator();cyui::space(5);
        if(cyui::iconButton(cyui::icon::chevronDown,tr("Show or hide tool windows (Ctrl+Shift+F12)"),false,0,cyui::dp(30,32)))toggleToolWindows();
    }
    ImGui::End();
    if(beginToolRail("##right-rail",ImGuiDir_Right)) {
        button(cyui::icon::folder,"Project","project",ImGuiDir_Right);
        button(cyui::icon::document,"Files","resources",ImGuiDir_Right);
        button(cyui::icon::sync,"Version control","vcs",ImGuiDir_Right);
        if(ImGui::GetWindowHeight()>cyui::dp(700)) {
        button(cyui::icon::list,"Structure","structure",ImGuiDir_Right);
        button(cyui::icon::link,"References","references",ImGuiDir_Right);
        button(cyui::icon::info,"Code information","information",ImGuiDir_Right);
        button(cyui::icon::lightbulb,"Code actions","codeActions",ImGuiDir_Right);
        button(cyui::icon::viewAll,"Analysis and inspections","analysis",ImGuiDir_Right);
        button(cyui::icon::warning,"Problems","problems",ImGuiDir_Right);
        button(cyui::icon::check,"Tests","tests",ImGuiDir_Right);
        } else {
            if(cyui::iconButton(cyui::icon::more,tr("More tool windows"),false,0,cyui::dp(30,32)))ImGui::OpenPopup("##right-tools");
            if(ImGui::BeginPopup("##right-tools")) {
                for(const auto &[text,id]:std::vector<std::pair<const char *,const char *>>{{"Structure","structure"},{"References","references"},{"Code information","information"},{"Code actions","codeActions"},{"Analysis and inspections","analysis"},{"Problems","problems"},{"Tests","tests"},{"Extensions","extensions"},{"MCP","mcp"}})
                    if(ImGui::MenuItem(tr(text)))dockToolPanel(text,id,ImGuiDir_Right);
                ImGui::EndPopup();
            }
        }
        if(ImGui::GetWindowHeight()>cyui::dp(350)) {
            ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(),ImGui::GetWindowHeight()-cyui::dp(2*35+18)-ImGui::GetStyle().ItemSpacing.y*5));
            ImGui::Separator();cyui::space(5);
        button(cyui::icon::library,"Extensions","extensions",ImGuiDir_Right);
        button(cyui::icon::connect,"MCP","mcp",ImGuiDir_Right);
        }
    }
    ImGui::End();
}
} // namespace cycode
