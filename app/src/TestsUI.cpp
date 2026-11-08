#include "cycode/Application.h"
#include <cyui/Core.h>
#include <imgui_stdlib.h>
namespace cycode {
void Application::testTask(const std::string &method, Json params) {
    if (testsFuture_.valid())
        return;
    testsError_.clear();
    testsOperation_ = method;
    if (method == "tests.discover") {
        testsCatalog_ = testsResults_ = Json::object();
        testsSelection_.clear();
    }
    const auto engine = engine_;
    testsFuture_ =
        std::async(std::launch::async, [engine, method, params] { return engine->dispatch(method, params); });
}
void Application::testsPanel() {
    if (testsFuture_.valid() && testsFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto result = testsFuture_.get();
            if (testsOperation_ == "tests.discover") {
                testsCatalog_ = std::move(result);
                testsResults_ = Json::object();
                testsSelection_.clear();
            } else
                testsResults_ = std::move(result);
        } catch (const std::exception &e) {
            testsError_ = e.what();
        }
    }
    if (!beginPanel("Tests", "tests"))
        return;
    const bool busy = testsFuture_.valid();
    {
        cyui::ScopedDisabled disabled(busy || taskFuture_.valid() || buildFuture_.valid());
        if (cyui::button(tr("Discover CTest tests")))
            testTask("tests.discover",
                     {{"buildDirectory", buildDirectory_}, {"configuration", configuration_}});
    }
    ImGui::SameLine();
    {
        cyui::ScopedDisabled disabled(busy || testsSelection_.empty() || !testsCatalog_.contains("id"));
        if (cyui::primaryButton(tr("Run selected tests"))) {
            Json selected = Json::array();
            for (auto i : testsSelection_)
                selected.push_back(i);
            testTask("tests.run", {{"id", testsCatalog_.at("id")}, {"tests", selected}});
        }
    }
    if (busy) {
        ImGui::SameLine();
        if (cyui::button(tr("Cancel")))
            engine_->dispatch("tests.cancel");
        ImGui::SameLine();
        cyui::spinner("##tests-busy");
    }
    if (!testsError_.empty())
        cyui::callout(cyui::Tone::error, testsError_.c_str());
    if (testsResults_.value("cancelled", false))
        cyui::callout(cyui::Tone::warning, tr("Test run cancelled"));
    ImGui::SetNextItemWidth(cyui::dp(320));
    ImGui::InputTextWithHint("##tests-filter", tr("Filter tests"), &testsFilter_);
    {
        cyui::ScopedDisabled disabled(busy);
        if (cyui::button(tr("Select all"))) {
            for (const auto &t : testsCatalog_.value("tests", Json::array()))
                if (!t.value("disabled", false) &&
                    t.at("name").get<std::string>().find(testsFilter_) != std::string::npos &&
                    testsSelection_.size() < 128)
                    testsSelection_.insert(t.at("index").get<size_t>());
        }
        ImGui::SameLine();
        if (cyui::button(tr("Clear selection")))
            testsSelection_.clear();
    }
    if (testsCatalog_.contains("id"))
        cyui::textDim("%s / %s", testsCatalog_.value("buildDirectory", "").c_str(),
                      testsCatalog_.value("configuration", "").c_str());
    if (ImGui::BeginTable("##tests-table", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                          {0, cyui::dp(230)})) {
        ImGui::TableSetupColumn(tr("Test"));
        ImGui::TableSetupColumn(tr("Status"));
        ImGui::TableSetupColumn(tr("Labels"));
        ImGui::TableHeadersRow();
        for (const auto &t : testsCatalog_.value("tests", Json::array())) {
            const auto name = t.at("name").get<std::string>();
            if (name.find(testsFilter_) == std::string::npos)
                continue;
            const auto i = t.at("index").get<size_t>();
            ImGui::PushID(int(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool selected = testsSelection_.contains(i);
            {
                cyui::ScopedDisabled disabled(busy || (!selected && testsSelection_.size() >= 128));
                if (ImGui::Checkbox(name.c_str(), &selected)) {
                    if (selected)
                        testsSelection_.insert(i);
                    else
                        testsSelection_.erase(i);
                }
            }
            std::string status = t.value("disabled", false) ? "Disabled" : "Not run";
            for (const auto &r : testsResults_.value("results", Json::array()))
                if (r.at("index") == i) {
                    const auto s = r.value("status", "");
                    status = s == "passed"         ? "Passed"
                             : s == "failed"       ? "Failed"
                             : s == "timeout"      ? "Timed out"
                             : s == "cancelled"    ? "Cancelled"
                             : s == "output-limit" ? "Output limit"
                             : s == "not-run"      ? "Not run"
                                                   : "Disabled";
                    break;
                }
            ImGui::TableNextColumn();
            cyui::badge(tr(status.c_str()),
                        status == "Passed" ? cyui::Tone::success
                        : status == "Failed" || status == "Timed out" || status == "Output limit"
                            ? cyui::Tone::error
                            : cyui::Tone::neutral);
            ImGui::TableNextColumn();
            std::string labels;
            for (const auto &l : t.at("labels")) {
                if (!labels.empty())
                    labels += ", ";
                labels += l.get<std::string>();
            }
            ImGui::TextUnformatted(labels.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    for (const auto &r : testsResults_.value("results", Json::array()))
        if (ImGui::CollapsingHeader(r.at("name").get_ref<const std::string &>().c_str())) {
            cyui::textDim("%lld ms", r.at("milliseconds").get<long long>());
            ImGui::TextUnformatted(r.at("output").get_ref<const std::string &>().c_str());
        }
    ImGui::End();
}
} // namespace cycode
