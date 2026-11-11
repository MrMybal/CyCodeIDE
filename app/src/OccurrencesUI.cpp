#include "cycode/Application.h"
#include "cycode/EditorOccurrences.h"
#include "cycode/EditorTextMap.h"
#include "cycode/OccurrenceCatalogue.h"

namespace cycode {
Json Application::occurrenceParameters() const {
    if (smoke_)
        return nullptr;
    for (const auto &tab : tabs_) {
        if (tab->path != active_ || tab->image || tab->pagedProfile || tab->path.starts_with("debug://") ||
            isUntitledDocument(tab->path) ||
            (LanguageService::supportsCpp(tab->path) && unrealAnalysisState_ == "preparing"))
            continue;
        const auto backend = engine_->languageBackendFor(tab->path);
        const auto servers = languageState_.find("servers");
        if (backend.empty() || servers == languageState_.end() || !servers->contains(backend))
            return nullptr;
        const auto &server = servers->at(backend);
        if (!server.value("enabled", false) ||
            (server.value("ready", false) &&
             !server.value("methods", Json::object()).value("documentHighlights", false)))
            return nullptr;
        const auto cursor = tab->editor.GetMainCursorPosition();
        return {{"path", tab->path},
                {"expectedRevision", tab->revision},
                {"expectedDocumentId", tab->identity},
                {"expectedEpoch", server.value("epoch", uint64_t(0))},
                {"includeExternal", tab->readOnly},
                {"limit", 4096},
                {"position",
                 {{"line", cursor.line},
                  {"character",
                   LanguageClient::utf16Length(tab->editor.GetSectionText({cursor.line, 0}, cursor))}}}};
    }
    return nullptr;
}
bool Application::occurrencesCurrent() const {
    return occurrences_.report.contains("occurrences") && navigationCurrent(occurrences_.report) &&
           occurrences_.wanted == occurrenceParameters().dump();
}
void Application::clearOccurrences() {
    occurrences_.annotations = {};
    for (auto &count : occurrences_.counts)
        count = 0;
    occurrences_.report = Json::object();
    occurrences_.labels.clear();
    occurrences_.matches.clear();
    occurrences_.paintKey.clear();
    occurrences_.filterKey.clear();
    occurrences_.painted = 0;
    occurrences_.paintTruncated = false;
}
void Application::updateOccurrences() {
    auto params = occurrenceParameters();
    auto key = params.is_null() ? std::string{} : params.dump();
    if (key != occurrences_.wanted) {
        if (key != occurrences_.suppressed)
            occurrences_.suppressed.clear();
        clearOccurrences();
        occurrences_.error.clear();
        occurrences_.wanted = key;
        occurrences_.sent.clear();
        occurrences_.changed = ImGui::GetTime();
        occurrences_.manual = false;
    }
    if (occurrences_.future.valid() &&
        occurrences_.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto reply = occurrences_.future.get();
            if (reply.at("key") == occurrences_.wanted && key != occurrences_.suppressed) {
                if (reply.contains("error"))
                    occurrences_.error = reply.at("error");
                else {
                    languageState_ = engine_->languageStatus();
                    if (navigationCurrent(reply.at("result")) && key == occurrenceParameters().dump()) {
                        occurrences_.report = std::move(reply.at("result"));
                        occurrences_.sent = key;
                        for (const auto &row : occurrences_.report.at("occurrences")) {
                            ++occurrences_.counts[row.at("kind").get<unsigned>() - 1];
                            const auto &start = row.at("range").at("start");
                            occurrences_.labels.push_back(
                                std::to_string(start.at("line").get<size_t>() + 1) + ":" +
                                std::to_string(start.at("character").get<size_t>() + 1));
                        }
                    }
                }
            }
        } catch (const std::exception &e) {
            occurrences_.error = e.what();
        }
    }
    if (key.empty() || key == occurrences_.suppressed)
        return;
    if (params != occurrenceParameters()) {
        clearOccurrences();
        occurrences_.wanted.clear();
        occurrences_.sent.clear();
        return;
    }
    const auto &server = languageState_.at("servers").at(engine_->languageBackendFor(active_));
    if (!occurrences_.future.valid() && occurrences_.sent != key &&
        (occurrences_.manual || (occurrences_.automatic && server.value("ready", false) &&
                                 ImGui::GetTime() - occurrences_.changed >= .35))) {
        occurrences_.sent = key;
        occurrences_.manual = false;
        auto engine = engine_;
        occurrences_.future = std::async(std::launch::async, [engine, params, key] {
            try {
                return Json{{"key", key},
                            {"result", engine->dispatch("language.documentHighlights", params)}};
            } catch (const std::exception &e) {
                return Json{{"key", key}, {"error", e.what()}};
            }
        });
    }
    if (!occurrencesCurrent())
        return;
    std::string paintKey = std::to_string(occurrences_.kinds);
    for (const auto &tab : tabs_)
        if (tab->path == active_)
            paintKey += ":" + std::to_string(tab->editor.GetTabSize());
    if (paintKey != occurrences_.paintKey)
        for (const auto &tab : tabs_)
            if (tab->path == active_ && !tab->image) {
                occurrences_.annotations = occurrenceAnnotations(
                    tab->editor, occurrences_.report.at("occurrences"), occurrences_.kinds);
                const auto &result = occurrences_.annotations;
                occurrences_.painted = result.count;
                occurrences_.paintTruncated = result.truncated;
                occurrences_.paintKey = paintKey;
            }
}
void Application::occurrenceAction(const std::string &id) {
    if (id == "code.clearOccurrences") {
        occurrences_.error.clear();
        occurrences_.suppressed = occurrenceParameters().dump();
        occurrences_.wanted.clear();
        occurrences_.sent.clear();
        occurrences_.manual = false;
        clearOccurrences();
        return;
    }
    if (id == "code.occurrences") {
        const auto params = occurrenceParameters();
        if (params.is_null() || occurrences_.future.valid())
            return;
        clearOccurrences();
        occurrences_.error.clear();
        occurrences_.suppressed.clear();
        occurrences_.wanted = params.dump();
        occurrences_.sent.clear();
        occurrences_.manual = true;
        showPanel("Occurrences", "occurrences");
        return;
    }
    if (!occurrencesCurrent() || occurrences_.future.valid())
        return;
    for (const auto &tab : tabs_)
        if (tab->path == active_ && !tab->image) {
            const auto &rows = occurrences_.report.at("occurrences");
            if (id == "code.selectOccurrences") {
                if (occurrences_.report.value("truncated", false) || occurrences_.report.value("invalid", 0))
                    throw std::runtime_error(tr("Incomplete occurrence report cannot replace selections."));
                selectOccurrenceRanges(tab->editor, occurrenceSelections(rows, occurrences_.kinds));
            } else {
                const auto params = occurrenceParameters();
                const auto index = adjacentOccurrence(rows, params.at("position"),
                                                      id == "code.previousOccurrence", occurrences_.kinds);
                if (index < 0)
                    return;
                const auto text = tab->editor.GetText();
                EditorTextMap coordinates(text);
                const auto pos = coordinates.position(rows.at(index).at("range").at("start"));
                if (!pos)
                    return;
                rememberNavigation();
                tab->editor.SetCursor(*pos);
                tab->editor.ScrollToLine(pos->line);
            }
            tab->pendingSession = nullptr;
            tab->snippet.cancel();
            tab->editor.SetFocus();
            focusTab_ = active_;
            // Navigation within this explicit report retains its anchor, without a new server query.
            occurrences_.wanted = occurrences_.sent = occurrenceParameters().dump();
            return;
        }
}
void Application::occurrencePanel() {
    panels_.try_emplace("occurrences", false);
    if (!beginPanel("Occurrences", "occurrences"))
        return;
    auto inlineButton = [](const char *label) {
        const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize(label).x +
                2 * ImGui::GetStyle().FramePadding.x <=
            right)
            ImGui::SameLine();
    };
    ImGui::Checkbox(tr("Automatic occurrence highlights"), &occurrences_.automatic);
    inlineButton(tr("Highlight occurrences at caret"));
    {
        cyui::ScopedDisabled disabled(!commandEnabled("code.occurrences"));
        if (cyui::button(tr("Highlight occurrences at caret")))
            occurrenceAction("code.occurrences");
    }
    inlineButton(tr("Clear occurrence highlights"));
    if (cyui::button(tr("Clear occurrence highlights")))
        occurrenceAction("code.clearOccurrences");
    for (const auto &[bit, label] : std::initializer_list<std::pair<unsigned, const char *>>{
             {1, "Text occurrences"}, {2, "Read occurrences"}, {4, "Write occurrences"}}) {
        bool selected = (occurrences_.kinds & bit) != 0;
        if (ImGui::Checkbox(tr(label), &selected))
            occurrences_.kinds ^= bit;
        ImGui::SameLine();
        cyui::textDim("(%zu)", occurrences_.counts[bit == 1 ? 0 : bit == 2 ? 1 : 2]);
    }
    bool firstButton = true;
    for (const auto *action : {"code.previousOccurrence", "code.nextOccurrence", "code.selectOccurrences"}) {
        const auto command = findCommand(action);
        if (!firstButton)
            inlineButton(tr(command->title.c_str()));
        firstButton = false;
        cyui::ScopedDisabled disabled(!commandEnabled(action));
        if (cyui::button(tr(command->title.c_str()))) {
            try {
                occurrenceAction(action);
            } catch (const std::exception &e) {
                occurrences_.error = e.what();
            }
        }
    }
    if (occurrences_.future.valid())
        cyui::spinner("##occurrence-loading");
    if (!occurrences_.error.empty())
        cyui::textColoured(cyui::palette().warning, "%s", occurrences_.error.c_str());
    if (!occurrencesCurrent()) {
        cyui::emptyState(tr("Occurrences"),
                         tr("Open a document with document-highlight support, then highlight at caret."),
                         cyui::icon::search);
        ImGui::End();
        return;
    }
    viewFilter("##occurrence-filter", occurrences_.filter);
    const auto &rows = occurrences_.report.at("occurrences");
    const auto filterKey = occurrences_.filter.query + ":" + std::to_string(occurrences_.kinds) + ":" +
                           std::string(cyui::locale());
    if (filterKey != occurrences_.filterKey) {
        occurrences_.matches.clear();
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto kind = rows.at(i).at("kind").get<unsigned>();
            const auto label = occurrences_.labels[i] + " " +
                               tr(kind == 3   ? "Write occurrences"
                                  : kind == 2 ? "Read occurrences"
                                              : "Text occurrences");
            if ((occurrences_.kinds & (1u << (kind - 1))) && occurrences_.filter.matches(label))
                occurrences_.matches.push_back(i);
        }
        occurrences_.filterKey = filterKey;
    }
    cyui::textDim("%zu / %zu %s | %zu %s", occurrences_.matches.size(), rows.size(), tr("matches"),
                  occurrences_.painted, tr("highlighted spans"));
    if (occurrences_.report.value("truncated", false) || occurrences_.paintTruncated)
        cyui::textColoured(cyui::palette().warning, "%s", tr("Occurrence query or display budget reached."));
    if (occurrences_.report.value("invalid", 0))
        cyui::textDim("%s: %d", tr("Invalid occurrences ignored"),
                      occurrences_.report.at("invalid").get<int>());
    cyui::textDim(
        "%s", tr("Navigation and selection use the kind filters; the text filter only filters this list."));
    ImGui::BeginChild("##occurrence-list", {}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    ImGuiListClipper clip;
    clip.Begin(static_cast<int>(occurrences_.matches.size()));
    while (clip.Step())
        for (int index = clip.DisplayStart; index < clip.DisplayEnd; ++index) {
            const auto i = occurrences_.matches[index];
            const auto kind = rows.at(i).at("kind").get<unsigned>();
            const auto label = occurrences_.labels[i] + " | " +
                               tr(kind == 3   ? "Write occurrences"
                                  : kind == 2 ? "Read occurrences"
                                              : "Text occurrences");
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(label.c_str())) {
                for (const auto &tab : tabs_)
                    if (tab->path == active_ && !tab->image) {
                        const auto text = tab->editor.GetText();
                        EditorTextMap coordinates(text);
                        if (const auto pos = coordinates.position(rows.at(i).at("range").at("start"))) {
                            rememberNavigation();
                            tab->editor.SetCursor(*pos);
                            tab->editor.ScrollToLine(pos->line);
                            tab->pendingSession = nullptr;
                            tab->snippet.cancel();
                            tab->editor.SetFocus();
                            focusTab_ = active_;
                            occurrences_.wanted = occurrences_.sent = occurrenceParameters().dump();
                        }
                    }
            }
            ImGui::PopID();
        }
    ImGui::EndChild();
    ImGui::End();
}
} // namespace cycode
