#include "cycode/Application.h"
#include "cycode/EditorLanguage.h"
#include "cycode/EditorTheme.h"
#include "cycode/FileIcons.h"
#include <algorithm>
#include <cyui/Core.h>
#include <imgui_stdlib.h>

namespace cycode {
void Application::vcsTask(const std::string &method, Json params) {
    if (vcsFuture_.valid() || taskFuture_.valid()) { error(tr("A task is already running.")); return; }
    auto engine = engine_; vcsOperation_ = method;
    if (method == "vcs.diff") {
        vcsDiffPath_ = params.value("path", std::string{});
        vcsDiffTitle_ = (vcsDiffPath_.empty() ? std::string(tr("All changes")) : pathUtf8(utf8Path(vcsDiffPath_).filename())) +
                        (params.value("staged", false) ? " (" + std::string(tr("staged")) + ")" : std::string{});
    } else if (method == "vcs.show") {
        vcsDiffPath_.clear();
        vcsDiffTitle_ = std::string(tr("Commit")) + " " + params.value("revision", std::string{}).substr(0, 10);
    }
    if (method.starts_with("vcs.")&&method!="vcs.configure") {
        const auto path=vcsRequestPath();
        if(gitFuture_.valid()||vcsContextPath_!=path) { refreshGit();error(tr("Waiting for version control context."));return; }
        if(!params.contains("provider"))params["provider"] = provider_;
        params["contextPath"]=path;
        if(provider_=="git") {
            const auto root=vcsContext_.value("gitRepository",std::string("."));
            if(params.contains("repository")&&params.at("repository")!=root){error(tr("Repository changed. Cancel and review the action again."));return;}
            params["repository"]=root;params["expectedRepository"]=vcsContext_.at("gitIdentity");
        }
    }
    vcsOperationRepository_=params.value("repository",std::string{});
    vcsOperationIdentity_=params.value("expectedRepository",std::string{});
    vcsFuture_ = std::async(std::launch::async,[engine,method,params] { return engine->dispatch(method,params); });
}
void Application::updateVcs() {
    if (!vcsFuture_.valid() || vcsFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        auto result = vcsFuture_.get();
        const bool oldRepository=!vcsOperationRepository_.empty()&&(provider_!="git"||vcsOperationRepository_!=vcsContext_.value("gitRepository",std::string("."))||vcsOperationIdentity_!=vcsContext_.value("gitIdentity",std::string{}));
        if(oldRepository&&(vcsOperation_=="vcs.hunks"||vcsOperation_=="vcs.conflicts"||vcsOperation_=="vcs.worktrees"||vcsOperation_=="vcs.submodules"||vcsOperation_=="vcs.diff"||vcsOperation_=="vcs.show"||vcsOperation_=="vcs.blame"||vcsOperation_=="vcs.log")) {lastGitRefresh_=-10;refreshGit();return;}
        // Same-context background refreshes after resource events must retain receipts and Undo.
        if(vcsOperation_.starts_with("vcs.replay.")) updateGitReplay(result,oldRepository||vcsContextPath_!=vcsRequestPath());
        else if(vcsOperation_.starts_with("vcs.merge.")) updateGitMerge(result,oldRepository||vcsContextPath_!=vcsRequestPath());
        else if(vcsOperation_.starts_with("vcs.checkout.")) updateGitCheckout(result,oldRepository||vcsContextPath_!=vcsRequestPath());
        else if(vcsOperation_.starts_with("vcs.conflict.")) updateGitConflicts(result,oldRepository||vcsContextPath_!=vcsRequestPath());
        else if(vcsOperation_=="vcs.submodules") { vcsSubmodules_=result; vcsSubmoduleAddPlan_=Json::object(); vcsSubmoduleReview_.erase("plan"); }
        else if(vcsOperation_=="vcs.submodule.addPlan") {
            vcsSubmoduleAddPlan_=Json::object();
            const auto &request=result.at("request");
            if(!oldRepository&&vcsSubmoduleAddOpen_&&vcsSubmodules_.contains("revision")&&result.at("catalogRevision")==vcsSubmodules_.at("revision")&&
                request.at("url")==vcsSubmoduleAddUrl_&&request.at("path")==vcsSubmoduleAddPath_&&request.at("branch")==vcsSubmoduleAddBranch_&&
                request.at("name")== (vcsSubmoduleAddName_.empty()?vcsSubmoduleAddPath_:vcsSubmoduleAddName_)) vcsSubmoduleAddPlan_=result;
        }
        else if(vcsOperation_=="vcs.submodule.deinitPlan") {
            if(!oldRepository&&vcsSubmoduleReview_.value("action",std::string{})=="deinit"&&
                vcsSubmodules_.contains("revision")&&result.at("catalogRevision")==vcsSubmodules_.at("revision")&&
                result.at("identity")==vcsSubmoduleReview_.at("expectedRepository")&&
                result.at("catalogRevision")==vcsSubmoduleReview_.at("expectedRevision")&&
                result.at("entry").at("path")==vcsSubmoduleReview_.at("entry").at("path"))
                vcsSubmoduleReview_["plan"]=result;
        }
        else if(vcsOperation_=="vcs.submodule.settingsPlan") {
            if(!oldRepository&&vcsSubmoduleReview_.value("action",std::string{})=="settings"&&
                vcsSubmodules_.contains("revision")&&result.at("catalogRevision")==vcsSubmodules_.at("revision")&&
                result.at("identity")==vcsSubmoduleReview_.at("expectedRepository")&&
                result.at("catalogRevision")==vcsSubmoduleReview_.at("expectedRevision")&&
                result.at("request").at("path")==vcsSubmoduleReview_.at("entry").at("path")&&
                result.at("request").at("setting")==vcsSubmoduleReview_.at("setting")&&
                result.at("request").at("value")==vcsSubmoduleReview_.at("value"))
                vcsSubmoduleReview_["plan"]=result;
        }
        else if(vcsOperation_.starts_with("vcs.submodule.")) {
            vcsSubmoduleAddPlan_=Json::object(); vcsSubmoduleAddOpen_=false;
            vcsSubmoduleReview_=Json::object(); vcsSubmodules_=Json::object();
            if(!oldRepository&&result.contains("catalog"))vcsSubmodules_=result.at("catalog");
            output_+="\n> "+vcsOperation_+" ["+vcsOperationRepository_+"]\n"+result.value("output",std::string{});
            if(result.value("exitCode",0)||result.value("timedOut",false)||result.value("truncated",false))error(tr("Task failed. See output."));
            else if(!result.contains("catalogError")) cyui::notify(tr("Task completed"),cyui::Tone::success);
            if(result.contains("catalogError"))error(result.at("catalogError"));
            vcsRepositories_=nullptr;refreshFiles(false);
        }
        else if(vcsOperation_=="vcs.worktrees") {vcsWorktrees_=result;vcsWorktreeReview_.erase("plan");}
        else if(vcsOperation_=="vcs.worktree.plan") {
            if(!oldRepository&&vcsWorktreeReview_.contains("action")&&vcsWorktrees_.contains("revision")&&
                result.at("catalogRevision")==vcsWorktrees_.at("revision")&&result.at("catalogRevision")==vcsWorktreeReview_.at("expectedRevision")&&
                result.at("identity")==vcsWorktreeReview_.at("expectedRepository")&&
                result.at("request").at("action")==vcsWorktreeReview_.at("action")&&
                result.at("request").at("path")==vcsWorktreeReview_.at("path")&&
                result.at("request").at("destination")==vcsWorktreeReview_.at("destination")) vcsWorktreeReview_["plan"]=result;
        }
        else if(vcsOperation_.starts_with("vcs.worktree.")) {
            vcsWorktreeReview_=Json::object();vcsWorktrees_=Json::object();vcsWorktreesRequested_=false;
            if(!oldRepository&&result.contains("catalog")){vcsWorktrees_=result.at("catalog");vcsWorktreesRequested_=true;}
            output_+="\n> "+vcsOperation_+" ["+vcsOperationRepository_+"]\n"+result.value("output",std::string{});
            if(result.value("exitCode",0)||result.value("timedOut",false)||result.value("truncated",false))error(tr("Task failed. See output."));
            else if(!result.contains("catalogError")) cyui::notify(tr("Task completed"),cyui::Tone::success);
            if(result.contains("catalogError"))error(result.at("catalogError"));
            vcsRepositories_=nullptr;refreshFiles(false);
        }
        else if(vcsOperation_=="vcs.hunks") {
            vcsHunks_=std::move(result);vcsHunkSelection_.clear();vcsSection_="Hunks";
            std::string patch;for(const auto &h:vcsHunks_.at("hunks"))patch+=h.at("patch").get<std::string>();
            vcsHunkDiff_.setUnified(patch,cyui::codeLanguageForPath(vcsHunks_.at("path").get<std::string>()));vcsHunkDiffState_={};
        }
        else if(vcsOperation_=="vcs.configure") { clearGitReplay();clearGitMerge();clearGitCheckout();clearGitConflicts();vcsRepository_.clear();vcsContext_=result;provider_=result.at("provider");vcsActionInputs_=Json::object();vcsConflict_=Json::object();vcsHunks_=Json::object();vcsHunkSelection_.clear();vcsPreviewTitle_.clear();synchronizeExtensions(); }
        else if (vcsOperation_ == "vcs.conflicts") {
            vcsConflict_ = std::move(result); vcsSection_ = "Conflicts";
            vcsBaseCode_.setText(vcsConflict_.at("versions").at("base").is_null() ? "" : vcsConflict_.at("versions").at("base").get<std::string>());
            vcsOursCode_.setText(vcsConflict_.at("versions").at("ours").is_null() ? "" : vcsConflict_.at("versions").at("ours").get<std::string>());
            vcsTheirsCode_.setText(vcsConflict_.at("versions").at("theirs").is_null() ? "" : vcsConflict_.at("versions").at("theirs").get<std::string>());
            open(vcsConflict_.at("path"));
            for (auto *view : {&vcsBaseCode_, &vcsOursCode_, &vcsTheirsCode_}) {
                configureCodeEditor(*view); view->setLanguageForPath(vcsConflict_.at("path").get<std::string>());
                view->setReadOnly(true); view->setSyntaxEnabled(!languageUsesLegacyLexer(vcsConflict_.at("path").get<std::string>()));
            }
            for (auto editor : {&vcsBase_,&vcsOurs_,&vcsTheirs_}) {
                const auto path=vcsConflict_.at("path").get<std::string>();
                if (languageUsesLegacyLexer(path)) editor->SetLanguage(editorLanguage(path,unreal_.value("available",false)));
                Syntax syntax; syntax.update(*editor,path,editor->GetText());
            }
        } else if (vcsOperation_ == "vcs.diff" || vcsOperation_ == "vcs.show" || vcsOperation_ == "vcs.blame" || vcsOperation_ == "vcs.log") {
            vcsPreviewTitle_ = vcsOperation_; vcsPreviewCode_.setText(result.value("output",""));
            configureCodeEditor(vcsPreviewCode_); vcsPreviewCode_.setReadOnly(true);
            vcsIsDiff_ = vcsOperation_ == "vcs.diff" || vcsOperation_ == "vcs.show";
            if (vcsIsDiff_) {
                vcsDiff_.setUnified(result.value("output",""), cyui::codeLanguageForPath(result.value("path",vcsDiffPath_.empty()?active_:vcsDiffPath_)));
                vcsDiffState_ = {};
                // Like Rider, a diff opens as an editor tab instead of inside the narrow tool window.
                vcsDiffWindow_ = true; vcsDiffFocus_ = true;
            } else {
                vcsSection_ = "Review";
                showPanel("Version control","vcs");
            }
            if (result.value("exitCode",0)) error(tr("Task failed. See output."));
        } else {
            if(vcsOperation_=="vcs.applyHunks"){vcsHunks_=Json::object();vcsHunkSelection_.clear();vcsSection_="Changes";}
            output_ += "\n> "+vcsOperation_+"\n"+result.value("output","");
            if (result.value("exitCode",0)) error(tr("Task failed. See output."));
            else { cyui::notify(tr("Task completed"),cyui::Tone::success); if (vcsOperation_ == "vcs.resolve") { clearGitConflicts();vcsConflict_ = Json::object(); vcsSection_ = "Changes"; } }
            refreshFiles();
        }
    } catch (const std::exception &e) { error(e.what()); }
    // A background status read may have started before the mutation. Let it finish,
    // then invalidate its refresh timestamp so the next frame obtains fresh data.
    if(!vcsOperation_.starts_with("vcs.merge.")&&vcsOperation_!="vcs.inspect"&&vcsOperation_!="vcs.log"&&vcsOperation_!="vcs.diff"&&vcsOperation_!="vcs.show"&&vcsOperation_!="vcs.blame")invalidateGitMerge();
    if(!vcsOperation_.starts_with("vcs.replay.")&&vcsOperation_!="vcs.inspect"&&vcsOperation_!="vcs.log"&&vcsOperation_!="vcs.diff"&&vcsOperation_!="vcs.show"&&vcsOperation_!="vcs.blame")invalidateGitReplay();
    lastGitRefresh_ = -10;
    if (!gitFuture_.valid()) refreshGit();
}
static bool conflictStatus(const std::string &status) { return status.find('U') != std::string::npos || status == "AA" || status == "DD"; }
void Application::gitPanel() {
    if(beginPanel("Version control","vcs")){gitPanelContents(false);ImGui::End();}
    if(beginPanel("Git commits","gitCommits","vcs")){gitPanelContents(true);ImGui::End();}
}
void Application::gitPanelContents(bool commits) {
    if(commits && provider_!="git"){cyui::emptyState(tr("Git commits"),tr("Select a Git repository to view commits."),cyui::icon::sync);return;}
    gitRepositoryControls();
    const bool working = vcsFuture_.valid() || taskFuture_.valid();
    ImGui::SetNextItemWidth(cyui::dp(140));
    ImGui::BeginDisabled(working || gitFuture_.valid());
    if (ImGui::BeginCombo("##provider",provider_.c_str())) {
        auto choose = [&](const std::string &id, const std::string &title) {
            if (ImGui::Selectable(title.c_str(),provider_ == id))vcsTask("vcs.configure",{{"root",vcsMappingRoot_},{"provider",id},{"expectedRevision",vcsContext_.value("revision",uint64_t(1))}});
        };
        choose("git","Git");
        for (const auto &provider : pluginInfo_.value("vcsProviders",Json::array())) choose(provider.at("id"),provider.at("title"));
        ImGui::EndCombo();
    }
    ImGui::EndDisabled(); ImGui::SameLine();
    if (cyui::button(tr("Refresh"))) refreshGit();
    ImGui::SameLine();
    if (working || gitFuture_.valid()) cyui::spinner("##git-busy");
    else if (git_.value("repository",false)) {
        cyui::textDim("%s",git_.value("branch","").c_str());
        if (vcsInfo_.value("hasUpstream",false)) { ImGui::SameLine(); cyui::textDim("  +%d / -%d",vcsInfo_.value("ahead",0),vcsInfo_.value("behind",0)); }
    }
    if(ImGui::GetContentRegionAvail().x>cyui::dp(680))ImGui::SameLine();
    ImGui::SetNextItemWidth(std::min(cyui::dp(220),ImGui::GetContentRegionAvail().x));ImGui::InputTextWithHint("##vcs-mapping-root",tr("Project / repository folder"),&vcsMappingRoot_);
    if(provider_!="git") {
        cyui::textDim("%s",vcsContext_.value("title",provider_).c_str());
        if(!vcsContext_.value("available",true))cyui::callout(cyui::Tone::warning,tr("Configured provider is unavailable"));
        const auto message=git_.value("message",std::string{});
        if(!message.empty())ImGui::TextWrapped("%s",message.c_str());
        const auto requested=vcsSection_;vcsSection_.clear();
        if(ImGui::BeginTabBar("##provider-sections")) {
        const auto changesTitle=std::string(tr("Changes"))+"###provider-changes";
        if(ImGui::BeginTabItem(changesTitle.c_str(),nullptr,requested=="Changes"?ImGuiTabItemFlags_SetSelected:ImGuiTabItemFlags_None)) {
        for(const auto &action:vcsContext_.value("actions",Json::array())) {
            const std::string id=action.at("id"),title=action.at("title");ImGui::PushID(id.c_str());
            auto values=vcsActionInputs_.value(id,Json::object());bool complete=true;
            if(cyui::beginSection(tr(title.c_str()))) {
                if(cyui::beginProperties("##provider-arguments",160)) {
                    for(const auto &field:action.value("fields",Json::array())) {
                        const std::string key=field.at("id"),name=field.at("title");std::string value=values.value(key,field.value("default",std::string{}));
                        cyui::property(tr(name.c_str()));ImGui::PushID(key.c_str());ImGui::InputText("##value",&value);ImGui::PopID();values[key]=value;
                        if(field.value("required",false)&&value.empty())complete=false;
                    }cyui::endProperties();
                }
                vcsActionInputs_[id]=values;
                const bool file=action.value("scope",std::string("repository"))=="file";
                cyui::ScopedDisabled disabled(working||!complete||(file&&(active_.empty()||active_.find("://")!=std::string::npos)));
                if(cyui::button(tr(title.c_str()))) { if(file)values["path"]=active_;vcsTask("vcs."+id,values); }
                cyui::endSection();
            }
            ImGui::PopID();
        }
        if(ImGui::BeginTable("##provider-files",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn(tr("Status"));ImGui::TableSetupColumn(tr("File"));ImGui::TableHeadersRow();
            for(const auto &file:git_.value("files",Json::array())) { ImGui::TableNextRow();ImGui::TableNextColumn();const auto status=file.value("status",std::string{});ImGui::TextUnformatted(status.c_str());ImGui::TableNextColumn();const std::string path=file.at("path");if(ImGui::Selectable(path.c_str()))open(path); }
            ImGui::EndTable();
        }
        ImGui::EndTabItem();
        }
        if(!vcsPreviewTitle_.empty()) {
            const auto reviewTitle=std::string(tr("Review"))+"###provider-review";
            if(ImGui::BeginTabItem(reviewTitle.c_str(),nullptr,requested=="Review"?ImGuiTabItemFlags_SetSelected:ImGuiTabItemFlags_None)) {
                cyui::textDim("%s",vcsPreviewTitle_.c_str());
                if(vcsIsDiff_)drawDiff("##provider-review",vcsDiffState_,vcsDiff_);
                else {vcsPreviewCode_.native().SetShowMiniMapEnabled(false);drawCode(vcsPreviewCode_,"##provider-review");}
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
        }
        return;
    }
    if (!git_.value("repository",false)) {
        cyui::emptyState(tr("No Git repository"),tr("Open a folder containing a Git repository."),cyui::icon::sync);
        if (provider_ == "git") { cyui::ScopedDisabled disabled(working); if (cyui::button(tr("Initialize Git repository"))) vcsTask("vcs.init"); }
        return;
    }
    auto button = [&](const char *title,const std::string &action,Json params = {}) {
        cyui::ScopedDisabled disabled(working || !vcsSupports(action));
        if (cyui::button(tr(title))) vcsTask("vcs."+action,params.is_null() ? Json::object() : params);
    };
    if (ImGui::BeginTabBar("##vcs-sections")) {
        // SetSelected is a one-frame request; user tab clicks remain free afterwards.
        const bool commitRequest=vcsSection_=="History" || vcsSection_=="Commit";
        const auto requested=commitRequest==commits?vcsSection_:std::string{};
        if(commitRequest==commits)vcsSection_.clear();
        auto begin = [&](const char *name) {
            const bool bottom=std::string_view(name)=="History" || std::string_view(name)=="Commit";
            if(bottom!=commits)return false;
            const auto title=std::string(tr(name))+"###vcs-section-"+name;
            return ImGui::BeginTabItem(title.c_str(),nullptr,requested==name?ImGuiTabItemFlags_SetSelected:ImGuiTabItemFlags_None);
        };
        if (begin("Changes")) {
            if(cyui::button(tr("Commit..."))){vcsSection_="Commit";openToolWindow("Git commits","gitCommits");}
            ImGui::SameLine();if(cyui::button(tr("History"))){vcsSection_="History";openToolWindow("Git commits","gitCommits");refreshGit();}
            button("Diff","diff"); ImGui::SameLine(); button("Staged diff","diff",{{"staged",true}});
            if (ImGui::BeginTable("##changes",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,{0,0})) {
                ImGui::TableSetupColumn(tr("Status"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(28));
                ImGui::TableSetupColumn(tr("File"),ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn(tr("Actions"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(82));ImGui::TableHeadersRow();
                for(const auto &file:git_.value("files",Json::array())) {
                    const auto path=file.at("path").get<std::string>(),status=file.at("status").get<std::string>();
                    ImGui::PushID(path.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();cyui::textDim("%s",status.c_str());ImGui::TableNextColumn();
                    // Rider: double-click shows the diff, Ctrl+click jumps to the source file.
                    if(fileSelectable(path,path,vcsDiffWindow_ && vcsDiffPath_==path,theme_)) {
                        if(ImGui::GetIO().KeyCtrl)openFromProject(path,true);
                        else if(ImGui::IsMouseDoubleClicked(0) && !working && vcsSupports("diff"))vcsTask("vcs.diff",{{"path",path}});
                    }
                    cyui::tooltip(tr("Double-click: show diff. Ctrl+click: open file."));
                    if(ImGui::BeginPopupContextItem("##file-actions")) {
                        if(ImGui::MenuItem(tr("Show diff"),nullptr,false,!working && vcsSupports("diff")))vcsTask("vcs.diff",{{"path",path}});
                        if(ImGui::MenuItem(tr("Open file")))openFromProject(path,true);
                        ImGui::Separator();
                        if(ImGui::MenuItem(tr("Review hunks")))vcsTask("vcs.hunks",{{"path",path}});
                        if(ImGui::MenuItem(tr("Staged hunks")))vcsTask("vcs.hunks",{{"path",path},{"staged",true}});
                        ImGui::EndPopup();
                    }
                    ImGui::TableNextColumn();
                    auto action=[&](const char *icon,const char *title,const char *method,Json args) {
                        cyui::ScopedDisabled disabled(working || !vcsSupports(method));
                        if(cyui::iconButton(icon,tr(title),false,0,cyui::dp(22,22)))vcsTask(std::string("vcs.")+method,std::move(args));
                    };
                    if(conflictStatus(status) && vcsSupports("conflicts"))action(cyui::icon::warning,"Resolve conflict",provider_=="git"?"conflict.list":"conflicts",provider_=="git"?Json::object():Json{{"path",path}});
                    else {action(cyui::icon::add,"Stage","stage",{{"path",path}});ImGui::SameLine(0,cyui::dp(2));action(cyui::icon::remove,"Unstage","unstage",{{"path",path}});}
                    ImGui::SameLine(0,cyui::dp(2));action(cyui::icon::edit,"Diff","diff",{{"path",path}});ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if(begin("Worktrees")) {gitWorktreeControls();ImGui::EndTabItem();}
        if(begin("Submodules")) {gitSubmoduleControls();ImGui::EndTabItem();}
        if (vcsSupports("inspect") && begin("Branches")) {
            // Worktrees has its own shared service/view; branch controls remain unchanged.
            ImGui::SetNextItemWidth(cyui::dp(210)); ImGui::InputTextWithHint("##new-branch",tr("New branch name"),&vcsNewBranch_); ImGui::SameLine();
            button("Create branch","branch.create",{{"name",vcsNewBranch_},{"start",vcsRevision_}});
            ImGui::SetNextItemWidth(cyui::dp(210)); ImGui::InputTextWithHint("##start",tr("Start revision"),&vcsRevision_);
            if (ImGui::BeginTable("##branches",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable)) {
                ImGui::TableSetupColumn(tr("Branch")); ImGui::TableSetupColumn(tr("Upstream")); ImGui::TableSetupColumn(tr("Actions")); ImGui::TableHeadersRow();
                for (const auto &branch : vcsInfo_.value("branches",Json::array())) {
                    const auto name = branch.at("name").get<std::string>(); ImGui::PushID(name.c_str()); ImGui::TableNextRow(); ImGui::TableNextColumn();
                    if (ImGui::Selectable((std::string(branch.at("current").get<bool>() ? "* " : "  ")+name).c_str(),vcsBranch_ == name)) vcsBranch_ = name;
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(branch.at("upstream").get<std::string>().c_str()); ImGui::TableNextColumn();
                    if (!branch.at("current").get<bool>()) { button(branch.at("remote").get<bool>() ? "Track branch" : "Switch branch","branch.switch",{{"name",name},{"track",branch.at("remote")}}); }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (!vcsBranch_.empty()) { cyui::textDim("%s",vcsBranch_.c_str()); button("Rename branch","branch.rename",{{"name",vcsBranch_},{"newName",vcsNewBranch_}}); ImGui::SameLine(); button("Delete merged branch","branch.delete",{{"name",vcsBranch_}}); }
            cyui::divider(); button("Merge revision","merge",{{"revision",vcsRevision_}});
            ImGui::TextWrapped("%s",tr("Switching and merging require saved buffers and a clean tracked working tree."));
            ImGui::EndTabItem();
        }
        if(begin("Commit")) {
            cyui::textDim("%s",tr("Commit message"));
            ImGui::SetNextItemWidth(std::max(cyui::dp(100),ImGui::GetContentRegionAvail().x));
            ImGui::InputTextMultiline("##commit-message",&commitMessage_,{0,cyui::dp(80)});
            {cyui::ScopedDisabled disabled(working || commitMessage_.empty() || !vcsSupports("commit"));
                if(cyui::primaryButton(tr("Commit staged")))vcsTask("vcs.commit",{{"message",commitMessage_}});}
            ImGui::SameLine();if(cyui::button(tr("Review changes")))openToolWindow("Version control","vcs");
            ImGui::EndTabItem();
        }
        if (vcsSupports("history") && begin("History")) {
            if (ImGui::BeginTable("##history",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,{0,0})) {
                ImGui::TableSetupColumn(tr("Commit"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(90)); ImGui::TableSetupColumn(tr("Author")); ImGui::TableSetupColumn(tr("Date")); ImGui::TableSetupColumn(tr("Message")); ImGui::TableHeadersRow();
                for (const auto &commit : vcsHistory_.value("commits",Json::array())) {
                    auto hash = commit.at("hash").get<std::string>(); ImGui::PushID(hash.c_str()); ImGui::TableNextRow(); ImGui::TableNextColumn();
                    if (ImGui::Selectable(hash.substr(0,10).c_str())) vcsTask("vcs.show",{{"revision",hash}});
                    if(provider_=="git"&&ImGui::BeginPopupContextItem("##replay-commit")) {
                        if(ImGui::MenuItem(tr("Cherry-pick commit"))) {vcsReplayCommit_=hash;vcsReplayAction_=0;invalidateGitReplay();vcsSection_="Replay";showPanel("Version control","vcs");}
                        if(ImGui::MenuItem(tr("Revert commit"))) {vcsReplayCommit_=hash;vcsReplayAction_=1;invalidateGitReplay();vcsSection_="Replay";showPanel("Version control","vcs");}
                        ImGui::EndPopup();
                    }
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(commit.at("author").get<std::string>().c_str()); ImGui::TableNextColumn(); ImGui::TextUnformatted(commit.at("date").get<std::string>().c_str());
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(commit.at("subject").get<std::string>().c_str()); ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (vcsSupports("inspect") && begin("Remotes")) {
            for (const auto &remote : vcsInfo_.value("remotes",Json::array())) {
                if (ImGui::Selectable((remote.at("name").get<std::string>()+"  "+remote.at("url").get<std::string>()).c_str(),remote.at("name") == vcsRemote_)) { vcsRemote_ = remote.at("name"); vcsRemoteUrl_ = remote.at("url"); }
            }
            if (cyui::beginProperties("##git-remote",140)) {
                cyui::property(tr("Remote name")); ImGui::InputText("##remote",&vcsRemote_); cyui::property(tr("Remote URL")); ImGui::InputText("##url",&vcsRemoteUrl_);
                cyui::property(tr("Branch")); ImGui::InputTextWithHint("##push-branch",git_.value("branch","").c_str(),&vcsBranch_); cyui::endProperties();
            }
            button("Add remote","remote.add",{{"remote",vcsRemote_},{"url",vcsRemoteUrl_}}); ImGui::SameLine(); button("Update remote URL","remote.set",{{"remote",vcsRemote_},{"url",vcsRemoteUrl_}}); ImGui::SameLine(); button("Remove remote","remote.remove",{{"remote",vcsRemote_}});
            cyui::divider(); ImGui::Checkbox(tr("Set upstream on push"),&vcsSetUpstream_);
            Json parameters = {{"remote",vcsRemote_},{"branch",vcsBranch_.empty() ? git_.value("branch","") : vcsBranch_},{"setUpstream",vcsSetUpstream_}};
            button("Fetch","fetch",{{"remote",vcsRemote_}}); ImGui::SameLine(); button("Pull fast-forward","pull",parameters); ImGui::SameLine(); button("Push","push",parameters);
            ImGui::TextWrapped("%s",tr("Uses configured Git credentials and hooks. Pull refuses divergent branches.")); ImGui::EndTabItem();
        }
        if (vcsSupports("stash.save") && begin("Stashes")) {
            ImGui::SetNextItemWidth(cyui::dp(300)); ImGui::InputTextWithHint("##stash-message",tr("Stash message"),&vcsStashMessage_); ImGui::SameLine(); ImGui::Checkbox(tr("Include untracked"),&vcsIncludeUntracked_);
            button("Save stash","stash.save",{{"message",vcsStashMessage_.empty() ? "CyCode stash" : vcsStashMessage_},{"includeUntracked",vcsIncludeUntracked_}});
            int index = 0; for (const auto &stash : vcsInfo_.value("stashes",Json::array())) { ImGui::PushID(index); if (ImGui::Selectable((stash.at("ref").get<std::string>()+"  "+stash.at("message").get<std::string>()).c_str(),vcsStashIndex_ == index)) vcsStashIndex_ = index; ImGui::PopID(); ++index; }
            { cyui::ScopedDisabled disabled(index == 0 || vcsStashIndex_ >= index); button("Apply stash","stash.apply",{{"index",vcsStashIndex_}}); ImGui::SameLine(); button("Pop stash","stash.pop",{{"index",vcsStashIndex_}}); ImGui::SameLine(); if (cyui::button(tr("Drop selected stash..."))) ImGui::OpenPopup(tr("Drop stash?")); }
            if (ImGui::BeginPopupModal(tr("Drop stash?"),nullptr,ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::TextWrapped("%s",tr("Permanently remove this saved stash entry?")); ImGui::Text("stash@{%d}",vcsStashIndex_); if (cyui::button(tr("Drop stash"))) { vcsTask("vcs.stash.drop",{{"index",vcsStashIndex_}}); ImGui::CloseCurrentPopup(); } ImGui::SameLine(); if (cyui::button(tr("Cancel"))) ImGui::CloseCurrentPopup(); ImGui::EndPopup(); }
            ImGui::EndTabItem();
        }
        if (vcsSupports("conflicts") && begin("Conflicts")) {
            if(provider_=="git")gitConflictControls();
            if(provider_=="git")gitCheckoutControls();
            if (!vcsConflict_.contains("path")) ImGui::TextWrapped("%s",tr("Choose Resolve conflict from a conflicted file in Changes."));
            else {
                auto path = vcsConflict_.at("path").get<std::string>(); ImGui::TextUnformatted(path.c_str()); ImGui::SameLine(); button("Refresh conflict","conflicts",{{"path",path}});
                auto use = [&](const char *title,const char *version) {
                    cyui::ScopedDisabled disabled(working || vcsConflict_.at("versions").at(version).is_null());
                    if (cyui::button(tr(title))) {
                        try { engine_->dispatch("documents.replace",{{"path",path},{"expectedRevision",vcsConflict_.at("revision")},{"text",vcsConflict_.at("versions").at(version)}}); vcsTask("vcs.conflicts",{{"path",path}}); }
                        catch (const std::exception &e) { error(e.what()); }
                    }
                };
                use("Use ours in editor","ours"); ImGui::SameLine(); use("Use theirs in editor","theirs"); ImGui::SameLine();
                { cyui::ScopedDisabled disabled(working); if (cyui::primaryButton(tr("Save and mark resolved"))) {
                    try { auto document = engine_->dispatch("documents.read",{{"path",path}}); vcsTask("vcs.resolve",{{"path",path},{"expectedRevision",document.at("revision")}}); } catch (const std::exception &e) { error(e.what()); }
                } }
                ImGui::TextWrapped("%s",tr("Edit the working file in its normal tab. These three source versions are read-only."));
                if (ImGui::BeginTable("##conflict-versions",3,ImGuiTableFlags_Resizable)) {
                    for (auto title : {"Base","Ours","Theirs"}) ImGui::TableSetupColumn(tr(title)); ImGui::TableHeadersRow(); ImGui::TableNextRow();
                    int column = 0;
                    for (auto *view : {&vcsBaseCode_, &vcsOursCode_, &vcsTheirsCode_}) {
                        ImGui::TableSetColumnIndex(column++); view->native().SetShowMiniMapEnabled(false);
                        ImGui::PushID(column); view->draw("##conflict-code", {0,std::max(cyui::dp(100),ImGui::GetContentRegionAvail().y)}); ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }
        if(provider_=="git"&&begin("Merge")) {gitMergeControls();ImGui::EndTabItem();}
        if(provider_=="git"&&begin("Replay")) {gitReplayControls();ImGui::EndTabItem();}
        if(vcsSupports("hunks")&&begin("Hunks")) {
            {cyui::ScopedDisabled disabled(working||active_.empty()||active_.find("://")!=std::string::npos);
             if(cyui::button(tr("Review hunks")))vcsTask("vcs.hunks",{{"path",active_}});ImGui::SameLine();
             if(cyui::button(tr("Staged hunks")))vcsTask("vcs.hunks",{{"path",active_},{"staged",true}});}
            if(vcsHunks_.contains("id")) {
                ImGui::TextUnformatted(vcsHunks_.at("path").get_ref<const std::string &>().c_str());
                const auto reason=vcsHunks_.value("reason","");if(!reason.empty())ImGui::TextWrapped("%s",tr(reason.c_str()));
                {cyui::ScopedDisabled disabled(working||vcsHunkSelection_.empty());if(cyui::primaryButton(tr(vcsHunks_.value("staged",false)?"Unstage selected hunks":"Stage selected hunks"))){Json chosen=Json::array();for(auto i:vcsHunkSelection_)chosen.push_back(i);vcsTask("vcs.applyHunks",{{"id",vcsHunks_.at("id")},{"hunks",chosen}});}}
                const auto height=std::max(cyui::dp(90),ImGui::GetContentRegionAvail().y);
                if(ImGui::BeginTable("##hunk-layout",2,ImGuiTableFlags_Resizable|ImGuiTableFlags_BordersInnerV)) {
                    ImGui::TableSetupColumn(tr("Hunks"),ImGuiTableColumnFlags_WidthFixed,cyui::dp(225));ImGui::TableSetupColumn(tr("Review"),ImGuiTableColumnFlags_WidthStretch);ImGui::TableNextRow();ImGui::TableNextColumn();
                    ImGui::BeginChild("##hunk-list",{0,height});
                    const auto &hunks=vcsHunks_.at("hunks");ImGuiListClipper clipper;clipper.Begin(int(hunks.size()),ImGui::GetFrameHeightWithSpacing());
                    while(clipper.Step())for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;++row){const auto &h=hunks.at(size_t(row));const auto i=h.at("index").get<size_t>();bool selected=vcsHunkSelection_.contains(i);ImGui::PushID(int(i));
                        if(ImGui::Checkbox(h.at("header").get_ref<const std::string &>().c_str(),&selected)){if(selected){vcsHunkSelection_.insert(i);if(i<vcsHunkDiff_.hunks().size())vcsHunkDiffState_.focus=vcsHunkDiff_.hunks()[i];}else vcsHunkSelection_.erase(i);}ImGui::PopID();}
                    ImGui::EndChild();ImGui::TableNextColumn();drawDiff("##hunk-review",vcsHunkDiffState_,vcsHunkDiff_,{0,height});ImGui::EndTable();
                }
            }
            ImGui::EndTabItem();
        }
        if (!vcsPreviewTitle_.empty() && begin("Review")) {
            cyui::textDim("%s",vcsPreviewTitle_.c_str());
            if (vcsIsDiff_) drawDiff("##git-review", vcsDiffState_, vcsDiff_);
            else { vcsPreview_.SetShowMiniMapEnabled(false); drawCode(vcsPreviewCode_,"##git-review"); }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
} // namespace cycode
