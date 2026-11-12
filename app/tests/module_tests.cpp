#include "cycode/Application.h"
#include "cycode/DiagnosticAnnotations.h"
#include "cycode/EditorOccurrences.h"
#include "cycode/EditorTextMap.h"
#include "cycode/EditorTheme.h"
#include "cycode/TextFile.h"
#include <imgui_internal.h>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <thread>

using namespace cycode;
void outlineModelContracts();
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
static void frame(const std::function<void()> &draw, ImGuiKey key = ImGuiKey_None) {
    auto &io = ImGui::GetIO();
    if (key != ImGuiKey_None) io.AddKeyEvent(key, true);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(io.DisplaySize);
    draw(); ImGui::Render();
    if (!ImGui::GetDrawData()->TotalVtxCount)
        throw std::runtime_error("No migrated view draw data at frame " + std::to_string(ImGui::GetFrameCount()));
    if (key != ImGuiKey_None) io.AddKeyEvent(key, false);
}
namespace cycode {
struct ApplicationModuleTests {
    static void interactionContracts(LaunchOptions options) {
        const auto workspace=options.workspace.parent_path()/"interaction-workspace";
        std::filesystem::create_directories(workspace);
        for(const auto *name:{"first.cpp","second.cpp","third.cpp"})
            std::ofstream(workspace/name)<<"int value = 1;\n";
        std::ofstream(workspace/"unicode.cpp",std::ios::binary)<<"\xc3\xa9" "clair \xf0\x9f\x98\x80\nnext \xc3\xa9" "clair\n";
        options.workspace=workspace;
        Application app(options,nullptr);
        app.openFromProject("first.cpp");
        require(app.tabs_.size()==1 && app.tabs_.back()->preview,"Single click did not create a preview");
        app.openFromProject("second.cpp");
        require(app.tabs_.size()==1 && app.tabs_.back()->path=="second.cpp","Preview clicks accumulated tabs");
        app.openFromProject("first.cpp",true);
        require(app.tabs_.size()==2 && !app.tabs_.back()->preview,"Ctrl-click did not retain a separate tab");
        const auto second=*std::find_if(app.tabs_.begin(),app.tabs_.end(),[](const auto &t){return t->path=="second.cpp";});
        second->editor.SetCursor({0,0});second->editor.ReplaceTextPreservingTabs("// edited\n");app.changed(second);
        app.openFromProject("third.cpp");
        require(app.tabs_.size()==3 && second->dirty && !second->preview,"Opening a preview lost or replaced modified code");
        require(app.rightmostTab_=="third.cpp" && app.focusTab_=="third.cpp","New file was not queued for visible rightmost selection");
        const auto snapshot=app.workspaceSessionSnapshot();
        require(std::none_of(snapshot.at("tabs").begin(),snapshot.at("tabs").end(),[](const Json &v){return v.at("path")=="third.cpp";}),"An unpinned preview leaked into session restoration");
        app.openFromProject("third.cpp",true);
        require(!app.tabs_.back()->preview,"Ctrl-click on the current preview did not pin it");
        app.panels_["project"]=true;app.panels_["build"]=true;app.panels_["mcp"]=true;
        app.toggleToolWindows();
        require(app.panels_.at("project") && !app.panels_.at("build") && !app.panels_.at("mcp"),"Tool-window toggle hid the project or left tools open");
        app.toggleToolWindows();
        require(app.panels_.at("build") && app.panels_.at("mcp"),"Hidden tools were not restored");
        app.open("unicode.cpp");const auto unicode=app.tabs_.back();
        app.openDocumentFind(unicode);
        unicode->findQuery="\xc3\xa9" "clair";
        app.findInDocument(unicode);
        require(unicode->editor.GetCursorText(0)==unicode->findQuery,"Find did not select the complete UTF-8 word");
        app.findInDocument(unicode);
        require(unicode->editor.GetMainCursorSelection().start.line==1,"Find next did not advance to the next match");
        app.findInDocument(unicode,true);
        require(unicode->editor.GetMainCursorSelection().start.line==0,"Find previous did not return to the first match");
        unicode->findReplacement="word";
        app.findInDocument(unicode,false,true,true);
        require(unicode->editor.GetText().find(unicode->findQuery)==std::string::npos,"Replace all missed a UTF-8 match");
        require(app.engine_->dispatch("documents.read",{{"path","unicode.cpp"}}).at("text")==unicode->editor.GetText(),"Find/replace created a second unsynchronized document");
        unicode->editor.Undo();app.changed(unicode);
        require(unicode->editor.GetText().starts_with(unicode->findQuery),"Replace all was not one undoable edit");
        app.setMinimap(false,false);app.executeCommand("view.minimap");
        require(app.minimap_ && unicode->editor.IsShowMiniMapEnabled(),"Minimap option was not applied");
        app.open("first.cpp");
        require(app.tabs_.front()->editor.IsShowMiniMapEnabled(),"Minimap option was not shared with existing code tabs");
        const auto sourceText=unicode->editor.GetText();const auto undoIndex=unicode->editor.GetUndoIndex();
        app.setCodeZoom(1.5f);
        require(std::all_of(app.tabs_.begin(),app.tabs_.end(),[](const auto &t){return t->image || t->editor.GetFontScale()==1.5f;}),"Code zoom remained local to a tab");
        require(app.searchPreview_.textScale()==1.5f && app.vcsPreviewCode_.textScale()==1.5f,"Code zoom was not shared with previews");
        std::ofstream(workspace/"new-view.cpp")<<"int another = 2;\n";
        app.open("new-view.cpp");require(app.tabs_.back()->editor.GetFontScale()==1.5f,"A new view reset the shared code zoom");
        require(unicode->editor.GetText()==sourceText && unicode->editor.GetUndoIndex()==undoIndex,"Code zoom changed document text or undo history");
        app.setCodeZoom(1.f);

    }
    static void run(const std::filesystem::path &root) {
        const auto folder = root / "runtime/module-fixture";
        std::filesystem::create_directories(folder / "workspace");
        std::filesystem::create_directories(folder / "app/locales");
        std::filesystem::copy_file(root / "app/locales/ui.json", folder / "app/locales/ui.json", std::filesystem::copy_options::overwrite_existing);
        std::ofstream(folder / "workspace/sample.cpp") << "int main() { return 1; }\n";
        std::ofstream(folder / "workspace/shader.hlsl") << "float4 main() : SV_Target { return 1; }\n";
        std::ofstream(folder / "workspace/events.log") << "Log: Warning: fixture\n";
        LaunchOptions options; options.installation = folder; options.workspace = folder / "workspace";
        options.smoke = true; options.testSession = true;
        interactionContracts(options);
        Application app(options, nullptr); // No SDL initialization or operating-system window.
        unsigned char *pixels=nullptr;int width=0,height=0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        ImGui::GetIO().Fonts->SetTexID(ImTextureID(1));
        app.open("sample.cpp");
        auto tab = app.tabs_.back();
        {
            require(app.commandEnabled("view.analysis"), "Analysis panel command is unavailable");
            app.executeCommand("view.analysis");
            require(app.panels_.at("analysis"), "Analysis command did not open its panel");
            frame([&] { app.analysisPanel(); });
            app.prepareAnalysis({{"paths", Json::array({"sample.cpp"})}, {"batchSize", 16}});
            require(!app.analysisView_.prepare.valid(), "Fixture implicitly started compiler traversal");
            app.analysisView_.status = {{"state", "pending"}, {"session", "current"}};
            app.analysisView_.serial = 8;
            std::promise<Json> stale;
            app.analysisView_.monitor = stale.get_future();
            stale.set_value({{"ticket", 7}, {"analysis", {{"state", "running"}, {"session", "old"}}}});
            app.updateAnalysis();
            require(app.analysisView_.status.at("session") == "current", "Old analysis monitor replaced a new session");
            app.analysisView_ = Application::AnalysisView{};
            app.panels_["analysis"] = false;
        }
        {
            auto paged=std::make_shared<Application::Tab>();paged->path="wide.txt";paged->pagedProfile=true;
            const Json view={{"version",1},{"line",uint64_t(12000000)},{"character",uint64_t(80000000)},
                {"scrollLine",uint64_t(11999995)},{"scrollColumn",uint64_t(79999995)},{"scale",1.5}};
            Json item={{"path","wide.txt"},{"windowKey","wide"},{"line",0},{"column",0},{"scroll",0},{"pagedView",view}};
            app.restoreSessionTab(paged,item);
            require(paged->pagedLocation.at("line")==12000001 && paged->pagedLocation.at("column")==80000001,"Paged session coordinates were truncated to ordinary limits");
            require(app.pagedSessionState(paged)==view,"Pending paged session presentation was lost");
            item["pagedView"]["character"]=uint64_t(-1);app.restoreSessionTab(paged,item);
            require(paged->pagedLocation.at("column")==1,"Malformed paged session coordinates were accepted");
        }
        {
            const auto oldStatus = app.languageState_;
            Json report = {{"symbols", Json::array()}, {"backend", "cpp"}, {"epoch", 17}, {"documentId", tab->identity},
                {"path", tab->path}, {"revision", tab->revision}};
            app.languageState_ = {{"servers", {{"cpp", {{"enabled", true}, {"ready", true}, {"epoch", 17}}}}}};
            require(app.symbolResultCurrent(report), "Current symbol snapshot was rejected");
            auto stale = report; stale["documentId"] = tab->identity + 1;
            require(!app.symbolResultCurrent(stale), "Replaced document retained symbol positions");
            stale = report; stale["epoch"] = 18;
            require(!app.symbolResultCurrent(stale), "Restarted server retained symbol positions");
            auto ready = [](Json value) { std::promise<Json> promise;auto future=promise.get_future();promise.set_value(std::move(value));return future; };
            auto navigation=report;
            require(app.navigationCurrent(navigation),"Current native navigation ownership was rejected");
            navigation["documentId"]=uint64_t(tab->identity+1);
            require(!app.navigationCurrent(navigation),"Replaced source retained navigation ownership");
            navigation=report; navigation["epoch"]=uint64_t(9000);
            require(!app.navigationCurrent(navigation),"Restarted backend retained navigation ownership");
            app.navigationTicket_=7;
            app.navigationFuture_=ready({{"ticket",6},{"error","late navigation error"}});
            app.updateNavigation(); require(app.navigationError_.empty(),"Late navigation error escaped its ticket");
            app.symbolWantedKey_ = app.symbolSentKey_ = "new";
            app.symbolFuture_ = ready({{"key", "old"}, {"error", "late backend error"}});
            app.updateSemantic();require(app.symbolError_.empty(), "Late palette error escaped its request ticket");
            app.symbolResolveKey_="selected";
            app.symbolResolveFuture_=ready({{"key","old"},{"selection","selected"},{"error","late resolution error"}});
            app.updateSemantic(); require(app.symbolError_.empty(),"Late resolve error escaped the query ticket");
            app.symbolResolveFuture_=ready({{"key","new"},{"selection","other"},{"result",report}});
            app.updateSemantic(); require(app.symbolResolved_.empty(),"Another selected symbol replaced the pending resolve");
            app.symbolResolveFuture_=ready({{"key","new"},{"selection","selected"},{"result",report}});
            app.updateSemantic(); require(app.symbolResolved_.at("result")==report,"Current symbol resolution was discarded");
            app.symbolResolved_=Json::object(); app.symbolResolveKey_.clear();
            std::promise<Json> failedResolve;
            app.symbolResolveFuture_=failedResolve.get_future();
            failedResolve.set_exception(std::make_exception_ptr(std::runtime_error("late future exception")));
            app.updateSemantic(); require(app.symbolError_.empty(),"Dismissed resolve future polluted a later query");
            app.symbolAttempts_ = 6;
            app.symbolFuture_ = ready({{"key", "new"}, {"result", report}});
            app.updateSemantic();require(app.symbolResults_==report, "Current palette reply was discarded");
            app.structure_.wanted = "new";
            app.structure_.future = ready({{"key", "old"}, {"result", report}});
            app.updateStructure();require(app.structure_.report.empty(), "Late structure reply escaped its request ticket");
            app.structure_.future = ready({{"key", "new"}, {"result", report}});
            app.updateStructure();require(app.structure_.report==report && app.structure_.sent=="new", "Current structure reply was discarded");
            app.panels_["structure"] = false;
            app.structurePanel();require(app.structure_.wanted.empty() && app.structure_.sent.empty(), "Hidden structure blocked the same query on reopen");
            app.structure_ = Application::StructureView{};
            app.symbolWantedKey_.clear();app.symbolSentKey_.clear();app.symbolResults_=Json::object();app.symbolAttempts_=0;
            app.languageState_ = oldStatus;
        }

        {
            TextEditor annotated;annotated.SetText("// 🛠\nint item;\n\n");
            auto diagnostic=[](size_t a,size_t b,size_t c,size_t d) {
                return Json{{"range",{{"start",{{"line",a},{"character",b}}},{"end",{{"line",c},{"character",d}}}}},{"message","Unicode diagnostic"},{"severity",1}};
            };
            require(applyDiagnosticAnnotations(annotated,Json::array({diagnostic(0,3,0,5)})).count==1&&annotated.HasSquiggles(),"Astral UTF-16 diagnostic was lost");
            require(applyDiagnosticAnnotations(annotated,Json::array({diagnostic(0,4,0,5)})).count==0&&!annotated.HasSquiggles(),"Diagnostic split a UTF-16 surrogate");
            require(applyDiagnosticAnnotations(annotated,Json::array({diagnostic(0,3,1,3),diagnostic(1,9,1,9)})).count==2,"Multiline or end-of-line point diagnostic lost");
            require(applyDiagnosticAnnotations(annotated,Json::array({diagnostic(9,0,9,1),diagnostic(1,3,0,1)})).count==0,"Malformed diagnostic painted onto clamped editor positions");
            annotated.AddSquiggle({1,0},{1,1},777,cyui::palette().info,"Other annotation family");
            applyDiagnosticAnnotations(annotated,Json::array({diagnostic(0,3,0,5)}));
            clearDiagnosticAnnotations(annotated);require(annotated.HasSquiggles(),"Host diagnostics removed another annotation family");
            annotated.ClearSquiggles();
            applyDiagnosticAnnotations(annotated,Json::array({diagnostic(0,3,0,5)}));
            const auto before = annotated.GetText(); const auto undo = annotated.GetUndoIndex();
            const Json occurrences=Json::array({{{"range",diagnostic(0,3,0,5).at("range")},{"kind",2}},{{"range",diagnostic(1,4,1,8).at("range")},{"kind",3}}});
            auto spans=Json::array({occurrences[0].at("range"),occurrences[1].at("range")});
            selectOccurrenceRanges(annotated,spans);
            require(annotated.GetNumberOfCursors()==2 && annotated.GetText()==before && annotated.GetUndoIndex()==undo,"Occurrence selection changed source/Undo or lost cursors");
            bool invalid=false; try {selectOccurrenceRanges(annotated,Json::array({spans[0],diagnostic(1,5,1,6).at("range"),spans[1]}));} catch (...) {invalid=true;}
            require(invalid && annotated.GetNumberOfCursors()==2 && annotated.GetText()==before,"Invalid preflight partially replaced selections");
            const auto model=occurrenceAnnotations(annotated,occurrences,7);
            require(model.count==2 && !model.truncated && model.lines.at(0).front().first==3,"Read/write Unicode geometry missing");
            require(annotated.HasSquiggles(),"Occurrence geometry removed host diagnostics");
            clearDiagnosticAnnotations(annotated);require(!annotated.HasSquiggles(),"Occurrence geometry mutated native glyph annotations");
            annotated.AddSquiggle({1,0},{1,1},777,cyui::palette().info,"Plugin annotation");
            require(occurrenceAnnotations(annotated,occurrences,4).count==1 && annotated.HasSquiggles(),"Kind filtering removed plugin annotations");
            annotated.ClearSquiggles();
            require(occurrenceAnnotations(annotated,occurrences,0).count==0 && !annotated.HasSquiggles(),"Kind filter ignored");
            auto invalidSpan=occurrences; invalidSpan[0]["range"]=diagnostic(0,4,0,5).at("range");
            require(occurrenceAnnotations(annotated,invalidSpan,7).count==1,"Geometry split a UTF-16 surrogate");
            TextEditor wrapped;wrapped.SetText("\titem 🛠 value item\tmore more more more more more more more more");wrapped.SetTabSize(4);wrapped.SetWordWrapEnabled(true);
            const auto wide=occurrenceAnnotations(wrapped,Json::array({{{"range",diagnostic(0,0,0,17).at("range")},{"kind",2}}}),7);
            require(wide.count==1 && wide.lines.at(0).front().widths.front()==4,"Native tab layout widths were lost");
            auto geometry=[&]{ImGui::Begin("Occurrence geometry");const auto id=ImGui::GetID("##geometry");wrapped.Render("##geometry",{180,130});drawOccurrenceAnnotations(wrapped,wide,id);ImGui::End();};
            frame(geometry);frame(geometry);
            Json crowded=Json::array();for(int i=0;i<513;++i)crowded.push_back(diagnostic(1,0,1,3));
            require(applyDiagnosticAnnotations(annotated,crowded).truncated,"Diagnostic display budget was not reported");
        }
        {
            const auto status=app.languageState_; app.smoke_=false; app.occurrences_.automatic=false;
            app.languageState_={{"servers",{{"cpp",{{"enabled",true},{"ready",true},{"epoch",17},{"methods",{{"documentHighlights",true}}}}}}}};
            const auto oldCursor=tab->editor.GetMainCursorPosition();
            auto params=app.occurrenceParameters(); require(!params.is_null(),"Occurrence context unavailable");
            app.occurrences_.wanted=app.occurrences_.sent=params.dump();
            auto report=Json{{"path",tab->path},{"documentId",tab->identity},{"revision",tab->revision},{"backend","cpp"},{"epoch",17},{"position",params.at("position")},{"positionEncoding","utf-16"},{"occurrences",Json::array()},{"invalid",0},{"truncated",false}};
            std::promise<Json> completion; app.occurrences_.future=completion.get_future();
            completion.set_value({{"key",params.dump()},{"result",report}});
            app.occurrenceAction("code.clearOccurrences");app.updateOccurrences();
            require(!app.occurrences_.report.contains("occurrences"),"Clear allowed a pending report to return");
            app.occurrences_.report=report;require(app.occurrencesCurrent(),"Current occurrence report rejected");
            app.occurrences_.report["documentId"]=tab->identity+1;require(!app.occurrencesCurrent(),"Reopened document reused occurrence report");
            app.occurrences_.report=report;tab->editor.SetCursor({0,oldCursor.index==0 ? 1u : 0u});require(!app.occurrencesCurrent(),"Caret movement reused a stale occurrence query");
            app.updateOccurrences();require(app.occurrences_.suppressed.empty(),"Caret change retained occurrence suppression");
            tab->editor.SetCursor(oldCursor);app.clearOccurrences();app.occurrences_=Application::OccurrenceView{};
            app.smoke_=true;app.languageState_=status;
        }
        {
            app.checkApplicationUpdates(true);require(!app.updateCheckFuture_.valid(),"Test session started update network I/O");
            app.downloadApplicationUpdate(updates::DownloadMode::fresh);
            require(!app.updateDownloadFuture_.valid(),"Test session started a fresh update transfer");
            app.refreshApplicationUpdates(false,true);
            require(!app.updateInspectFuture_.valid(),"Test session refreshed a failed download");
            app.refreshApplicationUpdates();require(!app.updateInspectFuture_.valid(),"Test session started update cache I/O");
            app.inspectUpdateStorage();require(!app.updateStorageFuture_.valid(),"Test session inspected update storage");
            app.updateStorageAcknowledged_=true;app.actOnUpdateStorage();
            require(!app.updateStorageActionFuture_.valid(),"Test session changed cached downloads");app.updateStorageAcknowledged_=false;
            app.updateInstallRequested_=true;app.done_=app.quitRequested_=true;
            require(!app.finishQuit() && !app.done_ && !app.quitRequested_,"Test session launched installer or closed the app");
            const auto skip=app.skippedUpdate_;app.smoke_=app.testSession_=false;app.updateStartupChecked_=true;
            app.updateCancellation_=std::make_shared<std::atomic<bool>>(false);app.skippedUpdate_="0.3.0";
            updates::Release release;release.version="0.3.0";release.page="https://github.com/MrMybal/CyCode/releases/tag/v0.3.0";
            std::promise<std::optional<updates::Release>> check;app.updateCheckFuture_=check.get_future();check.set_value(release);
            app.updateApplicationUpdates();require(app.latestUpdate_ && !app.updateOffer_,"Skipped update proposed on startup");
            app.updateCheckManual_=true;std::promise<std::optional<updates::Release>> manual;app.updateCheckFuture_=manual.get_future();manual.set_value(release);
            app.updateApplicationUpdates();require(app.updateOffer_,"Manual check could not review a skipped update");
            app.updateOffer_=false;app.updateCancellation_->store(true);
            std::promise<std::optional<updates::Release>> cancelled;app.updateCheckFuture_=cancelled.get_future();cancelled.set_value(std::nullopt);
            app.updateApplicationUpdates();require(!app.updateChecked_ && !app.updateOffer_,"Cancelled check reported up to date");
            app.updateCancellation_->store(false);app.updateChecked_=false;
            updates::Inventory cached;cached.prepared=updates::Prepared{};cached.prepared->version="0.3.0";
            std::promise<updates::Inventory> scan;app.updateInspectFuture_=scan.get_future();scan.set_value(cached);
            app.updateApplicationUpdates();require(app.preparedUpdate_ && !app.updateOffer_ && !app.updateInstallRequested_,"Cached skipped update restored installation consent");
            std::promise<std::optional<updates::Release>> refresh;app.updateCheckFuture_=refresh.get_future();refresh.set_value(std::nullopt);
            app.updateApplicationUpdates();require(app.preparedUpdate_.has_value(),"Network check discarded a verified cached update");
            app.preparedUpdate_->digest=std::string(64,'a');release.asset.digest=std::string(64,'b');
            std::promise<std::optional<updates::Release>> replaced;app.updateCheckFuture_=replaced.get_future();replaced.set_value(release);
            app.updateApplicationUpdates();require(!app.preparedUpdate_,"Replaced release asset retained stale cached digest");
            app.preparedUpdate_.reset();app.updateInventory_={};app.updateNotice_.clear();
            app.updateOffer_=false;app.latestUpdate_.reset();app.skippedUpdate_=skip;app.smoke_=app.testSession_=true;
            app.updateInstallRequested_=true;app.quitRequested_=false;app.smoke_=app.testSession_=false;
            app.updateApplicationUpdates();require(!app.updateInstallRequested_,"Cancelled close retained install intent");
            // Inject only completed futures: no transfer/cache operation is launched.
            app.preparedUpdate_.reset();app.latestUpdate_.reset();app.updateOffer_=false;
            app.updateInspectAfterDownload_=true;
            app.updateDownloadIssue_=updates::DownloadIssue::integrity;
            app.updateDownloadDetails_="Preserved diagnostic";
            app.updateNotice_="Download cancelled. Received bytes are retained for an explicit retry.";
            app.updateCancellation_->store(false);
            updates::Inventory afterDownload;afterDownload.prepared=updates::Prepared{};
            afterDownload.prepared->version="0.4.0";
            std::promise<updates::Inventory> retained;app.updateInspectFuture_=retained.get_future();retained.set_value(afterDownload);
            app.updateApplicationUpdates();
            require(app.preparedUpdate_ && !app.updateOffer_ && !app.updateInstallRequested_ &&
                    app.updateDownloadIssue_==updates::DownloadIssue::integrity && app.updateDownloadDetails_=="Preserved diagnostic",
                    "Post-download inspection lost feedback or reopened a proposal");
            app.latestUpdate_=updates::Release{};app.latestUpdate_->version="0.4.0";
            app.latestUpdate_->asset.digest=std::string(64,'b');afterDownload.prepared->digest=std::string(64,'a');
            app.updateInspectAfterDownload_=true;
            std::promise<updates::Inventory> quietReplaced;app.updateInspectFuture_=quietReplaced.get_future();quietReplaced.set_value(afterDownload);
            app.updateApplicationUpdates();require(!app.preparedUpdate_ && !app.updateOffer_,"Quiet refresh restored a replaced release digest");
            app.updateInspectAfterDownload_=true;app.updateCancellation_->store(true);
            std::promise<updates::Inventory> stopRefresh;app.updateInspectFuture_=stopRefresh.get_future();stopRefresh.set_value({});
            app.updateApplicationUpdates();
            require(app.updateNotice_=="Download cancelled. Received bytes are retained for an explicit retry." &&
                    !app.updateCacheNotice_.empty() && app.updateDownloadDetails_=="Preserved diagnostic",
                    "Cancelled local refresh overwrote the original download result");
            app.updateCancellation_->store(false);app.updateInspectAfterDownload_=true;
            std::promise<updates::Inventory> failedRefresh;app.updateInspectFuture_=failedRefresh.get_future();
            failedRefresh.set_exception(std::make_exception_ptr(std::runtime_error("Injected inspection failure")));
            app.updateApplicationUpdates();
            require(!app.updateCacheNotice_.empty() && app.updateDownloadIssue_==updates::DownloadIssue::integrity &&
                    app.updateDownloadDetails_=="Preserved diagnostic","Failed refresh lost download diagnosis");
            app.updateDownloadIssue_=updates::DownloadIssue::none;app.updateDownloadDetails_.clear();
            app.updateNotice_.clear();app.updateCacheNotice_.clear();app.latestUpdate_.reset();
            app.smoke_=app.testSession_=true;
        }
        const auto uri=LanguageClient::uri(folder/"workspace/sample.cpp");
        Json published={{"ready",true},{"enabled",true},{"semanticGeneration",1},{"diagnosticGeneration",1},
            {"diagnosticRevisions",{{tab->path,{{"revision",tab->revision},{"id",tab->identity},{"uri",uri}}}}},
            {"diagnostics",{{uri,Json::array({{{"range",{{"start",{{"line",0},{"character",0}}},{"end",{{"line",0},{"character",3}}}}},{"message","Current native revision"},{"severity",1}}})}}}};
        app.languageState_={{"servers",{{"cpp",published}}}};app.updateDiagnostics();
        require(tab->diagnosticCount==1&&tab->editor.HasSquiggles(),"Current versioned publication did not reach CodeEditor");
        app.languageState_["servers"]["cpp"]["diagnosticRevisions"][tab->path]["revision"]=tab->revision+1;
        app.updateDiagnostics();require(!tab->editor.HasSquiggles()&&tab->diagnosticCount==0,"Stale publication remained visible");
        app.languageState_["servers"]["cpp"]=published;app.updateDiagnostics();
        app.languageState_["servers"]["cpp"]["diagnosticRevisions"]=Json::object();
        app.updateDiagnostics();require(!tab->editor.HasSquiggles(),"Unversioned advisory positions were treated as a proven revision");
        app.languageState_=Json::object();
        app.debugAdapterId_="fixture.debug.adapter";app.debugAdapterOptions_=R"({"module":"demo","sourceMap":{"old":"new"}})";
        const auto pluginDebug=app.debugParameters();
        require(pluginDebug.at("adapterId")=="fixture.debug.adapter"&&pluginDebug.at("adapterOptions").at("module")=="demo","GUI lost contributed DAP selection/options");
        for(const auto *bad:{"{broken",R"({"cwd":"outside"})",R"({"args":["forged"]})"}) {
            app.debugAdapterOptions_=bad;bool denied=false;try{app.debugParameters();}catch(const std::exception &){denied=true;}
            require(denied,"GUI accepted malformed or host-owned DAP options");
        }
        app.debugAdapterId_="builtin";app.debugAdapterOptions_="{}";
        require(!app.debugParameters().contains("javaAttach"),"Native GUI profile leaked JVM attach options");
        app.debugAdapterId_="builtin.java";app.debugAttachHost_="192.0.2.24";app.debugAttachPort_=5105;app.debugAttachTimeout_=8000;
        const auto javaDebug=app.debugParameters();
        require(javaDebug.at("adapterId")=="builtin.java" && javaDebug.at("javaAttach").at("hostName")=="192.0.2.24" &&
                    javaDebug.at("javaAttach").at("port")==5105 && javaDebug.at("javaAttach").at("timeout")==8000 && !javaDebug.contains("pid"),
                "GUI lost the typed JVM endpoint or converted it to a native PID");
        app.debugAttachPort_=0;bool endpointDenied=false;try{app.debugParameters();}catch(const std::exception &){endpointDenied=true;}
        require(endpointDenied,"GUI accepted an invalid JDWP port");
        app.debugAdapterId_="builtin";app.debugAttachHost_="127.0.0.1";app.debugAttachPort_=5005;app.debugAttachTimeout_=5000;
        {
            const auto previous = app.debugState_;
            app.debugState_ = {{"sessionGeneration",3},{"stopGeneration",7},{"threadId",11},{"frameId",9}};
            app.debugStepGranularity_ = 3; app.debugSingleThread_ = true;
            const auto step = app.debugExecutionParameters("debug.stepBack",Json::object());
            require(step.at("threadId") == 11 && step.at("granularity") == "instruction" && step.at("singleThread") == true &&
                        step.at("sessionGeneration") == 3 && step.at("stopGeneration") == 7,
                    "GUI stepping lost observed context or instruction/thread options");
            require(!app.debugExecutionParameters("debug.continue",Json::object()).contains("granularity") &&
                        !app.debugExecutionParameters("debug.reverseContinue",Json::object()).contains("granularity"),
                    "Continue leaked stepping-only granularity");
            const auto restart = app.debugExecutionParameters("debug.restartFrame",Json::object());
            const auto runTo = app.debugExecutionParameters("debug.runToCursor",{{"path","source.cpp"},{"line",9},{"expectedDocumentId",5},{"expectedRevision",6},{"expectedBreakpoints",Json::array()},{"sessionGeneration",2},{"stopGeneration",6},{"threadId",10}});
            require(runTo.at("path") == "source.cpp" && runTo.at("line") == 9 && runTo.at("expectedDocumentId") == 5 && runTo.at("expectedRevision") == 6 &&
                        runTo.at("sessionGeneration") == 2 && runTo.at("stopGeneration") == 6 && runTo.at("threadId") == 10 && runTo.at("singleThread") == true && !runTo.contains("granularity"),
                    "Run-to-cursor GUI replaced captured source/context guards or injected stepping granularity");
            const auto targeted = app.debugExecutionParameters("debug.stepIntoTarget",{{"target","step-1-0"},{"frameId",8},{"stopGeneration",6},{"threadId",10},{"sessionGeneration",2}});
            const auto relocated = app.debugExecutionParameters("debug.gotoTarget",{{"target","goto-1-0"},{"frameId",8},{"stopGeneration",6},{"threadId",10},{"sessionGeneration",2}});
            require(relocated.at("frameId") == 8 && relocated.at("stopGeneration") == 6 && relocated.at("threadId") == 10 && relocated.at("sessionGeneration") == 2 &&
                        relocated.at("target") == "goto-1-0" && !relocated.contains("targetId") && !relocated.contains("granularity") && !relocated.contains("singleThread"),
                    "Next-statement GUI lost captured context or leaked unsupported execution options");
            require(targeted.at("frameId") == 8 && targeted.at("stopGeneration") == 6 && targeted.at("threadId") == 10 &&
                        targeted.at("sessionGeneration") == 2 && targeted.at("target") == "step-1-0" && targeted.at("granularity") == "instruction" && targeted.at("singleThread") == true && !targeted.contains("targetId"),
                    "Targeted-step GUI replaced captured frame/handle/context or lost shared step options");
            require(restart.at("frameId") == 9 && !restart.contains("threadId") && !restart.contains("granularity") && !restart.contains("singleThread"),
                    "Frame restart leaked execution options");
            const auto captured = app.debugExecutionParameters("debug.stepBack",{{"stopGeneration",6},{"threadId",8},{"sessionGeneration",2},{"granularity","line"},{"singleThread",false}});
            require(captured.at("stopGeneration") == 6 && captured.at("sessionGeneration") == 2 && captured.at("threadId") == 8 &&
                        captured.at("granularity") == "line" && captured.at("singleThread") == false,
                    "GUI replaced captured execution context with newer selection/defaults");
            app.debugState_["state"] = "stopped"; app.debugState_["instructionBreakpointRevision"] = 4;
            app.debugState_["instructionBreakpoints"] = Json::array({{{"instructionReference","0x100"},{"offset",0},{"enabled",true},{"condition",""},{"hitCondition",""},{"verification",{{"verified",true},{"message",""}}}}});
            app.beginInstructionBreakpointDraft();
            require(app.debugInstructionContext_.at("sessionGeneration") == 3 && app.debugInstructionContext_.at("stopGeneration") == 7 &&
                        app.debugInstructionContext_.at("expectedRevision") == 4 && app.debugInstructionDraft_.size() == 1 && app.debugInstructionRowIds_.size() == 1 && !app.debugInstructionDraft_[0].contains("verification"),
                    "Instruction draft lost its full installed list, captured revision or explicit context");
            app.debugInstructionContext_ = Json::object(); app.debugInstructionDraft_ = Json::array(); app.debugInstructionRowIds_.clear();
            app.debugState_ = previous; app.debugStepGranularity_ = 0; app.debugSingleThread_ = false;
        }
        const Json retainedNavigation={{"path",tab->path},{"revision",tab->revision},{"documentId",tab->identity},
            {"backend","cpp"},{"epoch",17},{"locations",Json::array()}};
        app.navigation_=retainedNavigation;
        const auto before = tab->editor.GetText();
        tab->editor.SetCursor({0, 0}); tab->editor.ReplaceTextPreservingTabs("// edited\n"); app.changed(tab);
        require(app.engine_->dispatch("documents.read", {{"path", tab->path}}).at("text") == tab->editor.GetText(), "Editor transaction did not reach Engine");
        app.executeCommand("edit.undo");
        require(tab->editor.GetText() == before, "Native undo lost across CodeEditor migration");
        app.executeCommand("edit.redo"); require(tab->editor.GetText() != before, "Native redo lost");
        // Exercise the actual widget -> Engine path, including the native
        // transaction callback already owned by EditorSnippet.
        require(app.navigation_==retainedNavigation && tab->revision!=app.navigation_.at("revision"),
            "Source edit discarded navigation observations instead of invalidating their ownership");
        app.navigation_=Json::object();
        frame([&] { app.editorPanel(); });frame([&] { app.editorPanel(); });
        const auto revision=tab->revision;
        ImGui::GetIO().AddInputCharactersUTF8("x");frame([&] { app.editorPanel(); });
        require(tab->revision>revision && app.engine_->dispatch("documents.read",{{"path",tab->path}}).at("text")==tab->editor.GetText(),
                "Typing in shared CodeEditor did not synchronize the Engine");
        app.executeCommand("edit.undo");
        app.open("shader.hlsl"); require(app.tabs_.back()->codeEditor.language() == cyui::CodeLanguage::hlsl, "HLSL catalogue not used");
        app.open("events.log"); require(app.tabs_.back()->codeEditor.language() == cyui::CodeLanguage::log, "Log syntax not used");
        app.options_ = true;
        app.panels_["problems"] = true; app.panels_["information"] = true; app.panels_["refactoring"] = true;
        app.buildDiagnostics_ = Json::array({{{"path","sample.cpp"},{"line",1},{"column",2},{"severity","warning"},{"message","test diagnostic"}}});
        app.informationState_ = {{"available",true},{"contents",Json::array({{{"kind","markdown"},{"text","# API\n**Description** [link](https://example.invalid)"}},{{"kind","code"},{"language","hlsl"},{"text","float4 value;\n// ``` literal"}},{{"kind","plaintext"},{"text","<literal> [plain](text)"}}})}};
        app.refactorPlan_ = {{"id","fixture"},{"state","preview"},{"title","Fixture"},{"files",Json::array({{{"path","sample.cpp"}}})},{"editCount",1}};
        app.refactorFile_ = "sample.cpp"; app.refactorPreview_ = {{"before",before},{"after","int main() { return 2; }\n"}};
        app.refactorDiff_.setTexts(before, app.refactorPreview_.at("after"), cyui::CodeLanguage::cpp);
        for (auto *view : {&app.beforePreviewCode_, &app.afterPreviewCode_}) { configureCodeEditor(*view); view->setReadOnly(true); view->native().SetWordWrapEnabled(true); }
        app.beforePreviewCode_.setText(before); app.afterPreviewCode_.setText(app.refactorPreview_.at("after"));
        app.git_={{"repository",true},{"branch","fixture"},{"files",Json::array()}};
        app.vcsPreviewTitle_="vcs.diff";app.vcsIsDiff_=true;
        app.vcsDiff_.setUnified("--- a/shader.hlsl\n+++ b/shader.hlsl\n@@ -1 +1 @@\n-float oldValue;\n+float newValue;\n",cyui::CodeLanguage::hlsl);
        app.vcsConflict_={{"path","shader.hlsl"},{"revision",1},{"versions",{{"base","float base;\n"},{"ours","float ours;\n"},{"theirs","float theirs;\n"}}}};
        for (auto *view : {&app.vcsBaseCode_,&app.vcsOursCode_,&app.vcsTheirsCode_}) {
            configureCodeEditor(*view);view->setLanguage(cyui::CodeLanguage::hlsl);view->setReadOnly(true);view->setText("float version;\n");
        }
        // Draw each actual app view independently so docking cannot hide it.
        for (const auto *theme : {"graphite","mid-grey","light-grey","amber-night","violet","navy"})
            for (auto density : {cyui::Density::compact, cyui::Density::comfortable})
                for (float dpi : {1.f,1.5f,2.f}) {
                    cyui::Config config; config.themeKey=theme;config.density=density;config.dpiScale=dpi;cyui::apply(config); applyEditorTheme(theme); app.theme_=theme;
                    published["diagnosticRevisions"][tab->path]["revision"]=tab->revision;
                    app.languageState_={{"servers",{{"cpp",published}}}};app.updateDiagnostics();
                    require(tab->diagnosticCount==1&&tab->editor.HasSquiggles(),"Theme/DPI refresh lost versioned editor diagnostics");
                    frame([&]{app.editorPanel();});
                    for (const auto *locale : {"en","fr"}) {
                        app.locale_=locale;cyui::setLocale(locale);
                        if (app.locale_=="fr") require(std::string(app.tr("Language"))=="Langue", "App French catalogue lost");
                        frame([&] { app.optionsPanel(); });
                        frame([&] { app.problemsPanel(); });
                        frame([&] { app.informationPanel(); });
                        app.previewWrap_=false;frame([&] { app.refactorPanel(); });
                        app.previewWrap_=true;frame([&] { app.refactorPanel(); });
                        frame([&] { app.outputPanel(); });
                        app.vcsSection_="Review";frame([&] { app.gitPanel(); });frame([&] { app.gitPanel(); });
                        app.vcsSection_="Conflicts";frame([&] { app.gitPanel(); });frame([&] { app.gitPanel(); });
                    }
                }
        require(app.informationDocuments_.at("1").code.text()=="float4 value;\n// ``` literal", "Literal code documentation was changed by Markdown parsing");
        app.languageState_=Json::object();app.updateDiagnostics();
        // Actual Inspector gestures: draft -> Escape, then draft -> click-away commit.
        app.optionsFilter_.query="clangd";const auto originalClangd=app.clangd_;ImRect field;
        auto property=[&] { ImGui::Begin("Settings gesture");app.optionsProperties();field=ImGui::GetCurrentContext()->LastItemData.Rect;ImGui::End(); };
        frame(property);frame(property);
        auto &propertyIO=ImGui::GetIO();
        propertyIO.AddMousePosEvent(field.Max.x-30,field.Min.y+8);propertyIO.AddMouseButtonEvent(0,true);frame(property);
        propertyIO.AddMouseButtonEvent(0,false);frame(property);propertyIO.AddInputCharactersUTF8("draft");frame(property);
        require(app.clangd_==originalClangd,"Property draft changed app settings before commit");
        frame(property,ImGuiKey_Escape);frame(property);require(app.clangd_==originalClangd,"Escape committed app settings");
        propertyIO.AddMousePosEvent(field.Max.x-30,field.Min.y+8);propertyIO.AddMouseButtonEvent(0,true);frame(property);
        propertyIO.AddMouseButtonEvent(0,false);frame(property);propertyIO.AddInputCharactersUTF8("commit");frame(property);
        propertyIO.AddMousePosEvent(20,900);propertyIO.AddMouseButtonEvent(0,true);frame(property);
        propertyIO.AddMouseButtonEvent(0,false);frame(property);
        require(app.clangd_!=originalClangd,"Completed property gesture was not committed");app.clangd_=originalClangd;
        const auto id=uiId("options/clangd");
        app.optionsInspector_.drafts[id]={std::string("draft"),std::string("clangd"),true};
        app.optionsFilter_.query="Theme";
        frame([&] { app.optionsPanel(); });
        require(!app.optionsInspector_.drafts.at(id).active && app.clangd_!="draft", "Filtered property draft escaped into settings");
        // Context-action filtering retains native indices and never submits from a text field.
        app.open("sample.cpp");
        const auto actionParams = app.codeActionParameters();
        require(!actionParams.is_null(), "Code-action fixture has no source context");
        app.codeActionsKey_ = actionParams.dump();
        app.codeActionsState_ = {
            {"path", actionParams.at("path")}, {"revision", actionParams.at("expectedRevision")},
            {"sessionId", "fixture-must-not-submit"},
            {"actions", Json::array({
                {{"title", "Extract##variable"}, {"kind", "refactor.extract"}, {"reason", ""},
                 {"index", 17}, {"available", true}},
                {{"title", "Unavailable action"}, {"kind", "quickfix"}, {"reason", "unsupported fixture"},
                 {"index", 23}, {"available", false}}
            })}
        };
        app.panels_["codeActions"] = true;
        auto actionsView = [&] { app.codeActionsPanel(); };
        app.codeActionsTextFilter_.query = "unsupported";
        frame(actionsView); frame(actionsView);
        require(app.codeActionSelected_ == 1, "Reason filtering lost the original native action index");
        app.codeActionsTextFilter_.query = "no-matching-action";
        frame(actionsView);
        require(app.codeActionSelected_ == -1, "An invisible code action remained selected");
        app.codeActionsTextFilter_.query = "Extract";
        app.codeActionsTextFilter_.focus();
        frame(actionsView); frame(actionsView);
        require(app.codeActionSelected_ == 0 && ImGui::GetActiveID() != 0,
                "Code-action search field did not take focus");
        frame(actionsView, ImGuiKey_Enter); frame(actionsView);
        require(!app.refactorFuture_.valid(), "Enter in the code-action filter submitted a correction");
        app.panels_["codeActions"] = false;
        app.codeActionsTextFilter_.clear();
        app.codeActionsState_ = Json::object(); app.codeActionsKey_.clear(); app.codeActionSelected_ = -1;
        app.paletteMode_="actions";app.paletteOpen_=true;
        auto palette=[&] { ImGui::Begin("Palette fixture");app.palette();ImGui::End(); };
        frame(palette);frame(palette);
        ImGui::GetIO().AddInputCharactersUTF8("file.saveAll");frame(palette);
        frame(palette,ImGuiKey_Enter);
        require(app.lastShortcut_=="file.saveAll" && !app.actionPaletteOpen_, "Shared palette did not dispatch the app command");
        frame(palette);
        for (bool mac : {false,true}) {
            auto &io=ImGui::GetIO();io.ConfigMacOSXBehaviors=mac;
            app.panels_["project"]=true; app.filesFilter_.clear();app.editorFocused_=false;
            auto project=[&] { app.shortcuts();app.projectPanel(); };
            frame(project);frame(project);
            ImGui::SetWindowFocus(app.label("Project","project").c_str());
            io.AddKeyEvent(mac ? ImGuiMod_Super : ImGuiMod_Ctrl,true);
            frame(project,ImGuiKey_F);frame(project);
            io.AddKeyEvent(mac ? ImGuiMod_Super : ImGuiMod_Ctrl,false);frame(project);
            io.AddInputCharactersUTF8("sample");frame(project);
            if (app.filesFilter_.query!="sample") throw std::runtime_error(
                std::string("Ctrl/Cmd+F filter focus: mac=") + std::to_string(mac) + " query=" + app.filesFilter_.query +
                " command=" + app.lastShortcut_ + " input=" + std::to_string(ImGui::GetActiveID()) +
                " ctrl=" + std::to_string(io.KeyCtrl));
            frame(project);frame(project,ImGuiKey_Escape);frame(project);
            require(!app.filesFilter_.active(), "Escape did not clear the panel filter");
        }
        ImGui::GetIO().ConfigMacOSXBehaviors=false;
        // A shared renderer must never save workspace files implicitly.
        require(normalizeEditorText(readTextFileBytes(folder / "workspace/sample.cpp")) == tab->editor.GetText(), "Explicit Save all did not persist native buffers");
    }
};
}
void problemNavigationContracts();
int main(int argc, char **argv) {
    try {
        require(argc==2,"Expected repository path");
        ImGui::CreateContext();auto &io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1600,1000};io.DeltaTime=1.f/60;
        io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;io.BackendFlags|=ImGuiBackendFlags_RendererHasTextures;
        cyui::apply({});cyui::loadFonts();
        problemNavigationContracts();
        outlineModelContracts();
        ProblemModel model; model.add("first",cyui::LogLevel::error,"src/é##file.cpp","Error with emoji 😀",{"src/é##file.cpp",4,8,true});
        const auto id=model.rows.front().id;model.add("second",cyui::LogLevel::warning,"other","Warning",{});
        cyui::TextFilter filter; filter.query="e##file";
        const auto matching=model.matching(filter);
        require(model.locations.at(id).utf16 && matching.size()==1 && matching.front().id==id,"Stable filtered diagnostic navigation");
        OutputLog log;std::string output="Error: failed\nWarning: retry\nnormal\n";log.synchronize(output);
        std::vector<cyui::LogEntry> rows;log.buffer.snapshot(rows,0);
        require(rows.size()==3 && rows[0].level==cyui::LogLevel::error && rows[1].level==cyui::LogLevel::warning && rows[2].level==cyui::LogLevel::info,"Shared log severity mapping");
        const auto first=rows[0].id;output+="Debug: trace\n";log.synchronize(output);log.buffer.snapshot(rows,0);require(rows[0].id==first,"Appending output changed existing identities");
        ApplicationModuleTests::run(utf8Path(argv[1]));
        ImGui::DestroyContext();std::cout<<"CyCode module integration passed (no native windows)\n";return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n';return 1; }
}
