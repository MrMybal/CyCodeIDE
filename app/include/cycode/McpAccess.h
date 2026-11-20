#pragma once
#include <cymcp/http.hpp>
#include <array>

namespace cycode {
struct McpAccessOption { cymcp::LocalAccessProfile profile; const char* id; const char* label; };
inline constexpr std::array<McpAccessOption,4> mcpAccessOptions{{
    {cymcp::LocalAccessProfile::free,"free","Free"},
    {cymcp::LocalAccessProfile::local_controlled,"local-controlled","Local controlled"},
    {cymcp::LocalAccessProfile::token_only,"token-only","Token only"},
    {cymcp::LocalAccessProfile::protected_access,"protected","Protected"}
}};
inline const char* mcpAccessProfileId(cymcp::LocalAccessProfile profile) {
    for(const auto& option:mcpAccessOptions)if(option.profile==profile)return option.id;
    throw cymcp::Error("InvalidOptions","Unknown local MCP access profile");
}
inline cymcp::LocalAccessProfile mcpAccessProfile(const cymcp::Json& preferences) {
    if(!preferences.is_object())throw cymcp::Error("InvalidOptions","MCP preferences must be an object");
    for(const auto* key:{"localAccessProfile","access_profile"})if(preferences.contains(key)) {
        auto id=preferences.at(key).get<std::string>();
        if(id=="local_controlled")id="local-controlled";
        if(id=="token_only")id="token-only";
        if(id=="protected_access")id="protected";
        for(const auto& option:mcpAccessOptions)if(id==option.id)return option.profile;
        throw cymcp::Error("InvalidOptions","Unknown local MCP access profile");
    }
    // Preserve an explicit older transport/policy choice; an unset choice migrates to Free.
    if(preferences.contains("requireToken") || preferences.contains("controlOperations")) {
        const cymcp::LocalAccessSettings access{preferences.value("requireToken",true),preferences.value("controlOperations",true)};
        for(const auto& option:mcpAccessOptions)if(cymcp::local_access_settings(option.profile)==access)return option.profile;
    }
    return cymcp::LocalAccessProfile::free;
}
} // namespace cycode
