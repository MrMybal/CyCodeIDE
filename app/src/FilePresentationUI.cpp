#include "cycode/Application.h"
#include "cycode/FileIcons.h"
#include <cyui/Core.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <algorithm>

namespace cycode {
void Application::entryIcon(ImDrawList *draw,ImVec2 position,float size,const std::string &path,bool folder) const {
    if(!folder){drawFileIcon(draw,position,size,path,theme_);return;}
    const auto point=[&](float x,float y){return ImVec2(position.x+x*size,position.y+y*size);};
    const auto &p=cyui::palette();
    // A wide filled tabbed folder is distinct from a folded document outline.
    draw->AddRectFilled(point(.05f,.2f),point(.45f,.5f),cyui::mix(p.bg3,p.warning,.65f),cyui::dp(1));
    draw->AddRectFilled(point(.05f,.33f),point(.95f,.86f),cyui::mix(p.bg3,p.warning,.8f),cyui::dp(1));
    draw->AddLine(point(.12f,.43f),point(.88f,.43f),cyui::alpha(p.text,.3f),cyui::dp(.8f));
}

bool Application::resourceTreeNode(const std::string &name,const std::string &path,ImGuiTreeNodeFlags flags,const std::string &theme,bool folder) {
    if(!folder)return fileTreeNode(name,path,flags,theme,false);
    const bool open=ImGui::TreeNodeEx(("     "+name).c_str(),flags);
    auto pos=ImGui::GetItemRectMin();pos.x+=ImGui::GetTreeNodeToLabelSpacing();
    const float size=std::min(cyui::dp(16),ImGui::GetItemRectSize().y);
    pos.y+=(ImGui::GetItemRectSize().y-size)*.5f;
    entryIcon(ImGui::GetWindowDrawList(),pos,size,path,true);return open;
}

cyui::AssetResult Application::resourceBrowser(const char *id,const std::vector<cyui::AssetItem> &items,ImVec2 size) {
    ImGui::PushID(id);auto &state=resourceAssets_;cyui::AssetResult result;
    const float control=ImGui::GetFrameHeight();
    if(cyui::iconButton(cyui::icon::chevronUp,tr("Up")))for(const auto &item:items)if(item.id==state.folder){state.folder=item.parent;break;}
    ImGui::SameLine();ImGui::SetNextItemWidth(std::max(cyui::dp(60),ImGui::GetContentRegionAvail().x-control-ImGui::GetStyle().ItemSpacing.x));
    if(resourceSearchFocus_){ImGui::SetKeyboardFocusHere();resourceSearchFocus_=false;}
    result.queryChanged=ImGui::InputTextWithHint("##filter",tr("Filter files..."),&state.query);
    ImGui::SameLine();if(cyui::iconButton(state.grid?cyui::icon::list:cyui::icon::grid,tr(state.grid?"List":"Grid")))state.grid=!state.grid;
    size.y=std::max(cyui::dp(1),size.y-ImGui::GetFrameHeightWithSpacing());
    ImGui::BeginChild("##entries",size,ImGuiChildFlags_Borders);
    cyui::TextFilter filter;filter.query=state.query;
    std::vector<const cyui::AssetItem *> shown;
    for(const auto &item:items)if((!filter.active()?item.parent==state.folder:filter.matches(item.label,item.type)))shown.push_back(&item);
    const float gap=ImGui::GetStyle().ItemSpacing.x;
    const int columns=state.grid?std::max(1,int(ImGui::GetContentRegionAvail().x/cyui::dp(120))):1;
    const float cell=(ImGui::GetContentRegionAvail().x-gap*(columns-1))/columns;
    const float height=state.grid?cyui::dp(106):std::max(cyui::dp(22),ImGui::GetFrameHeight());
    ImGuiListClipper clip;clip.Begin(int((shown.size()+columns-1)/columns),height+ImGui::GetStyle().ItemSpacing.y);
    while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row)for(int col=0;col<columns;++col) {
        const auto index=size_t(row)*columns+col;if(index>=shown.size())break;
        const auto &item=*shown[index];ImGui::PushID(static_cast<int>(item.id>>32));ImGui::PushID(static_cast<int>(item.id));
        if(col)ImGui::SameLine(0,gap);
        const auto pos=ImGui::GetCursorScreenPos();
        const bool selected=std::find(state.selection.begin(),state.selection.end(),item.id)!=state.selection.end();
        const bool clicked=ImGui::Selectable("##entry",selected,ImGuiSelectableFlags_AllowDoubleClick,{cell,height});
        const auto path=resourceAssetPaths_.at(item.id);auto draw=ImGui::GetWindowDrawList();
        draw->PushClipRect(pos,{pos.x+cell,pos.y+height},true);
        const float iconSize=state.grid?cyui::dp(44):cyui::dp(16);
        const ImVec2 iconPos=state.grid?ImVec2(pos.x+(cell-iconSize)*.5f,pos.y+cyui::dp(10)):ImVec2(pos.x+cyui::dp(4),pos.y+(height-iconSize)*.5f);
        entryIcon(draw,iconPos,iconSize,path,item.folder);
        const ImVec2 textPos=state.grid?ImVec2(pos.x+cyui::dp(6),pos.y+cyui::dp(67)):ImVec2(iconPos.x+iconSize+cyui::dp(8),pos.y+(height-ImGui::GetTextLineHeight())*.5f);
        cyui::clippedText(draw,textPos,pos.x+cell-textPos.x-cyui::dp(6),cyui::palette().text,item.label.c_str());
        draw->PopClipRect();
        if(clicked) {
            cyui::selectItem(state.selection,item.id,ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift);
            if(!item.folder || ImGui::IsMouseDoubleClicked(0)){result.id=item.id;result.action=cyui::AssetAction::open;}
        }
        if(ImGui::BeginDragDropSource()) {ImGui::SetDragDropPayload("CYUI_ASSET",&item.id,sizeof(item.id));ImGui::TextUnformatted(item.label.c_str());ImGui::EndDragDropSource();}
        if(ImGui::BeginPopupContextItem()) {
            for(const auto &[title,action]:std::vector<std::pair<const char *,cyui::AssetAction>>{{"Open",cyui::AssetAction::open},{item.favorite?"Remove favorite":"Favorite",cyui::AssetAction::favorite},{"Rename",cyui::AssetAction::rename},{"Remove",cyui::AssetAction::remove}})
                if(ImGui::MenuItem(tr(title))){result.id=item.id;result.action=action;}
            ImGui::EndPopup();
        }
        cyui::tooltip(path.c_str());
        ImGui::PopID();ImGui::PopID();
    }
    if(shown.empty())cyui::textDim("%s",tr("No matching files"));
    ImGui::EndChild();ImGui::PopID();return result;
}
} // namespace cycode
