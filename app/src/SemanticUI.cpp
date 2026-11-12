#include "cycode/Application.h"
#include "cycode/EditorTheme.h"
#include "cycode/EditorLanguage.h"
#include <algorithm>
#include <cyui/Core.h>
#include <imgui_stdlib.h>
#include <utility>

namespace cycode {
template<class T> static bool completed(std::future<T> &future) { return future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }
void Application::changeTask(const std::string &method, Json params) {
    if (refactorFuture_.valid()) return;
    refactorOperation_ = method;
    auto engine = engine_;
    refactorFuture_ = std::async(std::launch::async, [engine, method, params] { return engine->dispatch(method, params); });
}
void Application::beginRename() {
    for (const auto &tab : tabs_) if (tab->path == active_ && !tab->readOnly) {
        auto position = tab->editor.GetMainCursorPosition();
        auto start = tab->editor.FindWordStart(position, true), end = tab->editor.FindWordEnd(position, true);
        refactorName_ = tab->editor.GetSectionText(start, end);
        renameParameters_ = {{"path", active_}, {"expectedRevision", tab->revision},
                             {"position", {{"line", position.line}, {"character", LanguageClient::utf16Length(tab->editor.GetSectionText({position.line, 0}, position))}}}};
        renameOpen_ = true; return;
    }
}
bool Application::beginFormat(bool selection, const std::string &path) {
    if (refactorFuture_.valid()) return false;
    const auto &target = path.empty() ? active_ : path;
    for (const auto &tab : tabs_) if (tab->path == target && !tab->readOnly && !tab->image && !tab->pagedProfile) {
        Json params = {{"path", target}, {"expectedRevision", tab->revision}, {"tabSize", tab->editor.GetTabSize()}, {"insertSpaces", tab->editor.IsInsertSpacesOnTabs()}};
        if (selection) {
            auto range = tab->editor.GetMainCursorSelection();
            if (range.end < range.start) std::swap(range.start, range.end);
            if (range.start == range.end) return false;
            auto position = [&](auto p) { return Json{{"line", p.line}, {"character", LanguageClient::utf16Length(tab->editor.GetSectionText({p.line, 0}, p))}}; };
            params["range"] = {{"start", position(range.start)}, {"end", position(range.end)}};
        }
        changeTask("language.format", params); return true;
    }
    return false;
}
bool Application::rangeFormatAvailable(const std::string &path) const {
    if (smoke_ || !engine_ || !engine_->supportsLanguage(path)) return false;
    const auto servers = languageState_.find("servers");
    if (servers == languageState_.end()) return false;
    const auto server = servers->find(engine_->languageBackendFor(path));
    if (server == servers->end() || !server->value("enabled", false) || !server->value("ready", false)) return false;
    const auto methods = server->find("methods");
    return methods == server->end() || methods->value("rangeFormat", false);
}
void Application::formatOnType(const std::shared_ptr<Tab> &tab, size_t firstLine, size_t lastLine) {
    // Rider-style typing assistance: reformat the block or statement just completed.
    // Busy, unsupported or stale requests are skipped silently; typing always wins.
    if (!formatOnType_ || !tab || refactorFuture_.valid() || !formatSavePath_.empty() || !formatTypePath_.empty() ||
        tab->readOnly || tab->image || tab->pagedProfile || isUntitledDocument(tab->path) ||
        tab->editor.GetLineCount() > 20000 || !rangeFormatAvailable(tab->path))
        return;
    if (lastLine < firstLine) std::swap(firstLine, lastLine);
    const auto lastText = tab->editor.GetLineText(lastLine);
    Json params = {{"path", tab->path}, {"expectedRevision", tab->revision}, {"tabSize", tab->editor.GetTabSize()},
                   {"insertSpaces", tab->editor.IsInsertSpacesOnTabs()},
                   {"range", {{"start", {{"line", firstLine}, {"character", 0}}},
                              {"end", {{"line", lastLine}, {"character", LanguageClient::utf16Length(lastText)}}}}}};
    formatTypePath_ = tab->path;
    formatTypeRevision_ = tab->revision;
    changeTask("language.format", params);
}
void Application::typedFormatTrigger(const std::shared_ptr<Tab> &tab, char32_t typed) {
    if (!typed || !formatOnType_ || tab->editor.MainCursorHasSelection()) return;
    const auto caret = tab->editor.GetMainCursorPosition();
    if (caret.index == 0 || tab->editor.GetSectionText({caret.line, caret.index - 1}, caret) != std::string(1, char(typed))) return;
    size_t first = caret.line;
    if (typed == '}') {
        // Find the line of the matching opening brace so the whole block is reindented.
        const auto before = tab->editor.GetSectionText({caret.line > 2000 ? caret.line - 2000 : 0, 0}, caret);
        const size_t base = caret.line > 2000 ? caret.line - 2000 : 0;
        int depth = 0;
        size_t lines = 0;
        bool found = false;
        for (size_t i = before.size(); i-- > 0;) {
            const char c = before[i];
            if (c == '\n') ++lines;
            else if (c == '}') ++depth;
            else if (c == '{' && --depth == 0) { found = true; break; }
        }
        if (found) first = caret.line - lines;
        if (first < base) first = base;
    }
    formatOnType(tab, first, caret.line);
}
void Application::updateFormatOnType() {
    if (!formatTypeCaret_) return;
    std::shared_ptr<Tab> tab;
    for (const auto &item : tabs_) if (item->path == formatTypeCaretPath_) tab = item;
    if (!tab || ImGui::GetTime() - formatTypeCaretTime_ > 2.) { formatTypeCaret_ = false; return; }
    if (tab->revision == formatTypeCaretRevision_) return; // Wait for the formatted text to reach the editor.
    formatTypeCaret_ = false;
    // Keep the caret after the same non-blank character, whatever whitespace moved.
    const auto text = tab->editor.GetText();
    size_t count = 0, line = 0, index = 0;
    TextEditor::DocPos target{0, 0};
    for (size_t byte = 0; byte < text.size() && count < formatTypeAnchor_;) {
        const auto c = static_cast<unsigned char>(text[byte]);
        const size_t width = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        if (c == '\n') { ++line; index = 0; }
        else {
            ++index;
            if (c != ' ' && c != '\t' && c != '\r') ++count;
        }
        byte += width;
        if (count == formatTypeAnchor_) target = {line, index};
    }
    if (count == formatTypeAnchor_ && tab->path == active_) tab->editor.SetCursor(target);
}
void Application::reformatDialog() {
    std::shared_ptr<Tab> tab;
    for (const auto &item : tabs_) if (item->path == active_) tab = item;
    const bool selection = tab && !tab->image && !tab->pagedProfile && tab->editor.MainCursorHasSelection();
    if (reformatDialogOpen_) { ImGui::OpenPopup(tr("Reformat file")); reformatDialogOpen_ = false; reformatSelection_ = selection; }
    if (!ImGui::BeginPopupModal(tr("Reformat file"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (tab) cyui::textDim("%s", tab->displayPath.c_str());
    cyui::sectionTitle(tr("Scope"));
    {
        cyui::ScopedDisabled disabled(!selection);
        if (ImGui::RadioButton(tr("Selected text"), reformatSelection_ && selection)) reformatSelection_ = true;
    }
    if (ImGui::RadioButton(tr("Whole file"), !reformatSelection_ || !selection)) reformatSelection_ = false;
    cyui::sectionTitle(tr("Options"));
    if (ImGui::Checkbox(tr("Reformat code on save"), &reformatOnSave_)) { codePreferencesDirty_ = true; codePreferencesChanged_ = ImGui::GetTime(); ++optionsRevision_; }
    cyui::tooltip(tr("Saving a modified file first applies the language server formatting, like Rider's Actions on Save."));
    if (ImGui::Checkbox(tr("Auto-format while typing"), &formatOnType_)) { codePreferencesDirty_ = true; codePreferencesChanged_ = ImGui::GetTime(); ++optionsRevision_; }
    cyui::tooltip(tr("Reformats the block on }, the statement on ; and pasted code with the language server, like Rider."));
    cyui::textFaint("%s", tr("Formatting uses the language server configured for this file."));
    cyui::space(6);
    const bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
    {
        cyui::ScopedDisabled disabled(!tab || refactorFuture_.valid() || !commandEnabled("refactor.format"));
        if ((cyui::primaryButton(tr("Reformat")) || enter) && tab && !refactorFuture_.valid() && commandEnabled("refactor.format")) {
            beginFormat(reformatSelection_ && selection);
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (cyui::button(tr("Cancel")) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
void Application::requestSave(const std::shared_ptr<Tab> &tab) {
    if (!tab) return;
    const bool eligible = reformatOnSave_ && engine_ && engine_->supportsLanguage(tab->path) && tab->dirty && !tab->readOnly && !tab->image && !tab->pagedProfile &&
                          !isUntitledDocument(tab->path) && !tab->path.starts_with("debug://") && !tab->path.starts_with("sdk://");
    if (!eligible) { save(tab); return; }
    if (formatSavePath_ != tab->path && std::find(formatSaveQueue_.begin(), formatSaveQueue_.end(), tab->path) == formatSaveQueue_.end())
        formatSaveQueue_.push_back(tab->path);
}
void Application::updateFormatOnSave() {
    auto find = [&](const std::string &path) {
        std::shared_ptr<Tab> found;
        for (const auto &tab : tabs_) if (tab->path == path) found = tab;
        return found;
    };
    // Applied formatting reaches the tab's revision on the frame after changes.apply.
    if (formatSaveReady_) {
        formatSaveReady_ = false;
        if (const auto tab = find(std::exchange(formatSavePath_, {}))) save(tab);
    }
    if (!formatSavePath_.empty() || refactorFuture_.valid()) return;
    while (!formatSaveQueue_.empty()) {
        const auto path = formatSaveQueue_.front(); formatSaveQueue_.pop_front();
        const auto tab = find(path);
        if (!tab) continue;
        if (beginFormat(false, path)) { formatSavePath_ = path; return; }
        save(tab);
    }
}
void Application::renamePopup() {
    if (renameOpen_) { ImGui::OpenPopup(tr("Rename symbol")); renameOpen_ = false; }
    if (!ImGui::BeginPopupModal(tr("Rename symbol"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(cyui::dp(400));
    bool enter = ImGui::InputText("##rename-symbol", &refactorName_, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
    if (unreal_.value("available", false)) ImGui::TextWrapped("%s", tr("Renames C++ references. Blueprint assets and serialized Unreal names are not modified."));
    ImGui::BeginDisabled(refactorName_.empty() || refactorFuture_.valid());
    if (cyui::primaryButton(tr("Preview changes")) || (enter && !refactorName_.empty())) {
        renameParameters_["newName"] = refactorName_;
        changeTask("language.rename", renameParameters_); ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled(); ImGui::SameLine();
    if (cyui::button(tr("Cancel"))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
void Application::selectChangeFile(const std::string &path, uint64_t beforeOffset, uint64_t afterOffset) {
    if (!refactorPlan_.is_object() || refactorFuture_.valid()) return;
    refactorFile_ = path; changeTask("changes.preview", {{"id", refactorPlan_.at("id")}, {"path", path},
        {"beforeOffset", beforeOffset}, {"afterOffset", afterOffset}, {"length", 65536}});
}
void Application::updateSemantic() {
    if (completed(symbolResolveFuture_)) {
        try {
            auto reply=symbolResolveFuture_.get();
            if (reply.at("key")==symbolWantedKey_ && reply.at("selection")==symbolResolveKey_) {
                if (reply.contains("error")) { symbolError_=reply.at("error"); symbolResolveKey_.clear(); }
                else symbolResolved_=std::move(reply);
            }
        } catch (const std::exception &e) {
            if (!symbolResolveKey_.empty()) { symbolError_=e.what(); symbolResolveKey_.clear(); }
        }
    }
    if (completed(symbolFuture_)) {
        try {
            auto result = symbolFuture_.get();
            if (result.at("key") == symbolWantedKey_) {
                symbolSentKey_ = symbolWantedKey_;
                if (result.contains("error")) { symbolError_ = result.at("error"); }
                else {
                symbolResults_ = std::move(result.at("result")); symbolError_.clear();
                if (symbolResults_.at("symbols").empty() && symbolAttempts_ < 6) { symbolSentKey_.clear(); symbolChanged_ = ImGui::GetTime()+.7; }
                }
            }
        } catch (const std::exception &e) {
            if (symbolSentKey_ == symbolWantedKey_) symbolError_ = e.what();
        }
    }
    updateFormatOnSave();
    updateFormatOnType();
    if (!completed(refactorFuture_)) return;
    const bool formatType = !formatTypePath_.empty();
    // Reformat on save never interrupts saving: unsupported files, review-only
    // plans and language-server errors fall back to saving the text unchanged.
    const bool formatSave = !formatSavePath_.empty();
    try {
        auto result = refactorFuture_.get();
        const auto method = refactorOperation_;
        if (formatType && (method == "language.format" || method == "changes.apply")) {
            std::shared_ptr<Tab> tab;
            for (const auto &item : tabs_) if (item->path == formatTypePath_) tab = item;
            if (method == "language.format") {
                // Apply only if nothing was typed since the request.
                if (tab && tab->revision == formatTypeRevision_ && !result.at("files").empty() &&
                    result.value("annotations", Json::array()).empty())
                    changeTask("changes.apply", {{"id", result.at("id")}});
                else formatTypePath_.clear();
                return;
            }
            formatTypePath_.clear();
            if (tab) {
                const auto caret = tab->editor.GetMainCursorPosition();
                size_t count = 0;
                for (const unsigned char c : tab->editor.GetSectionText({0, 0}, caret))
                    if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && (c < 0x80 || c >= 0xc0)) ++count;
                formatTypeAnchor_ = count; formatTypeCaret_ = true; formatTypeCaretPath_ = tab->path;
                formatTypeCaretRevision_ = tab->revision; formatTypeCaretTime_ = ImGui::GetTime();
            }
            return;
        }
        if (formatSave && method == "language.format") {
            if (!result.at("files").empty() && result.value("annotations", Json::array()).empty()) changeTask("changes.apply", {{"id", result.at("id")}});
            else formatSaveReady_ = true;
            return;
        }
        if (formatSave && method == "changes.apply") { refactorPlan_ = result; formatSaveReady_ = true; return; }
        if(method=="changes.historyStatus")nativeHistory_=std::move(result);
        else if (method == "changes.preview") {
            refactorPreview_ = result;
            beforePreviewCode_.setText(result.at("before").get<std::string>());
            afterPreviewCode_.setText(result.at("after").get<std::string>());
            refactorDiff_.setTexts(result.at("before"), result.at("after"), cyui::codeLanguageForPath(refactorFile_));
            refactorDiffState_ = {};
            for (auto *view : {&beforePreviewCode_, &afterPreviewCode_}) {
                configureCodeEditor(*view); view->setLanguageForPath(refactorFile_);
                view->setSyntaxEnabled(!languageUsesLegacyLexer(refactorFile_)); view->setReadOnly(true);
            }
            for (auto editor : {&beforePreview_, &afterPreview_}) {
                if (languageUsesLegacyLexer(refactorFile_)) editor->SetLanguage(editorLanguage(refactorFile_,unreal_.value("available",false))); editor->SetReadOnlyEnabled(true);
                Syntax syntax; syntax.update(*editor,refactorFile_,editor->GetText());
                editor->SetShowMiniMapEnabled(false); editor->SetWordWrapEnabled(true);
                editor->SetShowWhitespacesEnabled(false); editor->SetShowCurrentLineHighlightEnabled(false);
                editor->SetCaretsVisible(false); editor->SetLineSpacing(1.15f);
            }

        } else if (method == "language.format" || method == "language.rename" || method=="language.codeActionPrepare" || method=="changes.historyPrepare") {
            refactorPlan_ = result; refactorPreview_ = nullptr;
            if (result.at("files").empty()) { cyui::notify(tr("No changes required"), cyui::Tone::neutral); return; }
            if ((method == "language.format" || (method=="language.codeActionPrepare" && codeActionApply_)) &&
                result.value("annotations",Json::array()).empty()) changeTask("changes.apply", {{"id", result.at("id")}});
            else { showPanel("Refactoring", "refactoring"); selectChangeFile(result.at("files")[0].at("path")); }
        } else if (method == "changes.apply" || method == "changes.undo") {
            refactorPlan_ = result;
            // Existing tabs receive transactions in update(); newly affected files
            // become visible without saving the native live-buffer changes.
            auto previous = active_;
            for (const auto &file : result.at("files")) open(file.at("path"), 0, 0, false, false);
            if (!previous.empty()) open(previous);
            cyui::notify(tr(method == "changes.undo" ? "Refactoring undone" : "Changes applied to live buffers"), cyui::Tone::success);
        }
    } catch (const std::exception &e) {
        if (formatSave) formatSaveReady_ = true;
        else if (formatType) formatTypePath_.clear();
        else error(e.what());
    }
}
void Application::refactorPanel() {
    if (!beginPanel("Refactoring", "refactoring")) return;
    nativeHistoryControls();
    if (!refactorPlan_.is_object()) {
        cyui::emptyState(tr("Refactoring"), tr("Rename a C++ symbol to preview changes across its references."), cyui::icon::edit);
        ImGui::End(); return;
    }
    cyui::textDim("%s", refactorPlan_.value("title", "").c_str());
    const auto state = refactorPlan_.value("state", "preview");
    const auto planId=refactorPlan_.at("id").get<std::string>();
    if(refactorAnnotationPlan_!=planId) {refactorAnnotationPlan_=planId;refactorConfirmed_.clear();}
    const auto annotations=refactorPlan_.value("annotations",Json::array());
    bool confirmed=true;
    if(!annotations.empty()) {
        const bool expanded=cyui::beginSection(tr("Change annotations"));
        for(const auto &annotation:annotations) {
            const auto id=annotation.at("id").get<std::string>();
            const bool required=annotation.at("needsConfirmation").get<bool>();
            if(expanded) {
                cyui::ScopedID annotationId(id.c_str());
                if(required) {
                    bool checked=refactorConfirmed_.contains(id);
                    {cyui::ScopedDisabled disabled(refactorFuture_.valid() || state!="preview");
                     if(ImGui::Checkbox("##confirm",&checked)) {
                         if(checked)refactorConfirmed_.insert(id);else refactorConfirmed_.erase(id);
                     }}
                    cyui::tooltip(tr("Confirm this annotated change"));ImGui::SameLine();
                }
                const auto &label=annotation.at("label").get_ref<const std::string &>();
                const bool details=ImGui::TreeNodeEx("##details",ImGuiTreeNodeFlags_SpanAvailWidth,"%s",label.c_str());
                if(ImGui::IsItemHovered())cyui::tooltip(label.c_str());
                if(details) {
                    const auto description=annotation.at("description").get<std::string>();
                    if(!description.empty())ImGui::TextWrapped("%s",description.c_str());
                    for(const auto &path:annotation.at("files"))cyui::textDim("%s",path.get_ref<const std::string &>().c_str());
                    ImGui::TreePop();
                }
            }
            if(required && !refactorConfirmed_.contains(id))confirmed=false;
        }
        if(expanded)cyui::endSection();
        if(!confirmed && state=="preview")cyui::textDim("%s",tr("Confirm the required annotations before applying."));
    }
    ImGui::BeginDisabled(refactorFuture_.valid() || state != "preview" || refactorPlan_.at("files").empty() || !confirmed);
    if (cyui::primaryButton(tr("Apply changes"))) changeTask("changes.apply", {{"id", refactorPlan_.at("id")},{"confirmedAnnotations",refactorConfirmed_}});
    ImGui::EndDisabled(); ImGui::SameLine();
    ImGui::BeginDisabled(refactorFuture_.valid() || state != "applied" || refactorPlan_.contains("history"));
    if (cyui::button(tr("Undo refactoring"))) changeTask("changes.undo", {{"id", refactorPlan_.at("id")}});
    ImGui::EndDisabled(); ImGui::SameLine();
    if (refactorFuture_.valid()) cyui::spinner("##refactoring");
    else cyui::textDim("%zu %s | %zu %s", refactorPlan_.at("files").size(), tr("files"), refactorPlan_.at("editCount").get<size_t>(), tr("edits"));
    if (state == "applied") cyui::textDim("%s", tr("Changes are in live buffers. Save all to write them to disk."));
    if (ImGui::BeginCombo("##change-file", refactorFile_.c_str())) {
        for (const auto &file : refactorPlan_.at("files")) {
            const auto path = file.at("path").get<std::string>();
            if (ImGui::Selectable(path.c_str(), path == refactorFile_)) selectChangeFile(path);
        }
        ImGui::EndCombo();
    }
    if (refactorPreview_.is_object()) {
        if (refactorPreview_.contains("beforeOffset")) {
            const auto beforeOffset = refactorPreview_.value("beforeOffset", uint64_t(0));
            const auto afterOffset = refactorPreview_.value("afterOffset", uint64_t(0));
            const bool partial = beforeOffset || afterOffset || refactorPreview_.value("hasMore", false);
            if (partial) {
                cyui::callout(cyui::Tone::neutral, tr("Partial byte preview. Apply and Undo affect the complete native change plan."));
                cyui::textDim("%s: %llu / %llu | %s: %llu / %llu", tr("Before"),
                    static_cast<unsigned long long>(beforeOffset),
                    static_cast<unsigned long long>(refactorPreview_.value("beforeBytes",uint64_t(0))), tr("After"),
                    static_cast<unsigned long long>(afterOffset),
                    static_cast<unsigned long long>(refactorPreview_.value("afterBytes",uint64_t(0))));
                { cyui::ScopedDisabled disabled(refactorFuture_.valid() || (!beforeOffset && !afterOffset));
                  if (cyui::button(tr("First preview page"))) selectChangeFile(refactorFile_); }
                ImGui::SameLine();
                { cyui::ScopedDisabled disabled(refactorFuture_.valid() || !refactorPreview_.value("hasMore",false));
                  if (cyui::button(tr("Next preview page"))) selectChangeFile(refactorFile_,
                      refactorPreview_.at("nextBeforeOffset"), refactorPreview_.at("nextAfterOffset")); }
            }
        }
        ImGui::Checkbox(tr("Wrap lines"), &previewWrap_);
        if (!previewWrap_) drawDiff("##change-preview", refactorDiffState_, refactorDiff_);
        else
        if (ImGui::BeginTable("##change-preview", 2, ImGuiTableFlags_Resizable)) {
            ImGui::TableNextColumn(); ImGui::TextUnformatted(tr("Before"));
            { cyui::ScopedFont mono(cyui::monoFont()); drawCode(beforePreviewCode_,"##before-preview"); }
            ImGui::TableNextColumn(); ImGui::TextUnformatted(tr("After"));
            { cyui::ScopedFont mono(cyui::monoFont()); drawCode(afterPreviewCode_,"##after-preview"); }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
} // namespace cycode
