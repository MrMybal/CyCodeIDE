#pragma once
#include <cycode/Automation.h>
#include <cymcp/http.hpp>

namespace cycode {
using Json=cymcp::Json;
// Presentation adapter: all business calls still belong to Core's automation host.
// UI handlers are drained exclusively by Application::frame().
class IdeAutomation {
    struct Impl;
    std::unique_ptr<Impl> impl_;
  public:
    IdeAutomation(const std::filesystem::path& nativeDescriptor,
                  const std::filesystem::path& catalogue,
                  const std::filesystem::path& state,
                  const std::filesystem::path& descriptor, const Json& options);
    ~IdeAutomation();
    void stop();
    IdeAutomation(const IdeAutomation&) = delete;
    void drain(const std::function<Json(const std::string&, const Json&)>&);
    void permissions(bool edit, bool settings);
    std::shared_ptr<cymcp::Registry> registry() const;
    std::shared_ptr<cymcp::ResourceRegistry> resources() const;
    Json descriptor() const;
    const std::filesystem::path& historyDirectory() const;
};
} // namespace cycode
