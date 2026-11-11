<table width="100%">
  <tr>
    <td width="300" align="left" valign="top">
      <a href="https://mrmybal.github.io/CyCode/Documentation.html#/en/overview">Manual</a><br>
      <a href="https://github.com/MrMybal/CyCodeIDE">Repository</a><br>
      <a href="VERSION.md">Version notes</a><br><br>
      <sub>README</sub><br>
      <a href="README.md">English</a> · <a href="README.fr.md">Français</a>
    </td>
    <td width="400" align="center" valign="middle">
      <img src="icons/CyCode.png" alt="CyCodeIDE" width="128">
    </td>
    <td width="300" align="right" valign="top">
      <p><sub>Application license</sub><br><a href="LICENSE"><strong>GPL-3.0-only</strong></a></p>
      <p><sub>IDE version</sub><br><a href="VERSION.md"><strong>0.2.3</strong></a><br><strong>IN DEVELOPMENT</strong></p>
    </td>
  </tr>
</table>

<h1 align="center">CyCodeIDE</h1>

A native C++20 IDE for code projects, using
[CyCode Core](https://github.com/MrMybal/CyCode) and the shared
[CyCodeReader editor](https://github.com/MrMybal/CyCodeReader).

> **IN DEVELOPMENT · CyCodeIDE 0.2.3**
>
> This source snapshot was extracted from the CyCode 0.2.3 desktop application.
> It is not a new installer or application release. Read the
> [version notes and documented limitations](VERSION.md).

## About CyCodeIDE

Edit project files, inspect diagnostics, navigate symbols and review code actions
in one desktop interface. Language analysis, completion and transformations use
Core's backends and depend on the configured server and project. The
[manual](https://mrmybal.github.io/CyCode/Documentation.html#/en/overview)
describes setup and available workflows.

CyCodeIDE owns windows, project panels, themes, keymaps and their presentation.
Core owns documents, compiler adapters, persistent language backends, semantic
indexes, diagnostics, plugins and guarded transactions. Reader supplies graphical
editor source API 1. They share native buffer, codec, process and protocol
components, with one Core and one compatible ImGui/editor instance per build.
Source API 1 does not promise a stable C++ binary ABI.

## Features

- **Code editing:** shared Reader editor with Undo/Redo, CyUI syntax colors and
  embedded JetBrains Mono. Ctrl + mouse wheel zooms the hovered code view
  independently; the percentage button above an editor resets it to 100%.
  Previews and comparisons share the code font and support their own wheel zoom.
- **Diagnostics and code actions:** filter problems, navigate file locations,
  search actions and review previews before applying guarded Core changes.
- **Project tools:** build, debugging and version-control panels use Core services
  and the tools configured for the project.
- **Workspace:** docked panels, multiple windows, themes and configurable keymaps.
- **English and French:** English is the default interface language; French is
  selectable. The bilingual CyDoc manual also works offline.

---

## Installation

This development snapshot is built from source. Installer/update identity and
distribution layouts still need migration and validation before a CyCodeIDE
release. Historical CyCode downloads predate this separation.

Source builds target Windows, Linux and macOS. See the [version notes](VERSION.md)
for validation limits; a build target does not establish full desktop or
installer acceptance. The graphical application needs an OpenGL driver.

### Read the manual

Use the [English manual](https://mrmybal.github.io/CyCode/Documentation.html#/en/overview)
or [French manual](https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview)
online. Standard builds generate the genuine offline CyDoc reader from the pinned
Core sources and stage it beside the executable.

Open `app/Documentation/Documentation.html#/en/overview` (or `#/fr/overview`)
from the executable's directory, or use the application's documentation command.
Keep the entire `app/Documentation/` folder, including `DocData/`, together.
The exported reader needs no server, account or Internet connection.

## Source, build and required libraries

### Requirements

- **CMake 3.24+**, **Python 3.11+**, **Git** and a **C++20** toolchain.
- **Windows:** Visual Studio 2022 C++ x64 tools.
- **Linux/macOS:** a suitable C++ toolchain, Ninja and platform OpenGL development libraries.
- **Node.js 22+ and npm** for the default documentation generation.
- Repository/package access for the initial dependency fetch.

### Get the source and build

```sh
git clone https://github.com/MrMybal/CyCodeIDE.git
cd CyCodeIDE
python app/scripts/build.py --fetch
```

CyCodeIDE currently requires repository access. Core and Reader are public.
Available Git authentication is used when needed; credentials do not belong in
source files or command examples.

The script builds production targets with `BUILD_TESTING=OFF` and never starts
the application or native tests. Default output:
`runtime/build/app/Release/CyCodeIDE.exe` on Windows, or
`runtime/build/app/CyCodeIDE` with a single-configuration generator.
The internal executable target is still named `CyCode`.

<details>
<summary>Separate build directories and development dependencies</summary>

Keep another build configuration without replacing an existing one:

```sh
python app/scripts/build.py --build-dir runtime/build-dev --fetch
```

Use explicit development checkouts when coordinating Core and Reader changes:

```sh
python app/scripts/build.py --core-root <CyCode> --editor-root <CyCodeReader> --fetch
```

Exact revisions are pinned in [Core](app/core-source.lock.json),
[Reader](app/editor-source.lock.json) and [CyUI](app/cyui-source.lock.json) locks.
Native/editor dependency locks remain upstream; [image dependencies](app/dependencies.lock.json)
are pinned here.

The offline manual is generated by default. `--without-documentation` is an
explicit compile-only option. Staging reuses Core's canonical facade, manifest
and CLI without creating another operation registry. It excludes local
workspaces, user settings and downloaded task data.

</details>

---

## License

**CyCodeIDE uses [GNU GPL v3 only](LICENSE) (`GPL-3.0-only`).**
See [COPYRIGHT](COPYRIGHT). Dependencies retain their own licenses and notices.

### Native components and interface

| Library · source | License · terms | Use / scope |
| --- | --- | --- |
| [CyCode Core](https://github.com/MrMybal/CyCode) | [GPL-3.0-only](https://github.com/MrMybal/CyCode/blob/5dc05f4bffe5d38e871250c9de23651c33d47221/LICENSE) | Shared native project services, documents and CLI. |
| [CyCodeReader editor](https://github.com/MrMybal/CyCodeReader) | [GPL-3.0-only](https://github.com/MrMybal/CyCodeReader/blob/d27e84f1097b68c12630dc4c10fcc406d4628760/LICENSE) | Shared graphical editor source API 1. |
| [CyUI](https://github.com/MrMybal/CyUI) | [MIT](https://github.com/MrMybal/CyUI/blob/6173630ad5eb1e585eee1a0e7d3617285563e774/LICENSE) | Selected interface, code, comparison and panel modules. |
| [Dear ImGui](https://github.com/ocornut/imgui) | [MIT](https://github.com/ocornut/imgui/blob/d682dc41f58b98c9343ffbaa7d94f07e9024b3ca/LICENSE.txt) | Shared native UI and SDL/OpenGL backends. |
| [SDL3](https://github.com/libsdl-org/SDL) | [zlib](https://github.com/libsdl-org/SDL/blob/release-3.4.18/LICENSE.txt) | Native windows, input and platform integration. |
| [ImGuiColorTextEdit](https://github.com/goossens/ImGuiColorTextEdit) | [MIT](https://github.com/MrMybal/CyUI/blob/6173630ad5eb1e585eee1a0e7d3617285563e774/third_party/texteditor/LICENSE) | CyUI editor backend; Reader adapts build-local copies. |
| [Lexilla / Scintilla](https://github.com/ScintillaOrg/lexilla) and [SciTE properties](https://github.com/mirror/scite) | [Permissive component notices](https://github.com/MrMybal/CyUI/blob/6173630ad5eb1e585eee1a0e7d3617285563e774/THIRD_PARTY_NOTICES.md) | Lexical engines, public headers and keyword data; no Scintilla editor UI. |
| [Tree-sitter and grammars](https://github.com/tree-sitter) | [MIT; component notices](app/THIRD_PARTY_NOTICES.md) | Shared editor syntax parsing. |
| [stb_image](https://github.com/nothings/stb) | [MIT or Unlicense](app/THIRD_PARTY_NOTICES.md) | Image decoding. |
| [libwebp](https://github.com/webmproject/libwebp) | [BSD-style and patent grant](app/THIRD_PARTY_NOTICES.md) | Decoder-only image support. |
| [PCRE2](https://github.com/PCRE2Project/pcre2) | [BSD-3-Clause with PCRE2 exception](app/THIRD_PARTY_NOTICES.md) | Core UTF-8 snippet matching. |
| [utf8proc](https://github.com/JuliaStrings/utf8proc) | [MIT and Unicode data terms](app/THIRD_PARTY_NOTICES.md) | Core Unicode snippet casing. |
| [Monocypher](https://monocypher.org) | [BSD-2-Clause or CC0-1.0](app/THIRD_PARTY_NOTICES.md) | Core package signature verification. |
| [TinyXML2](https://github.com/leethomason/tinyxml2) | [zlib](app/THIRD_PARTY_NOTICES.md) | Core XML project data. |
| [libvterm](https://www.leonerd.org.uk/code/libvterm/) | [MIT](app/THIRD_PARTY_NOTICES.md) | Native terminal emulation. |
| [nlohmann/json](https://github.com/nlohmann/json) | [MIT](app/THIRD_PARTY_NOTICES.md) | Native JSON data and protocols. |
| [JetBrains Mono 2.304](https://github.com/JetBrains/JetBrainsMono) | [SIL OFL 1.1](app/resources/fonts/JetBrainsMono-OFL.txt) | Embedded code font and offline fallback. |
| [Roboto](https://github.com/googlefonts/roboto-2) | [Apache-2.0](app/resources/fonts/Roboto-LICENSE.txt) | Offline interface font fallback. |
| [Microsoft Codicons 0.0.36](https://github.com/microsoft/vscode-codicons) | [CC BY 4.0](app/resources/fonts/Codicons-LICENSE.txt) | Adapted, renamed CyCode Controls icon font. |

### Documentation and optional tools

| Library · source | License · terms | Use / scope |
| --- | --- | --- |
| [CyDoc 0.1.0](https://github.com/MrMybal/CyDoc) | [MIT](https://github.com/MrMybal/CyDoc/blob/6ddf9a582defb2f11fbb3f992afef6f98dcadc93/LICENSE) | Distributed offline reader with image/gallery modules. |
| [marked 17.0.6](https://github.com/markedjs/marked) | [MIT](https://github.com/markedjs/marked/blob/v17.0.6/LICENSE.md) | Build-time Markdown compilation. |
| [PrismJS 1.30.0](https://github.com/PrismJS/prism) | [MIT](https://github.com/PrismJS/prism/blob/v1.30.0/LICENSE) | Build-time documentation syntax highlighting. |
| [esbuild 0.25.12](https://github.com/evanw/esbuild) | [MIT](https://github.com/evanw/esbuild/blob/v0.25.12/LICENSE.md) | Build-time documentation bundling. |

Language servers, compilers, debug adapters and optional SDK/MCP environments
retain their own component terms. They are separate prerequisites or selected
runtime packages, rather than universally bundled features. See the
[third-party notices](app/THIRD_PARTY_NOTICES.md) for their scope.

These tables index the main components and do not replace complete transitive
notices. Keep distributed license texts, font notices and
`Documentation/DocData/licenses/` with their components.

## References and links

- [English manual](https://mrmybal.github.io/CyCode/Documentation.html#/en/overview) · [Manuel en français](https://mrmybal.github.io/CyCode/Documentation.html#/fr/overview).
- [Source repository](https://github.com/MrMybal/CyCodeIDE) · [Version notes and limitations](VERSION.md).
- [CyCode Core](https://github.com/MrMybal/CyCode) · [CyCodeReader](https://github.com/MrMybal/CyCodeReader) · [CyUI](https://github.com/MrMybal/CyUI).

<details>
<summary><strong>AI Usage</strong></summary>

- [Start here: repository instructions](AGENTS.md) — scope, important guides and development restrictions.
- [Implement the interface](app/AGENTS.md) — CyUI presentation and shared service ownership.
- [Integrate Core](https://github.com/MrMybal/CyCode/blob/5dc05f4bffe5d38e871250c9de23651c33d47221/app/Docs/CoreExtraction.md) — pinned native architecture and source contracts.

</details>
