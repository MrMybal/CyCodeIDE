#include "cycode/Application.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace cycode;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
namespace cycode {
struct ApplicationExtensionTests {
    static void run(const std::filesystem::path &repository, const std::filesystem::path &library) {
        const auto folder =
            repository / "runtime" /
            ("extension-ui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(folder / "app/locales");
        std::filesystem::create_directories(folder / "runtime/plugins");
        std::filesystem::create_directories(folder / "workspace");
        std::filesystem::copy_file(repository / "app/locales/ui.json", folder / "app/locales/ui.json");
        std::filesystem::copy_file(library, folder / "runtime/plugins" / library.filename());
        std::ofstream(folder / "runtime/plugins/native.json") << Json({{"id", "fixture.native"},
                                                                       {"abi", 2},
                                                                       {"version", "1.0.0"},
                                                                       {"library", library.stem().string()},
                                                                       {"permissions", {"workspace.write"}}})
                                                                     .dump();
        std::ofstream(folder / "workspace/main.cpp") << "int main() {}\n";
        std::ofstream(folder / "workspace/custom.cytest") << "custom_keyword 42 # commentaire\n";
        std::filesystem::create_directories(folder / "data/settings");
        std::ofstream(folder / "data/settings/ui-preferences.json")
            << Json({{"keymap", "rider"},
                     {"keymapOverrides",
                      {{"rider",
                        {{"fixture.native.inspect", {"Ctrl+Alt+F11"}},
                         {"absent.plugin.action", {"Ctrl+Alt+F12"}}}}}}})
                   .dump();
        {
            LaunchOptions options;
            options.installation = folder;
            options.workspace = folder / "workspace";
            options.smoke = true;
            options.testSession = true;
            Application app(options, nullptr);
            app.open("main.cpp");
            require(app.keymap_.hint("fixture.native.inspect") == "Ctrl+Alt+F11",
                    "Persisted plugin shortcut was discarded during startup");
            require(app.keymapOverrides_.at("rider").contains("absent.plugin.action"),
                    "Unavailable plugin shortcut was discarded");
            auto draw = [&] {
                ImGui::NewFrame();
                app.menu();
                app.extensionsPanel();
                app.gitPanel();
                app.extensionPanels();
                ImGui::Render();
                require(ImGui::GetDrawData()->TotalVtxCount > 0, "Extension view produced no drawing data");
            };
            auto settle = [&] {
                const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                for (;;) {
                    app.update();
                    draw();
                    bool pending =
                        app.taskFuture_.valid() || app.gitFuture_.valid() || app.vcsFuture_.valid() || app.filesFuture_.valid();
                    for (const auto &[id, view] : app.extensionViews_)
                        pending = pending || view.pending.valid();
                    if (!pending)
                        return;
                    if (std::chrono::steady_clock::now() > until)
                        throw std::runtime_error("Extension GUI async work did not complete");
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            };
            settle();
            require(app.commandEnabled("fixture.native.inspect"),
                    "Plugin command is missing from GUI dispatch");
            app.open("custom.cytest");
            auto syntaxTab = app.tabs_.back();
            syntaxTab->codeEditor.refreshSyntax();
            require(!cyui::codeLanguageInfo(syntaxTab->codeEditor.language()).builtin,
                    "Plugin syntax was not selected by its file association");
            const auto tokens = cyui::highlightCode("custom_keyword 42", syntaxTab->codeEditor.language());
            require(!tokens.empty() && tokens.front().role == cyui::TokenRole::keyword,
                    "Plugin syntax did not use CyUI's shared lexer");
            app.executeCommand("fixture.native.inspect");
            settle();
            require(app.output_.find("counter") != std::string::npos,
                    "Plugin command result did not reach app output");
            app.executeCommand("extension.panel.fixture.native.panel");
            settle();
            require(app.extensionViews_.at("fixture.native.panel").model.contains("properties"),
                    "Asynchronous plugin panel model was not rendered");
            auto engine = app.engine_;
            app.extensionViews_.at("fixture.native.panel").pending = std::async(std::launch::async, [engine] {
                return engine->plugins().invoke("fixture.native.panel", {{"event", "invalid-model"}});
            });
            settle();
            require(!app.extensionViews_.at("fixture.native.panel").error.empty(),
                    "Malformed plugin model reached the renderer");
            app.engine_->dispatch("vcs.configure", {{"root", "."}, {"provider", "fixture.native.depot"}});
            app.refreshGit();
            settle();
            require(app.provider_ == "fixture.native.depot", "GUI ignored persisted non-Git provider");
            require(!app.commandEnabled("vcs.branches"),
                    "Git-specific menu action remained enabled for another provider");
            require(app.commandEnabled("extension.vcs.fixture.native.depot.checkout"),
                    "Custom provider menu action is unavailable");
            app.vcsTask("vcs.diff", {{"path", "main.cpp"}});
            settle();
            if (!(app.vcsPreviewTitle_ == "vcs.diff" && app.vcsIsDiff_))
                std::cerr << Json{{"error",app.lastError_},{"operation",app.vcsOperation_},
                    {"title",app.vcsPreviewTitle_},{"provider",app.provider_},
                    {"context",app.vcsContext_},{"contextPath",app.vcsContextPath_},
                    {"requestPath",app.vcsRequestPath()}}.dump() << '\n';
            require(app.vcsPreviewTitle_ == "vcs.diff" && app.vcsIsDiff_,
                    "Alternative provider diff did not reach the shared review view");
            require(!app.vcsDiff_.hunks().empty() &&
                        std::any_of(app.vcsDiff_.rows().begin(), app.vcsDiff_.rows().end(),
                                    [](const auto &row) { return row.kind == cyui::DiffKind::added; }),
                    "Provider review did not contain the actual unified diff");
            for (const auto *locale : {"en", "fr"})
                for (const auto *theme :
                     {"graphite", "mid-grey", "light-grey", "amber-night", "violet", "navy"})
                    for (float dpi : {1.f, 1.5f, 2.f}) {
                        app.locale_ = locale;
                        cyui::setLocale(locale);
                        cyui::setTheme(theme);
                        cyui::applyDpi(dpi);
                        app.vcsSection_ = "Changes";
                        draw();
                        app.vcsSection_ = "Review";
                        draw();
                    }
            syntaxTab->editor.SetCursor({0, 0});
            syntaxTab->editor.ReplaceTextPreservingTabs("# edit\n");
            app.changed(syntaxTab);
            const auto undoIndex = syntaxTab->editor.GetUndoIndex();
            const auto textRevision = syntaxTab->revision;
            app.pluginInfo_ =
                app.engine_->dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", false}});
            app.synchronizeExtensions();
            require(!app.commandEnabled("fixture.native.inspect"),
                    "Disabled plugin command remained executable");
            require(!app.panels_.at("extension:fixture.native.panel"), "Disabled plugin panel remained open");
            draw();
            settle();
            require(cyui::codeLanguageInfo(syntaxTab->codeEditor.language()).builtin,
                    "Disabling a syntax plugin did not restore the built-in association");
            require(syntaxTab->editor.GetUndoIndex() == undoIndex && syntaxTab->revision == textRevision,
                    "Syntax activation modified the document or Undo history");
            app.active_ = syntaxTab->path;
            app.executeCommand("edit.undo");
            require(syntaxTab->editor.GetText() == "custom_keyword 42 # commentaire\n",
                    "Undo was lost while removing a syntax extension");
        }
        {
            LaunchOptions options;
            options.installation = folder;
            options.workspace = folder / "workspace";
            options.smoke = true;
            options.testSession = true;
            options.disablePlugins = true;
            Application safe(options, nullptr);
            require(safe.pluginInfo_.at("plugins").empty(), "Safe mode loaded extension binaries");
        }
        std::filesystem::remove_all(folder);
    }
};
} // namespace cycode
int main(int argc, char **argv) {
    try {
        require(argc == 3, "Expected repository and fixture library");
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1600, 1000};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        cyui::apply({});
        cyui::loadFonts();
        ApplicationExtensionTests::run(utf8Path(argv[1]), utf8Path(argv[2]));
        ImGui::DestroyContext();
        std::cout << "Extension GUI passed: menus, actions, native panels and non-Git provider in six "
                     "themes/EN/FR/three DPI values; no native windows.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
