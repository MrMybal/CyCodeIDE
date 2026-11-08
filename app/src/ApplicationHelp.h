#pragma once
#include "cycode/Paths.h"
#include "HelpTopics.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#ifdef CYCODE_LOCAL_HELP
#include <cydoc_link.hpp>
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <shlwapi.h>
// clang-format on
#endif
#endif

/**
 * @doc help-routing
 * @locale en
 * @title Native help routing
 * @section API
 * @symbol app_help::open
 * ## Ownership {#ownership}
 * The GUI opens the exported CyDoc manual in the system browser only after an
 * explicit help action. The suppressed flag blocks automated/test sessions before
 * filesystem access. Documents, SDK state and editor Undo are not changed.
 * ## Addresses {#addresses}
 * Page, section and media identities are stable across English and French. Native
 * path conversion and the pinned CyDoc route helper keep spaces, Unicode and
 * reserved URL characters separate from the route.
 */
/**
 * @doc help-routing
 * @locale fr
 * @title Routage natif de l’aide
 * @section API
 * @symbol app_help::open
 * ## Responsabilités {#ownership}
 * L’interface ouvre le lecteur CyDoc dans le navigateur système uniquement après
 * une action explicite. Le paramètre suppressed bloque les sessions automatisées
 * avant tout accès aux fichiers. Documents, SDK et historique Undo restent inchangés.
 * ## Adresses {#addresses}
 * Les identifiants de page, section et média sont communs aux deux langues. Le
 * chemin local est converti en URL avant l’ajout de la route par CyDoc.
 */
namespace app_help {
inline bool identifier(std::string_view value) {
    return !value.empty() && value.size() <= 128 &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
           });
}
inline void open(const std::filesystem::path &installation, std::string_view installedDirectory,
                 std::string_view locale, std::string_view page, std::string_view anchor,
                 bool suppressed, std::string_view media = {}) {
    if (suppressed) return;
#ifdef CYCODE_LOCAL_HELP
    if (!identifier(page) || (!anchor.empty() && !identifier(anchor)) ||
        (!media.empty() && !identifier(media)))
        throw std::runtime_error("Invalid local documentation address");
    std::vector<std::filesystem::path> candidates = {
        installation / cycode::utf8Path(std::string(installedDirectory)) / "Documentation.html"};
    if (const auto base = SDL_GetBasePath())
        candidates.push_back(cycode::utf8Path(base) / cycode::utf8Path(std::string(installedDirectory)) / "Documentation.html");
    std::filesystem::path file;
    for (const auto &candidate : candidates)
        if (std::filesystem::is_regular_file(candidate)) { file = std::filesystem::absolute(candidate).lexically_normal(); break; }
    if (file.empty()) throw std::runtime_error("Local documentation is missing. Rebuild the documentation or reinstall the complete application package.");
    std::string base;
#ifdef _WIN32
    const auto path = file.wstring();
    if (path.size() >= 2084 || path.starts_with(L"\\\\?\\"))
        throw std::runtime_error("The Windows help URL converter requires a non-extended path shorter than 2084 characters.");
    std::wstring buffer(path.size() * 4 + 64, 0);
    DWORD size = static_cast<DWORD>(buffer.size());
    if (FAILED(UrlCreateFromPathW(path.c_str(), buffer.data(), &size, 0)))
        throw std::runtime_error("Cannot convert the local documentation path to a file URL");
    buffer.resize(size);
    while (!buffer.empty() && buffer.back() == L'\0') buffer.pop_back();
    const auto bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buffer.data(), static_cast<int>(buffer.size()), nullptr, 0, nullptr, nullptr);
    if (!bytes) throw std::runtime_error("Invalid documentation URL encoding");
    base.resize(bytes);
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buffer.data(), static_cast<int>(buffer.size()), base.data(), bytes, nullptr, nullptr))
        throw std::runtime_error("Cannot encode the local documentation URL");
#else
    // std::filesystem supplies absolute native path components; CyDoc encodes
    // each component, preserving only structural separators in the file URI.
    base = "file://";
    for (const auto &component : file) {
        if (component == file.root_path()) { base += '/'; continue; }
        if (!base.ends_with('/')) base += '/';
        base += cydoc_native::encode(cycode::pathUtf8(component));
    }
#endif
    std::vector<std::pair<std::string, std::string>> state;
    if (!media.empty()) state.push_back({"media", std::string(media)});
    const auto url = cydoc_native::link(base, locale == "fr" ? "fr" : "en", page, anchor, state);
    if (!SDL_OpenURL(url.c_str())) throw std::runtime_error(SDL_GetError());
#else
    (void)installation; (void)installedDirectory; (void)locale; (void)page; (void)anchor; (void)media;
    throw std::runtime_error("Local documentation was disabled in this build");
#endif
}
} // namespace app_help
