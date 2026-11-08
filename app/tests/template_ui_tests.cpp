#include "cycode/Application.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <iostream>
#include <thread>
using namespace cycode;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
namespace cycode {
struct ApplicationTemplateTests {
    static void run(const std::filesystem::path &repository, const std::filesystem::path &fixture) {
        namespace fs = std::filesystem;
        const auto root =
            repository / "runtime" /
            ("template-ui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root / "app/locales");
        fs::create_directories(root / "workspace");
        fs::create_directories(root / "projects été");
        fs::copy_file(repository / "app/locales/ui.json", root / "app/locales/ui.json");
        fs::create_directories(root / "extensions");
        fs::copy_file(fixture, root / "extensions" / fixture.filename());
        std::ofstream(root / "extensions/fixture.json") << Json({{"id", "fixture.native"},
                                                                 {"abi", 2},
                                                                 {"version", "1.0.0"},
                                                                 {"library", pathUtf8(fixture.stem())}})
                                                               .dump();
        std::ofstream(root / "workspace/main.cpp") << "int before=1;\n";
        {
            LaunchOptions options;
            options.installation = root;
            options.workspace = root / "workspace";
            options.smoke = true;
            options.testSession = true;
            options.disablePlugins = true;
            Application app(options, nullptr);
            app.open("main.cpp");
            auto draw = [&] {
                ImGui::NewFrame();
                app.menu();
                app.projectTemplatePanel();
                ImGui::Render();
                require(ImGui::GetDrawData()->TotalVtxCount > 0, "Project wizard produced no draw data");
            };
            auto settle = [&] {
                const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                for (;;) {
                    app.update();
                    draw();
                    if (!app.busy() && app.pendingWorkspace_.empty())
                        return;
                    require(std::chrono::steady_clock::now() < until, "Project wizard did not settle");
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            };
            settle();
            auto old = app.tabs_.front();
            old->editor.SetCursor({0, 0});
            old->editor.ReplaceTextPreservingTabs("// unsaved\n");
            app.changed(old);
            app.executeCommand("file.newProject");
            require(app.templateModal_, "New project command did not open wizard");
            app.templateParent_ = pathUtf8(root / "projects été");
            app.templateName_ = "CreatedProject";
            app.projectTemplateTask(false);
            settle();
            require(app.templatePlan_.at("state") == "preview", "Wizard preview failed");
            require(!fs::exists(root / "projects été/CreatedProject"), "GUI preview created a project");
            app.selectTemplateFile(1);
            require(app.templateFile_ == 1, "File preview selection failed");
            for (const auto *locale : {"en", "fr"})
                for (const auto *theme :
                     {"graphite", "mid-grey", "light-grey", "amber-night", "violet", "navy"})
                    for (float dpi : {1.f, 1.5f, 2.f}) {
                        app.locale_ = locale;
                        cyui::setLocale(locale);
                        cyui::setTheme(theme);
                        cyui::applyDpi(dpi);
                        draw();
                    }
            cyui::applyDpi(1);
            app.projectTemplateTask(true);
            settle();
            require(app.templatePlan_.at("state") == "created" && app.templateModal_,
                    "Dirty workspace switch lost generated project result");
            require(app.engine_->workspace() == root / "workspace" && old->dirty &&
                        old->editor.GetText().starts_with("// unsaved"),
                    "Creation discarded an existing dirty document");
            require(fs::exists(root / "projects été/CreatedProject/CMakeLists.txt"),
                    "Project was not published");
            std::weak_ptr<Engine> previousEngine = app.engine_;
            app.save(old);
            settle();
            app.openCreatedProject();
            settle();
            require(app.engine_->workspace() == root / "projects été/CreatedProject",
                    "Generated project was not opened");
            require(previousEngine.expired(),
                    "Project creation pinned the previous workspace services after switching");
            require(app.active_ == "src/main.cpp" && app.tabs_.size() == 1,
                    "Generated entry file was not opened");
            auto tab = app.tabs_.front();
            const auto before = tab->editor.GetText();
            tab->editor.SetCursor({0, 0});
            tab->editor.ReplaceTextPreservingTabs("// edited\n");
            app.changed(tab);
            app.editorFocused_ = true;
            app.executeCommand("edit.undo");
            require(tab->editor.GetText() == before, "Undo stopped working in generated source");
            app.executeCommand("edit.redo");
            require(tab->editor.GetText().starts_with("// edited"),
                    "Redo stopped working in generated source");
            app.save(tab);
            settle();
            app.engine_->plugins().load(root / "extensions");
            app.executeCommand("file.newProject");
            app.selectProjectTemplate("fixture.native.cmake");
            app.templateParent_ = pathUtf8(root / "projects été");
            app.templateName_ = "CustomProject";
            app.templateOptions_ = {{"standard", "17"}, {"feature", true}, {"scope", "custom"}};
            app.projectTemplateTask(false);
            settle();
            require(app.templatePlan_.at("plugin") == "fixture.native" &&
                        app.templatePlan_.at("options").at("standard") == "17",
                    "Plugin options did not reach the native preview");
            for (const auto *locale : {"en", "fr"})
                for (const auto *theme :
                     {"graphite", "mid-grey", "light-grey", "amber-night", "violet", "navy"})
                    for (float dpi : {1.f, 1.5f, 2.f}) {
                        cyui::setLocale(locale);
                        cyui::setTheme(theme);
                        cyui::applyDpi(dpi);
                        draw();
                    }
            cyui::applyDpi(1);
            app.engine_->dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", false}});
            settle();
            require(app.templatePlan_.empty() && app.templateId_ == "cpp.console",
                    "Disabled plugin left an actionable GUI preview");
            app.engine_->dispatch("plugins.configure", {{"id", "fixture.native"}, {"enabled", true}});
            settle();
            app.selectProjectTemplate("fixture.native.cmake");
            app.templateName_ = "CustomProject";
            app.projectTemplateTask(false);
            settle();
            app.projectTemplateTask(true);
            settle();
            require(app.active_ == "src/main.cpp" &&
                        app.engine_->workspace() == root / "projects été/CustomProject",
                    "Plugin-generated project was not opened");
            app.executeCommand("file.newProject");
            draw();
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true);
            draw();
            ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
            draw();
            require(!app.templateModal_, "Escape did not cancel project wizard");
        }
        fs::remove_all(root);
    }
};
} // namespace cycode
int main(int argc, char **argv) {
    try {
        require(argc == 3, "Expected repository and fixture");
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1700, 1100};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        cyui::apply({});
        cyui::loadFonts();
        ApplicationTemplateTests::run(utf8Path(argv[1]), utf8Path(argv[2]));
        ImGui::DestroyContext();
        std::cout << "Project wizard passed: preview, selection, creation/open, dirty workspace guard, "
                     "source Undo/Redo, Escape; EN/FR, six themes, three DPI values; no native windows\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
