#include "cycode/Application.h"
#include "cycode/ImageView.h"
#include "cycode/Recovery.h"
#include "cycode/WorkspaceSession.h"
#include <algorithm>

namespace cycode {
Json Application::editorSessionState(const std::shared_ptr<Tab> &tab, size_t &lineBudget,
                                     size_t &hashBudget) const {
    auto &editor = tab->editor;
    const auto anchor = tab->sessionAnchorLine;
    Json result = {{"version", 1},
                   {"wrap", editor.IsWordWrapEnabled()},
                   {"anchorLine", std::min<size_t>(anchor, 10000000)},
                   {"folds", Json::array()},
                   {"foldsComplete", false},
                   {"fingerprint", ""}};
    const auto count = editor.GetLineCount();
    // Public backend queries only: bounded checkpoint work on the GUI thread.
    if (!tab->sessionRendered || count > 200000 || count > lineBudget ||
        tab->synchronizedRevision != tab->revision)
        return result;
    lineBudget -= count;
    auto &folds = result.at("folds");
    for (size_t line = 0; line < count; ++line)
        if (editor.IsLineFolded(line)) {
            if (folds.size() == sessionMaxFolds) {
                folds = Json::array();
                return result;
            }
            folds.push_back(line);
        }
    if (!folds.empty()) {
        if (tab->sessionTextRevision != editor.GetTextRevision() ||
            tab->sessionTextIdentity != tab->identity || tab->sessionTextFingerprint.empty()) {
            if (!hashBudget || tab->synchronizedText.size() > hashBudget) {
                folds = Json::array();
                return result;
            }
            const auto text = editor.GetText();
            if (text.size() > hashBudget) {
                hashBudget = 0;
                folds = Json::array();
                return result;
            }
            hashBudget -= text.size();
            tab->sessionTextFingerprint = RecoveryStore::fingerprint(text);
            tab->sessionTextRevision = editor.GetTextRevision();
            tab->sessionTextIdentity = tab->identity;
        }
        result["fingerprint"] = tab->sessionTextFingerprint;
    }
    result["foldsComplete"] = true;
    return result;
}
void Application::restoreSessionTab(const std::shared_ptr<Tab> &tab, const Json &item) {
    const auto window = item.at("windowKey").get<std::string>();
    if (std::none_of(tabs_.begin(), tabs_.end(),
                     [&](const auto &other) { return other != tab && other->windowKey == window; }))
        tab->windowKey = window;
    if (tab->image) {
        if (item.contains("image"))
            tab->image->restoreSession(item.at("image"));
        return;
    }
    if (item.contains("editor")) {
        (void)validateSessionEditor(item.at("editor"));
        tab->editor.SetWordWrapEnabled(item.at("editor").at("wrap").get<bool>());
        tab->pendingSession = item;
    }
    tab->editor.SetCursor({item.at("line").get<size_t>(), item.at("column").get<size_t>()});
    const auto anchor = item.contains("editor") ? item.at("editor").at("anchorLine") : item.at("scroll");
    tab->editor.ScrollToLine(anchor.get<size_t>(), TextEditor::Scroll::alignTop);
}
void Application::applySessionEditor(const std::shared_ptr<Tab> &tab, bool changed) {
    if (!ImGui::IsItemVisible())
        return;
    if (tab->editor.GetLineHeight() > 0) {
        tab->sessionAnchorLine = tab->editor.VisPos2DocPos({tab->editor.GetFirstVisibleRow(), 0}).line;
        tab->sessionRendered = true;
    }
    if (!tab->pendingSession.is_object())
        return;
    const auto item = std::move(tab->pendingSession);
    tab->pendingSession = nullptr;
    // Commands/navigation and input take precedence over delayed restoration.
    const auto &io = ImGui::GetIO();
    bool input = changed;
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        input |= io.InputQueueCharacters.Size || io.MouseWheel || io.MouseWheelH;
        for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END && !input; ++key)
            input = ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false);
    }
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        for (int button = 0; button < 5; ++button)
            input |= ImGui::IsMouseClicked(button);
    if (input)
        return;
    const auto &view = item.at("editor");
    if (view.at("foldsComplete").get<bool>() && !view.at("folds").empty() &&
        view.at("fingerprint") == RecoveryStore::fingerprint(tab->editor.GetText())) {
        // Folding opportunities exist after the first real render. Descending
        // starts preserve the backend's own visible/nested-fold behavior.
        for (auto line = view.at("folds").rbegin(); line != view.at("folds").rend(); ++line) {
            const auto at = line->get<size_t>();
            if (at < tab->editor.GetLineCount() && tab->editor.IsLineFoldable(at))
                tab->editor.ToggleAtLine(at);
        }
    }
    tab->editor.SetCursor({item.at("line").get<size_t>(), item.at("column").get<size_t>()});
    tab->editor.ScrollToLine(view.at("anchorLine").get<size_t>(), TextEditor::Scroll::alignTop);
}
} // namespace cycode
