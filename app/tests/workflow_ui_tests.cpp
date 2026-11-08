#include "../src/UpdateFeedback.h"
static_assert(cycode::updates::detail::downloadFeedback(cycode::updates::DownloadIssue::response).code == "response");
#include "cycode/Application.h"
#include <fstream>
#include <iostream>
#include <thread>
namespace cycode {
void sessionViewContracts(const std::filesystem::path &);
struct ApplicationWorkflowTests {
    static void check(bool v, const char *message) {
        if (!v)
            throw std::runtime_error(message);
    }
    static void run(const std::filesystem::path &repository, const std::string &cmake) {
        namespace fs = std::filesystem;
        const auto root =
            repository / "runtime" /
            ("workflow-ui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root / "workspace");
        std::ofstream(root / "workspace/main.cpp") << "int before=1;\n";
        std::ofstream(root / "workspace/CMakeLists.txt")
            << "cmake_minimum_required(VERSION 3.24)\nproject(TestUI NONE)\nenable_testing()\nadd_test(NAME "
               "gui_smoke COMMAND \"${CMAKE_COMMAND}\" -E true)\n";
        const auto configured = runProcess(
            {cmake, "-S", pathUtf8(root / "workspace"), "-B", pathUtf8(root / "workspace/build")}, root);
        check(!configured.exitCode, "Configure GUI test project");
        auto git = [&](std::vector<std::string> args) {
            args.insert(args.begin(), {"git", "-C", pathUtf8(root / "workspace")});
            check(!runProcess(args, root).exitCode, "Prepare GUI Git fixture");
        };
        git({"init", "--initial-branch=main"});
        git({"config", "user.name", "Fixture"});
        git({"config", "user.email", "fixture@example.invalid"});
        git({"add", "main.cpp"});
        git({"commit", "-m", "Initial"});
        std::ofstream(root / "workspace/main.cpp") << "int after=2;\n";
        {
            LaunchOptions options;
            options.installation = root;
            options.workspace = root / "workspace";
            options.smoke = true;
            options.testSession = true;
            options.disablePlugins = true;
            Application app(options, nullptr);
            app.inspectUpdateHistory();
            check(!app.updateHistoryFuture_.valid() && !app.updateHistory_,
                  "Test session inspected real update history");
            app.open("main.cpp");
            auto settle = [&] {
                const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                do {
                    app.update();
                    ImGui::NewFrame();
                    app.testsPanel();
                    app.gitPanel();
                    ImGui::Render();
                    if (!app.testsFuture_.valid() && !app.vcsFuture_.valid() && !app.busy())
                        return;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                } while (std::chrono::steady_clock::now() < end);
                throw std::runtime_error("Workflow GUI did not settle");
            };
            settle();
            const Json annotation={{"id","fixture-plan:annotation-1"},{"label","Review this change"},
                {"description","Passive label"},{"needsConfirmation",true},{"files",{"main.cpp"}}};
            app.refactorPlan_={{"id","fixture-plan"},{"title","Annotated preview"},{"state","preview"},
                {"files",Json::array({{{"path","main.cpp"},{"expectedRevision",1},{"editCount",1}}})},
                {"editCount",1},{"annotations",Json::array({annotation})}};
            app.refactorAnnotationPlan_="previous-plan";app.refactorConfirmed_.insert("previous-plan:annotation-1");
            app.showPanel("Refactoring","refactoring");
            auto annotationFrame=[&] {ImGui::NewFrame();app.refactorPanel();ImGui::Render();};
            annotationFrame();
            check(app.refactorConfirmed_.empty(),"New plan reused previous annotation approvals");
            app.refactorConfirmed_.insert(annotation.at("id"));annotationFrame();
            check(app.refactorConfirmed_.size()==1,"Same-plan approval was lost during drawing");
            app.refactorPlan_["id"]="another-plan";annotationFrame();
            check(app.refactorConfirmed_.empty(),"Changed plan kept old annotation approval");
            app.refactorPlan_=nullptr;
            app.executeCommand("view.tests");
            check(app.panels_.at("tests"), "Tests command did not expose panel");
            app.testTask("tests.discover", {{"buildDirectory", "build"}});
            settle();
            check(app.testsCatalog_.at("tests").size() == 1, "GUI test discovery");
            app.testsSelection_.insert(0);
            app.testTask("tests.run", {{"id", app.testsCatalog_.at("id")}, {"tests", {0}}});
            settle();
            check(app.testsResults_.at("results")[0].at("status") == "passed", "GUI actual test result");
            app.executeCommand("view.vcs");
            app.vcsTask("vcs.hunks", {{"path", "main.cpp"}});
            settle();
            check(app.git_.value("repository", false),
                  "Git status did not follow the newly opened document context");
            check(app.vcsHunks_.at("hunks").size() == 1, "GUI hunk preview");
            auto hunkVisible = [&] {
                const auto state = app.testState();
                for (const auto &t : state.at("vcs").at("tabs"))
                    if (t.at("name").get<std::string>().ends_with("###vcs-section-Hunks") &&
                        t.value("visible", false))
                        return true;
                return false;
            };
            check(hunkVisible(), "Requested Git hunk tab is not actually visible");
            for (const auto &row : app.vcsHunkDiff_.rows())
                check(!row.code.text.starts_with("diff --git"),
                      "Redundant patch metadata obscures the changed code");
            app.vcsHunkSelection_.insert(0);
            for (const auto *locale : {"en", "fr"}) {
                app.locale_ = locale;
                cyui::setLocale(locale);
                for (const auto *theme :
                     {"graphite", "mid-grey", "light-grey", "amber-night", "violet", "navy"}) {
                    cyui::setTheme(theme);
                    for (float dpi : {1.f, 1.25f, 1.5f}) {
                        cyui::applyDpi(dpi);
                        ImGui::NewFrame();
                        app.testsPanel();
                        app.gitPanel();
                        ImGui::Render();
                        check(ImGui::GetDrawData()->TotalVtxCount > 0, "Workflow views have no draw data");
                        check(hunkVisible(), "Changing locale/theme/DPI lost the selected Git sub-tab");
                    }
                }
            }
            app.vcsTask("vcs.applyHunks", {{"id", app.vcsHunks_.at("id")}, {"hunks", {0}}});
            settle();
            check(app.vcsHunks_.empty(), "Consumed hunk preview remained actionable");
            check(app.testsError_.empty(), "GUI test runner error");
            app.testTask("tests.discover", {{"buildDirectory", "missing"}});
            settle();
            check(app.testsCatalog_.empty() && !app.testsError_.empty(),
                  "Failed discovery retained an actionable stale catalogue");
        }
        fs::remove_all(root);
    }
};
} // namespace cycode
int main(int argc, char **argv) {
    try {
        if (argc != 3)
            return 2;
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {1700, 1100};
        io.DeltaTime = 1.f / 60;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        cyui::apply({});
        cyui::loadFonts();
        cycode::sessionViewContracts(cycode::utf8Path(argv[1]));
        cycode::ApplicationWorkflowTests::run(cycode::utf8Path(argv[1]), argv[2]);
        ImGui::DestroyContext();
        std::cout << "Headless CTest/Git workflow UI passed; no native windows\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
