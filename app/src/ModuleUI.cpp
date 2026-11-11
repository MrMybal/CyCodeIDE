#include "cycode/Application.h"
#include "cycode/EditorTheme.h"
#include "cycode/BuildLimits.h"
#include <climits>
#include <cmath>
#include <imgui_internal.h>

namespace cycode {
void Application::viewFilter(const char *id, cyui::TextFilter &filter) {
    viewFilters_[ImGui::GetCurrentWindow()->RootWindow->ID] = &filter;
    cyui::filterBox(id, filter);
}
void Application::optionsProperties() {
    viewFilter("##options-filter", optionsFilter_);
    std::vector<cyui::Property> properties;
    auto add = [&](const char *key, cyui::PropertyValue value) -> cyui::Property & {
        cyui::Property property;
        property.id = uiId(std::string("options/") + key);
        property.label = key;
        property.value = std::move(value);
        properties.push_back(std::move(property));
        return properties.back();
    };
    add("Language", locale_).choices = {"en", "fr"};
    auto &theme = add("Theme", theme_);
    theme.choices = {"graphite"};
    for (const auto &entry : cyui::builtInThemes())
        theme.choices.push_back(entry.key);
    auto &scale = add("Text size", double(textScale_));
    scale.bounded = true;
    scale.minimum = .8;
    scale.maximum = 2.;
    add("clangd", clangd_);
    add("Restore last workspace session", restoreWorkspaceSession_);
    auto &buildLimit = add("Build time limit (seconds)", int64_t(configuredBuildTimeout()));
    buildLimit.bounded = true;
    buildLimit.integerMinimum = 1;
    buildLimit.integerMaximum = 86400;
    buildLimit.help = "Applies to CMake, MSBuild and Unreal build processes. Cancel remains available.";
    const auto validate = [&](cyui::ItemId id, const cyui::PropertyValue &value) -> std::string {
        if (id == uiId("options/Text size")) {
            const auto size = std::get<double>(value);
            if (!std::isfinite(size) || size < .8 || size > 2.)
                return tr("Text size must be between 0.8 and 2.0");
        }
        if (id == uiId("options/clangd") && std::get<std::string>(value).empty())
            return tr("Select a clangd executable");
        if (id == uiId("options/Build time limit (seconds)")) {
            try {
                buildTimeout(Json{{"timeoutSeconds", std::get<int64_t>(value)}});
            } catch (const std::exception &e) {
                return tr(e.what());
            }
        }
        return {};
    };
    // Drafts and previews never change settings; only a committed proposal does.
    // Hidden fields still participate so CyUI can emit cancellation.
    const auto edits = cyui::inspector("##options-properties", optionsInspector_, properties,
                                       optionsRevision_, validate, {&optionsFilter_, "Options"});
    for (const auto &proposal : edits)
        if (proposal.edit.committed()) {
            const auto &value = proposal.edit.value;
            if (proposal.id == uiId("options/Language")) {
                locale_ = std::get<std::string>(value);
                cyui::setLocale(locale_.c_str());
            } else if (proposal.id == uiId("options/Theme")) {
                theme_ = std::get<std::string>(value);
                applyEditorTheme(theme_);
            } else if (proposal.id == uiId("options/Text size")) {
                textScale_ = float(std::get<double>(value));
                ImGui::GetStyle().FontScaleMain = textScale_;
            } else if (proposal.id == uiId("options/clangd"))
                clangd_ = std::get<std::string>(value);
            else if (proposal.id == uiId("options/Restore last workspace session"))
                restoreWorkspaceSession_ = std::get<bool>(value);
            else if (proposal.id == uiId("options/Build time limit (seconds)"))
                settings_["buildTimeoutSeconds"] = std::get<int64_t>(value);
            ++optionsRevision_;
        }
}
void Application::problemRows() {
    synchronizeProblems();
    viewFilter("##problems-filter", problemsFilter_);
    for (const auto &[label, mask] : {std::pair{"Errors", 8u}, {"Warnings", 4u}, {"Information", 3u}}) {
        bool visible = (problemLevels_ & mask) != 0;
        if (ImGui::Checkbox(tr(label), &visible)) {
            if (visible)
                problemLevels_ |= mask;
            else
                problemLevels_ &= ~mask;
        }
        ImGui::SameLine();
    }
    if (problemsFilter_.active() || problemLevels_ != 15) {
        if (cyui::ghostButton(tr("Reset diagnostic filters"))) {
            problemsFilter_.clear();
            problemLevels_ = 15;
        }
    } else
        ImGui::NewLine();
    const auto &rows = problems_.matching(problemsFilter_, problemLevels_);
    const bool navigable = problems_.adjacent(problemsFilter_, selectedProblem_, false, {}, 0,
                                              problemLevels_) != 0;
    {
        cyui::ScopedDisabled disabled(!navigable);
        auto tip = [&](const char *label, const char *command) {
            const auto hint = keymap_.hint(command);
            return std::string(tr(label)) + (hint.empty() ? "" : " (" + hint + ")");
        };
        const auto previousTip = tip("Previous diagnostic", "problem.previous");
        if (cyui::iconButton(cyui::icon::chevronUp, previousTip.c_str()))
            navigateProblem(true);
        ImGui::SameLine();
        const auto nextTip = tip("Next diagnostic", "problem.next");
        if (cyui::iconButton(cyui::icon::chevronDown, nextTip.c_str()))
            navigateProblem(false);
    }
    ImGui::SameLine();
    cyui::textDim("%zu / %zu", rows.size(), problems_.rows.size());
    if (problems_.rows.empty()) {
        cyui::emptyState(tr("No diagnostics"), tr("Diagnostics from enabled language servers appear here."),
                         cyui::icon::check);
        return;
    }
    if (rows.empty()) {
        cyui::emptyState(tr("No matching diagnostics"),
                         tr("Change the text or severity filters, or reset them to show all diagnostics."),
                         cyui::icon::filter);
        return;
    }
    // Keep the selection for later, but do not display a detail hidden by the filters.
    if (selectedProblem_)
        for (const auto &row : rows)
            if (row.id == selectedProblem_) {
                const auto &location = problems_.locations.at(row.id);
                if (cyui::beginCard("##problem-detail", tr("Selected diagnostic"), 150.0f)) {
                    cyui::textDim("%s | %s:%d:%d", tr(location.provenance.c_str()), row.source.c_str(),
                                  row.line, row.column);
                    if (cyui::ghostButton(tr("Copy diagnostic"))) {
                        const auto text = row.source + ":" + std::to_string(row.line) + ":" +
                                          std::to_string(row.column) + "\n" + row.message;
                        ImGui::SetClipboardText(text.c_str());
                    }
                    if (!location.navigable)
                        cyui::textDim("%s", tr("Awaiting diagnostics for the current document."));
                    ImGui::TextWrapped("%s", row.message.c_str());
                }
                cyui::endCard();
                break;
            }
    const auto requested = cyui::diagnosticList("##diagnostics", rows);
    if (requested && problems_.locations.contains(requested)) {
        selectedProblem_ = requested;
        const auto location = problems_.locations.at(requested);
        if (location.navigable && !location.path.empty())
            open(location.path, location.line, location.column, location.utf16);
    }
}
void Application::synchronizeProblems() {
    // The cache includes eligibility and live document identity/revision: a text
    // edit invalidates navigation even before the worker publishes its next state.
    const auto empty = Json::object();
    const auto foundServers = languageState_.find("servers");
    const auto &servers =
        foundServers != languageState_.end() && foundServers->is_object() ? *foundServers : empty;
    Json documents = Json::object();
    const auto liveDocuments = engine_->dispatch("documents.list").at("documents");
    for (const auto &entry : liveDocuments)
        documents[entry.at("path").get<std::string>()] = {{"id", entry.value("id", uint64_t(0))},
                                                          {"revision", entry.at("revision")}};
    const bool preparingCpp = unrealAnalysisState_ == "preparing";
    bool changed = problemBuild_ != buildDiagnostics_ || !problemLanguage_.is_object() ||
                   !problemLanguage_.contains("preparingCpp") ||
                   problemLanguage_.at("preparingCpp") != preparingCpp ||
                   problemLanguage_.at("documents") != documents ||
                   problemLanguage_.at("servers").size() != servers.size();
    if (!changed)
        for (const auto &[backend, server] : servers.items()) {
            const auto previous = problemLanguage_.at("servers").find(backend);
            if (previous == problemLanguage_.at("servers").end() ||
                previous->at("diagnostics") != server.at("diagnostics") ||
                previous->at("diagnosticRevisions") != server.at("diagnosticRevisions") ||
                previous->at("enabled") != server.value("enabled", false) ||
                previous->at("ready") != server.value("ready", false)) {
                changed = true;
                break;
            }
        }
    if (changed) {
        // Copy payloads only on a real change; unchanged frames compare the
        // borrowed server snapshot instead of duplicating its full catalogue.
        Json snapshots = Json::object();
        for (const auto &[backend, server] : servers.items())
            snapshots[backend] = {{"diagnostics", server.at("diagnostics")},
                                  {"diagnosticRevisions", server.at("diagnosticRevisions")},
                                  {"enabled", server.value("enabled", false)},
                                  {"ready", server.value("ready", false)}};
        problemBuild_ = buildDiagnostics_;
        problemLanguage_ = {{"servers", std::move(snapshots)},
                            {"documents", std::move(documents)},
                            {"preparingCpp", preparingCpp}};
        const auto &diagnostics = problemLanguage_.at("servers");
        const auto &currentDocuments = problemLanguage_.at("documents");
        problems_ = {};
        for (const auto &entry : buildDiagnostics_) {
            const auto path = entry.value("path", "");
            const auto level = entry.value("severity", "error");
            problems_.add(
                "build/" + entry.dump(),
                level == "warning" ? cyui::LogLevel::warning
                : level == "error" ? cyui::LogLevel::error
                                   : cyui::LogLevel::info,
                path.empty() ? entry.value("displayPath", "") : path,
                entry.value("code", "") + " " + entry.value("message", ""),
                {path, entry.value("line", 1), entry.value("column", 1), false, true, "Last build result"});
        }
        for (const auto &[backend, server] : diagnostics.items()) {
            const auto publications = server.at("diagnosticRevisions");
            for (const auto &[uri, entries] : server.at("diagnostics").items()) {
                std::string path;
                try {
                    path = pathUtf8(engine_->resolve(pathUtf8(LanguageClient::fromUri(uri)))
                                        .lexically_relative(engine_->workspace()));
                } catch (const std::exception &) {
                    continue;
                }
                const bool enabled = server.at("enabled").get<bool>() && server.at("ready").get<bool>();
                const auto document = currentDocuments.find(path);
                const auto publication = publications.find(path);
                const bool current = document != currentDocuments.end() &&
                                     publication != publications.end() &&
                                     publication->value("uri", "") == uri &&
                                     publication->value("id", uint64_t(0)) == document->at("id") &&
                                     publication->value("revision", uint64_t(0)) == document->at("revision");
                const bool preparing = preparingCpp && LanguageService::supportsCpp(path);
                const bool navigable =
                    enabled && !preparing && (current || document == currentDocuments.end());
                const auto provenance = preparing  ? "Analysis configuration is changing"
                                        : !enabled ? "Inactive language server"
                                        : current  ? "Current document"
                                                   : "Position not verified";
                if (!entries.is_array())
                    continue;
                for (const auto &entry : entries)
                    try {
                        const auto &position = entry.at("range").at("start");
                        const auto line = position.at("line").get<int64_t>(),
                                   column = position.at("character").get<int64_t>();
                        if (line < 0 || line >= INT_MAX || column < 0 || column >= INT_MAX)
                            continue;
                        const auto severity = entry.value("severity", 1);
                        const auto code = entry.value("code", Json());
                        problems_.add("lsp/" + backend + "/" + uri + "/" + entry.dump(),
                                      severity == 1   ? cyui::LogLevel::error
                                      : severity == 2 ? cyui::LogLevel::warning
                                                      : cyui::LogLevel::info,
                                      path,
                                      (code.is_string()   ? code.get<std::string>()
                                       : code.is_number() ? code.dump()
                                                          : "") +
                                          " " + entry.value("message", ""),
                                      {path, static_cast<int>(line + 1), static_cast<int>(column + 1), true,
                                       navigable, provenance});
                    } catch (const Json::exception
                                 &) { /* A malformed backend row must not break the whole panel. */
                    }
            }
        }
    }
    if (!problems_.locations.contains(selectedProblem_))
        selectedProblem_ = 0;
}
void Application::navigateProblem(bool previous, bool currentFile) {
    synchronizeProblems();
    int line = 0;
    if (currentFile)
        for (const auto &tab : tabs_)
            if (tab->path == active_ && !tab->image)
                line = tab->editor.GetMainCursorPosition().line + 1;
    if (currentFile && line == 0)
        return;
    const auto selected = problems_.adjacent(problemsFilter_, selectedProblem_, previous,
                                             currentFile ? active_ : std::string{}, line, problemLevels_);
    if (!selected) {
        cyui::notify(tr("No matching diagnostic location"), cyui::Tone::neutral);
        return;
    }
    const auto &location = problems_.locations.at(selected);
    try {
        open(location.path, location.line, location.column, location.utf16);
        selectedProblem_ = selected;
    } catch (const std::exception &e) {
        error(e.what());
    }
}
} // namespace cycode
