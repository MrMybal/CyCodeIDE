#include "cycode/Application.h"
#include "cycode/WorkspaceSession.h"
#include "cycode/ImageView.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cyui/Core.h>

namespace cycode {
namespace {
bool sessionPath(const std::string &path) {
    if (isUntitledDocument(path)) return true;
    const auto value = utf8Path(path);
    if (path.empty() || path.find('\0') != std::string::npos || path.size() > 8192 || value.is_absolute() ||
        value.has_root_name() || path.find_first_of("\\:\r\n") != std::string::npos)
        return false;
    for (const auto &part : value)
        if (part == ".." || part == ".")
            return false;
    return true;
}
} // namespace
void Application::beginWorkspaceSession(const std::filesystem::path &legacy, const std::string &key) {
    workspaceSession_.reset();
    sessionRestoreQueue_.clear();
    sessionDraftTabs_.clear();
    sessionRestoreActive_.clear();
    sessionSaveError_.clear();
    sessionRestoreFocus_ = true;
    sessionRestoreLastActive_ = active_;
    lastSessionSave_ = ImGui::GetTime();
    layoutIni_ = pathUtf8(legacy);
    const auto loadLegacy = [&] {
        std::error_code error;
        if (std::filesystem::is_regular_file(legacy, error))
            ImGui::LoadIniSettingsFromDisk(pathUtf8(legacy).c_str());
    };
    try {
        const auto id =
            instance_ + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        workspaceSession_ = std::make_shared<WorkspaceSession>(
            legacy.parent_path() / (pathUtf8(legacy.stem()) + ".sessions"), key, id);
        const auto saved = workspaceSession_->latest();
        layoutIni_ = pathUtf8(workspaceSession_->layoutPath());
        if (restoreWorkspaceSession_ && !saved.is_null()) {
            const auto ini = saved.at("layout").get<std::string>();
            ImGui::LoadIniSettingsFromMemory(ini.data(), ini.size());
            for (const auto &tab : saved.at("tabs"))
                sessionRestoreQueue_.push_back(tab);
            sessionRestoreActive_ = saved.at("active");
            recentFiles_ = saved.at("recent").get<std::vector<std::string>>();
            if (saved.contains("git")) {
                const auto selection = validateSessionGit(saved.at("git"));
                const auto folder = selection.at("root").get<std::string>();
                if (!folder.empty()) {
                    try {
                        const auto current = gitRepositoryAt(engine_->workspace(), engine_->resolve(folder), false);
                        if (!current.marker || current.identity != selection.at("identity").get<std::string>())
                            throw std::runtime_error("Saved Git identity changed");
                        vcsRepository_ = folder;
                        vcsRepositoryIdentity_ = selection.at("identity");
                    } catch (const std::exception &) {
                        vcsRepository_.clear(); vcsRepositoryIdentity_.clear();
                        cyui::notify(tr("Saved Git repository is unavailable or changed. Using automatic selection."), cyui::Tone::warning);
                    }
                }
            }
            std::erase_if(recentFiles_,[](const auto &path){return isUntitledDocument(path);});
            for (const auto &[panel, visible] : saved.at("panels").items())
                panels_[panel] = visible.get<bool>();
        } else
            loadLegacy();
    } catch (const std::exception &e) {
        workspaceSession_.reset();
        sessionSaveError_ = e.what();
        cyui::notify(e.what(), cyui::Tone::warning);
        loadLegacy();
    }
    ImGui::GetIO().IniFilename = layoutIni_.c_str();
}
Json Application::workspaceSessionSnapshot() const {
    Json records = Json::array(), recent = Json::array(), panels = Json::object();
    size_t lineBudget = 1000000, hashBudget = 32 * 1024 * 1024, foldBudget = sessionMaxTotalFolds;
    std::set<std::string> paths, windows;
    auto add = [&](Json item) {
        const auto path = item.at("path").get<std::string>();
        if (records.size() >= 256 || !sessionPath(path) || !paths.insert(path).second)
            return;
        auto window = item.value("windowKey", path);
        if (window.empty())
            window = path;
        while (!windows.insert(window).second)
            window = path + ":session:" + std::to_string(windows.size());
        item["windowKey"] = window;
        if (item.contains("editor")) {
            auto &view = item.at("editor");
            const auto count = view.at("folds").size();
            if (count > foldBudget) {
                view["folds"] = Json::array();view["foldsComplete"] = false;view["fingerprint"] = "";
            } else foldBudget -= count;
        }
        records.push_back(std::move(item));
    };
    for (const auto &tab : tabs_) {
        if (!sessionPath(tab->path)) continue;
        if (tab->pendingSession.is_object()) {
            auto item = tab->pendingSession;
            item["path"] = tab->path;item["windowKey"] = tab->windowKey;
            add(std::move(item));continue;
        }
        const auto cursor = tab->image ? TextEditor::DocPos{} : tab->editor.GetMainCursorPosition();
        Json item = {{"path", tab->path},
             {"windowKey", tab->windowKey},
             {"line", std::min<size_t>(cursor.line, 10000000)},
             {"column", std::min<size_t>(cursor.index, 1000000)},
             {"scroll", std::min<size_t>(tab->image ? 0 : tab->editor.GetFirstVisibleRow(), 10000000)}};
        if(tab->pagedProfile) {
            item["paged"]=true;item["pagedView"]=pagedSessionState(tab);
            if(tab->paged){const auto p=tab->paged->view().selection().caret;
                item["line"]=std::min<uint64_t>(p.line,10000000);item["column"]=std::min<uint64_t>(p.character,1000000);
                item["scroll"]=std::min<uint64_t>(tab->paged->view().firstLine(),10000000);}
            else if(tab->pagedLocation.is_object()) {
                item["line"]=std::min<uint64_t>(tab->pagedLocation.value("line",uint64_t(1))-1,10000000);
                item["column"]=std::min<uint64_t>(tab->pagedLocation.value("column",uint64_t(1))-1,1000000);
                item["scroll"]=std::min<uint64_t>(tab->pagedScroll.value_or(0),10000000);
            }
        }
        else if (tab->image) item["image"] = tab->image->sessionState();
        else item["editor"] = editorSessionState(tab,lineBudget,hashBudget);
        add(std::move(item));
    }
    for (const auto &tab : sessionRestoreQueue_)
        add(tab);
    for (const auto &[path, tab] : sessionDraftTabs_) add(tab);
    for (const auto &path : recentFiles_)
        if (sessionPath(path) && !isUntitledDocument(path) && recent.size() < 100)
            recent.push_back(path);
    for (const auto &[key, visible] : panels_)
        if (panels.size() < 512)
            panels[key] = visible;
    auto active = paths.contains(active_) ? active_ : std::string{};
    if (!sessionRestoreQueue_.empty() && sessionRestoreFocus_ && paths.contains(sessionRestoreActive_))
        active = sessionRestoreActive_;
    size_t bytes = 0;
    const auto ini = ImGui::SaveIniSettingsToMemory(&bytes);
    if (bytes > 512 * 1024)
        throw std::runtime_error("Workspace layout exceeds its 512 KiB session limit");
    auto key = pathUtf8(entry_);
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
#endif
    return {{"schemaVersion", 1},
            {"workspace", key},
            {"tabs", records},
            {"recent", recent},
            {"active", active},
            {"panels", panels},
            {"git", {{"version", 1}, {"root", vcsRepositoryIdentity_.empty() ? std::string{} : vcsRepository_},
                     {"identity", vcsRepository_.empty() ? std::string{} : vcsRepositoryIdentity_}}},
            {"layout", std::string(ini, bytes)}};
}
void Application::saveWorkspaceSession(bool closed) {
    if (sessionSaveFuture_.valid())
        sessionSaveFuture_.get();
    if (workspaceSession_)
        workspaceSession_->save(workspaceSessionSnapshot(), closed);
}
void Application::updateWorkspaceSession() {
    if (smoke_ || testSession_ || !workspaceSession_)
        return;
    if (sessionSaveFuture_.valid() &&
        sessionSaveFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            sessionSaveFuture_.get();
            sessionSaveError_.clear();
        } catch (const std::exception &e) {
            if (sessionSaveError_ != e.what())
                cyui::notify(e.what(), cyui::Tone::warning);
            sessionSaveError_ = e.what();
        }
    }
    if (closeAll_)
        sessionRestoreQueue_.clear();
    if (active_ != sessionRestoreLastActive_)
        sessionRestoreFocus_ = false;
    for (int count = 0; count < 2 && !sessionRestoreQueue_.empty(); ++count) {
        const auto item = sessionRestoreQueue_.front();
        sessionRestoreQueue_.pop_front();
        const auto path = item.at("path").get<std::string>();
        if (isUntitledDocument(path)) { sessionDraftTabs_[path] = item; continue; }
        if (std::any_of(tabs_.begin(), tabs_.end(), [&](const auto &tab) { return tab->path == path; }) ||
            std::find(closedFiles_.begin(), closedFiles_.end(), path) != closedFiles_.end())
            continue;
        try {
            if (!std::filesystem::is_regular_file(engine_->resolve(path)))
                continue;
            if(item.contains("paged") && item.at("paged").is_boolean() && item.at("paged")==true)openPaged(path,0,0,false,false,true);
            else open(path, 0, 0, false, false);
            const auto found =
                std::find_if(tabs_.begin(), tabs_.end(), [&](const auto &tab) { return tab->path == path; });
            if (found == tabs_.end())
                continue;
            auto &tab = *found;
            restoreSessionTab(tab,item);
            if (active_.empty() || (sessionRestoreFocus_ && path == sessionRestoreActive_))
                active_ = focusTab_ = path;
            sessionRestoreLastActive_ = active_;
        } catch (
            const std::exception &) { /* A removed/inaccessible document must not interrupt restoration. */
        }
    }
    if (!sessionRestoreQueue_.empty() || sessionSaveFuture_.valid() ||
        ImGui::GetTime() - lastSessionSave_ < 5)
        return;
    lastSessionSave_ = ImGui::GetTime();
    try {
        auto snapshot = workspaceSessionSnapshot();
        auto store = workspaceSession_;
        sessionSaveFuture_ = std::async(std::launch::async,
                                        [store, snapshot = std::move(snapshot)] { store->save(snapshot); });
    } catch (const std::exception &e) {
        if (sessionSaveError_ != e.what())
            cyui::notify(e.what(), cyui::Tone::warning);
        sessionSaveError_ = e.what();
    }
}
} // namespace cycode
