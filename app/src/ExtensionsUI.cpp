#include "cycode/Application.h"
#include <algorithm>
#include <cyui/Core.h>
namespace cycode {
Json Application::extensionContext() const {
    bool readOnly = false;
    for (const auto &tab : tabs_)
        if (tab->path == active_)
            readOnly = tab->readOnly || bool(tab->image) || tab->pagedProfile;
    return {{"document", !active_.empty()},
            {"readOnly", readOnly},
            {"path", active_},
            {"provider", provider_},
            {"project", unreal_.value("available", false) ? "unreal"
                        : msbuildActive_                  ? "msbuild"
                        : projectActive_                  ? projectState_.value("kind", "folder")
                                                          : "folder"}};
}
bool Application::extensionVisible(const Json &entry) const {
    const auto context = extensionContext();
    const auto when = entry.value("when", Json::object());
    for (auto it = when.begin(); it != when.end(); ++it) {
        if (!context.contains(it.key()))
            return false;
        if (it.value().is_array()) {
            if (std::find(it.value().begin(), it.value().end(), context.at(it.key())) == it.value().end())
                return false;
        } else if (context.at(it.key()) != it.value())
            return false;
    }
    return true;
}
std::vector<Command> Application::commandCatalogue() const {
    auto catalogue = commands();
    for(auto &command:catalogue)if(command.id=="view.minimap"){command.title="Show minimap";command.context=CommandContext::global;}
    catalogue.push_back({"view.gitCommits","Git commits","View",CommandContext::global});
    catalogue.push_back({"file.keepOpen","Keep tab open","File",CommandContext::document});
    catalogue.push_back({"search.previous","Find previous","Search",CommandContext::document});
    catalogue.push_back({"view.toolWindows","Show or hide tool windows","View",CommandContext::global});
    catalogue.push_back({"view.backgroundTasks","Background tasks","View",CommandContext::global});
    catalogue.push_back({"view.analysis", "Analysis and inspections", "View", CommandContext::global});
    catalogue.push_back({"file.openPaged","Open with paged text","File",CommandContext::document});
    catalogue.push_back({"file.checkpointPaged","Checkpoint active paged text","File",CommandContext::document});
    for (const auto &entry : pluginInfo_.value("commands", Json::array())) {
        const auto scope = entry.value("context", std::string("global"));
        catalogue.push_back({entry.at("id"), entry.value("title", entry.at("id").get<std::string>()),
                             entry.value("category", std::string("Extensions")),
                             scope == "editor"     ? CommandContext::editor
                             : scope == "document" ? CommandContext::document
                                                   : CommandContext::global});
    }
    for (const auto &entry : pluginInfo_.value("panels", Json::array()))
        catalogue.push_back({"extension.panel." + entry.at("id").get<std::string>(),
                             entry.value("title", entry.at("id").get<std::string>()), "View"});
    for (const auto &entry : vcsContext_.value("actions", Json::array()))
        catalogue.push_back({"extension.vcs." + provider_ + "." + entry.at("id").get<std::string>(),
                             entry.at("title"), "Version control"});
    return catalogue;
}
void Application::synchronizeExtensions() {
    synchronizeSyntax(pluginInfo_.value("syntaxes", Json::array()));
    for (auto &[id, view] : extensionViews_) {
        const auto panels = pluginInfo_.value("panels", Json::array());
        if (std::none_of(panels.begin(), panels.end(), [&](const auto &p) { return p.at("id") == id; })) {
            view.inspector = {};
            panels_["extension:" + id] = false;
        }
    }
    std::vector<Command> extra;
    Json defaults = {{"view.toolWindows",Json::array({"Ctrl+Shift+F12"})},{"search.previous",Json::array({"Shift+F3"})}};
    for (const auto &entry :
         pluginInfo_.value("knownCommands", pluginInfo_.value("commands", Json::array()))) {
        const auto scope = entry.value("context", std::string("global"));
        extra.push_back({entry.at("id"), entry.value("title", entry.at("id").get<std::string>()),
                         "Extensions",
                         scope == "editor"     ? CommandContext::editor
                         : scope == "document" ? CommandContext::document
                                               : CommandContext::global});
        defaults[entry.at("id").get<std::string>()] = entry.value("shortcuts", Json::array());
    }
    for (const auto &command : commandCatalogue())
        if (!findCommand(command.id) &&
            std::none_of(extra.begin(), extra.end(), [&](const auto &c) { return c.id == command.id; }))
            extra.push_back(command);
    keymap_.extensions(std::move(extra), std::move(defaults));
    try {
        applyKeymap(keymapProfile_, keymapOverrides_.value(keymapProfile_, Json::object()));
    } catch (const std::exception &e) {
        error(e.what());
    }
}
void Application::applyKeymap(const std::string &profile, Json overrides) {
    if (!overrides.is_object())
        throw std::runtime_error("Keymap overrides must be an object");
    const auto known = pluginInfo_.value("knownCommands", pluginInfo_.value("commands", Json::array()));
    const auto catalogue = commandCatalogue();
    for (auto it = overrides.begin(); it != overrides.end();) {
        const bool exists =
            findCommand(it.key()) ||
            std::any_of(known.begin(), known.end(), [&](const auto &c) { return c.at("id") == it.key(); }) ||
            std::any_of(catalogue.begin(), catalogue.end(), [&](const auto &c) { return c.id == it.key(); });
        if (!exists)
            it = overrides.erase(it);
        else
            ++it;
    }
    // Retain unavailable extensions' overrides in preferences, ready for their next activation.
    keymap_.load(profile, overrides);
}
bool Application::extensionCommandEnabled(const std::string &id) const {
    if (id.starts_with("extension.panel.")) {
        const auto panel = id.substr(16);
        for (const auto &entry : pluginInfo_.value("panels", Json::array()))
            if (entry.at("id") == panel)
                return extensionVisible(entry);
        return false;
    }
    for (const auto &entry : vcsContext_.value("actions", Json::array()))
        if (id == "extension.vcs." + provider_ + "." + entry.at("id").get<std::string>())
            return !vcsFuture_.valid() && !taskFuture_.valid() &&
                   (entry.value("scope", std::string("repository")) != "file" ||
                    (!active_.empty() && active_.find("://") == std::string::npos));
    for (const auto &entry : pluginInfo_.value("commands", Json::array()))
        if (entry.at("id") == id) {
            if (!extensionVisible(entry) || taskFuture_.valid())
                return false;
            const auto scope = entry.value("context", std::string("global"));
            for(const auto &tab:tabs_)if(tab->path==active_ && tab->pagedProfile && scope!="global")return false;
            return scope == "global" || (!active_.empty() && (scope != "editor" || editorFocused_));
        }
    return false;
}
bool Application::executeExtensionCommand(const std::string &id) {
    if (id.starts_with("extension.panel.")) {
        const auto panel = id.substr(16);
        for (const auto &entry : pluginInfo_.value("panels", Json::array()))
            if (entry.at("id") == panel) {
                auto &view = extensionViews_[panel];
                if (!view.pending.valid()) {
                    view.model = Json::object();
                    view.error.clear();
                }
                showPanel(entry.value("title", panel).c_str(), ("extension:" + panel).c_str());
                return true;
            }
    }
    for (const auto &entry : vcsContext_.value("actions", Json::array()))
        if (id == "extension.vcs." + provider_ + "." + entry.at("id").get<std::string>()) {
            showPanel("Version control", "vcs");
            if (!entry.value("fields", Json::array()).empty())
                return true;
            Json params = Json::object();
            if (entry.value("scope", std::string("repository")) == "file")
                params["path"] = active_;
            vcsTask("vcs." + entry.at("id").get<std::string>(), params);
            return true;
        }
    for (const auto &entry : pluginInfo_.value("commands", Json::array()))
        if (entry.at("id") == id) {
            Json input = {{"context", extensionContext()}};
            for (const auto &tab : tabs_)
                if (tab->path == active_) {
                    if(tab->pagedProfile){input["storage"]="paged";input["documentId"]=tab->identity;}
                    else input["text"] = tab->image ? "" : tab->editor.GetText();
                    input["revision"] = tab->revision;
                }
            task("plugins.invoke", {{"command", id}, {"input", input}});
            return true;
        }
    return false;
}
void Application::extensionMenu(const std::string &location) {
    for (const auto &entry : pluginInfo_.value("menus", Json::array()))
        if (entry.value("location", std::string{}) == location && extensionVisible(entry)) {
            const std::string command = entry.at("command"), title = entry.value("title", command);
            if (ImGui::MenuItem(tr(title.c_str()), keymap_.hint(command).c_str(), false,
                                commandEnabled(command)))
                executeCommand(command);
        }
}
void Application::extensionPanels() {
    for (auto &[id, state] : extensionViews_)
        if (state.pending.valid() &&
            state.pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try {
                auto model = state.pending.get();
                if (!model.is_object())
                    throw std::runtime_error("Panel model must be an object");
                if (model.contains("error"))
                    throw std::runtime_error(model.at("error").get<std::string>());
                if (model.contains("text") && !model.at("text").is_string())
                    throw std::runtime_error("Invalid panel text");
                if (model.contains("revision") &&
                    ((!model.at("revision").is_number_unsigned() &&
                      !model.at("revision").is_number_integer()) ||
                     (model.at("revision").is_number_unsigned() ? model.at("revision").get<uint64_t>() < 1
                                                                : model.at("revision").get<int64_t>() < 1)))
                    throw std::runtime_error("Invalid panel revision");
                for (const auto *key : {"properties", "rows", "actions"})
                    if (model.contains(key) && !model.at(key).is_array())
                        throw std::runtime_error("Invalid panel collection");
                if (model.value("properties", Json::array()).size() > 128 ||
                    model.value("rows", Json::array()).size() > 2000 ||
                    model.value("actions", Json::array()).size() > 64)
                    throw std::runtime_error("Panel model exceeds view limits");
                std::set<std::string> ids;
                for (const auto &p : model.value("properties", Json::array())) {
                    if (!p.is_object() || !p.at("id").is_string() || !p.contains("value") ||
                        !ids.insert(p.at("id")).second)
                        throw std::runtime_error("Invalid panel property");
                    if ((p.contains("title") && !p.at("title").is_string()) ||
                        (p.contains("readOnly") && !p.at("readOnly").is_boolean()))
                        throw std::runtime_error("Invalid panel property metadata");
                    const auto &value = p.at("value");
                    if (!(value.is_boolean() || value.is_string() || value.is_number()) ||
                        (value.is_number_unsigned() && value.get<uint64_t>() > uint64_t(INT64_MAX)))
                        throw std::runtime_error("Unsupported panel property value");
                }
                for (const auto &row : model.value("rows", Json::array()))
                    if (!row.is_object() || !row.value("name", Json("")).is_string() ||
                        !row.value("value", Json("")).is_string())
                        throw std::runtime_error("Invalid panel row");
                for (const auto &a : model.value("actions", Json::array()))
                    if (!a.is_object() || !a.value("command", Json("")).is_string() ||
                        !a.value("title", Json("")).is_string())
                        throw std::runtime_error("Invalid panel action");
                state.model = std::move(model);
                state.error.clear();
            } catch (const std::exception &e) {
                state.error = e.what();
            }
        }
    for (const auto &entry : pluginInfo_.value("panels", Json::array())) {
        const std::string id = entry.at("id"), title = entry.value("title", id), key = "extension:" + id;
        if (!panels_.contains(key) || !panels_.at(key) || !extensionVisible(entry))
            continue;
        auto &state = extensionViews_[id];
        auto refresh = [&](Json event) {
            if (state.pending.valid())
                return;
            event["context"] = extensionContext();
            auto engine = engine_;
            state.pending = std::async(std::launch::async,
                                       [engine, id, event] { return engine->plugins().invoke(id, event); });
        };
        if (!beginPanel(title.c_str(), key.c_str()))
            continue;
        if (state.model.empty() && state.error.empty() && !state.pending.valid())
            refresh({{"event", "read"}});
        if (cyui::button(tr("Refresh")))
            refresh({{"event", "read"}});
        if (state.pending.valid()) {
            ImGui::SameLine();
            cyui::spinner("##extension-loading");
        }
        if (!state.error.empty())
            cyui::callout(cyui::Tone::error, state.error.c_str());
        const auto &model = state.model;
        if (model.contains("text") && model.at("text").is_string())
            ImGui::TextWrapped("%s", model.at("text").get_ref<const std::string &>().c_str());
        if (model.contains("properties") && model.at("properties").is_array()) {
            std::vector<cyui::Property> properties;
            std::map<cyui::ItemId, std::string> names;
            for (const auto &value : model.at("properties")) {
                if (properties.size() >= 128 || !value.is_object() || !value.contains("id") ||
                    !value.contains("value"))
                    break;
                const std::string name = value.at("id");
                cyui::Property property;
                property.id = uiId(id + "/" + name);
                property.label = value.value("title", name);
                property.readOnly = value.value("readOnly", true);
                const auto &v = value.at("value");
                if (v.is_boolean())
                    property.value = v.get<bool>();
                else if (v.is_number_integer())
                    property.value = v.get<int64_t>();
                else if (v.is_number())
                    property.value = v.get<double>();
                else if (v.is_string())
                    property.value = v.get<std::string>();
                else
                    continue;
                names[property.id] = name;
                properties.push_back(std::move(property));
            }
            cyui::ScopedDisabled disabled(state.pending.valid());
            for (const auto &edit : cyui::inspector("##extension-properties", state.inspector, properties,
                                                    model.value("revision", uint64_t(1))))
                if (edit.edit.committed()) {
                    Json value;
                    std::visit([&](const auto &v) { value = v; }, edit.edit.value);
                    refresh({{"event", "commit"},
                             {"property", names.at(edit.id)},
                             {"value", value},
                             {"expectedRevision", model.value("revision", uint64_t(1))}});
                }
        }
        if (model.contains("rows") && model.at("rows").is_array() &&
            ImGui::BeginTable("##extension-rows", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            size_t count = 0;
            for (const auto &row : model.at("rows")) {
                if (++count > 2000)
                    break;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                const auto name = row.value("name", std::string{}), value = row.value("value", std::string{});
                ImGui::TextUnformatted(name.c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            }
            ImGui::EndTable();
        }
        for (const auto &action : model.value("actions", Json::array())) {
            const std::string command = action.value("command", std::string{}),
                              name = action.value("title", command);
            // A panel cannot invoke another extension's commands through a model.
            bool owned = false;
            for (const auto &c : pluginInfo_.value("commands", Json::array()))
                if (c.at("id") == command && c.at("plugin") == entry.at("plugin"))
                    owned = true;
            cyui::ScopedDisabled disabled(!owned || !commandEnabled(command));
            if (cyui::button(tr(name.c_str())))
                executeCommand(command);
        }
        ImGui::End();
    }
}
} // namespace cycode
