#include "cycode/Commands.h"
#include "cycode/ModuleModels.h"
using namespace cycode;
namespace {
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
} // namespace
void problemNavigationContracts() {
    ProblemModel model;
    cyui::TextFilter filter;
    check(model.adjacent(filter, 0, false) == 0, "Empty diagnostics produced a location");
    model.add("first", cyui::LogLevel::error, "a.cpp", "first", {"a.cpp", 8, 2, false});
    model.add("second", cyui::LogLevel::warning, "a.cpp", "second", {"a.cpp", 3, 1, false});
    model.add("third", cyui::LogLevel::error, "b.cpp", "third", {"b.cpp", 1, 1, true});
    model.add("unmapped", cyui::LogLevel::error, "tool", "no source", {});
    const auto a = model.rows[0].id, b = model.rows[1].id, c = model.rows[2].id;
    check(model.adjacent(filter, 0, false) == a && model.adjacent(filter, 0, true) == c,
          "Global ordering is unstable");
    check(model.adjacent(filter, c, false) == a && model.adjacent(filter, a, true) == c,
          "Diagnostic wrapping failed");
    check(model.adjacent(filter, 0, false, "a.cpp", 4) == a &&
              model.adjacent(filter, 0, true, "a.cpp", 4) == b,
          "Current-file anchor ignored the current line");
    check(model.adjacent(filter, b, false, "a.cpp") == a && model.adjacent(filter, a, false, "a.cpp") == b,
          "Current-file order did not use source lines");
    check(model.adjacent(filter, a, false, "missing.cpp") == 0, "Current-file navigation escaped its scope");
    filter.query = "second";
    check(model.adjacent(filter, c, false) == b && model.adjacent(filter, b, true) == b,
          "Navigation ignored the diagnostic filter");
    filter.query = "unmatched";
    check(model.adjacent(filter, a, false) == 0, "Empty filter result produced a location");
    filter.query.clear();
    check(model.matching(filter, 4).size() == 1 && model.adjacent(filter, a, false, {}, 0, 4) == b,
          "Severity filtering and navigation disagree");
    check(model.matching(filter, 0).empty() && model.adjacent(filter, a, false, {}, 0, 0) == 0,
          "All severities hidden still navigated");
    model.add("stale", cyui::LogLevel::error, "a.cpp", "old position",
              {"a.cpp", 1, 1, true, false, "Position not verified"});
    const auto stale = model.rows.back().id;
    check(model.adjacent(filter, c, false) == a && model.adjacent(filter, stale, true) == c,
          "A non-current source position participated in navigation");
    check(model.matching(filter, 8).size() == 4 && model.matching(filter, 4).size() == 1,
          "Severity cache retained the previous mask after a model change");
    Keymap map;
    map.load("rider");
    check(map.press("F2", 1, true, true).command == "problem.nextInFile",
          "Rider current-file diagnostic shortcut missing");
    check(map.press("Alt+F2", 2, true, true).command == "problem.next",
          "Rider global diagnostic shortcut missing");
    for (const auto *profile : {"visualstudio", "vscode"}) {
        map.load(profile);
        check(map.press("F8", 3, true, true).command == "problem.next", "Global diagnostic shortcut missing");
        check(map.press("Shift+F8", 4, true, true).command == "problem.previous",
              "Previous diagnostic shortcut missing");
    }
    map.load("vscode");
    check(map.press("F2", 5, true, true).command == "refactor.rename",
          "VS Code rename shortcut was replaced");
}
