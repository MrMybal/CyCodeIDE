#include "cycode/Application.h"
#include "cycode/UiFonts.h"
#include "cycode/Tools.h"
#include "ApplicationHelp.h"
#include "cycode/SymbolCatalogue.h"
#include "cycode/DiagnosticAnnotations.h"
#include "cycode/Solution.h"
#include "cycode/BuildLimits.h"
#include "cycode/Completion.h"
#include "cycode/TextEdits.h"
#include "cycode/EditorCompletion.h"
#include "cycode/EditorTheme.h"
#include "cycode/EditorLanguage.h"
#include "cycode/FileIcons.h"
#include "cycode/ImageView.h"
#include "cycode/WorkspaceSession.h"
#include <algorithm>
#include <cyui/Core.h>
#include <fstream>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <random>
#include <sstream>
namespace cycode {
template <class T> static bool ready(std::future<T> &f) {
    return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
static std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
static int desktopBuildTimeout(const Json &settings) {
    try {return int(buildTimeout(Json{{"timeoutSeconds",settings.value("buildTimeoutSeconds",Json(1800))}}).count());}
    catch(const std::exception &) {return 1800;}
}
int Application::configuredBuildTimeout() const {return desktopBuildTimeout(settings_);}
static const TextEditor::Language *languageFor(const std::string &path) {
    return editorLanguage(path);
}
static bool cppFile(const std::string &path) {
    return LanguageService::supportsCpp(path);
}
Application::Application(LaunchOptions options, SDL_Window *window)
    : root_(options.installation), window_(window), smoke_(options.smoke) {
    disablePlugins_=options.disablePlugins;
    testHighlights_=options.testHighlights;
    try {
        std::ifstream in(root_ / "app/locales/ui.json");
        translations_ = Json::parse(in);
    } catch (...) {
        translations_ = Json::object();
    }
    for (const auto *locale : {"en", "fr"}) {
        cyui::TranslationCatalog catalog;
        for (const auto &[key, value] : translations_.items())
            if (value.contains(locale)) catalog.emplace(key, value.at(locale).get<std::string>());
        cyui::registerTranslations(locale, catalog);
    }
    loadSettings();
    if (!options.keymap.empty()) { applyKeymap(options.keymap, keymapOverrides_.value(options.keymap, Json::object())); keymapProfile_ = options.keymap; }
    options_ = options.showOptions;
    focusCmake_ = options.showCmake;
    focusProjectTasks_ = options.showProjectTasks;
    focusUnreal_ = options.showUnreal;
    testSession_ = options.testSession;
    if (!options.terminalProfile.empty()) terminalProfile_ = options.terminalProfile;
    testWindows_ = options.testWindows;
    std::ostringstream identity;
    std::random_device random;
    identity << std::hex << std::chrono::steady_clock::now().time_since_epoch().count() << '-' << random()
             << random();
    instance_ = identity.str();
    if (testSession_)
        testNavigation_ = std::move(options.testNavigation);
    if (testSession_) testCompletion_ = std::move(options.testCompletion);
    if (!options.theme.empty())
        theme_ = options.theme;
    if (!options.language.empty())
        locale_ = options.language;
    cyui::Config config;
    config.themeKey = theme_.c_str();
    config.dpiScale = window_ ? SDL_GetWindowDisplayScale(window_) : 1.f;
    config.squareWindows = true;
    cyui::apply(config);
    cyui::setLocale(locale_.c_str());
    applyEditorTheme(theme_);
    fontResources_ = loadApplicationFonts(root_);
    ImGui::GetStyle().FontScaleMain = textScale_;
    auto workspace = options.workspace;
    if (workspace.empty())
        workspace = utf8Path(settings_.value("workspace", pathUtf8(root_)));
    if (options.workspace.empty() && !std::filesystem::exists(workspace))
        workspace = root_;
    setWorkspace(workspace);
    loadBranding();
    updateStartupTime_=ImGui::GetTime();
    if (!options.openFile.empty())
        open(options.openFile, options.openLine, options.openColumn, true);
    else if (sessionRestoreQueue_.empty() && std::filesystem::exists(workspace / "app/examples/hello/main.cpp"))
        open("app/examples/hello/main.cpp");
}
Application::~Application() {
    if(updateCancellation_) *updateCancellation_=true;
    if(updateCheckFuture_.valid())updateCheckFuture_.wait();
    if(updateDownloadFuture_.valid())updateDownloadFuture_.wait();
    if(updateInspectFuture_.valid())updateInspectFuture_.wait();
    if(updateHistoryFuture_.valid())updateHistoryFuture_.wait();
    if(updateStorageFuture_.valid())updateStorageFuture_.wait();
    if(updateStorageActionFuture_.valid())updateStorageActionFuture_.wait();
    if (unrealAnalysisCancellation_) *unrealAnalysisCancellation_ = true;
    auto waitWithBuildStop = [&](std::future<Json> &future) {
        while (future.valid() && future.wait_for(std::chrono::milliseconds(20)) != std::future_status::ready) {
            engine_->dispatch("build.cancel");
            engine_->dispatch("project.cancel");
        }
    };
    engine_->dispatch("build.cancel");
    engine_->dispatch("project.cancel");
    waitWithBuildStop(taskFuture_);
    if(testsFuture_.valid()){engine_->dispatch("tests.cancel");testsFuture_.wait();}
    if(templateFuture_.valid())templateFuture_.wait();
    if(resourceFuture_.valid())resourceFuture_.wait();
    if(recoveryFuture_.valid())recoveryFuture_.wait();
    if(workspacePollFuture_.valid())workspacePollFuture_.wait();
    if(extensionsRefreshFuture_.valid())extensionsRefreshFuture_.wait();
    for(auto &[id,view]:extensionViews_)waitWithBuildStop(view.pending);
    if (unrealAnalysisCancellation_) *unrealAnalysisCancellation_ = true;
    if (unrealAnalysisFuture_.valid()) unrealAnalysisFuture_.wait();
    if (searchCancellation_)
        *searchCancellation_ = true;
    engine_->configureLanguage(clangd_, false, false);
    for (const auto &tab : tabs_) for (auto *future : {&tab->completion,&tab->completionAccept,&tab->highlighting}) if (future->valid()) future->wait();
    for (auto &future : retiredCompletions_) if (future.valid()) future.wait();
    for (auto *future : {&filesFuture_, &searchFuture_, &gitFuture_, &taskFuture_, &navigationFuture_,
                         &buildFuture_, &presetFuture_, &detectionFuture_, &structure_.future, &occurrences_.future, &symbolFuture_, &symbolResolveFuture_, &informationFuture_, &hierarchyFuture_, &codeActionsFuture_, &refactorFuture_, &terminalFuture_, &debugFuture_, &vcsFuture_, &snippetFuture_, &userSnippetsFuture_, &counterpartFuture_})
        if (future->valid())
            future->wait();
    bridge_.reset();
    if (coreService_.valid())
        coreService_.wait();
    if (!smoke_ && !testSession_) {
        try { saveWorkspaceSession(true); } catch(const std::exception &) { /* Session metadata must not prevent preferences or exit. */ }
        try {
            if(!layoutIni_.empty())ImGui::SaveIniSettingsToDisk(layoutIni_.c_str());
            saveSettings();
        } catch (const std::exception &) { /* A read-only settings directory must not prevent exit. */
        }
    }
    tabs_.clear();
    brandingLogo_.reset();
    brandingIcon_.reset();
    synchronizeSyntax(Json::array());
    if (imageTextures_) imageTextures_->collect();
    imageLoader_.reset();
    ImGui::GetIO().IniFilename = nullptr;
}
const char *Application::tr(const char *key) const {
    auto found = translations_.find(key);
    if (found != translations_.end() && found->contains(locale_))
        return (*found)[locale_].get_ref<const std::string &>().c_str();
    return key;
}
std::string Application::label(const char *text, const char *id) const {
    return std::string(tr(text)) + "###" + id;
}
void Application::loadSettings() {
    try {
        std::ifstream in(root_ / "data/settings/ui-preferences.json");
        settings_ = Json::parse(in);
    } catch (...) {
        settings_ = Json::object();
    }
    checkUpdatesAtStartup_=!settings_.contains("checkUpdatesAtStartup") || !settings_.at("checkUpdatesAtStartup").is_boolean() || settings_.at("checkUpdatesAtStartup").get<bool>();
    if(settings_.contains("skippedUpdate") && settings_.at("skippedUpdate").is_string())skippedUpdate_=settings_.at("skippedUpdate").get<std::string>();
    try {if(!skippedUpdate_.empty())(void)updates::version(skippedUpdate_);}catch(...){skippedUpdate_.clear();}
    restoreWorkspaceSession_=settings_.value("restoreWorkspaceSession",true);
    theme_ = settings_.value("theme", theme_);
    if (settings_.value("schemaVersion", 1) < 2 && theme_ == "navy")
        theme_ = "graphite";
    locale_ = settings_.value("language", locale_);
    keymapProfile_ = settings_.value("keymap", keymapProfile_);
    keymapOverrides_ = settings_.value("keymapOverrides", Json::object());
    if(!keymapOverrides_.is_object())keymapOverrides_=Json::object();
    try { applyKeymap(keymapProfile_, keymapOverrides_.value(keymapProfile_, Json::object())); }
    catch (const std::exception &e) { lastError_ = e.what(); keymapProfile_ = "rider"; keymap_.load(keymapProfile_); }
    textScale_ = settings_.value("textScale", 1.15f);
    if (settings_.value("schemaVersion", 1) < 3 && settings_.contains("textScale")) textScale_ *= 1.15f;
    textScale_ = std::clamp(textScale_, 0.8f, 2.0f);
    clangd_ = settings_.value("clangd", clangd_);
#ifdef _WIN32
    if (clangd_ == "clangd" && std::filesystem::exists("C:/Program Files/LLVM/bin/clangd.exe"))
        clangd_ = "C:/Program Files/LLVM/bin/clangd.exe";
#endif
}
void Application::saveSettings() {
    const auto buildLimit=desktopBuildTimeout(settings_);
    std::filesystem::create_directories(root_ / "data/settings");
    settings_ = {{"schemaVersion", 3}, {"checkUpdatesAtStartup",checkUpdatesAtStartup_}, {"skippedUpdate",skippedUpdate_},      {"theme", theme_},   {"language", locale_},
                 {"textScale", textScale_}, {"clangd", clangd_}, {"workspace", pathUtf8(entry_)},
                 {"keymap", keymapProfile_}, {"keymapOverrides", keymapOverrides_}, {"buildTimeoutSeconds",buildLimit},{"restoreWorkspaceSession",restoreWorkspaceSession_}};
    auto destination = root_ / "data/settings/ui-preferences.json";
    auto temporary = destination;
    temporary += "." + instance_ + ".tmp";
    {
        std::ofstream out(temporary);
        out << settings_.dump(2);
        if (!out)
            throw std::runtime_error("Cannot save preferences");
    }
    if (!SDL_RenamePath(pathUtf8(temporary).c_str(), pathUtf8(destination).c_str())) {
        std::filesystem::remove(temporary);
        throw std::runtime_error(SDL_GetError());
    }
}
bool Application::busy() const {
    if(counterpartFuture_.valid())return true;
    if(snippetFuture_.valid())return true;
    if(userSnippetsFuture_.valid())return true;
    if (engine_ && engine_->dispatch("build.status").value("running", false)) return true;
    if (engine_ && engine_->dispatch("project.status").value("running", false)) return true;
    if(testsFuture_.valid())return true;
    if(templateFuture_.valid())return true;
    if(resourceFuture_.valid())return true;
    if(recoveryFuture_.valid())return true;
    for (const auto &tab : tabs_) if ((tab->image && tab->image->loading()) || tab->highlighting.valid() || tab->completionAccept.valid()) return true;
    return filesFuture_.valid() || searchFuture_.valid() || gitFuture_.valid() || taskFuture_.valid() ||
           navigationFuture_.valid() || buildFuture_.valid() || detectionFuture_.valid() || unrealAnalysisFuture_.valid() || structure_.future.valid() || occurrences_.future.valid() || symbolFuture_.valid() || symbolResolveFuture_.valid() || informationFuture_.valid() || codeActionsFuture_.valid() || refactorFuture_.valid() || terminalFuture_.valid() || debugFuture_.valid() || vcsFuture_.valid();
}
Json Application::testState() const {
    Json result = {{"active", active_},
                   {"navigation", navigation_},
                   {"error", lastError_},
                   {"languageReady", languageState_.value("ready", false)}};
    result["build"] = buildState_;
    result["msbuild"] = msbuildState_; result["msbuildActive"] = msbuildActive_;
    result["projectTasks"] = projectState_; result["projectActive"] = projectActive_;
    result["tests"]={{"catalogue",testsCatalog_},{"results",testsResults_},{"error",testsError_}};
    result["vcsHunks"]=vcsHunks_;
    result["buildDiagnostics"] = buildDiagnostics_;
    result["toolchains"] = toolchains_;
    result["solution"] = solution_;
    result["unreal"] = unreal_;
    result["unrealAnalysis"] = unrealAnalysisState_;
    result["workspace"] = pathUtf8(engine_->workspace());
    result["entry"] = pathUtf8(entry_);
    result["theme"] = theme_;
    result["textScale"] = textScale_;
    result["fileCount"] = files_.size();
    result["ui"] = {{"quickOpen", quickOpenVisible_}, {"lastShortcut", lastShortcut_}};
    result["ui"]["branding"]={{"logo",bool(brandingLogo_)},{"windowIcon",brandingWindowIcon_},{"error",brandingError_}};
    result["ui"]["fonts"] = fontResources_;
    result["ui"]["paletteMode"] = paletteMode_;
    result["ui"]["keymap"] = keymapProfile_;
    result["ui"]["options"] = options_;
    result["ui"]["editorFocused"] = editorFocused_;
    if (auto focused = ImGui::GetCurrentContext()->NavWindow) result["ui"]["focusedWindow"] = focused->Name;
    result["ui"]["commands"] = executedCommands_;
    result["ui"]["searchQuery"] = searchQuery_;
    result["ui"]["paletteQuery"] = paletteFilter_;
    result["ui"]["search"] = search_;
    result["symbols"] = symbolResults_;
    result["information"] = {{"result",informationState_},{"error",informationError_},{"method",informationMethod_},{"query",informationQuery_}};
    result["refactoring"] = refactorPlan_;
    result["codeActions"]=codeActionsState_;
    result["terminals"] = engine_->terminals().list();
    result["debug"] = engine_->debugger().status();
    result["vcs"] = {{"status",git_},{"details",vcsInfo_},{"history",vcsHistory_},{"conflict",vcsConflict_},{"section",vcsSection_},{"preview",vcsPreviewTitle_}};
    result["vcs"]["tabs"]=Json::array();
    if(auto *window=ImGui::FindWindowByName(label("Version control","vcs").c_str()))if(auto *bar=ImGui::GetCurrentContext()->TabBars.GetByKey(window->GetID("##vcs-sections"))) {
        result["vcs"]["tabRect"]={bar->BarRect.Min.x,bar->BarRect.Min.y,bar->BarRect.Max.x,bar->BarRect.Max.y};
        result["vcs"]["visibleTab"]=bar->VisibleTabContentsId;
        for(auto &tab:bar->Tabs)result["vcs"]["tabs"].push_back({{"id",tab.ID},{"name",ImGui::TabBarGetTabName(bar,&tab)},{"selected",tab.ID==bar->SelectedTabId},{"visible",tab.ContentsIsVisible},{"width",tab.Width},{"offset",tab.Offset}});
    }
    if (!terminalId_.empty()) {
        try { result["terminal"] = engine_->dispatch("terminal.read", {{"id", terminalId_}, {"limit", 2000}}); } catch (const std::exception &) {}
    }
    result["instance"] = instance_;
    result["windowTest"] = windowTest_;
    result["nativeWindows"] = ImGui::GetPlatformIO().Viewports.Size;
    result["hiddenWindow"] = bool(SDL_GetWindowFlags(window_) & SDL_WINDOW_HIDDEN);
    for (const auto &tab : tabs_)
        if (tab->path == active_) {
            if (tab->image) {
                result["image"]={{"path",tab->path},{"loading",tab->image->loading()},{"error",tab->image->error()}};
                if (auto texture=tab->image->texture()) result["image"]["size"]={texture->data.width,texture->data.height};
            }
            result["text"] = tab->editor.GetText();
            result["dirty"] = tab->dirty;
            result["readOnly"] = tab->readOnly;
            result["displayPath"] = tab->displayPath;
            result["syntax"]={{"language",Syntax::language(tab->path)},{"spans",tab->syntax.spans()},{"truncated",tab->syntax.truncated()}};
            result["highlight"]={{"revision",tab->highlightRevision},{"generation",tab->highlightGeneration},{"count",tab->highlightCount},{"truncated",tab->highlightTruncated},{"error",tab->highlightError}};
            result["completionTest"] = {{"phase", testCompletionPhase_}, {"generation", tab->completionGeneration}, {"runningGeneration", tab->runningCompletionGeneration}, {"wordStart", {tab->runningCompletionStart.line, tab->runningCompletionStart.index}}, {"end", {tab->completionPosition.line, tab->completionPosition.index}}};
            result["completionTest"]["reply"] = tab->completionTrace;
            result["completionSuggestions"] = tab->lastSuggestions;
            const auto position = tab->editor.GetMainCursorPosition();
            result["cursor"] = {{"line", position.line},
                                {"codepoint", position.index},
                                {"character", LanguageClient::utf16Length(
                                                  tab->editor.GetSectionText({position.line, 0}, position))}};
        }
    return result;
}
void Application::setWorkspace(const std::filesystem::path &path) {
    // Validate the requested solution before tearing down the current session.
    auto nextEngine = std::make_shared<Engine>(path, smoke_ || testSession_ ? std::filesystem::path{} : std::filesystem::absolute(root_ / "data/settings/language-servers"),
        smoke_ || testSession_ ? std::filesystem::path{} : std::filesystem::absolute(root_ / "data/settings/snippets"));
    auto nextSolution = nextEngine->dispatch("workspace.describe").at("solution");
    auto nextUnreal = nextEngine->dispatch("unreal.inspect");
    const bool inspectMsbuild = !nextUnreal.value("available",false) && (!std::filesystem::is_directory(path) || (!std::filesystem::exists(nextEngine->workspace()/"CMakeLists.txt") && !std::filesystem::exists(nextEngine->workspace()/"app/CMakeLists.txt")));
    const auto nextMsbuild = inspectMsbuild ? nextEngine->dispatch("msbuild.inspect") : Json::object();
    if (engine_) {
        if(engine_->dispatch("build.status").value("running",false))throw std::runtime_error(tr("Wait for current tasks before switching workspace."));
        if(engine_->dispatch("project.status").value("running",false))throw std::runtime_error(tr("Wait for current tasks before switching workspace."));
        if(templateFuture_.valid())throw std::runtime_error(tr("Wait for current tasks before switching workspace."));
        if(resourceFuture_.valid())throw std::runtime_error(tr("Wait for current tasks before switching workspace."));
        if(recoveryFuture_.valid()) { recoveryFuture_.wait();recoveryFuture_={}; }
        if(workspacePollFuture_.valid()) { workspacePollFuture_.wait();workspacePollFuture_={}; }
        if(extensionsRefreshFuture_.valid()) { extensionsRefreshFuture_.wait();extensionsRefreshFuture_={}; }
        for(const auto &[id,view]:extensionViews_)if(view.pending.valid())throw std::runtime_error(tr("Wait for current tasks before switching workspace."));
        for (const auto &tab : tabs_)
            if (tab->dirty)
                throw std::runtime_error(tr("Save or close modified documents before switching workspace."));
        for (const auto &tab : tabs_) if (tab->completionAccept.valid()) throw std::runtime_error(tr("Wait for current tasks before switching workspace."));
        for (auto *future : {&testsFuture_,&filesFuture_, &searchFuture_, &gitFuture_, &taskFuture_, &navigationFuture_,
                             &buildFuture_, &presetFuture_, &detectionFuture_, &structure_.future, &occurrences_.future, &symbolFuture_, &symbolResolveFuture_, &informationFuture_, &hierarchyFuture_, &codeActionsFuture_, &refactorFuture_, &terminalFuture_, &debugFuture_, &vcsFuture_, &counterpartFuture_})
            if (future->valid())
                throw std::runtime_error(tr("Wait for current tasks before switching workspace."));
        engine_->configureLanguage(clangd_, false, false);
    }
    bridge_.reset();
    if (coreService_.valid())
        coreService_.get();
    for (const auto &tab : tabs_) {
        if (tab->highlighting.valid()) retiredCompletions_.push_back(std::move(tab->highlighting));
        if (tab->completion.valid()) retiredCompletions_.push_back(std::move(tab->completion));
    }
    if(workspaceSession_) {
        try{saveWorkspaceSession(true);}catch(const std::exception &e){cyui::notify(e.what(),cyui::Tone::warning);}
        workspaceSession_.reset();
    }
    if(vcsRepositoriesFuture_.valid())try{vcsRepositoriesFuture_.get();}catch(...){}
    vcsRepository_.clear();vcsRepositoryIdentity_.clear();vcsWorktreeReview_=Json::object();vcsRepositories_=nullptr;vcsWorktrees_=Json::object();vcsWorktreesRequested_=false;vcsWorktreeRemoveOpen_=false;vcsWorktreeRemoval_=Json::object();
    vcsSubmodules_=Json::object();vcsSubmoduleReview_=Json::object();vcsSubmodulesRequested_=false;
    vcsSubmoduleAddPlan_=Json::object();vcsSubmoduleAddOpen_=false;vcsSubmoduleAddUrl_.clear();vcsSubmoduleAddPath_.clear();vcsSubmoduleAddName_.clear();vcsSubmoduleAddBranch_.clear();
    sessionRestoreQueue_.clear();
    liveBuildState_=Json::object();liveBuildText_.clear();liveBuildError_.clear();lastLiveBuildPoll_=-1;
    tabs_.clear();
    saveAsQueue_.clear();saveAsDocument_=0;saveAsOpen_=false;saveAsError_.clear();
    languageDecisions_.clear();
    languageSettingsBackend_.clear();languageSettingsError_.clear();
    extensionViews_.clear();
    historyBack_.clear(); historyForward_.clear(); recentFiles_.clear(); closedFiles_.clear(); keymap_.cancel(); closeAll_ = false;
    clearOccurrences();
    occurrences_ = OccurrenceView{};
    structure_ = StructureView{};
    ++navigationTicket_; navigation_=Json::object(); navigationModel_.set(Json::array());
    navigationError_.clear(); navigationPath_.clear(); navigationSelected_=-1; navigationFilter_.query.clear();
    symbolBackend_.clear();
    symbolResults_ = Json::object(); symbolWantedKey_.clear(); symbolSentKey_.clear(); symbolResolveKey_.clear(); symbolResolved_=Json::object(); refactorPlan_ = nullptr; refactorPreview_ = nullptr;
    refactorAnnotationPlan_.clear();refactorConfirmed_.clear();
    panels_.try_emplace("refactoring", false);
    panels_.try_emplace("information", false);
    panels_.try_emplace("cmakePresets",false);presetInitialized_=false;presetCatalogue_=nullptr;presetPlan_=nullptr;presetName_.clear();presetError_.clear();
    panels_.try_emplace("hierarchy",false);hierarchyState_=Json::object();hierarchySelected_=Json::object();hierarchyBranches_.clear();hierarchyBranchErrors_.clear();hierarchyError_.clear();hierarchyValid_=false;hierarchyBytes_=0;
    panels_.try_emplace("codeActions",false);codeActionsState_=Json::object();codeActionsKey_.clear();codeActionsError_.clear();codeActionSelected_=-1;
    informationMethod_.clear(); informationWanted_.clear(); informationSent_.clear(); informationError_.clear();
    informationState_ = Json::object(); informationQuery_ = nullptr;
    panels_.try_emplace("terminal", false);
    terminalId_.clear(); terminalFocused_ = false; focusTerminal_ = false;
    debugProfileLoaded_ = false; debugState_ = Json::object(); debugChildren_ = Json::object(); debugJumpKey_.clear(); debugCurrentPath_.clear(); pendingDebugBuild_ = false;
    debugUnrealLaunch_ = nullptr; pendingUnrealDebug_ = Json::object();
    debugVariablePages_=debugScopePage_=debugMemory_=debugAssembly_=debugException_=Json::object();debugScopeReference_=debugScopeStart_=0;debugEvaluationMemory_.clear();
    debugBreakpointDrafts_ = Json::object(); debugCurrentLine_ = 0;
    vcsInfo_ = vcsHistory_ = vcsConflict_ = Json::object(); vcsSection_ = "Changes"; vcsPreviewTitle_.clear();
    clearGitConflicts();
    clearGitCheckout();
    clearUnrealAutomation();clearUnrealLaunch();unrealArtifacts_=Json::object();unrealArtifactsBound_=true;clearGitReplay();clearGitMerge();
    vcsHunks_=Json::object();vcsHunkSelection_.clear();vcsHunkDiff_={};vcsHunkDiffState_={};
    testsCatalog_=testsResults_=Json::object();testsSelection_.clear();testsError_.clear();panels_.try_emplace("tests",false);
    terminalSelectionStart_ = terminalSelectionEnd_ = {-1, 0};
    active_.clear();
    files_.clear();
    workspaceIndexRevision_=0;lastWorkspaceCheck_=-10;
    tree_ = {};
    lastError_.clear();
    search_ = Json::object();
    navigation_ = Json::object();
    buildState_ = Json::object();
    buildDiagnostics_ = Json::array();
    problemBuild_ = problemLanguage_ = nullptr; problems_ = {};
    toolchains_ = Json::object();
    buildEdited_ = false;
    buildToolchain_ = "automatic";
    buildGenerator_ = "automatic";
    buildTarget_.clear();
    engine_ = std::move(nextEngine);
    projectTemplates_.reset();templateModal_=false;templatePlan_=Json::object();
    resourcePlan_=resourceLast_=resourceItems_=resourceTrash_=Json::object();resourceFolder_=".";selectedResource_.clear();
    resourceError_.clear();lastResourceRevision_=0;resourceModal_=false;resourceAssets_={};resourceAssets_.grid=false;
    resourceAssetPaths_.clear();resourceFavorites_.clear();lastResourceRefresh_=-10;panels_.try_emplace("resources",false);
    recoveryInfo_=recoveryPreview_=Json::object();recoveryDiscard_=nullptr;
    recoveryError_.clear();recoveryHealthError_.clear();recoveryInitialCheck_=true;recoveryConfirm_=false;
    recoveryCode_={};recoveryCodeState_={};lastRecoveryRefresh_=-10;
    panels_.try_emplace("recovery",false);
    recoveryTask("recovery.list");
    solution_ = std::move(nextSolution);
    solutionConfiguration_ = solution_.is_object() && !solution_.at("configurations").empty()
                                 ? solution_.at("configurations")[0].get<std::string>()
                                 : "";
    applyUnreal(nextUnreal);
    msbuildState_ = Json::object(); msbuildActive_ = false;
    msbuildActive_ = nextMsbuild.value("available",false); if (msbuildActive_) applyBuild(nextMsbuild);
    projectActive_ = !unreal_.value("available",false) && !msbuildActive_ && !ProjectTasks::detect(engine_->workspace()).empty();
    liveBuildSource_=projectActive_?1:0;projectCancellationId_.clear();selectedProblem_=0;
    problems_={};problemBuild_=nullptr;problemLanguage_=nullptr;
    projectState_ = Json::object(); projectProfile_ = Json::object();
    unrealLanguage_ = *editorLanguage("source.cpp",true);
    entry_ = !std::filesystem::is_directory(path)
                 ? std::filesystem::canonical(path)
                 : (solution_.is_object() ? utf8Path(solution_.at("path")) : engine_->workspace());
    if (!smoke_ && !testSession_) {
        if (!layoutIni_.empty())
            ImGui::SaveIniSettingsToDisk(layoutIni_.c_str());
        // A stable workspace key keeps layouts for different projects independent.
        auto key = pathUtf8(entry_);
#ifdef _WIN32
        key = lower(key);
#endif
        std::uint64_t hash = 14695981039346656037ULL;
        for (unsigned char c : key) {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        auto directory = root_ / "data/settings/workspaces";
        std::filesystem::create_directories(directory);
        ImGui::ClearIniSettings();
        beginWorkspaceSession(directory/(std::to_string(hash)+".ini"),key);
        layoutReady_ = false;
    }
    unrealAnalysisState_.clear();
    engine_->configureLanguage(clangd_, !smoke_ && !unreal_.value("available", false), true);
    if(!disablePlugins_)engine_->plugins().loadInstalled(root_);
    pluginInfo_ = engine_->dispatch("plugins.list");
    synchronizeExtensions();
    auto instance =
        instance_ + '-' + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    bridge_ = std::make_unique<Bridge>(*engine_, root_ / ("data/bridge/native-" + instance + ".json"));
    coreDescriptor_ = root_ / ("data/bridge/core-" + instance + ".json");
    const auto python=mcpInterpreterPath(root_);
    if (!smoke_ && std::filesystem::exists(python)) {
        std::vector<std::string> command{pathUtf8(python), "-B", "-X", "utf8",
                                         pathUtf8(root_ / "app/tool.py"),
                                         "--desktop-service",
                                         "--native-descriptor",
                                         pathUtf8(bridge_->descriptor()),
                                         "--service-descriptor",
                                         pathUtf8(coreDescriptor_),
                                         "--data-dir",
                                         pathUtf8(root_ / ("data/jobs/instance-" + instance))};
        auto directory = root_;
        coreService_ = std::async(std::launch::async, [command, directory] {
            return runProcess(command, directory, std::chrono::hours(24), 1024 * 1024);
        });
    }
    buildSource_ = std::filesystem::exists(engine_->workspace() / "CMakeLists.txt")       ? "."
                   : std::filesystem::exists(engine_->workspace() / "app/CMakeLists.txt") ? "app"
                                                                                          : ".";
    output_ = std::string(tr("Workspace opened: ")) + pathUtf8(path) + "\n";
    refreshFiles();
    refreshGit();
    refreshBuild(!smoke_, true);
    if (!smoke_ && unreal_.value("available", false)) prepareUnreal();
    if (window_) SDL_SetWindowTitle(window_,
                       ("CyCodeIDE  |  " + (solution_.is_object() ? solution_.at("name").get<std::string>()
                                                               : pathUtf8(engine_->workspace().filename())))
                           .c_str());
}
void Application::openProject(const std::filesystem::path &path) {
    pendingWorkspace_ = pathUtf8(path);
    if (unrealAnalysisCancellation_) *unrealAnalysisCancellation_ = true;
}
void Application::prepareUnreal() {
    if (unrealAnalysisFuture_.valid() || !unreal_.value("available", false)) return;
    unrealAnalysisState_ = "preparing";
    auto engine = engine_; auto params = buildParameters();
    params["timeoutSeconds"]=desktopBuildTimeout(settings_);
    unrealAnalysisCancellation_ = std::make_shared<std::atomic<bool>>(false);
    auto cancellation = unrealAnalysisCancellation_;
    unrealAnalysisFuture_ = std::async(std::launch::async, [engine, params, cancellation] { return engine->prepareUnrealAnalysis(params, cancellation.get()); });
}
void Application::refreshFiles(bool force) {
    if (filesFuture_.valid())
        return;
    auto engine = engine_;
    filesFuture_ = std::async(std::launch::async, [engine,force] {
        auto result=engine->dispatch("workspace.files",{{"refresh",force}});
        const auto revision=result.at("indexRevision").get<uint64_t>(),generation=result.at("generation").get<uint64_t>();size_t attempts=0;
        while(!result.at("nextOffset").is_null()&&++attempts<32) {
            const auto page=engine->dispatch("workspace.files",{{"offset",result.at("nextOffset")}});
            if(!fileIndexPageMatches(revision,generation,page.at("indexRevision").get<uint64_t>(),page.at("generation").get<uint64_t>())) {
                result["indexRevision"]=0;result["generation"]=0;result["truncated"]=true;result["nextOffset"]=nullptr;break;
            }
            for(const auto &file:page.at("files"))result["files"].push_back(file);
            result["nextOffset"]=page.at("nextOffset");result["truncated"]=page.at("truncated");
        }
        return result;
    });
}
Json Application::buildParameters() const {
    if (projectActive_) return projectProfile_;
    if (unreal_.value("available", false))
        return {{"project", unrealProject_},
                {"engineRoot", unrealEngine_},
                {"target", unrealTarget_},
                {"platform", unrealPlatform_},
                {"configuration", unrealConfiguration_}};
    if (msbuildActive_) return {{"entry",msbuildState_.at("profile").at("entry")},{"configuration",msbuildConfiguration_},{"platform",msbuildPlatform_},{"project",msbuildProject_},{"tool",msbuildTool_}};
    return {{"source", buildSource_},          {"buildDirectory", buildDirectory_},
            {"configuration", configuration_}, {"toolchain", buildToolchain_},
            {"generator", buildGenerator_},    {"target", buildTarget_},
            {"configurePreset",buildConfigurePreset_},{"buildPreset",buildBuildPreset_}};
}
void Application::applyBuild(const Json &project) {
    if (project.contains("kind")) {
        projectState_ = project;
        if (!buildEdited_ || projectProfile_.empty()) projectProfile_ = project.at("profile");
        return;
    }
    if (project.contains("profile") && project.at("profile").contains("entry")) {
        msbuildState_ = project;
        if (msbuildState_.at("tools").empty() && toolchains_.contains("tools")) msbuildState_["tools"] = toolchains_.at("tools");
        if (!buildEdited_) { const auto &profile = project.at("profile"); msbuildConfiguration_ = profile.at("configuration"); msbuildPlatform_ = profile.at("platform"); msbuildProject_ = profile.at("project"); msbuildTool_ = profile.at("tool"); solutionConfiguration_ = msbuildConfiguration_+"|"+msbuildPlatform_; }
        return;
    }
    buildState_ = project;
    if(buildEdited_ && project.contains("profile")) {
        const auto &profile=project.at("profile");
        if((!buildConfigurePreset_.empty() || !buildBuildPreset_.empty()) && profile.value("configurePreset","")==buildConfigurePreset_ && profile.value("buildPreset","")==buildBuildPreset_) {
            buildDirectory_=profile.at("buildDirectory");configuration_=profile.at("configuration");
        }
    }
    if (buildEdited_ || !project.contains("profile"))
        return;
    const auto &profile = project.at("profile");
    buildSource_ = profile.at("source");
    buildDirectory_ = profile.at("buildDirectory");
    configuration_ = profile.at("configuration");
    buildToolchain_ = profile.at("toolchain");
    buildGenerator_ = profile.at("generator");
    buildTarget_ = profile.at("target");
    buildConfigurePreset_=profile.value("configurePreset","");buildBuildPreset_=profile.value("buildPreset","");
}
void Application::refreshBuild(bool detectTools, bool initial) {
    if (unreal_.value("available", false))
        return;
    auto engine = engine_;
    if (!buildFuture_.valid()) {
        auto parameters = !initial && buildEdited_ ? buildParameters() : Json::object();
        const auto method = projectActive_ ? "project.inspect" : msbuildActive_ ? "msbuild.inspect" : "build.inspect";
        buildFuture_ = std::async(std::launch::async, [engine, parameters, method] {
            return engine->dispatch(method, parameters);
        });
        lastBuildRefresh_ = ImGui::GetTime();
    }
    if (detectTools && !projectActive_ && !detectionFuture_.valid()) {
        const auto method = msbuildActive_ ? "msbuild.tools" : "toolchains.detect";
        detectionFuture_ = std::async(std::launch::async, [engine,method] {
            return engine->dispatch(method, {{"refresh", true}});
        });
    }
}
void Application::refreshGit() {
    if (gitFuture_.valid())
        return;
    auto engine = engine_;
    const auto explicitRepository=vcsRepository_;
    const auto path=vcsRequestPath();
    gitFuture_ = std::async(std::launch::async, [engine, path,explicitRepository] {
        Json query={{"contextPath",path}};
        if(!explicitRepository.empty()){query["provider"]="git";query["repository"]=explicitRepository;}
        const auto context=engine->dispatch("vcs.context",query);
        Json result = {{"context",context},{"contextPath",path}};
        Json request={{"provider",context.at("provider")},{"contextPath",path}};
        if(context.at("provider")=="git"){request["repository"]=context.at("gitRepository");request["expectedRepository"]=context.at("gitIdentity");}
        const std::string provider=context.at("provider");
        const auto caps=context.at("capabilities");
        if(!context.at("available").get<bool>())result["status"]={{"repository",false},{"files",Json::array()},{"message","Configured provider is unavailable"}};
        else if(std::find(caps.begin(),caps.end(),"status")!=caps.end())result["status"]=engine->dispatch("vcs.status",request);
        else result["status"]={{"repository",false},{"files",Json::array()},{"message","Provider does not advertise status"}};
        if (provider == "git") { result["details"] = engine->dispatch("vcs.inspect",request); if (result.at("details").value("repository",false)) result["history"] = engine->dispatch("vcs.history",request); }
        return result;
    });
    lastGitRefresh_ = ImGui::GetTime();
}
bool Application::vcsSupports(const std::string &action) const {
    if (provider_ == "git")
        return true;
    for (const auto &provider : pluginInfo_.value("vcsProviders", Json::array())) {
        if (provider.value("id", std::string{}) != provider_)
            continue;
        const auto &capabilities = provider["capabilities"];
        return std::find(capabilities.begin(), capabilities.end(), action) != capabilities.end();
    }
    return false;
}
void Application::error(const std::string &message) {
    lastError_ = message;
    output_ += "\n" + message + "\n";
    cyui::notify(message.c_str(), cyui::Tone::error, 6);
}
void Application::task(const std::string &method, Json params) {
    auto actualMethod = method;
    if (unreal_.value("available", false)) {
        if (method == "build.configure")
            actualMethod = "unreal.generate";
        if (method == "build.run")
            actualMethod = "unreal.build";
    }
    if (msbuildActive_) { if (method == "build.configure") actualMethod = "msbuild.restore"; if (method == "build.run") actualMethod = "msbuild.build"; }
    if (projectActive_) { if (method == "build.configure") actualMethod = "project.configure"; if (method == "build.run") actualMethod = "project.build"; }
    if (actualMethod.starts_with("unreal.") && actualMethod!="unreal.automationPlan" && actualMethod!="unreal.automationRun" && actualMethod!="unreal.launchProfile" && actualMethod!="unreal.artifacts" && !((actualMethod=="unreal.build"||actualMethod=="unreal.inspect")&&!params.empty()))
        params = buildParameters();
    if (method == "build.configure" || method == "build.run")
        params = buildParameters();
    if (actualMethod.starts_with("msbuild.")) { params = buildParameters(); if (msbuildRestore_ && (actualMethod == "msbuild.build" || actualMethod == "msbuild.rebuild")) params["restore"] = true; }
    if (actualMethod.starts_with("project.")) params = projectProfile_;
    if(supportsBuildTimeout(actualMethod)&&actualMethod!="unreal.automationRun")params["timeoutSeconds"]=desktopBuildTimeout(settings_);
    if (actualMethod == "msbuild.build" || actualMethod == "msbuild.rebuild" || actualMethod == "build.presetRun" || actualMethod.starts_with("project.")) {
        saveAll();
        if (std::any_of(tabs_.begin(),tabs_.end(),[](const auto &tab) { return tab->dirty; })) return;
    }
    if (method.starts_with("vcs."))
        params["provider"] = provider_;
    if (taskFuture_.valid()) {
        error(tr("A task is already running."));
        return;
    }
    auto engine = engine_;
    taskMethod_ = actualMethod;
    taskOperationId_.clear();
    buildCancellationId_.clear();
    projectCancellationId_.clear();
    if (BuildOperations::supports(actualMethod) || actualMethod.starts_with("project.")) {
        static std::atomic<uint64_t> sequence{0};
        taskOperationId_ = "gui-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                           "-" + std::to_string(++sequence);
        params["operationId"] = taskOperationId_;
    }
    if (actualMethod != "unreal.launchProfile" && actualMethod != "unreal.artifacts") showPanel("Build","build");
    output_ += "\n> " + actualMethod + "\n";
    taskFuture_ = std::async(std::launch::async, [engine, actualMethod, params] {
        return engine->dispatch(actualMethod, params);
    });
}
void Application::configureEditor(const std::shared_ptr<Tab> &tab) {
    configureCodeEditor(tab->codeEditor);
    tab->codeEditor.setLanguageForPath(tab->displayPath);
    // Rich project-aware lexical/semantic overlays keep their existing native hook.
    tab->codeEditor.setSyntaxEnabled(!cyui::codeLanguageInfo(tab->codeEditor.language()).builtin||!languageUsesLegacyLexer(tab->displayPath));
    tab->snippet.attach(tab->editor);
    if (languageUsesLegacyLexer(tab->displayPath)) tab->editor.SetLanguage(unreal_.value("available", false) && cppFile(tab->path) ? &unrealLanguage_ : languageFor(tab->displayPath));
    tab->syntax.update(tab->editor,tab->path,tab->editor.GetText());
    tab->editor.SetTabSize(4);
    tab->editor.SetInsertSpacesOnTabs(true);
    tab->editor.SetShowMiniMapEnabled(false);
    tab->editor.SetLineFoldingEnabled(true);
    tab->editor.SetReadOnlyEnabled(tab->readOnly);
    tab->editor.SetLineDecorator(2, [this, weak = std::weak_ptr<Tab>(tab)](TextEditor::Decorator &line) {
        auto tab = weak.lock(); if (!tab) return;
        const auto at = ImGui::GetCursorScreenPos(); auto draw = ImGui::GetWindowDrawList();
        bool breakpoint = false, enabled = true, verified = false;
        if (debugState_.contains("breakpoints")) {
            const auto &points = debugState_.at("breakpoints");
            if (points.contains(tab->path)) for (const auto &point : points.at(tab->path)) if (point.value("line",0) == int(line.line+1)) { breakpoint = true; enabled = point.value("enabled",true); }
            if (debugState_.contains("verifiedBreakpoints") && debugState_.at("verifiedBreakpoints").contains(tab->path))
                for (const auto &point : debugState_.at("verifiedBreakpoints").at(tab->path)) if (point.value("line",0) == int(line.line+1)) verified = point.value("verified",false);
        }
        if (breakpoint) {
            const ImVec2 center{at.x+line.width*.36f,at.y+line.height*.5f}; const auto color = enabled ? cyui::palette().error : cyui::palette().textFaint;
            if (verified || debugState_.value("state","idle") == "idle") draw->AddCircleFilled(center,cyui::dp(4.3f),color);
            else draw->AddCircle(center,cyui::dp(4.3f),color,12,cyui::dp(1.4f));
        }
        if (debugState_.value("state","") == "stopped" && debugCurrentPath_ == tab->path && debugCurrentLine_ == line.line+1)
            draw->AddTriangleFilled({at.x+line.width*.6f,at.y+cyui::dp(2)},{at.x+line.width,at.y+line.height*.5f},{at.x+line.width*.6f,at.y+line.height-cyui::dp(2)},cyui::palette().warning);
        ImGui::BeginDisabled(tab->readOnly || isUntitledDocument(tab->path) || debugFuture_.valid());
        if (ImGui::InvisibleButton("##breakpoint",{line.width,line.height})) toggleBreakpoint(tab->path,line.line+1);
        if (ImGui::IsItemHovered()) cyui::tooltip((std::string(tr("Toggle breakpoint"))+" ("+keymap_.hint("debug.breakpoint")+")").c_str());
        ImGui::EndDisabled();
    });
    std::weak_ptr<Tab> weak = tab;
    TextEditor::AutoCompleteConfig completion;
    completion.suggestionWidth = 48;
    completion.noSuggestionsLabel = tr("No matching suggestions");
    completion.callback = [this, weak](TextEditor::AutoCompleteState &state) {
        auto tab = weak.lock();
        state.suggestions.clear(); state.suggestionsPromise = false;
        if (!tab || tab->readOnly || tab->snippet.active() || !engine_->supportsLanguage(tab->path))
            return;
        tab->completionSession.clear();tab->completionItems.clear();
        if (cppFile(tab->path) && unreal_.value("available", false) && state.searchTerm.size() >= 2) {
            for (const auto &name : unrealLanguage_.identifiers) if (name.starts_with(state.searchTerm)) state.suggestions.push_back(name);
            for (const auto &name : unrealLanguage_.declarations) if (name.starts_with(state.searchTerm)) state.suggestions.push_back(name);
            std::sort(state.suggestions.begin(), state.suggestions.end());
        }
        if (smoke_) return;
        state.suggestionsPromise = true;
        auto position = state.searchTermEnd;
        auto prefix = tab->editor.GetSectionText({position.line, 0}, position);
        ++tab->completionGeneration;
        tab->queuedCompletionStart = state.searchTermStart;
        tab->queuedCompletionEnd = state.searchTermEnd;
        tab->queuedCompletion = {
            {"path", tab->path},
            {"expectedRevision", tab->revision},
            {"position", {{"line", position.line}, {"character", LanguageClient::utf16Length(prefix)}}}};
    };
    completion.accept=[this,weak](size_t selected) {
        auto tab=weak.lock();
        if (!tab || tab->completionSession.empty()) return false; // Local Unreal fallback.
        if (selected>=tab->completionItems.size() || tab->completionAccept.valid()) return true;
        if (tab->editor.GetNumberOfCursors()!=1) { error(tr("Completion with imports requires a single cursor."));return true; }
        if (tab->readOnly || tab->revision!=tab->completionRevision || tab->completionGeneration!=tab->runningCompletionGeneration || tab->editor.GetMainCursorPosition()!=tab->completionPosition) {
            error(tr("Completion context changed. Request suggestions again."));return true;
        }
        tab->acceptedRevision=tab->revision;tab->acceptedGeneration=tab->completionGeneration;tab->acceptedPosition=tab->editor.GetMainCursorPosition();
        tab->acceptedSelection=tab->editor.GetMainCursorSelection();
        tab->acceptedSnippetPluginRevision.reset();
        tab->acceptedUserSnippetGeneration.reset();
        auto engine=engine_;const Json parameters={{"sessionId",tab->completionSession},{"item",tab->completionItems[selected]},{"expectedRevision",tab->revision}};
        tab->queuedCompletion=nullptr;
        tab->completionAccept=std::async(std::launch::async,[engine,parameters] { return engine->dispatch("language.completionPrepare",parameters); });
        return true;
    };
    tab->editor.SetAutoCompleteConfig(&completion);
}
void Application::open(const std::string &path, size_t line, size_t column, bool utf16, bool activate) {
    if (!isUntitledDocument(path) && openImage(path,activate)) return;
    if (path.starts_with("debug://")) { openDebugSource(path,line,column); return; }
    try {
        auto doc = path.starts_with("sdk://") ? engine_->dispatch("sources.read", {{"source", path}})
                                             : engine_->dispatch("documents.open", {{"path", path}});
        std::string canonical = doc["path"];
        std::shared_ptr<Tab> tab;
        for (auto &candidate : tabs_)
            if (candidate->path == canonical)
                tab = candidate;
        if (!tab) {
            tab = std::make_shared<Tab>();
            tab->path = canonical;
            tab->displayPath = doc.value("displayPath", canonical);
            tab->readOnly = doc.value("readOnly", false);
            tab->codeEditor.setText(doc["text"].get<std::string>());
            tab->revision = doc["revision"];
            tab->dirty = doc["dirty"];
            configureEditor(tab);
            tabs_.push_back(tab);
        } else if (tab->revision != doc["revision"].get<std::uint64_t>()) {
            tab->snippet.cancel();
            const auto cursor = tab->editor.GetMainCursorPosition();
            tab->editor.SelectAll();
            tab->editor.ReplaceTextPreservingTabs(doc["text"].get<std::string>());
            tab->syntax.update(tab->editor,tab->path,doc["text"].get_ref<const std::string &>());
            tab->editor.SetCursor(cursor);
            tab->revision = doc["revision"];
            tab->dirty = doc["dirty"];
            tab->queuedCompletion = nullptr;
            ++tab->completionGeneration;
            tab->editor.SetAutoCompleteSuggestions({});
        }
        // Recovery may seed a tab before open() mirrors its revision. Apply the
        // same language-server activation policy to both paths.
        const auto backend=engine_->languageBackendFor(tab->path);
        if (!smoke_ && !backend.empty() && backend!="cpp" && backend!="csharp" && !languageDecisions_.contains(backend)) {
            languageDecisions_.insert(backend);
            const auto languageStatus=engine_->languageStatus();
            const auto server=languageStatus.value("servers",Json::object()).value(backend,Json::object());
            const auto saved=languageStatus.value("preferences",Json::object()).value("backends",Json::object());
            if (server.value("available",false) && !server.value("enabled",false) && saved.value(backend,true))
                engine_->dispatch("language.configure",{{"language",backend}});
        }
        tab->displayPath=doc.value("displayPath",canonical);
        tab->identity=doc.value("id",uint64_t(0));
        tab->synchronizedText=doc.at("text").get<std::string>();
        tab->synchronizedRevision=tab->revision;
        if (activate) {
            tab->pendingSession = nullptr;
            if (!historyMoving_ && (active_ != canonical || line)) rememberNavigation();
            if (!isUntitledDocument(canonical)) { std::erase(recentFiles_, canonical); recentFiles_.insert(recentFiles_.begin(), canonical); }
            if (recentFiles_.size() > 100) recentFiles_.resize(100);
            active_ = canonical;
            focusTab_ = canonical;
            revealPath_ = canonical;
        }
        if (line) {
            const auto text = tab->editor.GetLineText(line - 1);
            size_t units = 0, index = 0;
            const size_t offset = column ? column - 1 : 0;
            for (size_t byte = 0; byte < text.size();) {
                auto c = static_cast<unsigned char>(text[byte]);
                if ((utf16 ? units : byte) >= offset)
                    break;
                byte += c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
                units += c >= 0xf0 ? 2 : 1;
                ++index;
            }
            tab->editor.SetCursor({line - 1, index});
            tab->editor.ScrollToLine(line - 1);
        }
        if (activate) tab->editor.SetFocus();
    } catch (const std::exception &e) {
        error(e.what());
    }
}
void Application::changed(const std::shared_ptr<Tab> &tab) {
    if (tab->readOnly) return;
    clearDiagnosticAnnotations(tab->editor);tab->diagnosticKey.clear();tab->diagnosticCount=0;tab->diagnosticTruncated=false;
    tab->editor.ClearSemanticTokens(); tab->highlightRevision=0; tab->highlightCount=0; tab->highlightRetryAt=ImGui::GetTime()+.3;
    try {
        auto result = engine_->dispatch(
            "documents.replace",
            {{"path", tab->path}, {"text", tab->editor.GetText()}, {"expectedRevision", tab->revision}});
        tab->revision = result["revision"];
        tab->dirty = result["dirty"];
        tab->synchronizedText=result.at("text").get<std::string>();
        tab->synchronizedRevision=tab->revision;
        tab->syntax.update(tab->editor,tab->path,result["text"].get_ref<const std::string &>());
        ++tab->completionGeneration;
    } catch (const std::exception &e) {
        // A filesystem move/delete can complete between the UI's metadata poll
        // and this keystroke. Retry only when identity and text prove continuity.
        if(tab->identity)try {
            const auto documents=engine_->dispatch("documents.list").at("documents");
            for(const auto &info:documents)if(info.value("id",uint64_t(0))==tab->identity&&info.at("revision").get<uint64_t>()!=tab->revision) {
                const auto current=engine_->dispatch("documents.read",{{"path",info.at("path")}});
                if(current.at("text").get_ref<const std::string &>()!=tab->synchronizedText)break;
                resourceTabPath(tab,current.at("path"));tab->revision=current.at("revision");
                const auto result=engine_->dispatch("documents.replace",{{"path",tab->path},{"text",tab->editor.GetText()},{"expectedRevision",tab->revision}});
                tab->revision=result.at("revision");tab->dirty=result.at("dirty");tab->synchronizedText=result.at("text").get<std::string>();tab->synchronizedRevision=tab->revision;
                tab->syntax.update(tab->editor,tab->path,tab->synchronizedText);return;
            }
        } catch(const std::exception &) {}
        tab->dirty = true; // Preserve uncommitted editor text through a concurrent native close/conflict.
        error(e.what());
    }
}
void Application::save(const std::shared_ptr<Tab> &tab) {
    if (tab->readOnly) return;
    if (isUntitledDocument(tab->path)) { beginSaveAs(tab); return; }
    try {
        auto result =
            engine_->dispatch("documents.save", {{"path", tab->path}, {"expectedRevision", tab->revision}});
        tab->dirty = result["dirty"];
        cyui::notify(tr("Saved"), cyui::Tone::success);
        refreshGit();
    } catch (const std::exception &e) {
        error(e.what());
    }
}
void Application::saveAll() {
    for (const auto &tab : tabs_)
        if (tab->dirty)
            save(tab);
}
void Application::closeTab(const std::string &path, bool discard) {
    auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const auto &tab) { return tab->path == path; });
    if (found == tabs_.end())
        return;
    if ((*found)->dirty && !discard) {
        pendingClose_ = path;
        return;
    }
    try {
        if ((*found)->image) { }
        else if ((*found)->readOnly && path.starts_with("sdk://")) engine_->dispatch("sources.close", {{"source", path}});
        else if (path.starts_with("debug://")) { }
        else engine_->dispatch("documents.close", {{"path", path}, {"discard", discard},{"expectedRevision",(*found)->revision}});
    } catch (const std::exception &e) {
        closeAll_ = false;
        if (pendingClose_ == path) pendingClose_.clear();
        error(e.what());
        return; // A concurrent edit must leave this tab and its editor Undo alive.
    }
    retireTabView(path);
}
void Application::retireTabView(const std::string &path) {
    auto found = std::find_if(tabs_.begin(),tabs_.end(),[&](const auto &tab){return tab->path==path;});
    if (found == tabs_.end()) return;
    std::erase(saveAsQueue_,(*found)->identity);
    if (pendingClose_ == path) {
        if (auto *popup = ImGui::FindWindowByName(tr("Unsaved changes"))) {
            auto &stack = ImGui::GetCurrentContext()->OpenPopupStack;
            for (int i=0;i<stack.Size;++i) if (stack[i].Window==popup) { ImGui::ClosePopupToLevel(i,true); break; }
        }
        pendingClose_.clear();
    }
    if ((*found)->completion.valid())
        retiredCompletions_.push_back(std::move((*found)->completion));
    if ((*found)->completionAccept.valid()) retiredCompletions_.push_back(std::move((*found)->completionAccept));
    if ((*found)->highlighting.valid()) retiredCompletions_.push_back(std::move((*found)->highlighting));
    tabs_.erase(found);
    sessionDraftTabs_.erase(path);
    if (!isUntitledDocument(path)) closedFiles_.push_back(path);
    if (closedFiles_.size() > 100) closedFiles_.erase(closedFiles_.begin());
    if (active_ == path)
        active_ = tabs_.empty() ? "" : tabs_.front()->path;
}
void Application::update() {
    updateBuildCancellation();
    updateProjectCancellation();
    updateProjectTemplates();
    updateImages();
    updateResources();
    updateRecovery();
    if (ready(unrealAnalysisFuture_)) {
        try {
            auto result = unrealAnalysisFuture_.get();
            if (unrealAnalysisCancellation_ && *unrealAnalysisCancellation_) unrealAnalysisState_ = "cancelled";
            else {
                output_ += result.value("output", std::string{});
                applyUnreal(result.at("project"));
                unrealAnalysisState_ = result.value("exitCode", -1) == 0 ? "ready" : "failed";
                if (unrealAnalysisState_ == "failed") error(tr("Unreal analysis failed. See Build output."));
                engine_->configureLanguage(clangd_, !smoke_, true);
            }
        } catch (const std::exception &e) { unrealAnalysisState_ = "failed"; error(e.what()); engine_->configureLanguage(clangd_, !smoke_, true); }
    }
    std::erase_if(retiredCompletions_, [](auto &future) {
        if (!ready(future))
            return false;
        try {
            future.get();
        } catch (const std::exception &) {
        }
        return true;
    });
    languageState_ = engine_->languageStatus();
    if (ready(buildFuture_)) {
        try {
            applyBuild(buildFuture_.get());
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (ready(detectionFuture_)) {
        try {
            toolchains_ = detectionFuture_.get();
            if (msbuildActive_ && toolchains_.contains("tools")) { msbuildState_["tools"] = toolchains_.at("tools"); refreshBuild(); }
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    // Synchronize external edits before consuming any asynchronous language result.
    updateWorkspace();
    auto documents = engine_->dispatch("documents.list")["documents"];
    std::set<uint64_t> liveDocumentIds;
    for (const auto &info : documents) liveDocumentIds.insert(info.value("id",uint64_t(0)));
    const auto views = tabs_;
    for (const auto &tab : views) if (!tab->readOnly && !tab->image && tab->identity && !liveDocumentIds.contains(tab->identity)) {
        const auto text = tab->editor.GetText();
        if (text != tab->synchronizedText) {
            try {
                auto name = pathUtf8(utf8Path(tab->displayPath).filename());
                try { validateUntitledName(name); } catch (const std::exception &) { name="Recovered.txt"; }
                const auto rescued=engine_->dispatch("documents.createUntitled",{{"name",name},{"text",text}});
                resourceTabPath(tab,rescued.at("path"));
                tab->identity=rescued.at("id");tab->revision=rescued.at("revision");tab->dirty=true;tab->diskConflict.clear();
                tab->synchronizedText=rescued.at("text").get<std::string>();tab->synchronizedRevision=tab->revision;
                cyui::notify(tr("The document was closed externally. Local edits were kept in a new draft."),cyui::Tone::warning);
            } catch (const std::exception &e) {
                if (tab->diskConflict!="closed") error(e.what());
                tab->diskConflict="closed";tab->dirty=true;
            }
        } else retireTabView(tab->path);
    }
    for (const auto &info : documents) {
        auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const auto &tab) {
            return tab->identity?tab->identity==info.value("id",uint64_t(0)):tab->path == info["path"].get<std::string>();
        });
        if (found == tabs_.end()) {
            if (info.value("untitled",false)) open(info.at("path"),0,0,false,false);
            continue;
        }
        auto &tab = *found;
        if(info.at("revision").get<uint64_t>()==tab->revision&&tab->synchronizedRevision!=tab->revision) {
            tab->synchronizedText=tab->editor.GetText();tab->synchronizedRevision=tab->revision;
        }
        if(tab->path!=info.at("path").get<std::string>())resourceTabPath(tab,info.at("path"));
        tab->diskConflict=info.value("diskConflict",std::string{});
        if (info["revision"] != tab->revision) {
            auto doc = engine_->dispatch("documents.read", {{"path", tab->path}});
            tab->snippet.cancel();
            auto cursor = tab->editor.GetMainCursorPosition();
            if(tab->editor.GetText()!=doc.at("text").get_ref<const std::string &>()) {
                tab->editor.SelectAll();
                tab->editor.ReplaceTextPreservingTabs(doc["text"].get<std::string>());
            }
            tab->syntax.update(tab->editor,tab->path,doc["text"].get_ref<const std::string &>());
            tab->editor.SetCursor(cursor);
            tab->revision = doc["revision"];
            tab->dirty = doc["dirty"];
            tab->synchronizedText=doc.at("text").get<std::string>();
            tab->synchronizedRevision=tab->revision;
            tab->queuedCompletion = nullptr;
            ++tab->completionGeneration;
            tab->editor.SetAutoCompleteSuggestions({});
        }
    }
    updateApplicationUpdates();
    updateSemantic();
    updateStructure();
    updateOccurrences();
    updateInformation();
    updatePresetActions();
    updateHierarchy();
    updateCodeActions();
    updateHighlights();
    updateDiagnostics();
    updateTerminal();
    updateDebug();
    updateVcs();
    for (const auto &tab : tabs_) {
        if (ready(tab->completionAccept)) {
            try {
                const auto prepared=tab->completionAccept.get();
                if (tab->revision!=tab->acceptedRevision || tab->completionGeneration!=tab->acceptedGeneration || tab->editor.GetMainCursorPosition()!=tab->acceptedPosition || tab->editor.GetNumberOfCursors()!=1)
                    throw std::runtime_error(tr("Completion context changed. Request suggestions again."));
                const auto selection=tab->editor.GetMainCursorSelection();
                if(tab->acceptedSnippetPluginRevision && engine_->plugins().revision()!=*tab->acceptedSnippetPluginRevision)
                    throw std::runtime_error(tr("Snippet context changed. Request snippets again."));
                if(tab->acceptedUserSnippetGeneration && engine_->userSnippetGeneration()!=*tab->acceptedUserSnippetGeneration)
                    throw std::runtime_error(tr("Snippet context changed. Request snippets again."));
                if (tab->acceptedSelection && (selection.start!=tab->acceptedSelection->start || selection.end!=tab->acceptedSelection->end))
                    throw std::runtime_error(tr("Completion context changed. Request suggestions again."));
                const auto &plan=prepared.at("plan");
                if (!plan.at("files").empty()) {
                    const auto preview=engine_->dispatch("changes.preview",{{"id",plan.at("id")},{"path",tab->path}});
                    if (tab->editor.GetText()!=preview.at("before").get_ref<const std::string &>()) throw std::runtime_error(tr("Completion context changed. Request suggestions again."));
                    engine_->dispatch("changes.apply",{{"id",plan.at("id")}});
                    const auto doc=engine_->dispatch("documents.read",{{"path",tab->path}});
                    if (doc.at("text")!=preview.at("after")) throw std::runtime_error(tr("Completion context changed. Request suggestions again."));
                    const auto &cursor=prepared.at("cursor");
                    applyEditorCompletion(tab->editor,preview.at("before"),doc.at("text"),cursor.at("line"),cursor.at("character"));
                    tab->revision=doc.at("revision");tab->dirty=doc.at("dirty");
                    tab->syntax.update(tab->editor,tab->path,doc.at("text"));
                    tab->editor.ClearSemanticTokens();tab->highlightRevision=0;tab->highlightRetryAt=ImGui::GetTime()+.3;
                    ++tab->completionGeneration;
                }
                tab->snippet.begin(prepared.at("snippet"));
            } catch (const std::exception &e) { error(e.what()); }
            tab->acceptedSelection.reset();
            tab->acceptedSnippetPluginRevision.reset();
            tab->acceptedUserSnippetGeneration.reset();
            tab->completionSession.clear();tab->completionItems.clear();tab->editor.SetAutoCompleteSuggestions({});
        }
        if (ready(tab->completion)) {
            try {
                auto reply = tab->completion.get();
                if (tab->revision == tab->completionRevision &&
                    tab->completionGeneration == tab->runningCompletionGeneration &&
                    tab->editor.GetMainCursorPosition() == tab->completionPosition) {
                    const auto &response = reply["result"];
                    Json items = response.is_array()    ? response
                                 : response.is_object() ? response.value("items", Json::array())
                                                        : Json::array();
                    for (size_t i=0;i<items.size();++i) items[i]["cycodeItemIndex"]=i;
                    if (testSession_) tab->completionTrace = {{"count", items.size()}, {"first", items.empty() ? Json() : items[0]}};
                    const auto typedPrefix = tab->editor.GetSectionText(tab->runningCompletionStart,tab->completionPosition);
                    const bool rankPrefix=!LanguageService::supportsCpp(tab->path);
                    std::stable_sort(items.begin(), items.end(), [&](const Json &a, const Json &b) {
                        if (rankPrefix) {
                            const auto left=fuzzyScore(typedPrefix,a.value("filterText",a.value("label",std::string{})));
                            const auto right=fuzzyScore(typedPrefix,b.value("filterText",b.value("label",std::string{})));
                            if (left!=right) return left>right;
                        }
                        return a.value("sortText", a.value("label", std::string{})) < b.value("sortText", b.value("label", std::string{}));
                    });
                    std::vector<std::string> suggestions;
                    std::vector<size_t> indices;std::set<std::string> unique;
                    std::optional<std::string> completionText;
                    std::optional<CompletionSnippetContext> snippetContext;
                    for (const auto &item : items) {
                        if (!item.is_object() || !completionCommandAllowed(item,engine_->languageBackendFor(tab->path)))
                            continue;
                        // Selection/ranking and editor-only hints do not alter
                        // the complete text edit. No completion command is run.
                        // Roslyn returns the member set; filtering is the client's job.
                        if (!LanguageService::supportsCpp(tab->path) &&
                            fuzzyScore(typedPrefix,item.value("filterText",item.value("label",std::string{}))) < 0) continue;
                        auto text = item.value("insertText", item.value("label", std::string{}));
                        if (item.contains("textEdit")) {
                            const auto &edit = item.at("textEdit");
                            text = item["textEdit"].value("newText", text);
                        }
                        if (item.value("insertTextFormat",1)==2) {
                            if (!snippetContext) {
                                completionText = tab->editor.GetText();
                                const auto prefix = tab->editor.GetSectionText(
                                    {tab->completionPosition.line, 0}, tab->completionPosition);
                                const Json point = {{"line", tab->completionPosition.line},
                                    {"character", LanguageClient::utf16Length(prefix)}};
                                snippetContext = CompletionSnippetContext{
                                    engine_->workspace() / utf8Path(tab->path), engine_->workspace(),
                                    *completionText, utf16Offset(*completionText, point)};
                            }
                            bool deferredTransforms = false;
                            const auto plain=plainSnippet(text, *snippetContext, &deferredTransforms);
                            if (!plain) continue;
                            text=deferredTransforms ? item.value("label", std::string{}) : *plain;
                        }
                        if (text.find('\n')!=std::string::npos) text=item.value("label",std::string{});
                        const auto details=item.value("labelDetails",Json::object());
                        if (details.is_object() && details.contains("description") && details.at("description").is_string() && !details.at("description").get_ref<const std::string&>().empty()) text+="  — "+details.at("description").get<std::string>();
                        for (auto &c : text) if (static_cast<unsigned char>(c)<32 || c==127) c=' ';
                        if (text.size()>1024 || text.empty() || !unique.insert(text).second) continue;
                        suggestions.push_back(text);indices.push_back(item.at("cycodeItemIndex"));
                        if (suggestions.size() >= 100)
                            break;
                    }
                    tab->completionSession=reply.at("sessionId");tab->completionItems=std::move(indices);
                    tab->lastSuggestions = suggestions;
                    tab->editor.SetAutoCompleteSuggestions(suggestions);
                }
            } catch (const std::exception &e) {
                if (testSession_) tab->completionTrace = {{"error", e.what()}};
                tab->completionSession.clear();tab->completionItems.clear();
                tab->editor.SetAutoCompleteSuggestions({});
            }
        }
        if (!tab->completion.valid() && !tab->completionAccept.valid() && tab->queuedCompletion.is_object()) {
            if (tab->editor.GetMainCursorPosition() != tab->queuedCompletionEnd) {
                tab->queuedCompletion = nullptr; tab->editor.SetAutoCompleteSuggestions({}); continue;
            }
            // clangd's initialized state precedes the first parsed preamble. Before
            // that parse, completion returns unrelated index symbols (even with
            // isIncomplete=false). Keep the widget's promise until the document's
            // first diagnostics publication, including an empty diagnostics list.
            const auto uri = LanguageClient::uri(engine_->workspace() / utf8Path(tab->path));
            const auto backend=engine_->languageBackendFor(tab->path);
            const auto server = languageState_.value("servers",Json::object()).value(backend,Json::object());
            if (!server.value("ready", false) || (backend=="cpp" && (unrealAnalysisState_ == "preparing" || !server.value("diagnostics", Json::object()).contains(uri)))) continue;
            auto parameters = std::move(tab->queuedCompletion);
            tab->queuedCompletion = nullptr;
            tab->completionRevision = tab->revision;
            // The editor callback runs during Render(), before changed() commits the keystroke.
            parameters["expectedRevision"] = tab->revision;
            tab->completionPosition = tab->editor.GetMainCursorPosition();
            tab->runningCompletionStart = tab->queuedCompletionStart;
            tab->runningCompletionGeneration = tab->completionGeneration;
            auto engine = engine_;
            tab->completion = std::async(std::launch::async, [engine, parameters] {
                return engine->dispatch("language.complete", parameters);
            });
        }
    }
    updateNavigation();
    if (ready(filesFuture_)) {
        try {
            auto result = filesFuture_.get();
            workspaceIndexRevision_=result.value("indexRevision",uint64_t(0));
            files_ = result["files"].get<std::vector<std::string>>();
            tree_ = {};
            for (const auto &path : files_) {
                Tree *node = &tree_;
                auto fs = utf8Path(path);
                for (const auto &part : fs.parent_path())
                    node = &node->folders[pathUtf8(part)];
                node->files.push_back(path);
            }
            if (result.value("truncated", false))
                output_ += std::string(tr("File index is incomplete; refine the project scope or refresh.")) + "\n";
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (ready(gitFuture_)) {
        try {
            auto result = gitFuture_.get();
            const auto path=vcsRequestPath();
            if(result.value("contextPath",path)!=path) { lastGitRefresh_=-10;refreshGit(); }
            else {
            git_ = result.at("status");
            const auto oldGitRoot=vcsContext_.value("gitRepository",std::string{});const auto oldGitIdentity=vcsContext_.value("gitIdentity",std::string{});
            const auto oldProvider=provider_;const auto oldActions=vcsContext_.value("actions",Json::array());vcsContext_=result.value("context",Json::object());
            provider_=vcsContext_.value("provider",std::string("git"));vcsContextPath_=path;
            if(oldProvider!=provider_||oldGitRoot!=vcsContext_.value("gitRepository",std::string{})||oldGitIdentity!=vcsContext_.value("gitIdentity",std::string{})) { clearGitReplay();clearGitMerge();clearGitCheckout();clearGitConflicts();vcsWorktreeReview_=Json::object();vcsWorktrees_=Json::object();vcsWorktreesRequested_=false;vcsHunks_=Json::object();vcsHunkSelection_.clear();vcsConflict_=Json::object();vcsPreviewTitle_.clear();vcsSection_="Changes"; }
            if(oldProvider!=provider_||oldGitRoot!=vcsContext_.value("gitRepository",std::string{})||oldGitIdentity!=vcsContext_.value("gitIdentity",std::string{})) { vcsSubmodules_=Json::object();vcsSubmoduleReview_=Json::object();vcsSubmodulesRequested_=false; vcsSubmoduleAddPlan_=Json::object();vcsSubmoduleAddOpen_=false; }
            vcsInfo_ = result.value("details",Json::object()); vcsHistory_ = result.value("history",Json::object());
            if(oldProvider!=provider_||oldActions!=vcsContext_.value("actions",Json::array()))synchronizeExtensions();
            }
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (ready(taskFuture_)) {
        try {
            auto result = taskFuture_.get();
            if(taskMethod_=="unreal.automationPlan"||taskMethod_=="unreal.automationRun")unrealAutomationResult(result);
            if(taskMethod_=="unreal.launchProfile")unrealLaunchResult(result);
            if(taskMethod_=="unreal.artifacts")unrealArtifacts_=result;
            if(taskMethod_=="unreal.build"&&result.contains("artifacts"))unrealArtifacts_=result.at("artifacts");
            if (taskMethod_.starts_with("msbuild.") || taskMethod_.starts_with("project.")) buildDiagnostics_ = result.value("diagnostics",Json::array());
            if (taskMethod_ == "unreal.generate" && result.value("exitCode", -1) == 0) {
                solution_ = engine_->dispatch("workspace.describe").at("solution");
                refreshFiles();
            }
            if(taskMethod_=="plugins.configure"||taskMethod_=="plugins.list") { pluginInfo_=result;synchronizeExtensions();refreshGit(); }
            if (taskMethod_ == "unreal.inspect")
                applyUnreal(result);
            else if (taskMethod_.starts_with("unreal.") && result.contains("project"))
                applyUnreal(result.at("project"));
            else if (result.contains("project") && result["project"].is_object()) {
                buildEdited_ = false;
                applyBuild(result["project"]);
            }
            if(result.contains("inspectionError")) {
                output_+="\n"+result.at("inspectionError").get<std::string>()+"\n";
                error(result.at("inspectionError"));
            }
            // The launch preview carries a private saved environment for compare-and-swap.
            if (taskMethod_ == "unreal.launchProfile") output_ += tr("Unreal launch profile prepared.");
            else if (taskMethod_ == "unreal.artifacts") output_ += tr("Unreal output inspection finished.");
            else output_ += result.contains("output") ? result["output"].get<std::string>() : result.dump(2);
            output_ += "\n";
            if (result.contains("terminal")) {
                terminalId_ = result.at("terminal").at("id"); showPanel("Terminal","terminal"); focusTerminal_ = true; terminalScrollBottom_ = true;
            }
            if (taskMethod_=="project.debug" && result.contains("program")) {
                debugUnrealLaunch_ = nullptr;
                debugProgram_ = result.at("program"); debugRuntime_ = "native"; debugCwd_ = projectProfile_.value("directory",".");
                debugAdapterId_ = "builtin"; debugAdapterOptions_ = "{}";
                debugArgs_.clear(); for (const auto &arg : projectProfile_.value("args",Json::array())) debugArgs_ += arg.get<std::string>()+"\n";
            }
            if (taskMethod_ == "project.debug" && result.contains("debugProfile")) {
                debugUnrealLaunch_ = nullptr;
                const auto &profile = result.at("debugProfile");
                // Retain user VM/source options, replace the actual built main/runtime classpath.
                auto options = debugAdapterId_ == "builtin.java" ? Json::parse(debugAdapterOptions_) : Json::object();
                options.update(profile.at("adapterOptions"));
                debugAdapterId_ = profile.at("adapterId"); debugAdapterOptions_ = options.dump(2);
                debugProgram_.clear(); debugRuntime_ = "automatic"; debugCwd_ = profile.at("cwd");
                debugArgs_.clear(); for (const auto &arg : profile.at("args")) debugArgs_ += arg.get<std::string>()+"\n";
            }
            if (!continueUnrealDebug(result) && pendingDebugBuild_) { pendingDebugBuild_ = false; if (result.value("exitCode",-1) == 0 && !result.value("cancelled",false)) startDebug(false); }
            if (result.value("cancelled",false)) cyui::notify(tr("Task cancelled"),cyui::Tone::warning);
            else if (result.value("exitCode", 0) != 0)
                { error(tr("Task failed. See output.")); if (!buildDiagnostics_.empty()) showPanel("Problems","problems"); }
            else if (result.contains("inspectionError") || result.contains("receiptError"))
                cyui::notify(tr("Task finished with unconfirmed results."),cyui::Tone::warning);
            else
                cyui::notify(tr("Task completed"), cyui::Tone::success);
            refreshGit();
        } catch (const std::exception &e) {
            pendingDebugBuild_ = false;
            pendingUnrealDebug_ = Json::object();
            if (std::string(e.what()).starts_with("BuildCancelled:"))
                cyui::notify(tr("Task cancelled"), cyui::Tone::warning);
            else error(e.what());
        }
        taskOperationId_.clear();
        buildCancellationId_.clear();
    }
    if (ready(searchFuture_)) {
        try {
            auto result = searchFuture_.get();
            if (runningSearchGeneration_ == searchGeneration_)
                search_ = std::move(result);
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    if (searchQueued_ && !searchFuture_.valid() && ImGui::GetTime() - searchChanged_ > .2) {
        searchQueued_ = false;
        runningSearchGeneration_ = searchGeneration_;
        auto engine = engine_;
        searchCancellation_ = std::make_shared<std::atomic<bool>>(false);
        auto cancellation = searchCancellation_;
        auto parameters = Json{
            {"query", searchQuery_}, {"caseSensitive", caseSensitive_}, {"regex", regex_}, {"limit", 500}};
        searchFuture_ = std::async(std::launch::async, [engine, parameters, cancellation] {
            return engine->searchText(parameters, cancellation.get());
        });
    }
    {
        std::lock_guard lock(dialog_->mutex);
        if (dialog_->ready) {
            dialog_->ready = false;
            if (!dialog_->path.empty())
                try {
                    if (dialog_->image) openImageFile(utf8Path(dialog_->path));
                    else if (dialog_->newWindow)
                        newWindow(utf8Path(dialog_->path));
                    else
                        openProject(utf8Path(dialog_->path));
                } catch (const std::exception &e) {
                    error(e.what());
                }
            if (!dialog_->error.empty())
                error(dialog_->error);
        }
    }
    if (!pendingWorkspace_.empty() && !busy() && !userSnippetsOpen_) {
        auto path = std::move(pendingWorkspace_); pendingWorkspace_.clear();
        const bool generated=path==templateOpenEntry_;
        try { setWorkspace(utf8Path(path));if(generated&&!templateOpenFile_.empty())open(templateOpenFile_); }
        catch (const std::exception &e) { error(e.what());if(generated){templateError_=e.what();templateModal_=true;} }
        if(generated){templateOpenEntry_.clear();templateOpenFile_.clear();}
    }
    if (pendingWorkspace_.empty() && ImGui::GetTime() - lastGitRefresh_ > 5 && !smoke_)
        refreshGit();
    if (pendingWorkspace_.empty() && !smoke_ && !taskFuture_.valid() && ImGui::GetTime() - lastBuildRefresh_ > 5)
        refreshBuild();
    if (testSession_ && testCompletion_.is_object() && unrealAnalysisState_ != "preparing" && testCompletionPhase_ == 0) {
        const auto &position = testCompletion_.at("position");
        open(testCompletion_.at("path"), position.at("line").get<size_t>() + 1, position.at("character").get<size_t>() + 1, true);
        testCompletionPhase_ = 1;
    } else if (testCompletionPhase_ == 1 && editorFocused_ && languageState_.value("ready", false)) {
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true); ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftCtrl, true); ImGui::GetIO().AddKeyEvent(ImGuiKey_Space, true); testCompletionPhase_ = 2;
    } else if (testCompletionPhase_ == 2) {
        ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false); ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftCtrl, false); ImGui::GetIO().AddKeyEvent(ImGuiKey_Space, false); testCompletionPhase_ = 3;
    } else if (testCompletionPhase_ == 3 && testCompletion_.value("acceptFirst", false)) {
        for (const auto &tab : tabs_) if (tab->path == active_ && !tab->lastSuggestions.empty() && !tab->completion.valid()) { ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true); testCompletionPhase_ = 4; }
    } else if (testCompletionPhase_ == 4) {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false); testCompletionPhase_ = 5;
    }
    if (testSession_ && testNavigation_.is_object() && languageState_.value("ready", false)) {
        const auto position = testNavigation_.at("position");
        open(testNavigation_.at("path"), position.at("line").get<size_t>() + 1,
             position.at("character").get<size_t>() + 1, true);
        navigate(testNavigation_.value("references", false));
        testNavigation_ = nullptr;
    }
}
void Application::chooseFolder() {
    {
        std::lock_guard lock(dialog_->mutex);
        dialog_->path.clear();
        dialog_->error.clear();
        dialog_->newWindow = false;
        dialog_->image = false;
    }
    auto *state = new std::shared_ptr<Dialog>(dialog_);
    SDL_ShowOpenFolderDialog(
        [](void *user, const char *const *paths, int) {
            auto state = *static_cast<std::shared_ptr<Dialog> *>(user);
            delete static_cast<std::shared_ptr<Dialog> *>(user);
            std::lock_guard lock(state->mutex);
            state->ready = true;
            if (paths && paths[0])
                state->path = paths[0];
            else if (!paths)
                state->error = SDL_GetError();
        },
        state, window_, pathUtf8(engine_->workspace()).c_str(), false);
}
void Application::chooseSolution(bool separate) {
    {
        std::lock_guard lock(dialog_->mutex);
        dialog_->path.clear();
        dialog_->error.clear();
        dialog_->newWindow = separate;
        dialog_->image = false;
    }
    static const SDL_DialogFileFilter filters[] = {{"Solutions / Unreal / Cargo / npm / JVM", "sln;slnx;uproject;vcxproj;csproj;fsproj;vbproj;toml;json;java;xml;gradle;kts"},
                                                   {"Unreal project (.uproject)", "uproject"},
                                                   {"Visual Studio solution (.sln / .slnx)", "sln;slnx"}};
    auto *state = new std::shared_ptr<Dialog>(dialog_);
    SDL_ShowOpenFileDialog(
        [](void *user, const char *const *paths, int) {
            auto state = *static_cast<std::shared_ptr<Dialog> *>(user);
            delete static_cast<std::shared_ptr<Dialog> *>(user);
            std::lock_guard lock(state->mutex);
            state->ready = true;
            if (paths && paths[0])
                state->path = paths[0];
            else if (!paths)
                state->error = SDL_GetError();
        },
        state, window_, filters, 3, pathUtf8(engine_->workspace()).c_str(), false);
}
void Application::newWindow(const std::filesystem::path &entry) {
#ifdef _WIN32
    auto executable = utf8Path(SDL_GetBasePath()) / "CyCodeIDE.exe";
#else
    auto executable = utf8Path(SDL_GetBasePath()) / "CyCodeIDE";
#endif
    std::vector<std::string> command{pathUtf8(executable), "--workspace", pathUtf8(entry), "--theme", theme_,
                                     "--language",         locale_, "--keymap", keymapProfile_};
    if(disablePlugins_)command.push_back("--disable-plugins");
    if (std::filesystem::is_regular_file(entry) && !isSolutionPath(entry) &&
        lower(pathUtf8(entry.extension())) != ".uproject") {
        const auto ext = lower(pathUtf8(entry.extension()));
        if (ext != ".vcxproj" && ext != ".csproj" && ext != ".fsproj" && ext != ".vbproj") {
        command[2] = pathUtf8(entry.parent_path());
        command.push_back("--open");
        command.push_back(pathUtf8(entry.filename()));
        }
    }
    std::vector<const char *> argv;
    for (const auto &arg : command)
        argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    auto props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    auto process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    if (!process)
        error(SDL_GetError());
    else
        SDL_DestroyProcess(process);
}
bool Application::beginPanel(const char *text, const char *id) {
    auto [found, inserted] = panels_.try_emplace(id, true);
    if (!found->second)
        return false;
    if (focusPanel_ == id) {
        ImGui::SetNextWindowFocus();
        focusPanel_.clear();
    }
    if (ImGui::Begin(label(text, id).c_str(), &found->second)) {
        if (cyui::iconButton(cyui::icon::help, tr("Context help"))) {
            try { app_help::open(root_, "app/Documentation", locale_, app_help::topic(id), app_help::topicAnchor(id), smoke_ || testSession_); }
            catch (const std::exception &exception) { error(exception.what()); }
        }
        return true;
    }
    ImGui::End();
    return false;
}
void Application::showPanel(const char *text, const char *id) {
    if (std::string(id) == "search") focusSearch_ = true;
    panels_[id] = true;
    focusPanel_ = id;
    ImGui::SetWindowFocus(label(text, id).c_str());
}
void Application::menu() {
    if (!ImGui::BeginMainMenuBar()) return;
    for (const auto *category : {"File", "Edit", "Search", "Navigate", "Refactor", "Build", "Run", "View", "Project", "Version control", "Extensions", "Help"}) {
        if (!ImGui::BeginMenu(tr(category))) continue;
        std::vector<cyui::FilterMenuItem> items;
        const auto catalogue=commandCatalogue();
        for (const auto &command : commandCatalogue()) if (command.category == category &&
            !(std::string(category)=="Version control"&&provider_!="git"&&command.id.starts_with("vcs.")&&command.id!="vcs.changes"&&command.id!="vcs.refresh"&&command.id!="vcs.diff"&&command.id!="vcs.blame"))
            items.push_back({uiId("command/" + command.id), command.title, keymap_.hint(command.id), command.id,
                             commandEnabled(command.id), false, {}});
        const std::map<std::string,std::string> locations={{"File","file"},{"Edit","edit"},{"View","view"},{"Navigate","navigate"},{"Build","build"},{"Run","run"},{"Project","project"},{"Version control","vcs"},{"Extensions","extensions"}};
        if(locations.contains(category))for(const auto &entry:pluginInfo_.value("menus",Json::array()))if(entry.at("location")==locations.at(category)&&extensionVisible(entry)) {
            const std::string id=entry.at("command");const auto itemId=uiId("command/"+id);
            std::erase_if(items,[&](const auto &item) { return item.id==itemId; });
            items.push_back({itemId,entry.value("title",id),keymap_.hint(id),id,commandEnabled(id),false,{}});
        }
        const auto request = cyui::filterMenu(category, menuFilters_[category], items);
        if (request) for (const auto &command : catalogue) if (uiId("command/" + command.id) == request) { executeCommand(command.id); break; }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}
void Application::topBar() {
    cyui::beginTopBar("##top", 44);
    if(brandingLogo_) {
        const float size=cyui::dp(26);
        ImGui::Image(brandingLogo_->data.texture,{size*brandingLogo_->data.width/brandingLogo_->data.height,size});
        ImGui::SameLine();
    }
    {
        cyui::ScopedFont bold(cyui::boldFont(), 1.1f);
        cyui::textColoured(cyui::palette().accent, "CYCODE");
    }
    ImGui::SameLine();
    cyui::badge(CYCODE_IDE_VERSION, cyui::Tone::neutral);
    ImGui::SameLine();
    cyui::toolbarSeparator();
    ImGui::SameLine();
    if (cyui::iconTextButton(cyui::icon::folder, pathUtf8(engine_->workspace().filename()).c_str()))
        chooseFolder();
    ImGui::SameLine();
    cyui::textFaint("/");
    ImGui::SameLine();
    cyui::textDim("%s", (unreal_.value("available", false)
                             ? unrealTarget_ + " | " + unrealConfiguration_ + " | " + unrealPlatform_
                             : configuration_)
                            .c_str());
    ImGui::SameLine();
    cyui::toolbarSeparator();
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::save, (std::string(tr("Save")) + " (" + keymap_.hint("file.save") + ")").c_str()))
        executeCommand("file.save");
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::repair, (std::string(tr("Build project")) + " (" + keymap_.hint("build.run") + ")").c_str(), taskFuture_.valid()))
        executeCommand("build.run");
    ImGui::SameLine();
    cyui::alignRight(cyui::dp(335));
    if (cyui::ghostButton((std::string(tr("Go to file...")) + "   " + keymap_.hint("search.files")).c_str(), cyui::dp(260, 28)))
        executeCommand("search.files");
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::settings, tr("Options")))
        options_ = true;
    cyui::endTopBar();
}
void Application::statusBar() {
    cyui::beginStatusBar("##status");
    cyui::statusText(cyui::Tone::accent, "%s", busy() ? tr("Working...") : tr("Ready"));
    if (unrealAnalysisState_ == "preparing") { ImGui::SameLine(); cyui::textDim("%s", tr("Preparing Unreal code analysis...")); }
    if (auto pending = keymap_.pending(ImGui::GetTime()); !pending.empty()) { ImGui::SameLine(); cyui::textDim("%s", pending.c_str()); }
    ImGui::SameLine();
    cyui::textFaint(" | ");
    ImGui::SameLine();
    cyui::textDim("%s", git_.value("repository", false) ? git_.value("branch", std::string{}).c_str()
                                                        : tr("No repository"));
    for (const auto &tab : tabs_)
        if (tab->path == active_) {
            ImGui::SameLine();
            cyui::alignRight(cyui::dp(420));
            if (tab->image) { cyui::textDim("%s",tr("Image preview")); continue; }
            auto pos = tab->editor.GetMainCursorPosition();
            cyui::textDim("%s  |  Ln %zu, Col %zu  |  UTF-8", tab->editor.GetLanguageName().c_str(),
                          pos.line + 1, pos.index + 1);
        }
    cyui::endStatusBar();
}
void Application::rail() {
    cyui::beginSidebar("##rail", 48);
    auto button = [&](const char *icon, const char *text, const char *panel) {
        if (cyui::iconButton(icon, tr(text), false, 0, cyui::dp(32, 34)))
            showPanel(text, panel);
        cyui::space(6);
    };
    button(cyui::icon::folder, "Project", "project");
    button(cyui::icon::search, "Search", "search");
    button(cyui::icon::sync, "Version control", "vcs");
    button(cyui::icon::repair, "Build", "build");
    button(cyui::icon::library, "Extensions", "extensions");
    button(cyui::icon::connect, "MCP", "mcp");
    cyui::endSidebar();
}
void Application::layout(ImGuiID dock) {
    for (auto &[id, visible] : panels_)
        visible = id != "refactoring" && id != "terminal" && id != "debugger" && id != "codeActions" && id != "hierarchy" && id != "cmakePresets";
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
    auto center = dock;
    auto left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .23f, nullptr, &center);
    auto bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, .29f, nullptr, &center);
    ImGui::DockBuilderDockWindow(label("Project", "project").c_str(), left);
    ImGui::DockBuilderDockWindow(label("Search", "search").c_str(), left);
    ImGui::DockBuilderDockWindow(label("Files", "resources").c_str(), left);
    ImGui::DockBuilderDockWindow(label("Structure", "structure").c_str(), left);
    ImGui::DockBuilderDockWindow("##editors", center);
    for (const auto &[text, id] :
         std::vector<std::pair<const char *, const char *>>{{"Build", "build"},
                                                            {"Unreal Engine", "unreal"},
                                                            {"CMake", "cmake"},
                                                            {"CMake preset actions", "cmakePresets"},
                                                            {"MSBuild", "msbuild"},
                                                            {"Project tasks", "projectTasks"},
                                                            {"Tests", "tests"},
                                                            {"Version control", "vcs"},
                                                            {"Problems", "problems"},
                                                            {"References", "references"},
                                                            {"Code information", "information"},
                                                            {"Hierarchy", "hierarchy"},
                                                            {"Code actions", "codeActions"},
                                                            {"Refactoring", "refactoring"},
                                                            {"Terminal", "terminal"},
                                                            {"Debugger", "debugger"},
                                                            {"Extensions", "extensions"},
                                                            {"Recovery", "recovery"},
                                                            {"MCP", "mcp"}})
        ImGui::DockBuilderDockWindow(label(text, id).c_str(), bottom);
    ImGui::DockBuilderFinish(dock);
    layoutReady_ = true;
}
void Application::tree(Tree &node, const std::string &prefix) {
    for (auto &[name, child] : node.folders) {
        auto key = prefix + name;
        ImGui::PushID(key.c_str());
        if (revealPath_.starts_with(key + '/'))
            ImGui::SetNextItemOpen(true);
        bool open = fileTreeNode(name, key,
                                 ImGuiTreeNodeFlags_SpanAvailWidth |
                                     (prefix.empty() && (!unreal_.value("available", false) ||
                                                         name == "Source" || name == "Plugins")
                                          ? ImGuiTreeNodeFlags_DefaultOpen
                                          : 0),
                                 theme_, true);
        if(ImGui::IsItemClicked())selectedResource_=key;
        if(ImGui::BeginPopupContextItem()) { resourceMenu(key,true);ImGui::EndPopup(); }
        if (open) {
            tree(child, key + "/");
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(node.files.size()));
    while (clipper.Step())
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
            const auto &path = node.files[static_cast<size_t>(index)];
            bool selected = active_ == path;
            auto name = pathUtf8(utf8Path(path).filename());
            ImGui::PushID(path.c_str());
            if (fileSelectable(name, path, selected, theme_)) {
                selectedResource_=path;
                open(path);
            }
            if(ImGui::BeginPopupContextItem()) { resourceMenu(path,false);ImGui::EndPopup(); }
            cyui::tooltip(path.c_str());
            ImGui::PopID();
        }
}
void Application::solutionTree(const std::string &parent, int depth) {
    if (depth > 64)
        return;
    for (const auto &project : solution_.at("projects")) {
        if (project.at("parent") != parent)
            continue;
        const auto id = project.at("id").get<std::string>();
        const bool folder = project.at("folder"), exists = project.at("exists"),
                   external = project.at("external");
        auto name = project.at("name").get<std::string>();
        if (!exists)
            name += " (" + std::string(tr("Missing")) + ")";
        else if (external)
            name += " (" + std::string(tr("External")) + ")";
        ImGui::PushID(id.c_str());
        if (!exists)
            ImGui::PushStyleColor(ImGuiCol_Text, cyui::vec(cyui::palette().warning));
        bool expanded =
            fileTreeNode(name, project.at("path"),
                         ImGuiTreeNodeFlags_SpanAvailWidth | (folder ? 0 : ImGuiTreeNodeFlags_Leaf) |
                             (depth < 1 ? ImGuiTreeNodeFlags_DefaultOpen : 0),
                         theme_, folder);
        if (!exists)
            ImGui::PopStyleColor();
        cyui::tooltip(project.at("path").get_ref<const std::string &>().c_str());
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !folder &&
            exists && !external)
            open(project.at("relativePath"));
        if (ImGui::BeginPopupContextItem()) {
            extensionMenu("project");
            if (ImGui::MenuItem(tr("Open project file"), nullptr, false, exists && !folder && !external))
                open(project.at("relativePath"));
            if (ImGui::MenuItem(tr("Open in new window"), nullptr, false, exists && !folder))
                newWindow(utf8Path(project.at("path")));
            ImGui::EndPopup();
        }
        if (expanded) {
            for (const auto &item : project.at("items")) {
                auto path = item.at("path").get<std::string>();
                if (fileSelectable(pathUtf8(utf8Path(path).filename()), path, false, theme_) &&
                    item.at("exists").get<bool>() && !item.at("external").get<bool>())
                    open(item.at("relativePath"));
            }
            solutionTree(id, depth + 1);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}
void Application::projectPanel() {
    if (!beginPanel("Project", "project"))
        return;
    resourceFocused_=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    cyui::overline(pathUtf8(engine_->workspace().filename()).c_str());
    ImGui::SameLine();
    cyui::alignRight(cyui::dp(24));
    if (cyui::iconButton(cyui::icon::refresh, tr("Refresh")))
        refreshFiles();
    ImGui::SetNextItemWidth(-1);
    viewFilter("##filter", filesFilter_);
    cyui::divider();
    if (solution_.is_object()) {
        cyui::textDim("%s", pathUtf8(utf8Path(solution_.at("path")).filename()).c_str());
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##solution-config", solutionConfiguration_.c_str())) {
            for (const auto &config : solution_.at("configurations")) {
                auto text = config.get<std::string>();
                if (ImGui::Selectable(text.c_str(), text == solutionConfiguration_)) {
                    solutionConfiguration_ = text;
                    if (msbuildActive_) { auto split = text.find('|'); msbuildConfiguration_ = text.substr(0,split); msbuildPlatform_ = text.substr(split+1); buildEdited_ = true; refreshBuild(); }
                    if (unreal_.value("available", false)) {
                        auto bar = text.find('|'), space = text.find(' ');
                        unrealConfiguration_ = text.substr(0, std::min(space, bar));
                        if (bar != std::string::npos)
                            unrealPlatform_ = text.substr(bar + 1);
                        const bool editor = text.find("Editor") != std::string::npos;
                        for (const auto &t : unreal_.at("targets")) {
                            auto name = t.at("name").get<std::string>();
                            if (name.ends_with("Editor") == editor) {
                                unrealTarget_ = name;
                                break;
                            }
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }
        if (solution_.at("missingProjects").get<size_t>()) {
            cyui::textColoured(cyui::palette().warning, "%s: %zu", tr("Missing project files"),
                               solution_.at("missingProjects").get<size_t>());
            cyui::tooltip(tr("Generated Unreal project files may be absent. Available source files remain "
                             "accessible below."));
        }
    }
    ImGui::BeginChild("##files");
    if (solution_.is_object() && !filesFilter_.active()) {
        if (ImGui::TreeNodeEx(tr("Solution"), ImGuiTreeNodeFlags_DefaultOpen)) {
            solutionTree("");
            ImGui::TreePop();
        }
        cyui::divider();
        cyui::overline(tr("Workspace files"));
    }
    if (files_.empty())
        cyui::emptyState(tr(filesFuture_.valid() ? "Loading files..." : "No files"), tr("Open a workspace or refresh the file list."),
                         cyui::icon::folder);
    else if (!filesFilter_.active()) {
        tree(tree_, "");
        revealPath_.clear();
    } else {
        for (const auto &path : files_)
            if (filesFilter_.matches(path, path))
            {
                ImGui::PushID(path.c_str());
                if (fileSelectable(path, path, active_ == path, theme_)) {
                    selectedResource_=path;
                    open(path);
                }
                if(ImGui::BeginPopupContextItem()) { resourceMenu(path,false);ImGui::EndPopup(); }
                ImGui::PopID();
            }
    }
    ImGui::EndChild();
    ImGui::End();
}
TextEditor::Palette Application::editorPalette(const std::string &theme) {
    return editorColours(theme);
}
void Application::editorPanel() {
    editorFocused_ = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, cyui::dp(0, 0));
    ImGuiWindowClass hostClass;
    hostClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
    ImGui::SetNextWindowClass(&hostClass);
    bool visible = ImGui::Begin("##editors", nullptr, ImGuiWindowFlags_NoTitleBar);
    editorDock_ = ImGui::GetID("##document-dock");
    ImGui::DockSpace(editorDock_, ImVec2(0, 0),
                     ImGuiDockNodeFlags_PassthruCentralNode |
                         (visible && !tabs_.empty() ? 0 : ImGuiDockNodeFlags_KeepAliveOnly));
    if (visible && tabs_.empty()) {
        cyui::space(80);
        cyui::heading("CyCode", 1);
        cyui::textDim("%s", tr("A workspace for your code."));
        cyui::space(12);
        if (cyui::primaryButton(tr("Open solution...")))
            chooseSolution();
        ImGui::SameLine();
        if (cyui::button(tr("Open workspace...")))
            chooseFolder();
    }
    ImGui::End();
    std::string close;
    for (auto &tab : tabs_) {
        bool opened = true;
        auto title = tabTitle(tab);
        ImGui::SetNextWindowDockID(editorDock_, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(cyui::dp(850, 650), ImGuiCond_FirstUseEver);
        if (focusTab_ == tab->path) {
            ImGui::SetNextWindowFocus();
            focusTab_.clear();
        }
        const bool rendered =
            ImGui::Begin(title.c_str(), &opened, ImGuiWindowFlags_NoFocusOnAppearing | (tab->dirty ? ImGuiWindowFlags_UnsavedDocument : 0));
        documentIcon(tab->path, theme_);
        tab->visible=rendered;
        if (rendered) {
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
                active_ = tab->path;
            cyui::space(4);
            cyui::textFaint("  %s", tab->displayPath.c_str());
            if (tab->image) imagePanel(tab);
            else {
            if (tab->snippet.active()) {
                ImGui::SameLine();cyui::badge((std::string(tr("Snippet parameter"))+" "+std::to_string(tab->snippet.index())).c_str(),cyui::Tone::accent);
                cyui::tooltip(tr("Tab: next | Shift+Tab: previous | Esc: finish"));
                const auto choices=tab->snippet.choices();
                if (!choices.empty()) {
                    ImGui::SameLine();ImGui::SetNextItemWidth(cyui::dp(160));
                    if (ImGui::BeginCombo(("##snippet-choice"+tab->path).c_str(),tr("Snippet choices"))) {
                        for (size_t i=0;i<choices.size();++i) { ImGui::PushID(static_cast<int>(i));if (ImGui::Selectable(choices[i].c_str())) {
                            try { tab->snippet.choose(i);changed(tab);tab->editor.SetFocus(); } catch (const std::exception &e) { error(e.what()); }
                        }ImGui::PopID(); }
                        ImGui::EndCombo();
                    }
                }
            }
            if (tab->readOnly) { ImGui::SameLine(); cyui::badge(tr(tab->path.starts_with("debug://") ? "Read-only debugger source" : "Read-only SDK source"), cyui::Tone::neutral); }
            if(!tab->diskConflict.empty())cyui::callout(cyui::Tone::warning,tr(tab->diskConflict=="missing"?"File disappeared from disk; the open buffer is preserved.":"File changed on disk. Reload it or save a copy."));
            if(tab->diagnosticTruncated)cyui::textDim("%s",tr("Additional diagnostics are available in Problems."));
            cyui::divider(4);
            const auto undoBefore = tab->editor.GetUndoIndex();
            const auto occurrenceChild = ImGui::GetID(("##text" + tab->path).c_str());
            const bool renderedChange = tab->codeEditor.draw(("##text" + tab->path).c_str());
            if (!renderedChange && tab->editor.GetUndoIndex() == undoBefore && tab->path == active_ && occurrencesCurrent())
                drawOccurrenceAnnotations(tab->editor,occurrences_.annotations,occurrenceChild);

            // Popup edits occur after the widget's Render change flag is computed.
            // Include their transactions so completion/replace reaches Engine now.
            if (renderedChange || tab->editor.GetUndoIndex() != undoBefore)
                changed(tab);
            applySessionEditor(tab,tab->editor.GetUndoIndex() != undoBefore);
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !(ImGui::GetActiveID() && ImGui::GetCurrentContext()->InputTextState.ID == ImGui::GetActiveID())) editorFocused_ = true;
            }
        }
        if (ImGui::BeginPopupContextWindow("##document-actions", ImGuiPopupFlags_MouseButtonRight |
                                                                     ImGuiPopupFlags_NoOpenOverItems)) {
            extensionMenu("editor");
            if (ImGui::MenuItem(tr("Open included file"),nullptr,false,
                                tab->path==active_ && commandEnabled("navigate.include")))executeCommand("navigate.include");
            if (ImGui::MenuItem(tr("Find related C/C++ files..."),nullptr,false,
                                tab->path==active_ && commandEnabled("navigate.relatedFiles")))executeCommand("navigate.relatedFiles");
            if (ImGui::MenuItem(tr("Move to new window"))) {
                ImGui::SetWindowDock(ImGui::FindWindowByName(title.c_str()), 0, ImGuiCond_Always);
            }
            if (ImGui::MenuItem(tr("Return to editor"))) {
                ImGui::DockBuilderDockWindow(title.c_str(), editorDock_);
            }
            ImGui::EndPopup();
        }
        ImGui::End();
        if (!opened)
            close = tab->path;
    }
    if (!close.empty())
        closeTab(close);
    ImGui::PopStyleVar();
}
void Application::searchPanel() {
    if (!beginPanel("Search", "search"))
        return;
    if (focusSearch_) { ImGui::SetKeyboardFocusHere(); focusSearch_ = false; }
    bool changed = ImGui::InputTextWithHint("##query", tr("Find in workspace..."), &searchQuery_);
    changed |= ImGui::Checkbox(tr("Case sensitive"), &caseSensitive_);
    ImGui::SameLine();
    changed |= ImGui::Checkbox(tr("Regex"), &regex_);
    if (changed) {
        if (searchCancellation_)
            *searchCancellation_ = true;
        searchChanged_ = ImGui::GetTime();
        ++searchGeneration_;
        searchQueued_ = true;
    }
    if (searchFuture_.valid())
        cyui::spinner("##searching");
    cyui::divider();
    if (!search_.contains("matches"))
        cyui::emptyState(tr("Search your project"),
                         tr("Results include unsaved documents."), cyui::icon::search);
    else {
        cyui::textDim("%zu %s", search_["matches"].size(), tr("matches"));
        if (search_.value("truncated", false))
            cyui::badge(tr("Results limited"), cyui::Tone::warning);
        if (search_.value("unsavedRegexExcluded", false))
            cyui::callout(cyui::Tone::warning, tr("Regex excludes modified buffers."));
        ImGui::BeginChild("##results");
        for (const auto &hit : search_["matches"]) {
            auto path = hit["path"].get<std::string>();
            auto line = hit["line"].get<size_t>();
            auto title = path + ":" + std::to_string(line) + "  " + hit["text"].get<std::string>();
            ImGui::PushID(title.c_str());
            if (ImGui::Selectable(title.c_str()))
                open(path, line, hit["column"].get<size_t>());
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
void Application::outputPanel() {
    if (!beginPanel("Build", "build"))
        return;
    liveBuildOutput();
    if (commandEnabled("build.cancel")) {
        if (cyui::button(tr("Cancel build"))) executeCommand("build.cancel");
        if (!buildCancellationId_.empty()) { ImGui::SameLine(); cyui::textDim("%s", tr("Cancelling build...")); }
    }
    if (projectActive_) {
        cyui::textDim("%s",projectState_.value("name",projectState_.value("kind","")).c_str()); ImGui::SameLine();
        { cyui::ScopedDisabled disabled(taskFuture_.valid()); if (cyui::button(tr("Build project"))) task("build.run"); }
        ImGui::SameLine(); if (cyui::button(tr("Project tasks"))) showPanel("Project tasks","projectTasks");
        if ((taskFuture_.valid() && taskMethod_.starts_with("project.")) || engine_->dispatch("project.status").value("running",false)) { ImGui::SameLine(); if (cyui::button(tr("Cancel task"))) cancelProjectTask(); }
        ImGui::SameLine(); if (cyui::ghostButton(tr("Copy output"))) ImGui::SetClipboardText(output_.c_str());
        outputLog_.synchronize(output_); navigableOutput("##output", outputLog_);
        ImGui::End(); return;
    }
    if (msbuildActive_) {
        cyui::textDim("%s | %s",msbuildConfiguration_.c_str(),msbuildPlatform_.c_str()); ImGui::SameLine();
        { cyui::ScopedDisabled disabled(taskFuture_.valid()); if (cyui::button(tr("Build project"))) task("build.run"); ImGui::SameLine(); if (cyui::button(tr("MSBuild settings"))) showPanel("MSBuild","msbuild"); }
        ImGui::SameLine(); if (cyui::ghostButton(tr("Copy output"))) ImGui::SetClipboardText(output_.c_str());
        outputLog_.synchronize(output_); navigableOutput("##output", outputLog_);
        ImGui::End(); return;
    }
    ImGui::SetNextItemWidth(cyui::dp(100));
    auto &configuration = unreal_.value("available", false) ? unrealConfiguration_ : configuration_;
    const std::vector<std::string> configurations =
        unreal_.value("available", false)
            ? std::vector<std::string>{"Debug", "DebugGame", "Development", "Shipping", "Test"}
            : std::vector<std::string>{"Debug", "Release", "RelWithDebInfo"};
    if (ImGui::BeginCombo("##config", configuration.c_str())) {
        for (const auto &value : configurations)
            if (ImGui::Selectable(value.c_str(), configuration == value)) {
                configuration = value;
                buildEdited_ = true;
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (cyui::button(tr(unreal_.value("available", false) ? "Generate project files" : "Configure")))
        task("build.configure", {{"source", buildSource_}, {"buildDirectory", buildDirectory_}});
    ImGui::SameLine();
    if (cyui::button(tr("Build project")))
        task("build.run", {{"source", buildSource_},
                           {"buildDirectory", buildDirectory_},
                           {"configuration", configuration_}});
    ImGui::SameLine();
    if (taskFuture_.valid())
        cyui::spinner("##task");
    ImGui::SameLine();
    if (cyui::ghostButton(tr("Copy output")))
        ImGui::SetClipboardText(output_.c_str());
    ImGui::SameLine();
    if (cyui::ghostButton(tr(unreal_.value("available", false) ? "Unreal Engine" : "CMake settings")))
        showPanel(unreal_.value("available", false) ? "Unreal Engine" : "CMake",
                  unreal_.value("available", false) ? "unreal" : "cmake");
    outputLog_.synchronize(output_); navigableOutput("##output", outputLog_);
    ImGui::End();
}
void Application::applyUnreal(const Json &value) {
    unreal_ = value;
    if (!value.value("available", false))
        return;
    const auto &profile = value.at("profile");
    unrealProject_ = profile.at("project");
    unrealEngine_ = profile.at("engineRoot");
    unrealTarget_ = profile.at("target");
    unrealPlatform_ = profile.at("platform");
    unrealConfiguration_ = profile.at("configuration");
    if (solution_.is_object()) {
        auto selected =
            unrealConfiguration_ + (unrealTarget_.ends_with("Editor") ? " Editor|" : "|") + unrealPlatform_;
        for (const auto &config : solution_.at("configurations"))
            if (config == selected)
                solutionConfiguration_ = selected;
    }
}
void Application::unrealPanel() {
    if (!beginPanel("Unreal Engine", "unreal"))
        return;
    if (!unreal_.value("available", false)) {
        cyui::emptyState(tr("No Unreal project"), tr("Open a .uproject or its .sln solution."),
                         cyui::icon::folder);
        if (cyui::button(tr("Open solution...")))
            chooseSolution();
        ImGui::End();
        return;
    }
    if (unrealAnalysisState_ == "preparing") {
        cyui::spinner("##unreal-analysis"); ImGui::SameLine(); cyui::textDim("%s", tr("Preparing Unreal code analysis..."));
        ImGui::TextWrapped("%s", tr("Includes, reflection macros and generated headers are being prepared in the background."));
    } else if (unrealAnalysisState_ == "ready") cyui::textColoured(cyui::palette().success, "%s", tr("Unreal code analysis ready"));
    else if (unrealAnalysisState_ == "failed") {
        cyui::textColoured(cyui::palette().warning, "%s", tr("Unreal analysis failed. See Build output."));
        if (cyui::button(tr("Retry Unreal analysis"))) prepareUnreal();
    }
    cyui::sectionTitle(unreal_.value("name", "Unreal Engine").c_str());
    ImGui::SameLine();
    cyui::badge(unreal_.value("engineAssociation", "").c_str(), cyui::Tone::neutral);
    if (cyui::beginProperties("##unreal-settings", 150)) {
        cyui::property(tr("Project"));
        cyui::textDim("%s", unrealProject_.c_str());
        cyui::property(tr("Engine directory"));
        ImGui::InputText("##ue-engine", &unrealEngine_);
        cyui::property(tr("Target"));
        if (ImGui::BeginCombo("##ue-target", unrealTarget_.c_str())) {
            for (const auto &t : unreal_.at("targets")) {
                auto name = t.at("name").get<std::string>();
                if (ImGui::Selectable(name.c_str(), unrealTarget_ == name))
                    unrealTarget_ = name;
            }
            ImGui::EndCombo();
        }
        cyui::property(tr("Platform"));
        if (ImGui::BeginCombo("##ue-platform", unrealPlatform_.c_str())) {
            for (auto value : {"Win64", "Linux", "LinuxArm64", "Mac", "Android", "IOS"})
                if (ImGui::Selectable(value, unrealPlatform_ == value))
                    unrealPlatform_ = value;
            ImGui::EndCombo();
        }
        cyui::property(tr("Configuration"));
        if (ImGui::BeginCombo("##ue-configuration", unrealConfiguration_.c_str())) {
            for (auto value : {"Debug", "DebugGame", "Development", "Shipping", "Test"})
                if (ImGui::Selectable(value, unrealConfiguration_ == value))
                    unrealConfiguration_ = value;
            ImGui::EndCombo();
        }
        cyui::endProperties();
    }
    if (cyui::button(tr("Refresh Unreal project")))
        task("unreal.inspect");
    ImGui::SameLine();
    if (cyui::button(tr("Generate project files")))
        task("unreal.generate");
    ImGui::SameLine();
    if (cyui::button(tr("Generate code index")))
        task("unreal.database");
    ImGui::SameLine();
    if (cyui::primaryButton(tr("Build project")))
        task("unreal.build");
    cyui::divider();
    if (!unreal_.value("ready", false))
        cyui::textColoured(cyui::palette().warning, "%s", tr("Select the matching Unreal engine directory."));
    if (!unreal_.at("compilationDatabase").value("available", false))
        ImGui::TextWrapped("%s", tr("Generate the code index for Unreal include paths, macros and symbol "
                                    "navigation. The engine SDK and generated headers are required."));
    cyui::textDim("%s: %zu  |  %s: %zu  |  %s: %zu", tr("Targets"), unreal_.at("targets").size(),
                  tr("Modules"), unreal_.at("modules").size(), tr("Plugins"), unreal_.at("plugins").size());
    unrealLaunchControls();
    unrealArtifactsControls();
    unrealAutomationControls();
    ImGui::BeginChild("##unreal-rules");
    for (const auto *section : {"targets", "modules", "plugins"})
        for (const auto &item : unreal_.at(section)) {
            const auto path = item.at("path").get<std::string>();
            if (fileSelectable(path, path, active_ == path, theme_))
                open(path);
        }
    ImGui::EndChild();
    ImGui::End();
}
void Application::cmakePanel() {
    if (!beginPanel("CMake", "cmake"))
        return;
    if (projectActive_) { if (cyui::button(tr("Project tasks"))) showPanel("Project tasks","projectTasks"); ImGui::End(); return; }
    if (msbuildActive_) { cyui::textDim("%s",tr("This project uses MSBuild.")); if (cyui::button(tr("MSBuild settings"))) showPanel("MSBuild","msbuild"); ImGui::End(); return; }
    if (unreal_.value("available", false)) {
        cyui::textDim("%s", tr("This project uses UnrealBuildTool."));
        if (cyui::button(tr("Unreal Engine")))
            showPanel("Unreal Engine", "unreal");
        ImGui::End();
        return;
    }
    if (cyui::button(tr("Configure")))
        task("build.configure");
    ImGui::SameLine();
    if (cyui::button(tr("Build project")))
        task("build.run");
    ImGui::SameLine();
    if (cyui::ghostButton(tr("Detect toolchains")))
        refreshBuild(true);
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::refresh, tr("Refresh CMake project")))
        refreshBuild();
    ImGui::SameLine();
    if (buildFuture_.valid() || detectionFuture_.valid())
        cyui::spinner("##cmake-pending");
    cyui::divider(4);
    if (ImGui::BeginTable("##cmake-layout", 2, ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("##settings", ImGuiTableColumnFlags_WidthStretch, 0.58f);
        ImGui::TableSetupColumn("##model", ImGuiTableColumnFlags_WidthStretch, 0.42f);
        ImGui::TableNextColumn();
        {
            cyui::ScopedDisabled disabled(taskFuture_.valid());
            if (cyui::beginProperties("##cmake-settings", cyui::dp(120))) {
                cyui::property(tr("CMake source directory"));
                buildEdited_ |= ImGui::InputText("##cmake-source", &buildSource_);
                const auto presets = buildState_.value("presets", Json::object());
                cyui::property(tr("Configure preset"));
                if (ImGui::BeginCombo("##configure-preset", buildConfigurePreset_.empty()
                                                                ? tr("Manual settings")
                                                                : buildConfigurePreset_.c_str())) {
                    if (ImGui::Selectable(tr("Manual settings"), buildConfigurePreset_.empty())) {
                        buildConfigurePreset_.clear();
                        buildBuildPreset_.clear();
                        if(configuration_!="Debug" && configuration_!="Release" && configuration_!="RelWithDebInfo" && configuration_!="MinSizeRel")configuration_="Debug";
                        buildEdited_ = true;
                        refreshBuild();
                    }
                    for (const auto &preset : presets.value("configurePresets", Json::array())) {
                        const auto name = preset.at("name").get<std::string>();
                        cyui::ScopedDisabled disabled(preset.value("error", "") != "");
                        if (ImGui::Selectable(name.c_str(), name == buildConfigurePreset_)) {
                            buildConfigurePreset_ = name;
                            buildBuildPreset_.clear();
                            buildTarget_.clear();
                            buildToolchain_ = buildGenerator_ = "automatic";
                            buildEdited_ = true;
                            refreshBuild();
                        }
                        cyui::tooltip(preset.value("error", "") != ""
                                          ? preset.at("error").get_ref<const std::string &>().c_str()
                                          : preset.value("description", "").c_str());
                    }
                    ImGui::EndCombo();
                }
                if (!buildConfigurePreset_.empty()) {
                    cyui::property(tr("Build preset"));
                    if (ImGui::BeginCombo("##build-preset", buildBuildPreset_.empty()
                                                                ? tr("Default build")
                                                                : buildBuildPreset_.c_str())) {
                        if (ImGui::Selectable(tr("Default build"), buildBuildPreset_.empty())) {
                            buildBuildPreset_.clear();
                            buildEdited_ = true;
                            refreshBuild();
                        }
                        for (const auto &preset : presets.value("buildPresets", Json::array()))
                            if (preset.value("configurePreset", "") == buildConfigurePreset_) {
                                const auto name = preset.at("name").get<std::string>();
                                cyui::ScopedDisabled disabled(preset.value("error", "") != "");
                                if (ImGui::Selectable(name.c_str(), name == buildBuildPreset_)) {
                                    buildBuildPreset_ = name;
                                    buildTarget_.clear();
                                    buildEdited_ = true;
                                    refreshBuild();
                                }
                            }
                        ImGui::EndCombo();
                    }
                }
                if (presets.value("error", "") != "")
                    ImGui::TextWrapped("%s", presets.at("error").get_ref<const std::string &>().c_str());
                {
                    cyui::ScopedDisabled presetFields(!buildConfigurePreset_.empty());
                    cyui::property(tr("Build directory"));
                    buildEdited_ |= ImGui::InputText("##cmake-build", &buildDirectory_);
                    cyui::property(tr("Toolchain"));
                    std::string selected = tr("Automatic");
                    for (const auto &entry : toolchains_.value("toolchains", Json::array()))
                        if (entry.at("id") == buildToolchain_)
                            selected = entry.at("name");
                    if (buildToolchain_ != "automatic" && selected == tr("Automatic"))
                        selected = buildToolchain_;
                    if (ImGui::BeginCombo("##toolchain", selected.c_str())) {
                        if (ImGui::Selectable(tr("Automatic"), buildToolchain_ == "automatic")) {
                            buildToolchain_ = "automatic";
                            buildEdited_ = true;
                        }
                        for (const auto &entry : toolchains_.value("toolchains", Json::array())) {
                            if (ImGui::Selectable(entry.at("name").get_ref<const std::string &>().c_str(),
                                                  entry.at("id") == buildToolchain_)) {
                                buildToolchain_ = entry.at("id");
                                buildEdited_ = true;
                            }
                            cyui::tooltip(entry.at("cxxCompiler").get_ref<const std::string &>().c_str());
                        }
                        ImGui::EndCombo();
                    }
                    cyui::property(tr("Generator"));
                    if (ImGui::BeginCombo("##generator", buildGenerator_ == "automatic"
                                                             ? tr("Automatic")
                                                             : buildGenerator_.c_str())) {
                        if (ImGui::Selectable(tr("Automatic"), buildGenerator_ == "automatic")) {
                            buildGenerator_ = "automatic";
                            buildEdited_ = true;
                        }
                        for (const auto &entry : toolchains_.value("generators", Json::array())) {
                            const auto name = entry.at("name").get<std::string>();
                            if (ImGui::Selectable(name.c_str(), name == buildGenerator_)) {
                                buildGenerator_ = name;
                                buildEdited_ = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                const auto list = buildBuildPreset_.empty() ? presets.value("configurePresets", Json::array())
                                                            : presets.value("buildPresets", Json::array());
                bool fixedConfiguration = false;
                for (const auto &preset : list)
                    if (preset.value("name", "") ==
                        (buildBuildPreset_.empty() ? buildConfigurePreset_ : buildBuildPreset_))
                        fixedConfiguration = preset.value("configuration", "") != "";
                {
                    cyui::ScopedDisabled fixed(fixedConfiguration);
                    cyui::property(tr("Configuration"));
                    if (ImGui::BeginCombo("##configuration", configuration_.c_str())) {
                        for (const auto *config : {"Debug", "Release", "RelWithDebInfo", "MinSizeRel"})
                            if (ImGui::Selectable(config, configuration_ == config)) {
                                configuration_ = config;
                                buildEdited_ = true;
                            }
                        ImGui::EndCombo();
                    }
                }
                cyui::property(tr("Target"));
                if (ImGui::BeginCombo("##target",
                                      buildTarget_.empty() ? tr("All targets") : buildTarget_.c_str())) {
                    if (ImGui::Selectable(tr("All targets"), buildTarget_.empty())) {
                        buildTarget_.clear();
                        buildEdited_ = true;
                    }
                    for (const auto &entry : buildState_.value("targets", Json::array())) {
                        const auto name = entry.at("name").get<std::string>();
                        if (ImGui::Selectable(name.c_str(), name == buildTarget_)) {
                            buildTarget_ = name;
                            buildEdited_ = true;
                        }
                    }
                    ImGui::EndCombo();
                }
                cyui::endProperties();
            }
        }
        ImGui::TableNextColumn();
        const auto database = buildState_.value("compilationDatabase", Json::object());
        cyui::badge(database.value("available", false) ? tr("Compilation database ready")
                                                       : tr("Configure for code analysis"),
                    database.value("available", false) ? cyui::Tone::success : cyui::Tone::neutral);
        if (database.value("available", false)) {
            cyui::textDim("%zu %s / %s", database.value("entries", size_t(0)), tr("translation units"),
                          database.value("origin", "").c_str());
            ImGui::TextWrapped("%s", database.value("path", "").c_str());
        }
        auto generator = buildState_.value("generator", "");
        if (!generator.empty())
            cyui::textDim("%s", generator.c_str());
        for (const auto &compiler : buildState_.value("toolchains", Json::array())) {
            const auto data = compiler.value("compiler", Json::object());
            cyui::textDim("%s: %s %s", compiler.value("language", "").c_str(), data.value("id", "").c_str(),
                          data.value("version", "").c_str());
        }
        for (const auto &warning : toolchains_.value("warnings", Json::array()))
            ImGui::TextWrapped("%s", warning.get_ref<const std::string &>().c_str());
        if (buildEdited_)
            cyui::textDim("%s", tr("Settings apply on configure."));
        ImGui::EndTable();
    }
    ImGui::End();
}
void Application::problemsPanel() {
    if (!beginPanel("Problems", "problems"))
        return;
    languageServerControls();
    problemRows();
    auto message = languageState_.value("error", std::string{});
    if (!message.empty())
        ImGui::TextWrapped("%s", message.c_str());
    ImGui::End();
}
void Application::extensionsPanel() {
    if (!beginPanel("Extensions", "extensions"))
        return;
    cyui::textDim("%s 1 / 2", tr("Plugin ABI"));
    if (pluginInfo_.contains("host")) {
        const auto &host = pluginInfo_.at("host");
        cyui::textDim("CyCode %s | %s", host.value("version", std::string{}).c_str(),
                      host.value("platform", std::string{}).c_str());
    }
    if(disablePlugins_)cyui::callout(cyui::Tone::warning,tr("Extensions are disabled for this session (--disable-plugins)."));
    packageControls();
    cyui::filterBox("##extensions-filter",extensionsFilter_);
    if(cyui::button(tr("Refresh extensions")))task("plugins.list");
    for (const auto &plugin : pluginInfo_.value("plugins", Json::array())) {
        if(!extensionsFilter_.matches(plugin.value("name",plugin.value("id",std::string{})),plugin.value("id",std::string{})))continue;
        ImGui::PushID(plugin.value("id",plugin.value("manifest",std::string("error"))).c_str());
        cyui::sectionTitle(plugin.value("name", plugin.value("id", std::string("Plugin"))).c_str());
        cyui::badge(plugin.value("state", std::string{}).c_str(),
                    plugin.value("state", std::string{}) == "loaded" ? cyui::Tone::success
                                                                     : cyui::Tone::error);
        ImGui::SameLine();
        cyui::textDim("%s", plugin.value("version", std::string{}).c_str());
        if (plugin.contains("error"))
            ImGui::TextWrapped("%s", plugin["error"].get<std::string>().c_str());
        const auto lifecycle = plugin.value("lifecycle", Json::object());
        if (lifecycle.value("supported", Json(false)) == true)
            cyui::textDim("%s", tr("Activation notifications supported"));
        if (lifecycle.contains("error"))
            cyui::callout(cyui::Tone::error, lifecycle.at("error").get_ref<const std::string &>().c_str());
        if(plugin.contains("enabled")) {
            cyui::ScopedDisabled disabled(taskFuture_.valid() || plugin.contains("error"));
            if(cyui::button(tr(plugin.at("enabled").get<bool>()?"Disable for this project":"Enable for this project")))
                task("plugins.configure",{{"id",plugin.at("id")},{"enabled",!plugin.at("enabled").get<bool>()},{"expectedRevision",pluginInfo_.at("revision")}});
        }
        ImGui::PopID();
    }
    for(const auto &panel:pluginInfo_.value("panels",Json::array())) {
        const auto title=panel.value("title",panel.at("id").get<std::string>());
        if(extensionsFilter_.matches(title,"Panels")&&extensionVisible(panel))if(cyui::button(tr(title.c_str())))executeCommand("extension.panel."+panel.at("id").get<std::string>());
    }
    for(const auto &subscription:pluginInfo_.value("eventSubscriptions",Json::array())) {
        const auto title=subscription.value("title",subscription.at("id").get<std::string>());
        if(!extensionsFilter_.matches(title,"Events"))continue;
        cyui::textDim("%s: %s",tr("Event subscription"),title.c_str());
        if(subscription.value("error","")!="") {
            cyui::callout(cyui::Tone::warning,subscription.at("error").get_ref<const std::string &>().c_str());
            cyui::textDim("%s",tr("Disable and enable the plugin to restart its event subscription."));
        }
    }
    for (const auto &command : pluginInfo_.value("commands", Json::array())) {
        const std::string id=command.at("id"),title=command.at("title");
        if(!extensionVisible(command)||!extensionsFilter_.matches(title,id))continue;
        ImGui::PushID(id.c_str());cyui::ScopedDisabled disabled(!commandEnabled(id));
        if(cyui::button(tr(title.c_str())))executeCommand(id);
        ImGui::PopID();
    }
    ImGui::End();
}
Json Application::mcpConfiguration() const {
    const auto python=mcpInterpreterPath(root_);
    return {{"mcpServers",
             {{"cycode",
               {{"command", pathUtf8(python)},
                {"args",
                 Json::array({"-B", "-X", "utf8", pathUtf8(root_ / "app/tool.py"), "--bridge-config", pathUtf8(coreDescriptor_),
                              "--access", mcpWrite_ ? "edit" : "read", "mcp"})}}}}}};
}
void Application::mcpPanel() {
    if (!beginPanel("MCP", "mcp"))
        return;
    cyui::badge(tr("Native bridge ready"), cyui::Tone::success);
    ImGui::SameLine();
    cyui::textDim("CyToolsCore 0.10.1");
    ImGui::SameLine();
    cyui::badge(
        tr(std::filesystem::exists(coreDescriptor_) ? "Shared Core ready" : "Core starting or unavailable"),
        std::filesystem::exists(coreDescriptor_) ? cyui::Tone::success : cyui::Tone::neutral);
    ImGui::TextWrapped("%s", tr("Connect an assistant to inspect this workspace and its live documents."));
    ImGui::Checkbox(tr("Enable editing, builds and plugin commands"), &mcpWrite_);
    if (cyui::button(tr("Copy MCP configuration")))
        ImGui::SetClipboardText(mcpConfiguration().dump(2).c_str());
    ImGui::SameLine();
    if (cyui::button(tr("Save MCP configuration"))) {
        auto path = root_ / "data/mcp-config.json";
        std::ofstream out(path);
        out << mcpConfiguration().dump(2);
        cyui::notify(pathUtf8(path).c_str(), cyui::Tone::success);
    }
    cyui::divider();
    cyui::textDim("%s", tr("Available services"));
    ImGui::TextWrapped("%s", tr("Workspace, live documents, language services, search, Git, CMake/MSBuild/Unreal, terminal, debugger and plugins. "
                                "MCP sessions use CyToolsCore jobs and permissions."));
    ImGui::End();
}
void Application::optionsPanel() {
    if (!options_)
        return;
    ImGui::SetNextWindowSize(cyui::dp(620, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(label("Options", "options").c_str(), &options_)) {
        cancelPropertyDrafts(optionsInspector_); ImGui::End(); return;
    }
    optionsProperties();
    auto buildLimit=desktopBuildTimeout(settings_);
    if(ImGui::InputInt(tr("Build time limit (seconds)"),&buildLimit,60,600))settings_["buildTimeoutSeconds"]=std::clamp(buildLimit,1,86400);
    cyui::textDim("%s",tr("Applies to CMake, MSBuild and Unreal build processes. Cancel remains available."));
    keymapOptions();
    if (cyui::button(tr("Save preferences")))
        saveSettings();
    ImGui::SameLine();
    if (cyui::button(tr("Save configuration"))) {
        auto path = root_ / "data/settings/configuration.json";
        saveSettings();
        std::ofstream out(path);
        out << settings_.dump(2);
    }
    ImGui::SameLine();
    if (cyui::button(tr("Load configuration"))) {
        try {
            std::ifstream in(root_ / "data/settings/configuration.json");
            auto value = Json::parse(in);
            cancelPropertyDrafts(optionsInspector_); ++optionsRevision_;
            theme_ = value.at("theme");
            locale_ = value.at("language");
            textScale_ = std::clamp(value.at("textScale").get<float>() * (value.value("schemaVersion", 1) < 3 ? 1.15f : 1.0f), .8f, 2.0f);
            auto profile = value.value("keymap", std::string("rider"));
            auto overrides = value.value("keymapOverrides", Json::object());
            applyKeymap(profile, overrides.value(profile, Json::object())); keymapProfile_ = profile; keymapOverrides_ = overrides;
            clangd_ = value.at("clangd");
            settings_["buildTimeoutSeconds"]=desktopBuildTimeout(value);
            applyEditorTheme(theme_);
            ImGui::GetStyle().FontScaleMain = textScale_;
        } catch (const std::exception &e) {
            error(e.what());
        }
    }
    cyui::divider();
    cyui::sectionTitle(tr("Licenses"));
    ImGui::TextWrapped("CyUI (MIT): https://github.com/MrMybal/CyUI/blob/ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3/LICENSE");
    ImGui::TextWrapped("%s", tr("CyCode: GPL-3.0-only. Copyright (c) 2026 Cyberalien. "
                                "This program comes without any warranty. See LICENSE and COPYRIGHT."));
    ImGui::TextWrapped("%s", tr("CyUI, Dear ImGui, TextEditor, CyToolsCore and nlohmann/json: MIT. SDL3: zlib."));
    ImGui::TextWrapped("%s", tr("libvterm and CodeLLDB: MIT. LLVM: Apache 2.0 with LLVM exceptions. Embedded Python: PSF."));
    ImGui::TextWrapped("%s", "NetCoreDbg: MIT (Samsung). app/licenses/NetCoreDbg.txt");
    ImGui::TextWrapped("%s", tr("csharp-ls: MIT. Private .NET SDK: included Microsoft terms and component notices."));
    if (cyui::button(tr("Open third-party notices")))
        open("app/THIRD_PARTY_NOTICES.md");
    ImGui::End();
}
void Application::palette() {
    if (counterpartsPalette()) { quickOpenVisible_=true;return; }
    if (snippetsPalette()) { quickOpenVisible_=true;return; }
    const auto catalogue=commandCatalogue();
    if (paletteOpen_ && paletteMode_ == "actions") {
        actionPaletteOpen_ = true; paletteOpen_ = false;
    }
    std::vector<cyui::Command> actions;
    const auto loadActions = [&] {
        if (!actions.empty()) return;
        for (const auto &command : catalogue) actions.push_back({uiId("command/" + command.id),
            command.title, keymap_.hint(command.id), command.category + " " + command.id, commandEnabled(command.id)});
    };
    if (actionPaletteOpen_) loadActions();
    const auto requested = cyui::commandPalette("Find action...", actionPaletteOpen_, actionPaletteState_, actions, cyui::dp(850, 570));
    if (requested) for (const auto &command : catalogue) if (uiId("command/" + command.id) == requested) { executeCommand(command.id); break; }
    quickOpenVisible_ = actionPaletteOpen_;
    if (actionPaletteOpen_) { paletteFilter_ = actionPaletteState_.query; return; }
    if (paletteOpen_) {
        ImGui::OpenPopup("##quickopen"); paletteOpen_ = false; paletteFilter_.clear(); paletteSelection_ = 0;
        symbolWantedKey_.clear(); symbolSentKey_.clear(); symbolResults_ = Json::object(); symbolError_.clear(); symbolAttempts_ = 0; symbolResolveKey_.clear(); symbolResolved_=Json::object();
    }
    ImGui::SetNextWindowSize(cyui::dp(850, 570), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(.5f, .5f));
    if (!ImGui::BeginPopup("##quickopen")) { symbolWantedKey_.clear(); symbolResolveKey_.clear(); symbolResolved_=Json::object(); return; }
    quickOpenVisible_ = true;
    const char *heading = paletteMode_ == "actions" ? "Find action..." : paletteMode_ == "everywhere" ? "Search Everywhere" : paletteMode_ == "recent" ? "Recent files" : paletteMode_ == "types" ? "Go to type..." : paletteMode_ == "symbols" ? "Go to symbol..." : paletteMode_ == "members" ? "File structure" : "Go to file...";
    cyui::heading(tr(heading), 2);
    for (const auto &[id, text] : std::initializer_list<std::pair<const char *, const char *>>{{"everywhere", "All"}, {"files", "Files"}, {"types", "Types"}, {"symbols", "Symbols"}, {"members", "Members"}, {"actions", "Actions"}, {"recent", "Recent files"}}) {
        bool selected = paletteMode_ == id;
        ImGui::BeginDisabled(std::string(id) == "members" && !engine_->supportsLanguage(active_));
        if (cyui::toggleButton(tr(text), &selected)) { paletteMode_ = id; paletteSelection_ = 0; }
        ImGui::EndDisabled(); ImGui::SameLine();
    }
    ImGui::NewLine();
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##quickquery", tr("Type a name; > actions, @ members, # symbols..."), &paletteFilter_)) paletteSelection_ = 0;
    auto query = paletteFilter_;
    auto mode = paletteMode_;
    if (query.starts_with(">") || query.starts_with("@") || query.starts_with("#")) {
        mode = query[0] == '>' ? "actions" : query[0] == '@' ? "members" : "symbols";
        query.erase(0,1); while (query.starts_with(" ")) query.erase(0,1);
    }
    const bool backendChanged = (mode == "types" || mode == "symbols" || mode == "everywhere") && symbolBackendSelector();
    if (backendChanged) paletteSelection_ = 0;
    const bool memberSource = engine_->supportsLanguage(active_) && !active_.starts_with("debug://");
    if (mode == "members" && !memberSource) cyui::textDim("%s", tr("Open a source file with a language server."));
    const bool wantSymbols = !smoke_ && (mode == "types" || mode == "symbols" || (mode == "members" && memberSource) || (mode == "everywhere" && !query.empty()));
    bool symbolRefresh=false;
    if (wantSymbols) {
        if (cyui::button(tr("Refresh symbols"))) { ++symbolSearchSerial_; symbolRefresh=true; }
        auto backend=mode=="members" || symbolBackend_.empty() ? engine_->languageBackendFor(active_) : symbolBackend_;
        if (backend.empty()) backend="cpp";
        const auto emptyServer = Json::object();
        const auto servers = languageState_.find("servers");
        const auto &server = servers != languageState_.end() && servers->contains(backend) ? servers->at(backend) : emptyServer;
        Json parameters = {{"query", query}, {"expectedEpoch",server.value("epoch",uint64_t(0))}, {"language",backend}, {"typesOnly", mode == "types"}, {"includeExternal", true}, {"limit", 500}};
        std::string method = "language.symbols";
        if (mode == "members") {
            method = "language.documentSymbols";
            parameters = {{"path", active_}, {"expectedEpoch",server.value("epoch",uint64_t(0))}, {"includeExternal", true}, {"limit", 1000}};
            for (const auto &tab : tabs_) if (tab->path == active_) { parameters["expectedRevision"] = tab->revision; parameters["expectedDocumentId"] = tab->identity; }
        }
        auto key = method + parameters.dump() + ":" + std::to_string(symbolSearchSerial_);
        if (key != symbolWantedKey_) { paletteSelection_ = 0; symbolWantedKey_ = key; symbolWantedParams_ = parameters; symbolWantedMethod_ = method; symbolResults_ = Json::object(); symbolError_.clear(); symbolResolveKey_.clear(); symbolResolved_=Json::object(); symbolChanged_ = ImGui::GetTime(); symbolAttempts_ = 0; }
        if (!symbolFuture_.valid() && symbolSentKey_ != symbolWantedKey_ && ImGui::GetTime()-symbolChanged_ > .12 && !(backend=="cpp" && unrealAnalysisState_ == "preparing") && parameters.contains(mode == "members" ? "expectedRevision" : "query")) {
            auto engine = engine_; symbolSentKey_ = symbolWantedKey_; ++symbolAttempts_;
            symbolFuture_ = std::async(std::launch::async, [engine, parameters, method, key] {
                try { return Json{{"key", key}, {"result", engine->dispatch(method, parameters)}}; }
                catch (const std::exception &e) { return Json{{"key", key}, {"error", e.what()}}; }
            });
        }
    }
    if (!wantSymbols) { symbolWantedKey_.clear(); symbolResolveKey_.clear(); symbolResolved_=Json::object(); }
    struct Item { std::string id, title; int kind, score; Json location; };
    std::vector<Item> items;
    if (mode == "actions" || mode == "everywhere") {
        loadActions();
        const auto ranked = cyui::rankCommands(actions, query);
        for (size_t commandIndex = 0; commandIndex < catalogue.size(); ++commandIndex) {
            const auto &command = catalogue[commandIndex];
            const auto name = std::string(tr(command.title.c_str()));
            auto title = std::string(tr(command.category.c_str())) + ": " + name;
            int score = std::max({fuzzyScore(query, name),fuzzyScore(query, title),fuzzyScore(query, command.id)});
            if (!query.empty() && (lower(query) == lower(name) || lower(query) == lower(command.id))) score += 10000;
            const auto common = std::find(ranked.begin(), ranked.end(), commandIndex);
            if (common != ranked.end()) score = std::max(score, 1000 - int(common - ranked.begin()));
            if (score >= 0) items.push_back({command.id, title, 1, score, nullptr});
        }
    }
    if (mode == "files" || mode == "recent" || mode == "everywhere") {
        const auto &paths = mode == "recent" ? recentFiles_ : files_;
        for (const auto &path : paths) {
            auto score = fuzzyScore(query, path);
            if (score < 0) continue;
            auto recent = std::find(recentFiles_.begin(), recentFiles_.end(), path);
            if (recent != recentFiles_.end()) score += static_cast<int>(100 - (recent - recentFiles_.begin()));
            items.push_back({path, path, 0, score, nullptr});
        }
    }
    if (wantSymbols && symbolResultCurrent(symbolResults_) && symbolResults_.contains("symbols")) {
        for (const auto &symbol : symbolResults_.at("symbols")) {
            const auto name = symbol.at("name").get<std::string>(), container = symbol.value("container", "");
            auto score = std::max(fuzzyScore(query, name), fuzzyScore(query, container+"::"+name));
            if (score < 0) continue;
            auto display = symbol.value("displayPath", symbol.at("path").get<std::string>());
            auto title = std::string(tr(symbolKindName(symbol.at("kind")))) + "  " + name + (container.empty() ? "" : "  ("+container+")") + "   " + pathUtf8(utf8Path(display).filename()) + (symbol.value("unresolved", false) ? std::string("  [")+tr("Location on selection")+"]" : ":" + std::to_string(symbol.at("range").at("start").at("line").get<size_t>()+1));
            items.push_back({symbol.at("path"), title, 2, score+30, symbol});
        }
    }
    auto compare = [](const auto &a, const auto &b) { return a.score == b.score ? a.title < b.title : a.score > b.score; };
    if (items.size() > 100) { std::partial_sort(items.begin(), items.begin()+100, items.end(), compare); items.resize(100); }
    else std::sort(items.begin(), items.end(), compare);
    paletteSelection_ = std::clamp(paletteSelection_, 0, std::max(0, static_cast<int>(items.size())-1));
    bool moved = false;
    const bool paletteKeys = !backendChanged && !symbolRefresh && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows | ImGuiFocusedFlags_NoPopupHierarchy);
    if (paletteKeys && ImGui::IsKeyPressed(ImGuiKey_DownArrow) && !items.empty()) { paletteSelection_ = (paletteSelection_+1) % items.size(); moved = true; }
    if (paletteKeys && ImGui::IsKeyPressed(ImGuiKey_UpArrow) && !items.empty()) { paletteSelection_ = (paletteSelection_+items.size()-1) % items.size(); moved = true; }
    const bool enter = paletteKeys && ImGui::IsKeyPressed(ImGuiKey_Enter);
    if (wantSymbols && (symbolFuture_.valid() || symbolSentKey_ != symbolWantedKey_)) { cyui::spinner("##symbols"); ImGui::SameLine(); cyui::textDim("%s", tr("Searching symbols...")); }
    if (wantSymbols && !symbolResolveKey_.empty() && symbolResolveFuture_.valid()) { cyui::spinner("##symbolresolve"); ImGui::SameLine(); cyui::textDim("%s", tr("Resolving symbol location...")); }
    if (wantSymbols && !symbolError_.empty()) cyui::textColoured(cyui::palette().warning, "%s", symbolError_.c_str());
    if (wantSymbols && symbolResults_.value("truncated", false)) cyui::textDim("%s", tr("Results limited. Refine the symbol name."));
    std::string chosen; int kind = 0; Json destination;
    ImGui::BeginChild("##quickresults");
    for (size_t i = 0; i < items.size(); ++i) {
        const auto &item = items[i]; bool clicked = false;
        ImGui::PushID(static_cast<int>(i));
        const bool enabled = (item.kind != 1 || commandEnabled(item.id)) && (item.kind != 2 || !symbolResolveFuture_.valid());
        ImGui::BeginDisabled(!enabled);
        if (item.kind == 1) {
            clicked = ImGui::Selectable(item.title.c_str(), paletteSelection_ == static_cast<int>(i));
            const auto shortcut = keymap_.hint(item.id);
            if (!shortcut.empty()) { ImGui::SameLine(); cyui::textFaint("  %s", shortcut.c_str()); }
        } else if (item.kind == 2) {
            clicked = ImGui::Selectable(item.title.c_str(), paletteSelection_ == static_cast<int>(i));
            const auto detail = item.location.value("detail", std::string{});
            if (!detail.empty()) cyui::tooltip(detail.c_str());
        }
        else clicked = fileSelectable(item.id, item.title, paletteSelection_ == static_cast<int>(i), theme_);
        if (moved && paletteSelection_ == static_cast<int>(i)) ImGui::SetScrollHereY();
        ImGui::EndDisabled();
        if (enabled && (clicked || (enter && paletteSelection_ == static_cast<int>(i)))) { chosen = item.id; kind = item.kind; destination = item.location; }
        ImGui::PopID();
    }
    if (items.empty()) cyui::textDim("%s", tr("No matching results"));
    ImGui::EndChild();
    if (!chosen.empty() && kind==2 && destination.value("unresolved", false)) {
        const auto session=symbolResults_.at("session").get<std::string>();
        const auto index=destination.at("resolveIndex");
        const auto selection=session+":"+index.dump();
        const auto key=symbolWantedKey_; const auto epoch=symbolResults_.at("epoch");
        auto engine=engine_; symbolResolveKey_=selection; symbolError_.clear(); symbolResolved_=Json::object();
        symbolResolveFuture_=std::async(std::launch::async,[engine,session,index,key,selection,epoch] {
            try { return Json{{"key",key},{"selection",selection},{"result",engine->dispatch("language.symbolResolve",{{"session",session},{"index",index},{"expectedEpoch",epoch}})}}; }
            catch (const std::exception &e) { return Json{{"key",key},{"selection",selection},{"error",e.what()}}; }
        });
        chosen.clear();
    }
    if (!chosen.empty()) { symbolResolved_=Json::object(); symbolResolveKey_.clear(); }
    if (symbolResolved_.contains("result")) {
        if (symbolResolved_.at("key")==symbolWantedKey_ && symbolResolved_.at("selection")==symbolResolveKey_ && symbolResultCurrent(symbolResolved_.at("result"))) {
            destination=symbolResolved_.at("result").at("symbol"); chosen=destination.at("path"); kind=2;
        }
        symbolResolved_=Json::object(); symbolResolveKey_.clear();
    }
    if (!chosen.empty()) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    if (!chosen.empty()) {
        if (kind == 1) executeCommand(chosen);
        else if (kind == 2) { const auto p = destination.at("range").at("start"); open(chosen, p.at("line").get<size_t>()+1, p.at("character").get<size_t>()+1, true); }
        else open(chosen);
    }
}
void Application::windowTest() {
    ++windowTestFrame_;
    auto build = ImGui::FindWindowByName(label("Build", "build").c_str());
    auto document = tabs_.empty() ? nullptr : ImGui::FindWindowByName(tabTitle(tabs_[0]).c_str());
    if (windowTestFrame_ == 20 && build) {
        windowTest_.push_back({{"stage", "initial"}, {"buildDock", build->DockId}});
        ImGui::SetWindowDock(build, 0, ImGuiCond_Always);
        auto pos = ImGui::GetMainViewport()->Pos;
        ImGui::SetWindowPos(build, ImVec2(pos.x + cyui::dp(100), pos.y + cyui::dp(100)));
        ImGui::SetWindowSize(build, cyui::dp(650, 350));
    }
    if (windowTestFrame_ == 25 && document) {
        ImGui::SetWindowDock(document, 0, ImGuiCond_Always);
        auto pos = ImGui::GetMainViewport()->Pos;
        ImGui::SetWindowPos(document, ImVec2(pos.x + cyui::dp(250), pos.y + cyui::dp(150)));
        ImGui::SetWindowSize(document, cyui::dp(850, 550));
    }
    if (windowTestFrame_ == 55 && build && document) {
        auto viewport = build->Viewport;
        auto native = SDL_GetWindowFromID(
            static_cast<SDL_WindowID>(reinterpret_cast<uintptr_t>(viewport->PlatformHandle)));
        auto editorNative = SDL_GetWindowFromID(
            static_cast<SDL_WindowID>(reinterpret_cast<uintptr_t>(document->Viewport->PlatformHandle)));
        int count = 0;
        auto windows = SDL_GetWindows(&count);
        SDL_free(windows);
        windowTest_.push_back(
            {{"stage", "detached"},
             {"nativeWindows", count},
             {"panelNative", native && native != window_},
             {"editorNative", editorNative && editorNative != window_ && editorNative != native}});
        if (native) {
            SDL_SetWindowPosition(native, int(ImGui::GetMainViewport()->Pos.x) + 150,
                                  int(ImGui::GetMainViewport()->Pos.y) + 120);
            SDL_SetWindowSize(native, 700, 380);
        }
    }
    if (windowTestFrame_ == 75 && document)
        ImGui::SetWindowDock(document, editorDock_, ImGuiCond_Always);
    if (windowTestFrame_ == 90 && build) {
        auto native = SDL_GetWindowFromID(
            static_cast<SDL_WindowID>(reinterpret_cast<uintptr_t>(build->Viewport->PlatformHandle)));
        if (native && native != window_) {
            SDL_Event event{};
            event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            event.window.windowID = SDL_GetWindowID(native);
            SDL_PushEvent(&event);
        }
    }
    if (windowTestFrame_ == 115) {
        windowTest_.push_back({{"stage", "closed"},
                               {"panelHidden", !panels_["build"]},
                               {"editorDocked", document && document->DockId == editorDock_}});
        showPanel("Build", "build");
        if (build && !windowTest_.empty())
            ImGui::SetWindowDock(build, windowTest_[0].value("buildDock", ImGuiID(0)), ImGuiCond_Always);
    }
    if (windowTestFrame_ == 140) {
        int count = 0;
        auto windows = SDL_GetWindows(&count);
        SDL_free(windows);
        windowTest_.push_back({{"stage", "restored"},
                               {"nativeWindows", count},
                               {"panelVisible", panels_["build"]},
                               {"panelDocked", build && build->DockId != 0}});
    }
}
void Application::frame() {
    outputLog_.synchronize(output_);
    cyui::setLocale(locale_.c_str());
    if (!options_) cancelPropertyDrafts(optionsInspector_);
    update();
    shortcuts();
    if (testWindows_)
        windowTest();
    menu();
    topBar();
    statusBar();
    rail();
    auto dock = cyui::dockSpace("##workspace");
    bool rebuiltLayout = false;
    if (!layoutReady_ || resetLayout_) {
        layoutReady_ = true;
        if (resetLayout_ || smoke_ || !ImGui::DockBuilderGetNode(dock) ||
            ImGui::DockBuilderGetNode(dock)->IsLeafNode()) {
            layout(dock);
            rebuiltLayout = true;
        }
        resetLayout_ = false;
    }
    projectPanel();
    resourcePanel();
    projectTemplatePanel();
    userSnippetsPanel();
    searchPanel();
    editorPanel();
    outputPanel();
    unrealPanel();
    cmakePanel();
    presetActionsPanel();
    msbuildPanel();
    projectTasksPanel();
    testsPanel();
    gitPanel();
    problemsPanel();
    referencesPanel();
    informationPanel();
    hierarchyPanel();
    structurePanel();
    occurrencePanel();
    applicationUpdatesPanel();
    codeActionsPanel();
    refactorPanel();
    terminalPanel();
    debugPanel();
    extensionsPanel();
    extensionPanels();
    recoveryPanel();
    mcpPanel();
    if (rebuiltLayout) {
        panels_["build"] = true;
        focusPanel_.clear();
        if (!active_.empty()) focusTab_ = active_;
        else ImGui::SetWindowFocus("##editors");
    }
    if (focusCmake_) {
        showPanel("CMake", "cmake");
        focusCmake_ = false;
    }
    if (focusProjectTasks_) { showPanel("Project tasks","projectTasks"); focusProjectTasks_ = false; }
    if (focusUnreal_) {
        showPanel("Unreal Engine", "unreal");
        focusUnreal_ = false;
    }
    optionsPanel();
    updateWorkspaceSession();
    palette();
    goToLine();
    renamePopup();
    if (closeAll_ && pendingClose_.empty() && !saveAsDocument_ && saveAsQueue_.empty()) { if (tabs_.empty()) closeAll_ = false; else closeTab(tabs_.back()->path); }
    documentDraftDialogs();
    gitWorktreeConfirmation();
    if (newFile_) {
        ImGui::OpenPopup(tr("New file..."));
        newFile_ = false;
    }
    if (ImGui::BeginPopupModal(tr("New file..."), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetNextItemWidth(cyui::dp(420));
        ImGui::InputTextWithHint("##newpath", tr("Workspace-relative path"), &newFilePath_);
        if (cyui::button(tr("Open unsaved"))) {
            try {
                const auto doc = engine_->dispatch("documents.createUntitled", {{"name",newFilePath_}});
                open(doc.at("path"));
                ImGui::CloseCurrentPopup();
            } catch (const std::exception &e) { error(e.what()); }
        }
        ImGui::SameLine();
        if (cyui::primaryButton(tr("Create"))) {
            try {
                engine_->dispatch("documents.create", {{"path", newFilePath_}});
                open(newFilePath_);
                refreshFiles();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception &e) {
                error(e.what());
            }
        }
        ImGui::SameLine();
        if (cyui::button(tr("Cancel")))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (!pendingClose_.empty() && !saveAsDocument_ && saveAsQueue_.empty()) {
        ImGui::OpenPopup(tr("Unsaved changes"));
        if (ImGui::BeginPopupModal(tr("Unsaved changes"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(pendingClose_.c_str());
            if (cyui::primaryButton(tr("Save"))) {
                const bool draft = isUntitledDocument(pendingClose_);
                for (auto &tab : tabs_)
                    if (tab->path == pendingClose_)
                        save(tab);
                if (draft) ImGui::CloseCurrentPopup();
                bool dirty = false;
                for (auto &tab : tabs_)
                    if (tab->path == pendingClose_)
                        dirty = tab->dirty;
                if (!dirty) {
                    auto close = pendingClose_;
                    pendingClose_.clear();
                    closeTab(close);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (cyui::dangerButton(tr("Discard"))) {
                auto close = pendingClose_;
                pendingClose_.clear();
                closeTab(close, true);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (cyui::button(tr("Cancel"))) {
                pendingClose_.clear();
                closeAll_ = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    if(quitRequested_ && (userSnippetsOpen_ || userSnippetsFuture_.valid())) {
        if(userSnippetsFuture_.valid()) {
            quitRequested_=false;updateInstallRequested_=false;
            cyui::notify(tr("Wait for user snippet storage before closing."),cyui::Tone::info);
        }
        else if(userSnippetsDirty_) {quitRequested_=false;userSnippetsDiscard_="quit";}
        else userSnippetsOpen_=false;
    }
    if (quitRequested_ && !saveAsDocument_ && saveAsQueue_.empty()) {
        bool dirty = std::any_of(tabs_.begin(), tabs_.end(), [](const auto &tab) { return tab->dirty; });
        if (!dirty)
            done_ = true;
        else {
            ImGui::OpenPopup(tr("Close workspace?"));
            if (ImGui::BeginPopupModal(tr("Close workspace?"), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted(tr("Some documents have unsaved changes."));
                if (cyui::primaryButton(tr("Save all"))) {
                    saveAll();
                    if (!saveAsQueue_.empty()) ImGui::CloseCurrentPopup();
                    done_ =
                        std::none_of(tabs_.begin(), tabs_.end(), [](const auto &tab) { return tab->dirty; });
                }
                ImGui::SameLine();
                if (cyui::dangerButton(tr("Discard"))) {
                    try {
                        // An explicit discard must retire recovery data as well as live tabs.
                        const auto closing=tabs_;
                        for(const auto &tab:closing)closeTab(tab->path,true);
                        done_=tabs_.empty();
                    } catch(const std::exception &e) { error(e.what()); }
                }
                ImGui::SameLine();
                if (cyui::button(tr("Cancel"))) {
                    quitRequested_ = false;
                    updateInstallRequested_ = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }
    cyui::renderNotifications();
}
} // namespace cycode
