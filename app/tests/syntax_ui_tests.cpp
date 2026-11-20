#include "cycode/Application.h"
#include "cycode/WorkspaceSession.h"
#include "../src/EnginePagedBackend.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace cycode {
struct ApplicationSyntaxTests {
    static void check(bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
    }
    static void run(const std::filesystem::path &repository) {
        namespace fs = std::filesystem;
        using L = cyui::CodeLanguage;
        using R = cyui::TokenRole;
        using C = TextEditor::Color;
        const auto root = repository / "runtime" /
            ("syntax-ui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root / "workspace");
        for (const auto *name : {"main.zig", "build.zig", "data.zon", "build.zig.zon", "UPPER.ZIG"})
            std::ofstream(root / "workspace" / name, std::ios::binary) << "nan";
        {
            LaunchOptions options;
            options.installation = root; options.workspace = root / "workspace";
            options.smoke = options.testSession = options.disablePlugins = true;
            Application app(options, nullptr);
            for (const auto *name : {"main.zig", "build.zig", "data.zon", "build.zig.zon", "UPPER.ZIG"}) {
                app.open(name);
                const auto tab = app.tabs_.back();
                const auto expected = std::string(name).ends_with("zon") ? L::zon : L::zig;
                check(tab->codeEditor.language() == expected && !tab->legacySyntax, "Automatic Zig/ZON opening");
                check(app.engine_->languageBackendFor(name).empty() && !app.engine_->supportsLanguage(name), "Syntax unexpectedly activated a language server");
            }
            const auto tab = app.tabs_[3]; // build.zig.zon
            check(tab->editor.GetSharedSyntaxColor({0, 0}) == C::number, "ZON literal did not use CyUI");
            const auto original = tab->editor.GetText();
            const auto undo = tab->editor.GetUndoIndex();
            tab->editor.SetCursor({0, 2});
            app.selectSyntaxLanguage(tab, "zig");
            check(tab->editor.GetSharedSyntaxColor({0, 0}) == C::text, "Manual Zig did not override ZON");
            check(tab->editor.GetText() == original && tab->editor.GetUndoIndex() == undo && tab->editor.GetMainCursorPosition() == TextEditor::DocPos{0, 2}, "Language selection changed document state");
            auto saved = app.workspaceSessionSnapshot();
            auto item = *std::find_if(saved.at("tabs").begin(), saved.at("tabs").end(), [](const auto &value) { return value.at("path") == "build.zig.zon"; });
            check(item.at("syntaxLanguage") == "zig", "Session did not store the stable text ID");
            check(validateWorkspaceSession(saved, saved.at("workspace")).at("tabs") == saved.at("tabs"), "Native session rejected syntax metadata");
            app.selectSyntaxLanguage(tab, ""); app.restoreSessionTab(tab, item);
            check(tab->syntaxLanguageId == "zig" && tab->codeEditor.language() == L::zig, "Manual session restoration");
            const std::string edited = "\\\\\xc3\xa9\xf0\x9f\x99\x82 // fn\nconst a: u7 = 3; // comment";
            tab->editor.SelectAll(); tab->editor.ReplaceTextPreservingTabs(edited); app.changed(tab);
            check(tab->syntaxLanguageId == "zig" && tab->editor.GetSharedSyntaxColor({0, 3}) == C::string && tab->editor.GetSharedSyntaxColor({1, 0}) == C::keyword, "Edited Unicode/string/comment syntax");
            const auto editedUndo = tab->editor.GetUndoIndex();
            app.resourceTabPath(tab, "renamed.zon");
            check(tab->codeEditor.language() == L::zig && tab->editor.GetUndoIndex() == editedUndo, "Rename overwrote manual syntax or history");
            app.resourceTabPath(tab, "build.zig.zon");
            tab->editor.Undo(); app.changed(tab);
            check(tab->editor.GetText() == original && tab->editor.CanRedo(), "Syntax selection broke Undo");
            app.selectSyntaxLanguage(tab, "zon");
            tab->editor.Redo(); app.changed(tab);
            check(tab->editor.GetText() == edited, "Syntax selection broke Redo");
            const Json plugin = {{"id", "test/ide-zig-dialect"}, {"name", "IDE Zig dialect"}, {"base", "zig"}, {"extensions", {"zon"}}, {"filenames", {"special.zig"}}};
            app.synchronizeSyntax(Json::array({plugin}));
            check(tab->syntaxLanguageId == "zon" && tab->codeEditor.language() == L::zon, "Plugin refresh overwrote manual language");
            app.selectSyntaxLanguage(tab, "");
            check(cyui::codeLanguageInfo(tab->codeEditor.language()).id == "test/ide-zig-dialect", "Auto mode ignored plugin extension association");
            check(cyui::codeLanguageInfo(app.codeLanguageForDocument("special.zig")).id == "test/ide-zig-dialect", "Plugin exact filename association");
            app.selectSyntaxLanguage(tab, "test/ide-zig-dialect");
            app.synchronizeSyntax(Json::array());
            check(tab->syntaxLanguageId == "test/ide-zig-dialect" && tab->codeEditor.language() == L::plain, "Unavailable plugin ID was discarded");
            item["syntaxLanguage"] = "test/ide-zig-dialect"; app.restoreSessionTab(tab, item);
            app.synchronizeSyntax(Json::array({plugin}));
            check(cyui::codeLanguageInfo(tab->codeEditor.language()).id == "test/ide-zig-dialect", "Returning plugin did not restore saved ID");
            app.selectSyntaxLanguage(tab, ""); app.synchronizeSyntax(Json::array());
            check(tab->codeEditor.language() == L::zon && tab->syntaxLanguageId.empty(), "Return to builtin auto association");
            auto legacy = item; legacy.erase("syntaxLanguage"); app.restoreSessionTab(tab, legacy);
            check(tab->syntaxLanguageId.empty(), "Legacy session lost automatic detection");
            CodeComparison diff;
            diff.setTexts("nan", "inf", tab->path, L::zon);
            const auto prepared = diff.document().revision(); diff.refresh(L::zon);
            check(diff.document().revision() == prepared, "Unchanged diff was re-prepared");
            diff.refresh(L::zig);
            check(diff.document().revision() != prepared, "Diff ignored syntax selection");
            diff.setUnified("@@ -1 +1 @@\n-inf\n+nan\n", tab->path, L::zon);
            for (const auto &row : diff.document().rows())
                if (row.kind == cyui::DiffKind::added || row.kind == cyui::DiffKind::removed)
                    check(row.code.spans.size() == 1 && row.code.spans[0].role == R::number, "ZON unified diff roles");
            // A string crosses the 64 KiB native read boundary; all offsets below are UTF-8 bytes.
            const auto source = std::string(65520, ' ') + "const s = \"\xc3\xa9\xf0\x9f\x99\x82 // fn\"; // trailing\r\n";
            { std::ofstream out(root / "workspace/context.zig", std::ios::binary); out << source; }
            const auto opened = app.engine_->dispatch("documents.readPage", {{"path", "context.zig"}, {"length", 4}});
            const auto id = opened.at("id").get<uint64_t>();
            EnginePagedBackend backend(app.engine_, id, "context.zig");
            auto state = backend.state(id);
            const auto at = source.find("\xf0\x9f\x99\x82");
            const auto tokens = backend.syntax(id, state.revision, L::zig, at, 4);
            check(tokens.size() == 1 && tokens[0].start == 0 && tokens[0].length == 4 && tokens[0].role == R::string, "Paged spans lack preceding context or use non-byte coordinates");
            const auto trailing = source.find("trailing");
            const auto comments = backend.syntax(id, state.revision, L::zig, trailing, 8);
            check(comments.size() == 1 && comments[0].start == 0 && comments[0].length == 8 && comments[0].role == R::comment, "Paged comment clipping/rebasing");
            check(backend.readBytes(id, state.revision, 0, 65536).nextOffset <= 65536, "Native UTF-8 chunk boundary");
            {
                int calls = 0;
                cyui::CodeLanguageDefinition plugin;
                plugin.id = "test/ide-paged-cache"; plugin.name = "Paged cache fixture";
                plugin.highlight = [&](std::string_view text) {
                    ++calls; return std::vector<cyui::CodeToken>{{0, text.size(), R::string}};
                };
                struct Cleanup { ~Cleanup() { cyui::unregisterCodeLanguage("test/ide-paged-cache"); } } cleanup;
                const auto language = cyui::registerCodeLanguage(plugin);
                backend.syntax(id, state.revision, language, at, 4);
                backend.syntax(id, state.revision, language, trailing, 8);
                check(calls == 1, "Paged syntax re-lexed an unchanged snapshot for another fragment");
                cyui::registerCodeLanguage(plugin);
                backend.syntax(id, state.revision, language, at, 4);
                check(calls == 2, "Paged syntax did not refresh after registry revision");
            }
            const auto beforeEdit = state.revision;
            const auto plan = backend.prepare({1, id, state.revision, {{0, 0}, {0, 0}}, "// "});
            state = backend.apply(plan).status; backend.discard(plan);
            const auto afterEdit = backend.syntax(id, state.revision, L::zig, at + 3, 4);
            check(afterEdit.size() == 1 && afterEdit[0].role == R::comment, "Paged syntax did not follow edited lexical context");
            state = backend.undo(id, state.revision, false).status;
            const auto undone = backend.syntax(id, state.revision, L::zig, at, 4);
            check(undone.size() == 1 && undone[0].role == R::string, "Paged Undo did not restore syntax");
            state = backend.undo(id, state.revision, true).status;
            const auto redone = backend.syntax(id, state.revision, L::zig, at + 3, 4);
            check(redone.size() == 1 && redone[0].role == R::comment, "Paged Redo did not restore syntax");
            bool stale = false;
            try { backend.syntax(id, beforeEdit, L::zig, at, 4); }
            catch (const std::exception &) { stale = true; }
            check(stale, "Stale paged lexical revision was accepted");
            app.openPaged("context.zig", 0, 0, false, false, true);
            const auto paged = app.tabs_.back(); app.selectSyntaxLanguage(paged, "zon");
            auto pagedSaved = app.workspaceSessionSnapshot();
            const auto &record = pagedSaved.at("tabs").back();
            check(record.at("syntaxLanguage") == "zon" && record.at("paged") == true, "Paged language session persistence");
            app.selectSyntaxLanguage(paged, ""); app.restoreSessionTab(paged, record);
            check(paged->syntaxLanguageId == "zon", "Paged language session restoration");
        }
        fs::remove_all(root);
    }
};
void syntaxViewContracts(const std::filesystem::path &repository) { ApplicationSyntaxTests::run(repository); }
} // namespace cycode
