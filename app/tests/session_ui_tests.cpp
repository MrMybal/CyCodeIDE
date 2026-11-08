#include "cycode/Application.h"
#include "cycode/WorkspaceSession.h"
#include <fstream>
#include <stdexcept>

namespace cycode {
struct ApplicationSessionTests {
    static void check(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }
    static void run(const std::filesystem::path &repository) {
        namespace fs = std::filesystem;
        const auto root =
            repository / "runtime" /
            ("session-ui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root / "workspace");
        const std::string text = "int main() {\n  int a=0;\n  if (a) {\n    ++a;\n  }\n  return a;\n}\n";
        std::ofstream(root / "workspace/example.cpp") << text;
        {
            LaunchOptions options;
            options.installation = root;
            options.workspace = root / "workspace";
            options.smoke = true;
            options.testSession = true;
            options.disablePlugins = true;
            Application app(options, nullptr);
            app.open("example.cpp");
            const auto tab = app.tabs_.front();
            auto frame = [&] {
                ImGui::NewFrame();
                app.editorPanel();
                ImGui::Render();
            };
            frame();
            frame();
            check(tab->editor.IsLineFoldable(0), "Shared backend has no fold opportunity after render");
            tab->editor.SetCursor({0, 0});
            tab->editor.ToggleAtLine(0);
            tab->editor.SetWordWrapEnabled(true);
            frame();
            const auto undo = tab->editor.GetUndoIndex();
            const auto saved = app.workspaceSessionSnapshot();
            const auto item = saved.at("tabs")[0];
            check(item.at("editor").at("wrap") && item.at("editor").at("folds") == Json::array({0}),
                  "Snapshot lost wrap or actual folded lines");
            check(validateWorkspaceSession(saved, saved.at("workspace")).at("tabs") == saved.at("tabs"),
                  "UI metadata does not satisfy the native record contract");
            tab->editor.UnfoldAll();
            tab->editor.SetWordWrapEnabled(false);
            frame();
            app.restoreSessionTab(tab, item);
            check(app.workspaceSessionSnapshot().at("tabs")[0].at("editor") == item.at("editor"),
                  "Unrendered pending restoration was replaced by defaults");
            frame();
            frame();
            check(tab->editor.IsWordWrapEnabled() && tab->editor.IsLineFolded(0),
                  "Deferred restoration did not restore the shared editor view");
            check(tab->editor.GetText() == text && tab->editor.GetUndoIndex() == undo,
                  "Restoring editor metadata changed source text or Undo");
            tab->editor.UnfoldAll();
            frame();
            // Recovery/external synchronization may have queued widget transactions
            // before this frame. They must not look like fresh user input.
            tab->editor.SelectAll();
            tab->editor.ReplaceTextPreservingTabs(text + "\n");
            tab->editor.SelectAll();
            tab->editor.ReplaceTextPreservingTabs(text);
            const auto preparedUndo = tab->editor.GetUndoIndex();
            app.restoreSessionTab(tab, item);
            frame();
            frame();
            check(tab->editor.IsLineFolded(0) && tab->editor.GetUndoIndex() == preparedUndo,
                  "Previously committed widget transactions cancelled view restoration");
            tab->editor.UnfoldAll();
            frame();
            auto stale = item;
            stale["editor"]["fingerprint"] = "different-source-snapshot";
            app.restoreSessionTab(tab, stale);
            frame();
            frame();
            check(!tab->editor.IsLineFolded(0), "Stale source fingerprint restored folds");
            app.restoreSessionTab(tab, item);
            app.editorFocused_ = true;
            app.executeCommand("view.wrap");
            check(tab->pendingSession.is_null(), "Explicit editor command retained a delayed restore");
            for (const auto *theme : {"graphite", "light-grey", "navy"})
                for (float dpi : {1.f, 1.5f, 2.f}) {
                    for (const auto density : {cyui::Density::compact, cyui::Density::comfortable}) {
                        cyui::Config config;
                        config.themeKey = theme;
                        config.dpiScale = dpi;
                        config.density = density;
                        cyui::apply(config);
                        tab->editor.UnfoldAll();
                        frame();
                        app.restoreSessionTab(tab, item);
                        frame();
                        frame();
                        check(tab->editor.IsLineFolded(0), "Fold restoration failed after theme/DPI change");
                    }
                }
        }
        fs::remove_all(root);
    }
};
void sessionViewContracts(const std::filesystem::path &repository) {
    ApplicationSessionTests::run(repository);
}
} // namespace cycode
