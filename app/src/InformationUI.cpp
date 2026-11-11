#include "cycode/Application.h"
#include <cyui/Core.h>
#include <algorithm>
namespace cycode {
Json Application::informationParameters() const {
    for (const auto &tab : tabs_) if (tab->path==active_ && !tab->pagedProfile && engine_->supportsLanguage(tab->path)) {
        const auto cursor=tab->editor.GetMainCursorPosition();
        return {{"path",tab->path},{"expectedRevision",tab->revision},
            {"position",{{"line",cursor.line},{"character",LanguageClient::utf16Length(tab->editor.GetSectionText({cursor.line,0},cursor))}}}};
    }
    return nullptr;
}
void Application::requestInformation(bool signature) {
    informationMethod_=signature ? "language.signature" : "language.hover";
    informationWanted_.clear(); informationSent_.clear(); informationState_=Json::object(); informationError_.clear();
    informationChanged_=ImGui::GetTime()-.2;
    showPanel("Code information","information");
}
void Application::updateInformation() {
    const bool visible=panels_.contains("information") && panels_.at("information");
    const auto parameters=visible && !informationMethod_.empty() ? informationParameters() : Json();
    const auto wanted=parameters.is_object() ? informationMethod_+parameters.dump() : "";
    if (wanted!=informationWanted_) {
        const bool initial=informationWanted_.empty(); informationWanted_=wanted;informationQuery_=parameters;
        informationState_=Json::object();informationError_.clear();
        informationChanged_=ImGui::GetTime()-(initial ? .2 : 0);
    }
    if (informationFuture_.valid() && informationFuture_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try {
            auto result=informationFuture_.get();
            if (!wanted.empty() && informationSent_==wanted) informationState_=std::move(result);
        } catch (const std::exception &e) {
            if (!wanted.empty() && informationSent_==wanted) {
                if (std::string(e.what()).starts_with("RevisionConflict")) { informationSent_.clear();informationChanged_=ImGui::GetTime(); }
                else informationError_=e.what();
            }
        }
    }
    if (wanted.empty() || informationFuture_.valid() || informationSent_==wanted || ImGui::GetTime()-informationChanged_<.15) return;
    const auto command=informationMethod_=="language.hover" ? "code.documentation" : "code.parameters";
    if (!commandEnabled(command)) return;
    auto engine=engine_; const auto method=informationMethod_;informationSent_=wanted;
    informationFuture_=std::async(std::launch::async,[engine,method,parameters] { return engine->dispatch(method,parameters); });
}
void Application::informationPanel() {
    if (!beginPanel("Code information","information")) return;
    auto action=[&](const char *icon,const char *text,const char *command,bool signature) {
        cyui::ScopedDisabled disabled(!commandEnabled(command));
        const auto tip=std::string(tr(text))+" ("+keymap_.hint(command)+")";
        if (cyui::iconButton(icon,tip.c_str(),informationMethod_==(signature ? "language.signature" : "language.hover"))) requestInformation(signature);
    };
    action(cyui::icon::info,"Quick documentation","code.documentation",false);ImGui::SameLine();
    action(cyui::icon::code,"Parameter information","code.parameters",true);ImGui::SameLine();
    { cyui::ScopedDisabled disabled(informationState_.empty()); if (cyui::iconButton(cyui::icon::copy,tr("Copy information"))) {
        std::string text;
        for (const auto &part : informationState_.value("contents",Json::array())) text+=part.at("text").get<std::string>()+"\n";
        for (const auto &signature : informationState_.value("signatures",Json::array())) {
            text+=signature.at("label").get<std::string>()+"\n";
            for (const auto &part : signature.at("documentation")) text+=part.at("text").get<std::string>()+"\n";
        }
        ImGui::SetClipboardText(text.c_str());
    } }
    if (informationFuture_.valid()) { ImGui::SameLine();cyui::spinner("##information-pending"); }
    cyui::divider();
    if (!informationError_.empty()) cyui::callout(cyui::Tone::warning,informationError_.c_str());
    if (informationState_.empty()) {
        if (!informationFuture_.valid() && informationError_.empty()) cyui::emptyState(tr("Code information"),tr("Request documentation or parameter information at the cursor."),cyui::icon::info);
        ImGui::End();return;
    }
    size_t documentSlot = 0;
    auto contents=[&](const Json &sections) {
        for (const auto &part : sections) {
            const auto slot = std::to_string(documentSlot++);
            auto &view = informationDocuments_[slot];
            const auto &text = part.at("text").get_ref<const std::string &>();
            const auto kind = part.value("kind", "plaintext");
            // Code and plaintext bypass the Markdown subset parser so literal
            // backticks, template arguments and language IDs survive unchanged.
            if (kind == "plaintext") { ImGui::TextWrapped("%s", text.c_str()); continue; }
            const auto measured = ImGui::CalcTextSize(text.c_str(), nullptr, false, std::max(1.f, ImGui::GetContentRegionAvail().x));
            const ImVec2 size{0, std::clamp(measured.y + ImGui::GetFrameHeightWithSpacing() * 3, cyui::dp(95), cyui::dp(380))};
            if (kind == "code") {
                view.code.setText(text, cyui::codeLanguageFromName(part.value("language", "")));
                cyui::codeViewer(slot.c_str(), view.codeState, view.code, size);
            } else {
                view.document.setMarkdown(text);
                // Link events remain inert, matching the existing information policy.
                cyui::documentView(slot.c_str(), view.state, view.document, size);
            }
        }
    };
    cyui::textDim("%s",informationState_.value("path","").c_str());
    if (!informationState_.value("available",false)) cyui::textDim("%s",tr("No information at this position."));
    contents(informationState_.value("contents",Json::array()));
    if (informationState_.contains("signatures") && !informationState_.at("signatures").empty()) {
        const auto &signatures=informationState_.at("signatures");auto selected=informationState_.value("activeSignature",0);
        selected=std::clamp(selected,0,static_cast<int>(signatures.size()-1));
        if (signatures.size()>1 && ImGui::BeginCombo(tr("Overload"),signatures[selected].at("label").get_ref<const std::string &>().c_str())) {
            for (size_t i=0;i<signatures.size();++i) { ImGui::PushID(static_cast<int>(i));if (ImGui::Selectable(signatures[i].at("label").get_ref<const std::string &>().c_str(),i==selected)) { selected=static_cast<int>(i);informationState_["activeSignature"]=selected; }ImGui::PopID(); }
            ImGui::EndCombo();
        }
        const auto &signature=signatures[selected];
        contents(Json::array({{{"kind","code"},{"language",engine_->languageBackendFor(active_)},{"text",signature.at("label")}}}));
        const auto active=signature.value("activeParameter",-1);
        if (active>=0 && active<signature.at("parameters").size()) {
            const auto &parameter=signature.at("parameters")[active];
            cyui::sectionTitle(tr("Active parameter"));
            contents(Json::array({{{"kind","plaintext"},{"text",parameter.at("label")}}}));
            contents(parameter.at("documentation"));
        }
        contents(signature.at("documentation"));
    }
    if (informationState_.value("truncated",false)) cyui::badge(tr("Information truncated"),cyui::Tone::warning);
    ImGui::End();
}
}
