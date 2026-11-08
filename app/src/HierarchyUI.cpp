#include "cycode/Application.h"
#include <algorithm>
#include <cyui/Core.h>
#include <functional>
namespace cycode {
void Application::hierarchyTask(const std::string &method, const Json &params, const std::string &key) {
    if (hierarchyFuture_.valid())
        return;
    hierarchyOperation_ = method;
    hierarchyRequestKey_ = key;
    hierarchyError_.clear();
    auto engine = engine_;
    hierarchyFuture_ =
        std::async(std::launch::async, [engine, method, params] { return engine->dispatch(method, params); });
}
void Application::requestHierarchy(const std::string &kind) {
    if (hierarchyFuture_.valid())
        return;
    for (const auto &tab : tabs_) {
        if (tab->path != active_ || tab->image || tab->path.find("://") != std::string::npos ||
            !engine_->supportsLanguage(tab->path))
            continue;
        const auto cursor = tab->editor.GetMainCursorPosition();
        Json params = {{"path", tab->path},
                       {"kind", kind},
                       {"expectedRevision", tab->revision},
                       {"includeExternal", true},
                       {"position",
                        {{"line", cursor.line},
                         {"character", LanguageClient::utf16Length(
                                           tab->editor.GetSectionText({cursor.line, 0}, cursor))}}}};
        hierarchyKind_ = kind;
        hierarchyDirection_ = kind == "call" ? "incoming" : "supertypes";
        hierarchyState_ = Json::object();
        hierarchySelected_ = Json::object();
        hierarchyBranches_.clear();
        hierarchyBranchErrors_.clear();
        hierarchyBytes_ = 0;
        hierarchyValid_ = false;
        hierarchyTask("language.hierarchyPrepare", params);
        showPanel("Hierarchy", "hierarchy");
        return;
    }
}
void Application::updateHierarchy() {
    if (hierarchyFuture_.valid() &&
        hierarchyFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto result = hierarchyFuture_.get();
            if (hierarchyOperation_ == "language.hierarchyPrepare") {
                hierarchyState_ = std::move(result);
                hierarchyValid_ = true;
            } else if (hierarchyOperation_ == "language.hierarchyStatus") {
                hierarchyValid_ = result.at("valid");
                if (!hierarchyValid_)
                    hierarchyError_ = result.at("reason");
            } else {
                const auto bytes = result.dump().size();
                if (hierarchyBranches_.size() >= 64 || hierarchyBytes_ + bytes > 8 * 1024 * 1024)
                    hierarchyBranchErrors_[hierarchyRequestKey_] =
                        tr("Hierarchy view limit reached. Start again from a symbol.");
                else {
                    hierarchyBytes_ += bytes;
                    hierarchyBranches_[hierarchyRequestKey_] = std::move(result);
                }
            }
        } catch (const std::exception &e) {
            hierarchyError_ = e.what();
            if (hierarchyOperation_ == "language.hierarchyExpand")
                hierarchyBranchErrors_[hierarchyRequestKey_] = e.what();
            if (hierarchyOperation_ == "language.hierarchyStatus" ||
                std::string(e.what()).starts_with("RevisionConflict"))
                hierarchyValid_ = false;
        }
        hierarchyLastCheck_ = ImGui::GetTime();
    }
    if (!hierarchyState_.contains("sessionId"))
        return;
    // A local source edit is visible immediately; other documents, disk notifications
    // and backend restarts are checked by the common service without a server request.
    for (const auto &tab : tabs_)
        if (tab->path == hierarchyState_.at("path").get<std::string>() &&
            tab->revision != hierarchyState_.at("revision").get<uint64_t>()) {
            hierarchyValid_ = false;
            hierarchyError_ = tr("Hierarchy context changed. Refresh at the symbol.");
        }
    if (panels_.contains("hierarchy") && panels_.at("hierarchy") && hierarchyValid_ &&
        !hierarchyFuture_.valid() && ImGui::GetTime() - hierarchyLastCheck_ > .5) {
        hierarchyLastCheck_ = ImGui::GetTime();
        hierarchyTask("language.hierarchyStatus", {{"sessionId", hierarchyState_.at("sessionId")}});
    }
}
void Application::hierarchyPanel() {
    if (!beginPanel("Hierarchy", "hierarchy"))
        return;
    const bool busy = hierarchyFuture_.valid();
    {
        cyui::ScopedDisabled disabled(busy);
        const char *kinds[] = {tr("Call hierarchy"), tr("Type hierarchy")};
        int kind = hierarchyKind_ == "call" ? 0 : 1;
        if (cyui::segmented("##hierarchy-kind", kinds, 2, &kind))
            requestHierarchy(kind ? "type" : "call");
        ImGui::SameLine();
        const auto command = hierarchyKind_ == "call" ? "navigate.callHierarchy" : "navigate.typeHierarchy";
        const auto shortcut = keymap_.hint(command);
        const auto tip =
            std::string(tr("Refresh at caret")) + (shortcut.empty() ? "" : " (" + shortcut + ")");
        if (cyui::iconButton(cyui::icon::refresh, tip.c_str()))
            requestHierarchy(hierarchyKind_);
    }
    if (busy) {
        ImGui::SameLine();
        cyui::spinner("##hierarchy-query");
    }
    if (!hierarchyError_.empty())
        cyui::callout(cyui::Tone::warning, hierarchyError_.c_str());
    if (!hierarchyState_.contains("sessionId")) {
        cyui::emptyState(tr("Symbol hierarchy"),
                         tr("Place the caret on a function or type, then choose a hierarchy."),
                         cyui::icon::code);
        {
            cyui::ScopedDisabled disabled(busy || !commandEnabled(hierarchyKind_ == "call"
                                                                      ? "navigate.callHierarchy"
                                                                      : "navigate.typeHierarchy"));
            if (cyui::primaryButton(tr("Inspect symbol at caret")))
                requestHierarchy(hierarchyKind_);
        }
        ImGui::End();
        return;
    }
    cyui::textDim("%s", hierarchyState_.at("path").get_ref<const std::string &>().c_str());
    const char *directions[] = {hierarchyKind_ == "call" ? "incoming" : "supertypes",
                                hierarchyKind_ == "call" ? "outgoing" : "subtypes"};
    const char *labels[] = {tr(hierarchyKind_ == "call" ? "Callers" : "Base types"),
                            tr(hierarchyKind_ == "call" ? "Callees" : "Derived types")};
    int direction = hierarchyDirection_ == directions[0] ? 0 : 1;
    if (cyui::segmented("##hierarchy-direction", labels, 2, &direction)) {
        hierarchyDirection_ = directions[direction];
        hierarchySelected_ = Json::object();
    }
    if (!hierarchyValid_)
        cyui::textDim("%s", tr("This hierarchy is stale. Refresh before expanding or navigating."));
    auto navigate = [&](const Json &location) {
        if (!hierarchyValid_)
            return;
        const auto state =
            engine_->dispatch("language.hierarchyStatus", {{"sessionId", hierarchyState_.at("sessionId")}});
        if (!state.at("valid").get<bool>()) {
            hierarchyValid_ = false;
            hierarchyError_ = state.at("reason");
            return;
        }
        const auto start = location.at("range").at("start");
        open(location.at("path"), start.at("line").get<size_t>() + 1, start.at("character").get<size_t>() + 1,
             true);
    };
    auto truncated = [&](const Json &state) {
        if (state.value("truncated", false) || state.value("omitted", size_t(0)) > 0)
            cyui::textDim("%s", tr("Some hierarchy results were omitted or reached a limit."));
    };
    truncated(hierarchyState_);
    const auto available = ImGui::GetContentRegionAvail().y;
    if (ImGui::BeginChild("##hierarchy-tree", {0, std::max(cyui::dp(100), available * .6f)},
                          ImGuiChildFlags_Borders)) {
        std::set<std::string> ancestors;
        size_t rows = 0;
        std::function<void(const Json &, size_t)> draw = [&](const Json &edge, size_t depth) {
            if (++rows > 4096)
                return;
            const auto &item = edge.at("item");
            const auto id = item.at("id").get<std::string>();
            const auto identity = item.at("path").get<std::string>() + item.at("range").dump();
            const bool cycle = ancestors.contains(identity), leaf = cycle || depth >= 32;
            cyui::ScopedID row(id.c_str());
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (leaf)
                flags |= ImGuiTreeNodeFlags_Leaf;
            if (hierarchySelected_.contains("item") && hierarchySelected_.at("item").at("id") == id)
                flags |= ImGuiTreeNodeFlags_Selected;
            if (!depth && rows == 1)
                flags |= ImGuiTreeNodeFlags_DefaultOpen;
            const auto text = std::string(cyui::icon::code) + " " + item.at("name").get<std::string>();
            const bool opened = ImGui::TreeNodeEx("##node", flags, "%s", text.c_str());
            if ((ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) || ImGui::IsItemFocused())
                hierarchySelected_ = edge;
            if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter, false))
                navigate(item);
            if (ImGui::IsItemHovered()) {
                const auto detail = item.at("detail").get<std::string>() + "\n" +
                                    item.value("displayPath", item.at("path").get<std::string>());
                cyui::tooltip(detail.c_str());
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    navigate(item);
            }
            if (opened) {
                if (leaf)
                    cyui::textDim("%s",
                                  tr(cycle ? "Recursive relationship" : "Hierarchy depth limit reached"));
                else {
                    const auto key = hierarchyDirection_ + "/" + id;
                    if (hierarchyBranches_.contains(key)) {
                        const auto &branch = hierarchyBranches_.at(key);
                        truncated(branch);
                        if (branch.at("nodes").empty())
                            cyui::textDim("%s", tr("No relationships"));
                        ancestors.insert(identity);
                        for (size_t i = 0; i < branch.at("nodes").size(); ++i) {
                            cyui::ScopedID child(static_cast<int>(i));
                            draw(branch.at("nodes").at(i), depth + 1);
                        }
                        ancestors.erase(identity);
                    } else if (hierarchyBranchErrors_.contains(key)) {
                        cyui::textDim("%s", hierarchyBranchErrors_.at(key).c_str());
                        cyui::ScopedDisabled disabled(busy || !hierarchyValid_);
                        if (cyui::button(tr("Retry branch"))) {
                            hierarchyBranchErrors_.erase(key);
                            hierarchyTask("language.hierarchyExpand",
                                          {{"sessionId", hierarchyState_.at("sessionId")},
                                           {"item", id},
                                           {"direction", hierarchyDirection_}},
                                          key);
                        }
                    } else if (hierarchyValid_ && !hierarchyFuture_.valid()) {
                        if (hierarchyBranches_.size() >= 64 || hierarchyBytes_ >= 8 * 1024 * 1024)
                            cyui::textDim("%s",
                                          tr("Hierarchy view limit reached. Start again from a symbol."));
                        else
                            hierarchyTask("language.hierarchyExpand",
                                          {{"sessionId", hierarchyState_.at("sessionId")},
                                           {"item", id},
                                           {"direction", hierarchyDirection_}},
                                          key);
                    } else if (hierarchyValid_)
                        cyui::textDim("%s", tr("Loading hierarchy..."));
                }
                ImGui::TreePop();
            }
        };
        for (size_t i = 0; i < hierarchyState_.at("nodes").size(); ++i) {
            cyui::ScopedID root(static_cast<int>(i));
            draw(hierarchyState_.at("nodes").at(i), 0);
        }
        if (hierarchyState_.at("nodes").empty())
            cyui::textDim("%s", tr("No hierarchy at this position"));
        if (rows > 4096)
            cyui::textDim("%s", tr("Hierarchy view limit reached. Start again from a symbol."));
    }
    ImGui::EndChild();
    if (hierarchySelected_.contains("item")) {
        cyui::ScopedDisabled disabled(!hierarchyValid_);
        const auto &item = hierarchySelected_.at("item");
        cyui::textDim("%s", item.at("name").get_ref<const std::string &>().c_str());
        if (cyui::button(tr("Open symbol")))
            navigate(item);
        ImGui::SameLine();
        cyui::textDim("%zu %s", hierarchySelected_.at("sites").size(), tr("Call sites"));
        if (ImGui::BeginChild("##hierarchy-sites", {0, 0}, ImGuiChildFlags_Borders))
            for (size_t i = 0; i < hierarchySelected_.at("sites").size(); ++i) {
                const auto &site = hierarchySelected_.at("sites").at(i);
                const auto &start = site.at("range").at("start");
                const auto title = site.value("displayPath", site.at("path").get<std::string>()) + ":" +
                                   std::to_string(start.at("line").get<size_t>() + 1) + ":" +
                                   std::to_string(start.at("character").get<size_t>() + 1);
                cyui::ScopedID id(static_cast<int>(i));
                if (ImGui::Selectable(title.c_str()))
                    navigate(site);
            }
        ImGui::EndChild();
    }
    ImGui::End();
}
} // namespace cycode
