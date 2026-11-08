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
struct ApplicationResourceTests {
    static void run(const std::filesystem::path &repository) {
        const auto folder =
            repository / "runtime" /
            ("resource-ui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(folder / "app/locales");
        std::filesystem::create_directories(folder / "workspace/src");
        std::filesystem::copy_file(repository / "app/locales/ui.json", folder / "app/locales/ui.json");
        std::ofstream(folder / "workspace/src/main.cpp", std::ios::binary) << "int value=1;\n";
        {
            LaunchOptions options;
            options.installation = folder;
            options.workspace = folder / "workspace";
            options.smoke = true;
            options.testSession = true;
            options.disablePlugins = true;
            Application app(options, nullptr);
            app.open("src/main.cpp");
            app.executeCommand("view.resources");
            auto draw = [&] {
                ImGui::NewFrame();
                app.menu();
                app.resourcePanel();
                ImGui::Render();
                require(ImGui::GetDrawData()->TotalVtxCount > 0, "File view produced no draw data");
            };
            auto settle = [&] {
                const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                for (;;) {
                    app.update();
                    draw();
                    if (!app.busy())
                        return;
                    if (std::chrono::steady_clock::now() > until)
                        throw std::runtime_error("Resource GUI did not settle");
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            };
            settle();
            auto tab = app.tabs_.front();
            const auto identity = tab->identity;
            const auto titleBefore = app.tabTitle(tab);
            const auto stableBefore = titleBefore.substr(titleBefore.find("###"));
            tab->editor.SetCursor({0, 0});
            tab->editor.ReplaceTextPreservingTabs("// history\n");
            app.changed(tab);
            app.save(tab);
            settle();
            const auto undo = tab->editor.GetUndoIndex();
            app.resourceTask(
                "resources.preview",
                {{"operation", "move"}, {"path", "src/main.cpp"}, {"destination", "src/renommé.cpp"}});
            settle();
            require(!app.resourcePlan_.at("blocked").get<bool>(), "Saved file preview was blocked");
            app.resourceModal_ = true;
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
            app.resourceTask("resources.apply", {{"id", app.resourcePlan_.at("id")}});
            settle();
            require(tab->path == "src/renommé.cpp" && tab->identity == identity && app.active_ == tab->path,
                    "Rename did not preserve the active document");
            require(tab->editor.GetUndoIndex() == undo, "Rename changed editor Undo history");
            const auto titleAfter = app.tabTitle(tab);
            require(titleAfter.substr(titleAfter.find("###")) == stableBefore,
                    "Rename replaced the native window identity");
            app.executeCommand("project.undoFile");
            settle();
            require(tab->path == "src/main.cpp" && tab->editor.GetUndoIndex() == undo,
                    "Undo file operation lost tab/history");
            app.editorFocused_ = true;
            app.executeCommand("edit.undo");
            require(tab->editor.GetText() == "int value=1;\n", "Text Undo stopped working after file moves");
            app.executeCommand("edit.redo");
            app.save(tab);
            settle();
            app.resourceTask("resources.preview", {{"operation", "delete"}, {"path", "src/main.cpp"}});
            settle();
            app.resourceTask("resources.apply", {{"id", app.resourcePlan_.at("id")}});
            settle();
            require(app.tabs_.size() == 1 && tab->diskConflict == "missing",
                    "Deleted open buffer was not retained");
            app.executeCommand("project.undoFile");
            settle();
            require(tab->diskConflict.empty() && tab->identity == identity,
                    "Trash Undo did not reattach its buffer");
            const auto native = app.engine_->dispatch(
                "resources.preview", {{"operation", "move"}, {"path", "src"}, {"destination", "moved"}});
            app.engine_->dispatch("resources.apply", {{"id", native.at("id")}});
            // Type before the next metadata/event poll, after an external move committed.
            tab->editor.SetCursor({0, 0});
            tab->editor.ReplaceTextPreservingTabs("// queued keystroke\n");
            app.changed(tab);
            require(app.engine_->dispatch("documents.read", {{"path", "moved/main.cpp"}}).at("text") ==
                        tab->editor.GetText(),
                    "A keystroke racing an external move was lost");
            settle();
            require(tab->path == "moved/main.cpp" && tab->identity == identity,
                    "MCP/plugin-originated move left a stale GUI path");
            app.resourceTask("resources.preview", {{"operation", "createDirectory"}, {"path", "empty"}});
            settle();
            app.resourceTask("resources.apply", {{"id", app.resourcePlan_.at("id")}});
            settle();
            app.resourceTask("resources.children", {{"path", "."}});
            settle();
            const auto &items = app.resourceItems_.at("items");
            require(std::any_of(items.begin(), items.end(),
                                [](const auto &item) {
                                    return item.at("path") == "empty" &&
                                           item.at("directory").template get<bool>();
                                }),
                    "Empty folders disappeared from the common Assets view");
            const auto draft = app.engine_->dispatch("documents.createUntitled",
                                                     {{"name", "mcp.cpp"}, {"text", "int remote=1;\n"}});
            settle();
            auto view = std::find_if(app.tabs_.begin(), app.tabs_.end(), [&](const auto &item) {
                return item->identity == draft.at("id").get<uint64_t>();
            });
            require(view != app.tabs_.end(), "MCP draft did not acquire a real editor tab");
            auto draftTab = *view;
            const auto draftWindow = app.tabTitle(draftTab);
            draftTab->editor.SetCursor({0, 0});
            draftTab->editor.ReplaceTextPreservingTabs("// local queued edit\n");
            const auto localText = draftTab->editor.GetText();
            app.engine_->dispatch(
                "documents.close",
                {{"path", draft.at("path")}, {"expectedRevision", draft.at("revision")}, {"discard", true}});
            settle();
            require(draftTab->identity != draft.at("id").get<uint64_t>() &&
                        isUntitledDocument(draftTab->path) && draftTab->editor.GetText() == localText,
                    "Native close discarded an uncommitted GUI edit");
            const auto savedDraft =
                app.engine_->dispatch("documents.saveAs", {{"path", draftTab->path},
                                                           {"destination", "draft-saved.cpp"},
                                                           {"expectedRevision", draftTab->revision}});
            settle();
            require(draftTab->path == "draft-saved.cpp" &&
                        draftTab->identity == savedDraft.at("id").get<uint64_t>() &&
                        app.tabTitle(draftTab).substr(app.tabTitle(draftTab).find("###")) ==
                            draftWindow.substr(draftWindow.find("###")) &&
                        draftTab->editor.CanUndo(),
                    "Native Save as lost editor Undo or window identity");
            draftTab->editor.Undo();
            app.changed(draftTab);
            require(app.engine_->dispatch("documents.read", {{"path", draftTab->path}}).at("text") ==
                        draftTab->editor.GetText(),
                    "Undo after native Save as did not reach the engine");
            const auto retiredId = draftTab->identity;
            app.engine_->dispatch(
                "documents.close",
                {{"path", draftTab->path}, {"expectedRevision", draftTab->revision}, {"discard", true}});
            settle();
            require(std::none_of(app.tabs_.begin(), app.tabs_.end(),
                                 [&](const auto &item) { return item->identity == retiredId; }),
                    "Native document close left an orphan GUI tab");
        }
        std::filesystem::remove_all(folder);
    }
};
} // namespace cycode
int main(int argc, char **argv) {
    try {
        require(argc == 2, "Expected repository");
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1700, 1100};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        cyui::apply({});
        cyui::loadFonts();
        ApplicationResourceTests::run(utf8Path(argv[1]));
        ImGui::DestroyContext();
        std::cout << "Resources GUI passed: common Assets, empty folders, guarded actions, stable tab/window "
                     "identity, Undo and external mutation synchronization; EN/FR, six themes, three DPI "
                     "values; no native windows.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
