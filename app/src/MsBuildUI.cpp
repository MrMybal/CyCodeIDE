#include "cycode/Application.h"
#include "cycode/DebugRuntime.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
void Application::msbuildPanel() {
    if (!beginPanel("MSBuild","msbuild")) return;
    if (!msbuildActive_) { cyui::emptyState(tr("No MSBuild project"),tr("Open a .sln, .vcxproj or managed project."),cyui::icon::folder); ImGui::End(); return; }
    const bool busy = taskFuture_.valid();
    auto action = [&](const char *title,const char *method) { cyui::ScopedDisabled disabled(busy); if (cyui::button(tr(title))) task(method); };
    action("Build project","msbuild.build"); ImGui::SameLine(); action("Rebuild","msbuild.rebuild"); ImGui::SameLine(); action("Clean","msbuild.clean"); ImGui::SameLine(); action("Restore packages","msbuild.restore");
    ImGui::SameLine(); if (cyui::button(tr("Refresh"))) refreshBuild();
    if (busy || buildFuture_.valid()) { ImGui::SameLine(); cyui::spinner("##msbuild-pending"); }
    cyui::divider();
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::beginProperties("##msbuild-properties",140)) {
            cyui::property(tr("Solution / project")); cyui::textDim("%s",msbuildState_.value("profile",Json::object()).value("entry","").c_str());
            cyui::property(tr("Configuration / platform"));
            const auto combined = msbuildConfiguration_+"|"+msbuildPlatform_;
            if (!msbuildState_.value("configurations",Json::array()).empty()) {
                if (ImGui::BeginCombo("##msbuild-configuration",combined.c_str())) {
                    for (const auto &configuration : msbuildState_.at("configurations")) { const auto text = configuration.get<std::string>(); if (ImGui::Selectable(text.c_str(),text == combined)) { auto bar = text.find('|'); msbuildConfiguration_ = text.substr(0,bar); msbuildPlatform_ = text.substr(bar+1); solutionConfiguration_ = text; buildEdited_ = true; refreshBuild(); } }
                    ImGui::EndCombo();
                }
            } else { buildEdited_ |= ImGui::InputText("##msbuild-config",&msbuildConfiguration_); cyui::property(tr("Platform")); buildEdited_ |= ImGui::InputText("##msbuild-platform",&msbuildPlatform_); }
            cyui::property(tr("Target")); std::string selected = tr("Whole solution");
            for (const auto &item : msbuildState_.value("projects",Json::array())) if (item.at("id") == msbuildProject_) selected = item.at("name");
            if (ImGui::BeginCombo("##msbuild-project",selected.c_str())) {
                if (ImGui::Selectable(tr("Whole solution"),msbuildProject_.empty())) { msbuildProject_.clear(); buildEdited_ = true; refreshBuild(); }
                for (const auto &item : msbuildState_.value("projects",Json::array())) if (!item.at("folder").get<bool>() && !item.at("external").get<bool>() && item.at("exists").get<bool>()) {
                    if (ImGui::Selectable(item.at("name").get_ref<const std::string &>().c_str(),item.at("id") == msbuildProject_)) { msbuildProject_ = item.at("id"); buildEdited_ = true; refreshBuild(); }
                }
                ImGui::EndCombo();
            }
            cyui::property(tr("Build tool"));
            if (ImGui::BeginCombo("##msbuild-tool",msbuildTool_.c_str())) {
                if (ImGui::Selectable(tr("Automatic"),msbuildTool_ == "automatic")) { msbuildTool_ = "automatic"; buildEdited_ = true; }
                for (const auto &tool : msbuildState_.value("tools",Json::array())) if (ImGui::Selectable(tool.at("name").get_ref<const std::string &>().c_str(),tool.at("id") == msbuildTool_)) { msbuildTool_ = tool.at("id"); buildEdited_ = true; }
                ImGui::EndCombo();
            }
            cyui::property(tr("Packages")); ImGui::Checkbox(tr("Restore before build"),&msbuildRestore_); cyui::endProperties();
        }
    }
    cyui::divider(); action("Evaluate project","msbuild.evaluate"); ImGui::SameLine(); action("Prepare code analysis","msbuild.prepare");
    ImGui::TextWrapped("%s",tr("Evaluation loads project imports and rules. Code analysis uses evaluated C++ include paths, definitions and language modes."));
    const auto database = msbuildState_.value("compilationDatabase",Json::object());
    if (database.value("available",false)) cyui::badge(tr("Compilation database ready"),cyui::Tone::success);
    const auto message = msbuildState_.value("message",""); if (!message.empty()) ImGui::TextWrapped("%s",message.c_str());
    for (const auto &warning : msbuildState_.value("analysisWarnings",Json::array())) ImGui::TextWrapped("%s",warning.get_ref<const std::string &>().c_str());
    if (ImGui::BeginTable("##msbuild-project-model",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn(tr("Project")); ImGui::TableSetupColumn(tr("Mapped configuration")); ImGui::TableSetupColumn(tr("Included in solution build")); ImGui::TableHeadersRow();
        for (const auto &item : msbuildState_.value("projects",Json::array())) if (!item.at("folder").get<bool>()) {
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(item.at("name").get_ref<const std::string &>().c_str());
            ImGui::TableNextColumn(); const auto config = item.at("activeConfiguration"); ImGui::Text("%s | %s",config.value("configuration","").c_str(),config.value("platform","").c_str());
            ImGui::TableNextColumn(); cyui::textDim("%s",tr(config.value("build",false) ? "Yes" : "No"));
        }
        ImGui::EndTable();
    }
    for (const auto &artifact : msbuildState_.value("artifacts",Json::array())) {
        const auto path = artifact.value("path",""); if (path.empty()) continue;
        ImGui::TextWrapped("%s: %s",artifact.value("name","").c_str(),path.c_str());
        if (isRunnableArtifact(utf8Path(path))) {
            ImGui::PushID(path.c_str()); if (cyui::button(tr("Use in debug profile"))) { debugProgram_ = path; showPanel("Debugger","debugger"); } ImGui::PopID();
        }
    }
    ImGui::End();
}
} // namespace cycode
