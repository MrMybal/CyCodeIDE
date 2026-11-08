#include "cycode/ModuleModels.h"
#include "cycode/SymbolCatalogue.h"
using namespace cycode;
void outlineModelContracts() {
    using Json = nlohmann::json;
    auto check = [](bool ok, const char *message) {
        if (!ok)
            throw std::runtime_error(message);
    };
    auto row = [](uint64_t id, uint64_t parent, const std::string &name, int line) {
        return Json{
            {"node", id},
            {"parent", parent},
            {"name", name},
            {"detail", "signature"},
            {"container", "Root"},
            {"fullRange",
             {{"start", {{"line", line}, {"character", 0}}}, {"end", {{"line", line}, {"character", 1}}}}}};
    };
    OutlineModel model;
    model.set(Json::array({row(1, 0, "Root", 0), row(2, 1, "Zoo", 2), row(3, 1, "alpha", 5)}));
    check(model.children(1, false) == std::vector<uint64_t>({2, 3}) &&
              model.children(1, true) == std::vector<uint64_t>({3, 2}),
          "Outline sort changed hierarchy/IDs");
    cyui::TextFilter filter;
    filter.query = "alpha";
    check(model.visible(filter) == std::set<uint64_t>({1, 3}), "Matched descendants lost ancestor context");
    filter.query = "missing";
    check(model.visible(filter).empty(), "Outline retained the previous filter cache");
    model.set(Json::array({row(7, 0, "missing", 1)}));
    check(model.visible(filter) == std::set<uint64_t>({7}) && model.children(1, false).empty(),
          "Replaced document retained old nodes/cache");
    filter.query.clear();
    check(model.visible(filter).size() == 1, "Empty outline query hid a node");
    LocationModel locations;
    const Json point={{"start",{{"line",0},{"character",0}}},{"end",{{"line",0},{"character",1}}}};
    locations.set(Json::array({{{"path","alpha.cpp"},{"range",point}},{{"path","beta.cpp"},{"range",point}}}));
    check(locations.matching(filter)==std::vector<size_t>({0,1}),"Empty location filter lost rows");
    check(locations.adjacent(filter,-1,false)==0 && locations.adjacent(filter,0,true)==1 &&
          locations.adjacent(filter,1,false)==0,"Location keyboard selection did not wrap");
    filter.query="beta";
    check(locations.matching(filter)==std::vector<size_t>({1}) && locations.adjacent(filter,0,false)==1,"Location filtering retained an obsolete selection");
    locations.set(Json::array({{{"path","gamma.cpp"},{"range",point}}}));
    check(locations.matching(filter).empty() && locations.adjacent(filter,-1,false)==-1,"Location replacement retained cached matches");
    filter.query.clear();
    check(locations.label(0)=="gamma.cpp:1:1" && locations.matching(filter)==std::vector<size_t>({0}),"Location display coordinates or cache reset failed");

}
