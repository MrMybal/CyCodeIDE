#include "cycode/Application.h"
#include <algorithm>
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
void Application::newUntitled() {
    const auto doc = engine_->dispatch("documents.createUntitled");
    open(doc.at("path"));
}
void Application::beginSaveAs(const std::shared_ptr<Tab> &tab) {
    if (!tab || tab->readOnly || tab->image)
        return;
    if (std::find(saveAsQueue_.begin(), saveAsQueue_.end(), tab->identity) == saveAsQueue_.end() &&
        saveAsDocument_ != tab->identity)
        saveAsQueue_.push_back(tab->identity);
}
void Application::documentDraftDialogs() {
    if (!saveAsDocument_ && !saveAsQueue_.empty()) {
        saveAsDocument_ = saveAsQueue_.front();
        saveAsQueue_.erase(saveAsQueue_.begin());
        saveAsOpen_ = true;
        saveAsError_.clear();
        saveAsDestination_.clear();
        for (const auto &tab : tabs_)
            if (tab->identity == saveAsDocument_) {
                if (isUntitledDocument(tab->path))
                    saveAsDestination_ = untitledDocumentName(tab->path);
                else {
                    auto p = utf8Path(tab->path);
                    saveAsDestination_ =
                        pathUtf8(p.parent_path() / (pathUtf8(p.stem()) + "-copy" + pathUtf8(p.extension())));
                }
            }
    }
    if (!saveAsDocument_)
        return;
    const auto found = std::find_if(tabs_.begin(), tabs_.end(),
                                    [&](const auto &tab) { return tab->identity == saveAsDocument_; });
    if (found == tabs_.end()) {
        if (saveAsOpen_) {
            const auto title = label("Save as...", "save-document-as");
            if (cyui::beginModal(title.c_str(), &saveAsOpen_, 560)) {
                ImGui::CloseCurrentPopup();
                cyui::endModal();
            }
        }
        saveAsDocument_ = 0;
        saveAsOpen_ = false;
        return;
    }
    auto tab = *found;
    bool committed = false;
    const auto title = label("Save as...", "save-document-as");
    if (cyui::beginModal(title.c_str(), &saveAsOpen_, 560)) {
        cyui::textDim("%s", tab->displayPath.c_str());
        bool destinationFocused = false;
        if (cyui::beginProperties("##save-as-properties")) {
            cyui::property(tr("New workspace file"));
            ImGui::InputText("##save-as-destination", &saveAsDestination_);
            destinationFocused = ImGui::IsItemActive() || ImGui::IsItemFocused();
            cyui::endProperties();
        }
        cyui::textDim("%s",
                      tr("Choose a new file in an existing workspace folder. Existing files are preserved."));
        if(tab->pagedProfile)cyui::callout(cyui::Tone::warning,tr("Save As publishes the complete paged document to a new file and resets native history for the old path."));
        if (!saveAsError_.empty())
            cyui::callout(cyui::Tone::error, saveAsError_.c_str());
        if (cyui::primaryButton(tr("Save")) ||
            (destinationFocused && ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
            try {
                if(tab->pagedProfile) {
                    if(!tab->paged || !tab->paged->save(utf8Path(saveAsDestination_)))throw std::runtime_error(tr("Finish pending input and native operations before saving."));
                    tab->pagedCloseAfterSave=pendingClose_==tab->path;
                    saveAsOpen_=false;committed=true;ImGui::CloseCurrentPopup();
                } else {
                const auto result =
                    engine_->dispatch("documents.saveAs", {{"path", tab->path},
                                                           {"destination", saveAsDestination_},
                                                           {"expectedRevision", tab->revision}});
                saveAsError_.clear();
                const auto old = tab->path;
                resourceTabPath(tab,
                                result.at("path")); // Keeps the editor object, Undo and OS window identity.
                tab->revision = result.at("revision");
                tab->dirty = false;
                tab->diskConflict.clear();
                tab->synchronizedRevision = tab->revision;
                tab->synchronizedText = result.at("text").get<std::string>();
                open(tab->path); // Apply the existing saved-file language activation policy.
                for (auto *items : {&recentFiles_, &closedFiles_})
                    std::erase(*items, old);
                for (auto *history : {&historyBack_, &historyForward_})
                    for (auto &position : *history)
                        if (position.path == old)
                            position.path = tab->path;
                std::erase(recentFiles_, tab->path);
                recentFiles_.insert(recentFiles_.begin(), tab->path);
                if (recentFiles_.size() > 100)
                    recentFiles_.resize(100);
                const bool closing = pendingClose_ == tab->path;
                if (closing) {
                    const auto path = tab->path;
                    pendingClose_.clear();
                    closeTab(path);
                }
                for (const auto &warning : result.at("warnings"))
                    cyui::notify(warning.get_ref<const std::string &>().c_str(), cyui::Tone::warning);
                saveAsOpen_ = false;
                committed = true;
                ImGui::CloseCurrentPopup();
                refreshFiles(false);
                refreshGit();
                cyui::notify(tr("Saved"), cyui::Tone::success);
                }
            } catch (const std::exception &e) {
                saveAsError_ = e.what();
            }
        }
        ImGui::SameLine();
        if (cyui::button(tr("Cancel")) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            saveAsOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        cyui::endModal();
    }
    if (!saveAsOpen_) {
        saveAsDocument_ = 0;
        if (!committed) {
            saveAsQueue_.clear();
            pendingClose_.clear();
            closeAll_ = false;
            quitRequested_ = false;
        }
    }
}
} // namespace cycode
