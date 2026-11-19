#include "cycode/Application.h"
#include <cymcp/core.hpp>

namespace cycode {
Json Application::automationUi(const std::string& method,const Json& arguments) {
    if(method=="ide.state") {
        const auto offset=arguments.value("offset",std::size_t(0)),limit=arguments.value("limit",std::size_t(50));
        Json documents=Json::array(),panels=Json::array();
        for(auto index=std::min(offset,tabs_.size());index<tabs_.size() && documents.size()<limit;++index) {
            const auto& tab=tabs_[index];
            documents.push_back({{"path",tab->path},{"revision",tab->revision},{"dirty",tab->dirty},
                {"readOnly",tab->readOnly},{"paged",tab->pagedProfile},{"preview",tab->preview}});
        }
        for(const auto& [id,visible]:panels_)if(visible)panels.push_back(id);
        return {{"activeDocument",active_},{"workspace",pathUtf8(engine_->workspace())},{"documents",documents},
            {"offset",offset},{"nextOffset",offset+documents.size()<tabs_.size()?Json(offset+documents.size()):Json(nullptr)},
            {"visiblePanels",panels},{"zoom",codeZoom_},{"fontSize",codeFontSize_},{"minimap",minimap_}};
    }
    if(method=="ide.commands") {
        const auto catalogue=commandCatalogue();Json rows=Json::array();
        const auto offset=arguments.value("offset",std::size_t(0)),limit=arguments.value("limit",std::size_t(50));
        for(auto index=std::min(offset,catalogue.size());index<catalogue.size() && rows.size()<limit;++index) {
            const auto& command=catalogue[index];rows.push_back({{"id",command.id},{"title",command.title},
                {"category",command.category},{"enabled",commandEnabled(command.id)},{"shortcut",keymap_.hint(command.id)}});
        }
        return {{"commands",rows},{"nextOffset",offset+rows.size()<catalogue.size()?Json(offset+rows.size()):Json(nullptr)}};
    }
    if(method=="ide.showPanel") {
        const std::map<std::string,std::string> panels={{"project","Project"},{"resources","Files"},{"structure","Structure"},
            {"search","Find in workspace..."},{"build","Build"},{"terminal","Terminal"},{"debugger","Debugger"},
            {"vcs","Version control"},{"gitCommits","Git commits"},{"problems","Problems"},{"analysis","Analysis and inspections"},
            {"references","References"},{"information","Code information"},{"mcp","MCP"},{"assistant","Assistant"}};
        const auto id=arguments.at("panel").get<std::string>();const auto found=panels.find(id);
        if(found==panels.end())throw cymcp::Error("InvalidParameters","Unsupported IDE panel");
        showPanel(found->second.c_str(),id.c_str());return {{"panel",id},{"visible",true}};
    }
    if(method=="ide.editorPreferences") {
        if(arguments.contains("zoom"))setCodeZoom(arguments.at("zoom").get<float>());
        if(arguments.contains("fontSize")){codeFontSize_=arguments.at("fontSize").get<float>();codePreferencesDirty_=true;codePreferencesChanged_=ImGui::GetTime();}
        if(arguments.contains("minimap"))setMinimap(arguments.at("minimap").get<bool>());
        ++optionsRevision_;
        return {{"zoom",codeZoom_},{"fontSize",codeFontSize_},{"minimap",minimap_}};
    }
    if(method=="ide.openDocument") {
        const auto path=arguments.at("path").get<std::string>();
        lastError_.clear();
        open(path,arguments.value("line",std::size_t(0))+1,arguments.value("character",std::size_t(0))+1,true);
        if(!lastError_.empty())throw cymcp::Error("OpenFailed",lastError_);
        return {{"path",active_},{"opened",true}};
    }
    const auto path=arguments.value("path",active_);
    const auto found=std::find_if(tabs_.begin(),tabs_.end(),[&](const auto& tab){return tab->path==path;});
    if(found==tabs_.end())throw cymcp::Error("DocumentNotOpen","The document is not open in this IDE instance");
    const auto tab=*found;
    if(method=="ide.activateDocument"){active_=path;focusTab_=path;return {{"path",path},{"active",true}};}
    if(method=="ide.closeDocument") {
        if(tab->revision!=arguments.at("expectedRevision").get<std::uint64_t>())throw cymcp::Error("RevisionConflict","The document revision changed");
        if(tab->dirty || (tab->paged && tab->paged->unsettled()))throw cymcp::Error("ConfirmationRequired","Save or discard this document explicitly in the IDE before closing it");
        closeTab(path);
        if(std::any_of(tabs_.begin(),tabs_.end(),[&](const auto& candidate){return candidate->path==path;}))
            throw cymcp::Error("ConfirmationRequired","The document could not be closed without further user action");
        return {{"path",path},{"closed",true}};
    }
    if(method=="ide.selection") {
        if(tab->image || tab->pagedProfile)throw cymcp::Error("UnsupportedProfile","Selection coordinates are available for ordinary code editors");
        auto range=tab->editor.GetMainCursorSelection();if(range.end<range.start)std::swap(range.start,range.end);
        const auto point=[&](TextEditor::DocPos value){return Json{{"line",value.line},
            {"character",LanguageClient::utf16Length(tab->editor.GetSectionText({value.line,0},value))}};};
        return {{"path",path},{"expectedRevision",tab->revision},{"start",point(range.start)},{"end",point(range.end)}};
    }
    throw cymcp::Error("UnknownOperation","Unknown IDE operation");
}
} // namespace cycode
