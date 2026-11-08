#include "cycode/Application.h"
#include "cycode/EditorTheme.h"
#include "cycode/TextFile.h"
#include <algorithm>
namespace cycode {
void Application::beginUserSnippets(bool capture) {
    if(!commandEnabled(capture?"edit.captureSnippet":"edit.userSnippets"))return;
    Json params=Json::object();
    if(capture)for(const auto &tab:tabs_)if(tab->path==active_) {
        auto selection=tab->editor.GetMainCursorSelection();
        if(selection.end<selection.start)std::swap(selection.start,selection.end);
        const auto point=[&](TextEditor::DocPos at) {
            return Json{{"line",at.line},{"character",LanguageClient::utf16Length(tab->editor.GetSectionText({at.line,0},at))}};
        };
        params={{"path",tab->path},{"expectedRevision",tab->revision},
            {"selection",{{"start",point(selection.start)},{"end",point(selection.end)}}}};
        break;
    }
    userSnippetsEngine_=engine_;userSnippetsState_=Json::object();userSnippetsDiscard_.clear();
    userSnippetsEditor_=std::make_unique<cyui::CodeEditor>();
    configureCodeEditor(*userSnippetsEditor_);userSnippetsEditor_->setLanguageForPath("user.json");
    userSnippetsEditor_->native().SetTabSize(4);userSnippetsEditor_->native().SetInsertSpacesOnTabs(true);
    userSnippetsDirty_=false;userSnippetsCaptured_=false;userSnippetsOpen_=true;
    if(capture) {
        userSnippetsSaving_=false;userSnippetsError_.clear();auto owner=userSnippetsEngine_;
        userSnippetsFuture_=std::async(std::launch::async,[owner,params] {return owner->dispatch("snippets.userDraft",params);});
    } else userSnippetsTask(false);
}
void Application::userSnippetsTask(bool save) {
    if(userSnippetsFuture_.valid() || !userSnippetsEngine_)return;
    userSnippetsSaving_=save;userSnippetsError_.clear();
    const Json params=save ? Json{{"text",userSnippetsEditor_->text()},
        {"expectedRevision",userSnippetsState_.at("revision")}} : Json::object();
    auto owner=userSnippetsEngine_;
    userSnippetsFuture_=std::async(std::launch::async,[owner,params,save] {
        return owner->dispatch(save?"snippets.userSave":"snippets.userRead",params);
    });
}
void Application::userSnippetsPanel() {
    if(userSnippetsFuture_.valid() && userSnippetsFuture_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto response=userSnippetsFuture_.get();
            userSnippetsCaptured_=response.contains("draft");
            const auto draft=userSnippetsCaptured_ ? response.at("draft").get<std::string>() : std::string{};
            userSnippetsState_=userSnippetsCaptured_ ? std::move(response.at("catalogue")) : std::move(response);
            if(userSnippetsState_.value("editable",false)) {
                userSnippetsBaseline_=normalizeEditorText(userSnippetsState_.at("text"));
                userSnippetsEditor_->setText(userSnippetsCaptured_ ? draft : userSnippetsBaseline_);
                userSnippetsDirty_=userSnippetsCaptured_;
                userSnippetsVersion_=userSnippetsEditor_->native().GetTextRevision();
                userSnippetsEditor_->native().SetFocus();
            }
            userSnippetsError_=userSnippetsState_.value("error",std::string{});
            if(userSnippetsSaving_) {
                if(userSnippetsDiscard_=="quit")quitRequested_=true;
                userSnippetsOpen_=false;userSnippetsDiscard_.clear();
            }
        } catch(const std::exception &e) {userSnippetsError_=e.what();userSnippetsDiscard_.clear();}
        userSnippetsSaving_=false;
    }
    const bool requested=userSnippetsOpen_;
    const auto work=ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSizeConstraints(cyui::dp(320,120),{std::max(cyui::dp(320),work.x-cyui::dp(24)),std::max(cyui::dp(120),work.y-cyui::dp(24))});
    if(cyui::beginModal(label("Edit user snippets...","user-snippets").c_str(),&userSnippetsOpen_,900)) {
        if(!requested)ImGui::CloseCurrentPopup();
        const bool pending=userSnippetsFuture_.valid();
        const bool editable=userSnippetsState_.value("available",false) && userSnippetsState_.value("editable",false);
        if(requested && !userSnippetsOpen_) {
            if(pending)userSnippetsOpen_=true;
            else if(userSnippetsDirty_){userSnippetsOpen_=true;userSnippetsDiscard_="close";}
        }
        cyui::textDim("%s",tr("Shared by all projects in this portable installation."));
        if(!pending && userSnippetsState_.contains("available") && !userSnippetsState_.value("available",false))
            cyui::callout(cyui::Tone::warning,tr("User snippet storage is not configured in this host."));
        if(userSnippetsCaptured_)
            cyui::callout(cyui::Tone::info,tr("Selection copied into a new draft. Edit its name, prefix and scope, then Save."));
        if(!userSnippetsError_.empty())cyui::callout(cyui::Tone::error,userSnippetsError_.c_str());
        if(pending)cyui::spinner("##user-snippets-pending");
        if(!userSnippetsDiscard_.empty() && !pending) {
            cyui::callout(cyui::Tone::warning,tr("User snippets have unsaved changes."));
            if(cyui::primaryButton(tr("Keep editing"))) {
                if(userSnippetsDiscard_=="quit")updateInstallRequested_=false;
                userSnippetsDiscard_.clear();
            }
            ImGui::SameLine();
            if(userSnippetsDiscard_!="reload") {
                cyui::ScopedDisabled disabled(!editable);
                if(cyui::button(tr(userSnippetsDiscard_=="quit"?"Save and quit":"Save and close")))userSnippetsTask(true);
                ImGui::SameLine();
            }
            if(cyui::dangerButton(tr(userSnippetsDiscard_=="reload"?"Discard and reload":"Discard changes"))) {
                const auto action=userSnippetsDiscard_;userSnippetsDiscard_.clear();
                if(action=="reload")userSnippetsTask(false);
                else {userSnippetsOpen_=false;if(action=="quit")quitRequested_=true;ImGui::CloseCurrentPopup();}
            }
        } else {
            {
                cyui::ScopedDisabled disabled(pending || !editable);
                if(cyui::primaryButton(tr("Save")))userSnippetsTask(true);
                ImGui::SameLine();
                {
                    cyui::ScopedDisabled noUndo(!userSnippetsEditor_ || !userSnippetsEditor_->native().CanUndo());
                    if(cyui::button(tr("Undo")))userSnippetsEditor_->native().Undo();
                }
                ImGui::SameLine();
                {
                    cyui::ScopedDisabled noRedo(!userSnippetsEditor_ || !userSnippetsEditor_->native().CanRedo());
                    if(cyui::button(tr("Redo")))userSnippetsEditor_->native().Redo();
                }
            }
            ImGui::SameLine();
            {
                cyui::ScopedDisabled disabled(pending);
                if(cyui::button(tr("Reload saved snippets"))) {
                    if(userSnippetsDirty_)userSnippetsDiscard_="reload";else userSnippetsTask(false);
                }
                ImGui::SameLine();
                if(cyui::button(tr("Close"))) {
                    if(userSnippetsDirty_)userSnippetsDiscard_="close";
                    else {userSnippetsOpen_=false;ImGui::CloseCurrentPopup();}
                }
            }
        }
        if(userSnippetsEditor_) {
            userSnippetsEditor_->setReadOnly(!editable || userSnippetsFuture_.valid() || !userSnippetsDiscard_.empty());
            userSnippetsEditor_->draw("##user-snippet-json",{0,std::clamp(work.y-cyui::dp(260),cyui::dp(100),cyui::dp(480))});
            const auto version=userSnippetsEditor_->native().GetTextRevision();
            if(version!=userSnippetsVersion_) {
                userSnippetsDirty_=userSnippetsEditor_->text()!=userSnippetsBaseline_;userSnippetsVersion_=version;
            }
            const auto &io=ImGui::GetIO();
            if(!userSnippetsFuture_.valid() && userSnippetsDiscard_.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                if(editable && io.KeyCtrl && !io.KeyShift && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_S,false))userSnippetsTask(true);
                if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)) {
                    if(userSnippetsDirty_)userSnippetsDiscard_="close";
                    else {userSnippetsOpen_=false;ImGui::CloseCurrentPopup();}
                }
            }
        }
        cyui::endModal();
    }
    if(!userSnippetsOpen_ && !userSnippetsFuture_.valid()) {
        userSnippetsEditor_.reset();userSnippetsEngine_.reset();userSnippetsDirty_=false;
    }
}
} // namespace cycode
