#include "cycode/UiFonts.h"
#include "cycode/Paths.h"
#include <iterator>
#include <cyui/Core.h>
#include <imgui_internal.h>
#include <cstring>
#include <fstream>
namespace cycode {
bool applicationIconAvailable(const char *glyph) {
    unsigned int codepoint=0;
    if (!glyph || !ImGui::GetCurrentContext()) return false;
    ImTextCharFromUtf8(&codepoint,glyph,nullptr);
    return codepoint && codepoint<=IM_UNICODE_CODEPOINT_MAX &&
           ImGui::GetFont()->IsGlyphInFont(static_cast<ImWchar>(codepoint));
}
nlohmann::json loadApplicationFonts(const std::filesystem::path &installation) {
    const auto directory=installation/"app/resources/fonts";
    const auto folder=pathUtf8(directory);
    cyui::FontConfig configuration;
    configuration.fallbackFolder=folder.c_str();
    cyui::loadFonts(configuration);
    auto &atlas=*ImGui::GetIO().Fonts;
    bool merged=false;
    const auto iconFile=directory/"CyCodeControls-Regular.ttf";
    std::error_code error;
    const auto bytes=std::filesystem::file_size(iconFile,error);
    // The mapped font contains only the public control glyphs, so it cannot
    // replace source text glyphs. Atlas owns one buffer per merged font source.
    if (!cyui::iconsAvailable() && !error && bytes>0 && bytes<=256*1024) {
        std::ifstream input(iconFile,std::ios::binary);
        const std::string data((std::istreambuf_iterator<char>(input)),{});
        if (!input.bad() && data.size()==bytes) {
            merged=true;
            for (auto *font : {cyui::uiFont(),cyui::boldFont(),cyui::monoFont()}) {
                if (!font) {merged=false;continue;}
                auto *buffer=IM_ALLOC(data.size());
                if (!buffer) {merged=false;continue;}
                std::memcpy(buffer,data.data(),data.size());
                ImFontConfig icons;
                icons.MergeMode=true;icons.DstFont=font;icons.PixelSnapH=true;
                icons.GlyphMinAdvanceX=cyui::metrics().fontSize;
                const auto *added=atlas.AddFontFromMemoryTTF(buffer,int(data.size()),
                    cyui::metrics().fontSize,&icons);
                if (!added) { IM_FREE(buffer);merged=false; }
            }
        }
    }
    const auto name=[](ImFont *font){return font?std::string(font->GetDebugName()):std::string{};};
    const auto available=[](ImFont *font){return font && font->IsGlyphInFont(0xe768) &&
        font->IsGlyphInFont(0xe8b7) && font->IsGlyphInFont(0xe74e);};
    return {{"ui",name(cyui::uiFont())},{"bold",name(cyui::boldFont())},
        {"mono",name(cyui::monoFont())},{"portableIconsMerged",merged},
        {"icons",available(cyui::uiFont()) && available(cyui::boldFont())},
        {"systemIcons",cyui::iconsAvailable()}};
}
}
