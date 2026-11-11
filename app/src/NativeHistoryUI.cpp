#include "cycode/Application.h"
#include <algorithm>

namespace cycode {
void Application::prepareNativeHistory(bool redo) {
    if(refactorFuture_.valid())return;
    const auto direction=redo?"redo":"undo";
    if(!nativeHistory_.contains(direction) || !nativeHistory_.at(direction).is_object())return;
    const auto head=nativeHistory_.at(direction),version=nativeHistory_.at("revision");
    auto owner=engine_;refactorOperation_="changes.historyPrepare";
    refactorFuture_=std::async(std::launch::async,[owner,head,version,redo] {
        const auto live=owner->dispatch("documents.list").at("documents");Json guards=Json::array();
        for(const auto &path:head.at("files")) {
            const auto found=std::find_if(live.begin(),live.end(),[&](const auto &row){return row.at("path")==path;});
            if(found==live.end())throw std::runtime_error("Native history document was closed; refresh history");
            guards.push_back({{"path",path},{"expectedDocumentId",found->at("id")},{"expectedRevision",found->at("revision")}});
        }
        return owner->dispatch("changes.historyPrepare",{{"id",head.at("id")},{"direction",redo?"redo":"undo"},
            {"expectedHistoryRevision",version},{"documents",std::move(guards)}});
    });
}
void Application::nativeHistoryControls() {
    if(!cyui::beginSection(tr("Native change history")))return;
    cyui::textDim("%s",tr("Native transactions are shared with attached agents. Prepare and review the complete change before applying."));
    {cyui::ScopedDisabled disabled(refactorFuture_.valid());
      if(cyui::button(tr("Refresh native history")))changeTask("changes.historyStatus",Json::object());}
    cyui::textDim("%s: %llu | %s: %llu",tr("Undo"),static_cast<unsigned long long>(nativeHistory_.value("undoCount",uint64_t(0))),
        tr("Redo"),static_cast<unsigned long long>(nativeHistory_.value("redoCount",uint64_t(0))));
    for(const auto *direction:{"undo","redo"}) {
        const auto head=nativeHistory_.find(direction);
        const bool available=head!=nativeHistory_.end() && head->is_object();
        {cyui::ScopedDisabled disabled(refactorFuture_.valid() || !available);
          if(cyui::button(tr(direction==std::string("undo")?"Prepare native Undo":"Prepare native Redo")))prepareNativeHistory(direction==std::string("redo"));}
        if(available){ImGui::SameLine();ImGui::TextUnformatted(head->value("title","").c_str());}
    }
    if(nativeHistory_.value("dropped",uint64_t(0)))cyui::textDim("%s: %llu",tr("Discarded native history entries"),static_cast<unsigned long long>(nativeHistory_.at("dropped").get<uint64_t>()));
    cyui::endSection();
}
}
