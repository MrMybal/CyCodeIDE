#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
#include <sstream>
#include <algorithm>
namespace cycode {
void Application::projectTasksPanel() {
    if (!beginPanel("Project tasks","projectTasks")) return;
    if (!projectActive_&&projectProfile_.empty()) {
        cyui::emptyState(tr("No Cargo, npm, Java, Maven or Gradle project"),tr("Open a project manifest or its folder."),cyui::icon::folder);
        {cyui::ScopedDisabled disabled(taskFuture_.valid());if(cyui::button(tr("Use shader tasks in this project"))){projectProfile_={{"system","shader"},{"args",Json::array()}};task("project.configure");}}
        ImGui::End(); return;
    }
    if (projectProfile_.empty()) { cyui::spinner("##project-pending"); ImGui::End(); return; }
    const auto kind=projectProfile_.value("system",""); const bool busy=taskFuture_.valid()||engine_->dispatch("project.status").value("running",false);
    auto action=[&](const char *text,const char *method,bool enabled=true) { cyui::ScopedDisabled disabled(busy || !enabled); if (cyui::button(tr(text))) task(method); };
    action("Configure","project.configure"); ImGui::SameLine(); action("Build project","project.build");
    ImGui::SameLine(); action("Run project tests","project.test",kind!="java"&&kind!="shader");
    const auto cargoKind=projectProfile_.value("cargoTargetKind",std::string("bin"));
    ImGui::SameLine(); action("Run without debugging","project.launch",kind!="shader"&&(kind!="cargo"||cargoKind=="bin"||cargoKind=="example"));
    ImGui::SameLine(); { cyui::ScopedDisabled disabled(kind=="shader"||!commandEnabled("debug.start")); if (cyui::button(tr("Debug / Continue"))) executeCommand("debug.start"); }
    if ((taskFuture_.valid()&&taskMethod_.starts_with("project."))||engine_->dispatch("project.status").value("running",false)) { ImGui::SameLine(); if (cyui::button(tr("Cancel task"))) cancelProjectTask(); }
    cyui::divider();
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::beginProperties("##project-properties",155)) {
            auto input=[&](const char *title,const char *key) { cyui::property(tr(title)); auto text=projectProfile_.value(key,""); if (ImGui::InputText((std::string("##project-")+key).c_str(),&text)) { projectProfile_[key]=text; buildEdited_=true; } };
            cyui::property(tr("Project system"));
            if (ImGui::BeginCombo("##project-system",kind.c_str())) {
                for (const auto *system : {"cargo","npm","java","maven","gradle","shader"}) if (ImGui::Selectable(system,kind==system)) { projectProfile_["system"]=system;projectProfile_["package"]="";projectProfile_["target"]="";projectProfile_["script"]="";projectProfile_["args"]=Json::array(); buildEdited_=true; }
                ImGui::EndCombo();
            }
            input("Project directory","directory");
            cyui::property(tr("Configuration")); const auto configuration=projectProfile_.value("configuration","Debug");
            if (ImGui::BeginCombo("##project-configuration",configuration.c_str())) {
                for (const auto *item : {"Debug","Release"}) if (ImGui::Selectable(item,configuration==item)) { projectProfile_["configuration"]=item; buildEdited_=true; }
                ImGui::EndCombo();
            }
            if(kind=="shader") {
                input("Shader source (workspace path)","shaderSource");
                cyui::property(tr("Detected shader sources"));
                if(ImGui::BeginCombo("##shader-sources",tr("Select a shader source"))) {
                    for(const auto &file:projectState_.value("sources",Json::array()))if(ImGui::Selectable(file.get_ref<const std::string &>().c_str())){projectProfile_["shaderSource"]=file;buildEdited_=true;}
                    ImGui::EndCombo();
                }
                auto choices=[&](const char *title,const char *key,std::initializer_list<const char *> values) {
                    cyui::property(tr(title));const auto selected=projectProfile_.value(key,std::string("automatic"));
                    if(ImGui::BeginCombo((std::string("##shader-")+key).c_str(),selected.c_str())) {for(const auto *item:values)if(ImGui::Selectable(item,selected==item)){projectProfile_[key]=item;buildEdited_=true;}ImGui::EndCombo();}
                };
                choices("Shader language","shaderLanguage",{"automatic","hlsl","glsl"});
                choices("Shader stage","shaderStage",{"automatic","vert","frag","geom","tesc","tese","comp","mesh","task","rgen","rint","rahit","rchit","rmiss","rcall"});
                choices("Shader target","shaderTarget",{"automatic","dxil","vulkan1.0","vulkan1.1","vulkan1.2","vulkan1.3","opengl"});
                input("Shader entry point","shaderEntry");input("HLSL shader model","shaderModel");
                auto list=[&](const char *title,const char *key){cyui::property(tr(title));std::string text;for(const auto &item:projectProfile_.value(key,Json::array())){if(!text.empty())text+='\n';text+=item.get<std::string>();}if(ImGui::InputTextMultiline((std::string("##shader-list-")+key).c_str(),&text,{-1,cyui::dp(65)})){Json items=Json::array();std::istringstream lines(text);std::string line;while(std::getline(lines,line))if(!line.empty())items.push_back(line);projectProfile_[key]=items;buildEdited_=true;}};
                list("Shader macros (one NAME=value per line)","shaderDefines");list("Shader include directories (workspace paths)","shaderIncludes");
            } else if (kind=="cargo") {
                input("Rustup toolchain (empty is automatic)","cargoToolchain");
                cyui::helpMarker(tr("Uses an installed Rustup toolchain for Cargo. Apply the Cargo options below to use it for Rust analysis too. No installation is requested."));
                if(!projectProfile_.value("cargoToolchain",std::string{}).empty()&&projectState_.value("tools",Json::object()).value("rustup",std::string{}).empty())
                    cyui::badge(tr("Rustup was not found. Configure PATH or CYCODE_RUSTUP."),cyui::Tone::warning);
                auto flag=[&](const char *title,const char *key){cyui::property(tr(title));auto enabled=projectProfile_.value(key,false);if(ImGui::Checkbox((std::string("##project-")+key).c_str(),&enabled)){projectProfile_[key]=enabled;buildEdited_=true;return true;}return false;};
                if(flag("Entire Cargo workspace","cargoWorkspace")&&projectProfile_.value("cargoWorkspace",false))projectProfile_["package"]="";
                {
                cyui::ScopedDisabled allPackages(projectProfile_.value("cargoWorkspace",false));
                input("Cargo package","package");
                cyui::property(tr("Workspace packages"));
                if(ImGui::BeginCombo("##cargo-packages",tr("Select a package"))) {
                    if(ImGui::Selectable(tr("Automatic"))){projectProfile_["package"]="";projectProfile_["target"]="";buildEdited_=true;}
                    for(const auto &package:projectState_.value("packages",Json::array())) {
                        const auto name=package.at("name").get<std::string>();cyui::ScopedID id(package.at("id").get_ref<const std::string &>().c_str());
                        if(ImGui::Selectable(name.c_str())){projectProfile_["package"]=name;projectProfile_["target"]="";buildEdited_=true;}
                        cyui::tooltip(package.at("manifest").get_ref<const std::string &>().c_str());
                    }
                    ImGui::EndCombo();
                }
                }
                cyui::property(tr("Cargo target kind"));const auto selectedKind=projectProfile_.value("cargoTargetKind",std::string("bin"));
                if(ImGui::BeginCombo("##cargo-kind",selectedKind.c_str())) {
                    for(const auto *item:{"bin","example","lib","test","bench"})if(ImGui::Selectable(item,selectedKind==item)){projectProfile_["cargoTargetKind"]=item;projectProfile_["target"]="";buildEdited_=true;}
                    ImGui::EndCombo();
                }
                if(selectedKind!="lib")input("Cargo target name","target");
                if (!projectState_.value("targets",Json::array()).empty()) {
                    cyui::property(tr("Detected targets"));
                    if (ImGui::BeginCombo("##cargo-targets",tr("Select a Cargo target"))) {
                        for (const auto &target : projectState_.at("targets")) {
                            const auto &kinds=target.at("kind");std::string type;
                            for(const auto *candidate:{"bin","example","test","bench"})if(std::find(kinds.begin(),kinds.end(),candidate)!=kinds.end())type=candidate;
                            if(type.empty())type="lib";
                            const auto name=target.at("name").get<std::string>(),package=target.at("package").get<std::string>();
                            cyui::ScopedID id((target.value("packageId",package)+"/"+type+"/"+name).c_str());
                            if (ImGui::Selectable((package+" / "+name+" ["+type+"]").c_str())) { projectProfile_["package"]=package;projectProfile_["cargoWorkspace"]=false;projectProfile_["cargoTargetKind"]=type; projectProfile_["target"]=name; buildEdited_=true; }
                            const auto required=target.value("requiredFeatures",Json::array());
                            if(!required.empty()){std::string names=tr("Required Cargo features");for(const auto &feature:required)names+="\n"+feature.get<std::string>();cyui::tooltip(names.c_str());}
                        }
                        ImGui::EndCombo();
                    }
                }
                input("Cargo profile (empty uses Configuration)","cargoProfile");
                cyui::property(tr("Cargo features (one per line)"));std::string featureText;
                for(const auto &feature:projectProfile_.value("cargoFeatures",Json::array())){if(!featureText.empty())featureText+='\n';featureText+=feature.get<std::string>();}
                if(ImGui::InputTextMultiline("##cargo-features",&featureText,{-1,cyui::dp(65)})){Json items=Json::array();std::istringstream lines(featureText);std::string line;while(std::getline(lines,line))if(!line.empty())items.push_back(line);projectProfile_["cargoFeatures"]=items;buildEdited_=true;}
                cyui::property(tr("Declared Cargo features"));
                if(ImGui::BeginCombo("##cargo-declared-features",tr("Add a workspace feature"))) {
                    for(const auto &package:projectState_.value("packages",Json::array()))for(const auto &feature:package.at("features")) {
                        const auto name=package.at("name").get<std::string>()+"/"+feature.get<std::string>();
                        const auto selected=projectProfile_.value("cargoFeatures",Json::array());
                        if(ImGui::Selectable(name.c_str(),std::find(selected.begin(),selected.end(),name)!=selected.end())){auto &items=projectProfile_["cargoFeatures"];if(!items.is_array())items=Json::array();if(items.size()<128&&std::find(items.begin(),items.end(),name)==items.end()){items.push_back(name);buildEdited_=true;}}
                    }
                    ImGui::EndCombo();
                }
                flag("Enable all Cargo features","cargoAllFeatures");flag("Disable default Cargo features","cargoNoDefaultFeatures");
                flag("Offline Cargo mode","offline");flag("Require unchanged Cargo.lock","cargoLocked");
                cyui::property(tr("Rust analysis"));
                {
                    cyui::ScopedDisabled disabled(smoke_);
                    if(cyui::button(tr("Apply Cargo options and restart Rust"))) {
                        try {
                            const auto inspected=engine_->dispatch("project.inspect",projectProfile_);
                            Json params={{"language","rust"},{"cargoProject",inspected.at("rustAnalysis")}};
                            if(cargoAnalysisRemember_) {
                                const auto preferences=engine_->dispatch("language.preferences");
                                if(preferences.at("available").get<bool>()) {
                                    if(preferences.at("error")!="")throw std::runtime_error(preferences.at("error").get<std::string>());
                                    params["save"]=true;params["expectedPreferencesRevision"]=preferences.at("revision");
                                }
                            }
                            languageState_=engine_->dispatch("language.configure",params);languageDecisions_.insert("rust");
                            cyui::notify(tr("Cargo options applied to Rust analysis"),cyui::Tone::success);
                        } catch(const std::exception &e){error(e.what());}
                    }
                }
                cyui::property(tr("Rust analysis preferences"));ImGui::Checkbox(tr("Remember for this project"),&cargoAnalysisRemember_);
                cyui::helpMarker(tr("Applies manifest, features, default-feature and offline/locked settings. Package, target and custom compilation profiles remain build controls."));
            } else if (kind=="npm") {
                cyui::property(tr("npm script")); const auto selected=projectProfile_.value("script","");
                if (ImGui::BeginCombo("##npm-script",selected.empty() ? tr("Automatic") : selected.c_str())) {
                    if (ImGui::Selectable(tr("Automatic"),selected.empty())) { projectProfile_["script"]=""; buildEdited_=true; }
                    for (const auto &script : projectState_.value("scripts",Json::array())) { const auto name=script.at("name").get<std::string>(); if (ImGui::Selectable(name.c_str(),name==selected)) { projectProfile_["script"]=name; buildEdited_=true; } }
                    ImGui::EndCombo();
                }
            } else if (kind=="java"||kind=="maven"||kind=="gradle") {
                if(kind=="java")input("Java source directory","sourceDirectory");
                if(kind=="gradle")input("Gradle module (empty selects all)","package");
                if(kind=="maven"||kind=="gradle") {
                    input("Build task","script");
                    cyui::property(tr("Offline dependency resolution"));auto offline=projectProfile_.value("offline",false);if(ImGui::Checkbox("##jvm-offline",&offline)){projectProfile_["offline"]=offline;buildEdited_=true;}
                }
                input("Java main class","mainClass");
                if (!projectState_.value("artifacts",Json::array()).empty()) {
                    cyui::property(tr("Detected targets"));
                    if (ImGui::BeginCombo("##java-mains",tr("Select a main class"))) {
                        for (const auto &artifact : projectState_.at("artifacts")) { const auto name=artifact.value("mainClass",""); if (!name.empty() && ImGui::Selectable(name.c_str())) { projectProfile_["mainClass"]=name; buildEdited_=true; } }
                        ImGui::EndCombo();
                    }
                }
            }
            cyui::property(tr("Task timeout (seconds)"));int timeout=projectProfile_.value("timeoutSeconds",180);if(ImGui::InputInt("##project-timeout",&timeout)&&timeout>=1&&timeout<=86400){projectProfile_["timeoutSeconds"]=timeout;buildEdited_=true;}
            if(kind!="shader") {cyui::property(tr("Arguments (one per line)")); std::string args;
            for (const auto &argument : projectProfile_.value("args",Json::array())) { if (!args.empty()) args+='\n'; args+=argument.get<std::string>(); }
            if (ImGui::InputTextMultiline("##project-args",&args,{-1,cyui::dp(70)})) {
                auto values=Json::array(); std::istringstream lines(args); std::string line; while (std::getline(lines,line)) values.push_back(line);
                projectProfile_["args"]=values; buildEdited_=true;
            }
            }
            cyui::endProperties();
        }
    }
    if (kind=="npm") {
        action("Run npm script","project.script"); ImGui::SameLine(); action("Install npm dependencies","project.install");
        for (const auto &script : projectState_.value("scripts",Json::array())) { cyui::ScopedFont mono(cyui::monoFont(),.9f); ImGui::TextWrapped("%s: %s",script.at("name").get_ref<const std::string &>().c_str(),script.at("command").get_ref<const std::string &>().c_str()); }
    }
    if (kind=="maven"||kind=="gradle") {
        action("Run build task","project.script",!projectProfile_.value("script",std::string{}).empty());
        ImGui::TextWrapped("%s",tr("Maven/Gradle use project-defined builds and runtime classpaths. Java debugging requires JDT LS with the Microsoft java-debug add-on."));
    }
    if (kind=="java") ImGui::TextWrapped("%s",tr("Standalone Java: compilation and Run. Select Maven or Gradle for build-tool projects."));
    if(kind=="cargo")ImGui::TextWrapped("%s",tr("Configure refreshes Cargo packages, targets and declared features. Run/debug require a binary or executable example; custom profiles override Configuration."));
    const auto message=projectState_.value("message",""); if (!message.empty()) ImGui::TextWrapped("%s",message.c_str());
    if (projectState_.value("stale",false)) cyui::badge(tr("Project metadata changed; configure or build again."),cyui::Tone::warning);
    for (const auto &artifact : projectState_.value("artifacts",Json::array())) cyui::textDim("%s: %s",artifact.value("name","").c_str(),artifact.value("path","").c_str());
    ImGui::End();
}
} // namespace cycode
