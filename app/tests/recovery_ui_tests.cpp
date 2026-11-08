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
struct ApplicationRecoveryTests {
    static void run(const std::filesystem::path &repository) {
        const auto folder =
            repository / "runtime" /
            ("recovery-ui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(folder / "app/locales");
        std::filesystem::create_directories(folder / "workspace");
        std::filesystem::copy_file(repository / "app/locales/ui.json", folder / "app/locales/ui.json");
        const std::string original = "// initial\nint value=1;\n", edited = "// récupéré 🛠\nint value=42;\n";
        std::ofstream(folder / "workspace/main.cpp", std::ios::binary) << original;
        {
            RecoveryStore seed(folder / "workspace");
            seed.checkpoint({"main.cpp", edited, original, 17, false, false, false, true});
        }
        {
            LaunchOptions options;
            options.installation = folder;
            options.workspace = folder / "workspace";
            options.smoke = true;
            options.testSession = true;
            options.disablePlugins = true;
            Application app(options, nullptr);
            auto draw = [&] {
                ImGui::NewFrame();
                app.menu();
                app.recoveryPanel();
                ImGui::Render();
                require(ImGui::GetDrawData()->TotalVtxCount > 0, "Recovery view produced no drawing data");
            };
            auto settle = [&] {
                const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                for (;;) {
                    app.update();
                    draw();
                    if (!app.busy())
                        return;
                    if (std::chrono::steady_clock::now() > until)
                        throw std::runtime_error("Recovery UI tasks did not complete");
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            };
            settle();
            require(app.panels_.at("recovery"), "Abandoned documents did not open the Recovery view");
            require(app.recoveryInfo_.at("items").size() == 1, "Checkpoint missing from GUI inventory");
            const auto entry = app.recoveryInfo_.at("items").at(0);
            const Json ref = {{"session", entry.at("session")},
                              {"id", entry.at("id")},
                              {"checkpoint", entry.at("checkpoint")}};
            app.recoveryTask("recovery.preview", ref);
            settle();
            require(app.recoveryCode_.text() == edited, "Shared CyUI preview lost recovery text");
            require(app.tabs_.empty(), "Preview opened a mutable document");
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
            auto params = ref;
            params["expectedRevision"] = app.recoveryPreview_.at("targetRevision");
            params["expectedDiskState"] = app.recoveryPreview_.at("diskState");
            params["includePreviousText"] = true;
            app.recoveryTask("recovery.restore", params);
            settle();
            require(app.tabs_.size() == 1 && app.tabs_.front()->dirty,
                    "Restoration did not open a dirty editor buffer");
            auto tab = app.tabs_.front();
            require(tab->editor.GetText() == edited && tab->editor.GetUndoIndex() == 1,
                    "Restoration did not create one Undo transaction");
            require(readTextFileBytes(folder / "workspace/main.cpp") == original,
                    "GUI restoration implicitly saved the source");
            app.editorFocused_ = true;
            app.executeCommand("edit.undo");
            require(tab->editor.GetText() == original, "Undo could not cancel restoration");
            app.executeCommand("edit.redo");
            require(tab->editor.GetText() == edited, "Redo could not restore recovered text");
            const auto before = tab->revision;
            tab->editor.SetCursor({1, 0});
            tab->editor.ReplaceTextPreservingTabs("// typed\n");
            app.changed(tab);
            require(tab->revision > before, "Recovered editor no longer accepted normal edits");
            app.engine_->dispatch("recovery.flush");
            app.save(tab);
            settle();
            require(!tab->dirty && app.engine_->dispatch("recovery.list").at("items").empty(),
                    "Save did not retire the recovery checkpoint");
            app.panels_["recovery"] = false;
            app.executeCommand("view.recovery");
            settle();
            require(app.panels_["recovery"], "Find Action / View command cannot open Recovery");
            // Dirty live buffers survive normal Engine shutdown as well as abrupt exits.
            tab->editor.SetCursor({0, 0});
            tab->editor.ReplaceTextPreservingTabs("// unsaved\n");
            app.changed(tab);
        }
        {
            Engine verify(folder / "workspace");
            const auto inventory = verify.dispatch("recovery.list");
            require(inventory.at("items").size() == 1 &&
                        !inventory.at("items").at(0).at("active").get<bool>(),
                    "Graceful unsaved shutdown discarded recovery content");
        }
        std::filesystem::remove_all(folder);
    }
};
} // namespace cycode
int main(int argc, char **argv) {
    try {
        require(argc == 2, "Expected repository directory");
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1700, 1050};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        cyui::apply({});
        cyui::loadFonts();
        ApplicationRecoveryTests::run(utf8Path(argv[1]));
        ImGui::DestroyContext();
        std::cout
            << "Recovery GUI passed: startup offer, shared preview, Restore, one-step Undo/Redo, editing, "
               "Save, graceful retention, six themes/EN/FR/three DPI values; no native windows.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
