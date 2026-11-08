#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
void Application::gitSubmoduleAddControls(bool busy) {
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::button(cyui::trLabel("Add submodule").c_str())) {
            vcsSubmoduleAddOpen_ = true;
            vcsSubmoduleAddPlan_ = Json::object();
            vcsSubmoduleReview_ = Json::object();
        }
    }
    if (!vcsSubmoduleAddOpen_)
        return;
    cyui::sectionTitle(tr("New Git submodule"));
    bool changed = false;
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::beginProperties("##add-submodule-fields")) {
            cyui::property(tr("Repository address"));
            changed |= ImGui::InputText("##add-module-url", &vcsSubmoduleAddUrl_);
            cyui::property(tr("New repository-relative folder"));
            changed |= ImGui::InputText("##add-module-path", &vcsSubmoduleAddPath_);
            cyui::property(tr("Module name (optional)"));
            changed |= ImGui::InputText("##add-module-name", &vcsSubmoduleAddName_);
            cyui::property(tr("Remote branch (optional)"));
            changed |= ImGui::InputText("##add-module-branch", &vcsSubmoduleAddBranch_);
            cyui::endProperties();
        }
    }
    if (changed)
        vcsSubmoduleAddPlan_ = Json::object();
    cyui::textDim("%s",
                  tr("Use a new folder with an existing parent. Close .gitmodules and destination documents; "
                     "keep .gitmodules unchanged. Use Git credentials or SSH authentication."));
    auto parameters = [&] {
        return Json{{"url", vcsSubmoduleAddUrl_},
                    {"path", vcsSubmoduleAddPath_},
                    {"name", vcsSubmoduleAddName_},
                    {"branch", vcsSubmoduleAddBranch_},
                    {"repository", vcsSubmodules_.at("repository")},
                    {"expectedRepository", vcsSubmodules_.at("identity")},
                    {"expectedRevision", vcsSubmodules_.at("revision")}};
    };
    {
        cyui::ScopedDisabled disabled(busy || vcsSubmoduleAddUrl_.empty() || vcsSubmoduleAddPath_.empty());
        if (cyui::button(cyui::trLabel("Preview submodule addition").c_str())) {
            vcsSubmoduleAddPlan_ = Json::object();
            vcsTask("vcs.submodule.addPlan", parameters());
        }
    }
    if (vcsSubmoduleAddPlan_.contains("revision")) {
        const auto &plan = vcsSubmoduleAddPlan_;
        const auto &request = plan.at("request");
        cyui::sectionTitle(tr("Submodule addition preview"));
        ImGui::TextUnformatted(request.at("url").get_ref<const std::string &>().c_str());
        ImGui::TextUnformatted(plan.at("root").get_ref<const std::string &>().c_str());
        cyui::textDim("%s: %s", tr("Module name"), request.at("name").get_ref<const std::string &>().c_str());
        cyui::textDim("%s: %s", tr("Remote branch"),
                      request.at("branch").get_ref<const std::string &>().empty()
                          ? tr("Remote default branch")
                          : request.at("branch").get_ref<const std::string &>().c_str());
        cyui::textDim(
            "%s",
            tr("The remote commit is resolved during cloning. This creates a checkout and stages .gitmodules "
               "plus its gitlink. Existing staged files stay staged; no commit is created."));
        cyui::textDim("%s", tr("Cloning may contact the remote and execute configured Git helpers or hooks. "
                               "Failed additions keep partial files; inspect them before trying again."));
        const bool stale = plan.at("repository") != vcsSubmodules_.at("repository") ||
                           plan.at("identity") != vcsSubmodules_.at("identity") ||
                           plan.at("catalogRevision") != vcsSubmodules_.at("revision");
        {
            cyui::ScopedDisabled disabled(busy || stale);
            if (cyui::primaryButton(cyui::trLabel("Clone and stage submodule").c_str())) {
                auto input = parameters();
                input["expectedPlanRevision"] = plan.at("revision");
                vcsTask("vcs.submodule.add", input);
                vcsSubmoduleAddOpen_ = false;
                vcsSubmoduleAddPlan_ = Json::object();
            }
        }
        if (stale)
            cyui::textColoured(cyui::palette().warning, "%s",
                               tr("Submodule state changed. Review the action again."));
    }
    if (cyui::button(cyui::trLabel("Cancel submodule addition").c_str()) ||
        ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        vcsSubmoduleAddOpen_ = false;
        vcsSubmoduleAddPlan_ = Json::object();
    }
    cyui::divider();
}
} // namespace cycode
