#include "cycode/Application.h"
#include "cycode/ImageFile.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cyui/Core.h>
#include <filesystem>
#include <fstream>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <imgui_internal.h>
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif
static std::vector<std::string> arguments(int argc, char **argv) {
    std::vector<std::string> result;
#ifdef _WIN32
    int count = 0;
    auto args = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 0; i < count; ++i) {
        const auto length = int(wcslen(args[i]));
        const auto bytes = WideCharToMultiByte(CP_UTF8, 0, args[i], length, nullptr, 0, nullptr, nullptr);
        std::string value(bytes, 0); WideCharToMultiByte(CP_UTF8, 0, args[i], length, value.data(), bytes, nullptr, nullptr);
        result.push_back(std::move(value));
    }
    LocalFree(args);
#else
    for (int i = 0; i < argc; ++i)
        result.emplace_back(argv[i]);
#endif
    return result;
}
int main(int argc, char **argv) {
    SDL_Window *window = nullptr;
    SDL_GLContext glContext = nullptr;
    bool automated = false;
    std::unique_ptr<cycode::NativeFileLease> installationLease;
    std::filesystem::path installation;
    try {
#ifdef _WIN32
        // SDL's class icons also apply to independently detached platform windows.
        SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "1");
        SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "1");
#endif
        if (!SDL_Init(SDL_INIT_VIDEO))
            throw std::runtime_error(SDL_GetError());
        cycode::LaunchOptions options;
        auto base = cycode::utf8Path(SDL_GetBasePath());
        for (int i = 0; i < 5; ++i) {
            if (std::filesystem::exists(base / "app/CyTool.json")) {
                options.installation = base;
                break;
            }
            base = base.parent_path();
        }
        if (options.installation.empty())
            throw std::runtime_error("Cannot locate app/CyTool.json beside the CyCode installation");
        installation = options.installation;
        installationLease=cycode::updates::installationUse(installation);
        std::string capture, testReport, capturePanel;
        cycode::Json testKeys = cycode::Json::array();
        std::string testProject;
        bool testWaitIdle = false;
        int frameLimit = 0;
        auto args = arguments(argc, argv);
        for (size_t i = 1; i < args.size(); ++i) {
            auto next = [&]() {
                if (i + 1 >= args.size())
                    throw std::runtime_error("Missing command line value");
                return args[++i];
            };
            if (args[i] == "--workspace")
                options.workspace = cycode::utf8Path(next());
            else if (args[i] == "--open")
                options.openFile = next();
            else if (args[i] == "--line")
                options.openLine = std::stoull(next());
            else if (args[i] == "--column")
                options.openColumn = std::stoull(next());
            else if (args[i] == "--theme")
                options.theme = next();
            else if (args[i] == "--language")
                options.language = next();
            else if (args[i] == "--keymap")
                options.keymap = next();
            else if (args[i] == "--disable-plugins") options.disablePlugins = true;
            else if (args[i] == "--terminal-profile") options.terminalProfile = next();
            else if (args[i] == "--show-options")
                options.showOptions = true;
            else if (args[i] == "--show-cmake")
                options.showCmake = true;
            else if (args[i] == "--show-project-tasks")
                options.showProjectTasks = true;
            else if (args[i] == "--show-unreal")
                options.showUnreal = true;
            else if (args[i] == "--test-windows")
                options.testWindows = true;
            else if (args[i] == "--test-hidden") options.testHidden = true;
            else if (args[i] == "--test-highlights") options.testHighlights = true;
            else if (args[i] == "--test-frames") {
                frameLimit = std::stoi(next());
                options.testSession = true;
            } else if (args[i] == "--test-capture")
                capture = next();
            else if (args[i] == "--test-report")
                testReport = next();
            else if (args[i] == "--capture-panel")
                capturePanel = next();
            else if (args[i] == "--test-navigation")
                options.testNavigation = cycode::Json::parse(next());
            else if (args[i] == "--test-completion") options.testCompletion = cycode::Json::parse(next());
            else if (args[i] == "--test-keys") testKeys = cycode::Json::parse(next());
            else if (args[i] == "--test-project") testProject = next();
            else if (args[i] == "--test-wait-idle") testWaitIdle = true;
            else if (args[i] == "--capture") {
                capture = next();
                options.smoke = true;
                frameLimit = 60;
            } else if (args[i] == "--smoke-frames") {
                frameLimit = std::stoi(next());
                options.smoke = true;
            } else if (!args[i].starts_with('-')) {
                const auto path=std::filesystem::absolute(cycode::utf8Path(args[i]));
                if (cycode::imageFileExtension(args[i])) { options.workspace=path.parent_path();options.openFile=cycode::pathUtf8(path.filename()); }
                else options.workspace=path;
            }
        }
        automated = options.smoke || options.testSession;
        if (!capturePanel.empty() && (!automated || capture.empty())) throw std::runtime_error("Panel capture requires an automated capture session");
        if ((!testReport.empty() || !options.testNavigation.is_null() || !options.testCompletion.is_null() || options.testWindows || options.testHidden || options.testHighlights || !testKeys.empty() || !testProject.empty() || testWaitIdle) &&
            !options.testSession)
            throw std::runtime_error("Navigation test/report requires --test-frames");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#ifdef __APPLE__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        const char *glsl = "#version 150";
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        const char *glsl = "#version 330 core";
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        window = SDL_CreateWindow("CyCode", 1536, 960,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (options.testHidden ? SDL_WINDOW_HIDDEN : 0));
        if (!window)
            throw std::runtime_error(SDL_GetError());
        glContext = SDL_GL_CreateContext(window);
        if (!glContext)
            throw std::runtime_error(SDL_GetError());
        SDL_GL_MakeCurrent(window, glContext);
        SDL_GL_SetSwapInterval(1);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable |
                          ImGuiConfigFlags_ViewportsEnable;
        if (options.testHidden) io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoAutoMerge = true;
        io.ConfigViewportsNoTaskBarIcon = false;
        io.ConfigViewportsNoDefaultParent = true;
        io.ConfigDpiScaleFonts = true;
        io.ConfigDpiScaleViewports = true;
        std::filesystem::create_directories(options.installation / "data/settings");
        io.IniFilename = nullptr; // Application selects the workspace's layout and owns its filename.
        ImGui_ImplSDL3_InitForOpenGL(window, glContext);
        if (!ImGui_ImplOpenGL3_Init(glsl))
            throw std::runtime_error("OpenGL renderer initialization failed");
        {
            cycode::Application app(options, window);
            bool exit = false;
            int frames = 0, idleFrames = 0;
            Uint64 gateStarted = 0;
            double maximumObservedInputFrameSeconds = 0;
            const auto started = SDL_GetTicks();
            SDL_Event queued{};
            bool hasQueued = false;
            while (!exit) {
                SDL_Event event;
                auto handle = [&](SDL_Event &e) {
                    // Scripted keys enter through ImGui's real input queue below.
                    // Keep physical typing out of deterministic owned test fixtures.
                    if (!testKeys.empty() && (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP ||
                        e.type == SDL_EVENT_TEXT_INPUT || e.type == SDL_EVENT_TEXT_EDITING)) return;
                    ImGui_ImplSDL3_ProcessEvent(&e);
                    if(e.type==SDL_EVENT_WINDOW_SHOWN)
                        app.applyWindowIcon(SDL_GetWindowFromID(e.window.windowID));
                    if (e.type == SDL_EVENT_QUIT || (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                                                     e.window.windowID == SDL_GetWindowID(window)))
                        app.requestQuit();
                    if (e.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)
                        cyui::applyDpi(SDL_GetWindowDisplayScale(window));
                    if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) {
                        if (cycode::imageFileExtension(e.drop.data)) app.openImageFile(cycode::utf8Path(e.drop.data));
                        else app.openProject(cycode::utf8Path(e.drop.data));
                    }
                };
                if (hasQueued) {
                    handle(queued);
                    hasQueued = false;
                }
                while (SDL_PollEvent(&event))
                    handle(event);
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplSDL3_NewFrame();
                if (!testKeys.empty()) {
                    // Script events are authored in 60 Hz frame units. Keep their
                    // timing independent of a slow software-rendered CI frame.
                    // Physical input and native worker/deadline clocks stay real.
                    maximumObservedInputFrameSeconds=std::max(maximumObservedInputFrameSeconds,
                        static_cast<double>(io.DeltaTime));
                    io.DeltaTime=1.0f/60.0f;
                }
                if (!testProject.empty() && frames == 30) app.openProject(cycode::utf8Path(testProject));
                bool advanceTestFrame = true;
                for (const auto &event : testKeys) if (event.at("frame") == frames && event.contains("waitForCommand")) {
                    const auto command = event.at("waitForCommand").get<std::string>();
                    if (!app.testCommandReady(command)) {
                        advanceTestFrame = false;
                        if (!gateStarted) gateStarted = SDL_GetTicks();
                        if (SDL_GetTicks()-gateStarted > 30000) throw std::runtime_error("Timed out waiting for test command: "+command);
                    }
                }
                if (advanceTestFrame) gateStarted = 0;
                for (const auto &event : testKeys) if (advanceTestFrame && event.at("frame") == frames) {
                    for (const auto &value : event.at("keys")) {
                        const auto key = value.get<std::string>(); const bool down = event.value("down", true);
                        if (key == "Primary") {
                            // Script the same physical primary modifier as the platform.
                            // ImGui swaps Command/Control in macOS behavior mode.
                            io.AddKeyEvent(io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl, down);
                            io.AddKeyEvent(io.ConfigMacOSXBehaviors ? ImGuiKey_LeftSuper : ImGuiKey_LeftCtrl, down);
                        }
                        else if (key == "Ctrl") { io.AddKeyEvent(ImGuiMod_Ctrl, down); io.AddKeyEvent(ImGuiKey_LeftCtrl, down); }
                        else if (key == "Shift") { io.AddKeyEvent(ImGuiMod_Shift, down); io.AddKeyEvent(ImGuiKey_LeftShift, down); }
                        else if (key == "Alt") { io.AddKeyEvent(ImGuiMod_Alt, down); io.AddKeyEvent(ImGuiKey_LeftAlt, down); }
                        else if (key.size() == 1 && key[0] >= 'A' && key[0] <= 'Z') io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiKey_A + key[0] - 'A'), down);
                        else if (key.size() == 1 && key[0] >= '0' && key[0] <= '9') io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiKey_0 + key[0] - '0'), down);
                        else if (key.size() > 1 && key[0] == 'F') { auto number = std::stoi(key.substr(1)); if (number >= 1 && number <= 12) io.AddKeyEvent(static_cast<ImGuiKey>(ImGuiKey_F1 + number - 1), down); }
                        else if (key == "Space") io.AddKeyEvent(ImGuiKey_Space, down);
                        else if (key == "Tab") io.AddKeyEvent(ImGuiKey_Tab, down);
                        else if (key == "Up") io.AddKeyEvent(ImGuiKey_UpArrow, down);
                        else if (key == "Down") io.AddKeyEvent(ImGuiKey_DownArrow, down);
                        else if (key == "Left") io.AddKeyEvent(ImGuiKey_LeftArrow, down);
                        else if (key == "Right") io.AddKeyEvent(ImGuiKey_RightArrow, down);
                        else if (key == "Slash") io.AddKeyEvent(ImGuiKey_Slash, down);
                        else if (key == "Escape") io.AddKeyEvent(ImGuiKey_Escape, down);
                        else if (key == "Enter") io.AddKeyEvent(ImGuiKey_Enter, down);
                    }
                    if (event.contains("text")) io.AddInputCharactersUTF8(event.at("text").get<std::string>().c_str());
                }
                ImGui::NewFrame();
                app.frame();
                ImGui::Render();
                auto ground = cyui::vec(cyui::palette().bg0);
                SDL_GL_MakeCurrent(window, glContext);
                int width = 0, height = 0;
                SDL_GetWindowSizeInPixels(window, &width, &height);
                glViewport(0, 0, width, height);
                glClearColor(ground.x, ground.y, ground.z, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
                SDL_GL_MakeCurrent(window, glContext);
                if (advanceTestFrame) ++frames;
                // Async data can finish during this frame while ImGui queues
                // tab selection/font updates for the next one. Capture settled UI.
                idleFrames=app.busy()?0:std::min(idleFrames+1,4);
                if (frameLimit && frames >= frameLimit &&
                    (!options.smoke || app.readyToCapture() || SDL_GetTicks() - started > 15000) &&
                    (!testWaitIdle || idleFrames>=4 || SDL_GetTicks() - started > 180000) &&
                    (app.readyToFinishTests() || (!app.busy() && SDL_GetTicks() - started > 30000) || SDL_GetTicks() - started > 180000)) {
                    if (!testReport.empty()) {
                        auto path = cycode::utf8Path(testReport);
                        std::filesystem::create_directories(path.parent_path());
                        std::ofstream out(path);
                        auto state=app.testState();
                        if (!testKeys.empty()) state["scriptedInput"]={
                            {"fixedDeltaSeconds",1.0/60.0},
                            {"maximumObservedFrameSeconds",maximumObservedInputFrameSeconds}};
                        const auto graphics=[](GLenum field) {
                            const auto *value=glGetString(field);
                            return value?std::string(reinterpret_cast<const char *>(value)):std::string{};
                        };
                        state["graphics"]={{"vendor",graphics(GL_VENDOR)},
                            {"renderer",graphics(GL_RENDERER)},{"version",graphics(GL_VERSION)}};
                        out << state.dump(2);
                        if (!out)
                            throw std::runtime_error("Cannot write desktop test report");
                    }
                    if (!capture.empty()) {
                        if (!capturePanel.empty()) {
                            ImGuiWindow *panel = nullptr;
                            for (auto candidate : ImGui::GetCurrentContext()->Windows)
                                if (candidate->Active && std::string(candidate->Name).ends_with("###" + capturePanel)) panel = candidate;
                            if (!panel || !panel->Viewport || !panel->Viewport->DrawData) throw std::runtime_error("Capture panel is not visible: " + capturePanel);
                            auto draw = panel->Viewport->DrawData;
                            width = static_cast<int>(draw->DisplaySize.x * draw->FramebufferScale.x);
                            height = static_cast<int>(draw->DisplaySize.y * draw->FramebufferScale.y);
                            glClear(GL_COLOR_BUFFER_BIT);
                            ImGui_ImplOpenGL3_RenderDrawData(draw);
                        }
                        auto path = cycode::utf8Path(capture);
                        if (!path.parent_path().empty())
                            std::filesystem::create_directories(path.parent_path());
                        auto surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA32);
                        if (!surface)
                            throw std::runtime_error(SDL_GetError());
                        glPixelStorei(GL_PACK_ALIGNMENT, 1);
                        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
                        auto pixels = static_cast<unsigned char *>(surface->pixels);
                        for (int y = 0; y < height / 2; ++y)
                            std::swap_ranges(pixels + y * surface->pitch, pixels + (y + 1) * surface->pitch,
                                             pixels + (height - 1 - y) * surface->pitch);
                        bool saved = SDL_SaveBMP(surface, capture.c_str());
                        SDL_DestroySurface(surface);
                        if (!saved)
                            throw std::runtime_error(SDL_GetError());
                    }
                    exit = true;
                }
                SDL_GL_SwapWindow(window);
                if (options.smoke && frames >= frameLimit)
                    SDL_Delay(5);
                exit |= app.shouldQuit() && app.finishQuit();
                // Sleep on the OS event queue at rest instead of repainting at
                // monitor refresh rate. Caret animation still receives 30 Hz.
                if (!options.smoke && !app.busy() && !ImGui::IsAnyItemActive())
                    hasQueued = SDL_WaitEventTimeout(&queued, 33);
            }
        }
        ImGui::DestroyPlatformWindows();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    } catch (const std::exception &error) {
        std::filesystem::path log = (installation.empty() ? std::filesystem::current_path() : installation) /
                                    "data/reports/startup-error.txt";
        std::error_code ec;
        std::filesystem::create_directories(log.parent_path(), ec);
        std::ofstream out(log);
        out << error.what();
        if (!automated)
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "CyCode", error.what(), window);
        if (glContext)
            SDL_GL_DestroyContext(glContext);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
}
