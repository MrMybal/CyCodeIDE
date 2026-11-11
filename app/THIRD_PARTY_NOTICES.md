# Third-party notices

CyCode's original source is licensed under [GNU GPL v3 only](../LICENSE)
(`GPL-3.0-only`), Copyright (c) 2026 Cyberalien. See [COPYRIGHT](../COPYRIGHT).
Dependencies retain their original licenses. Copies are in licenses/ or the
corresponding runtime package, as detailed below.

| Component | License | Source |
|---|---|---|
| PCRE2 10.49 | BSD-3-Clause WITH PCRE2-exception, [local notice](licenses/PCRE2.txt) | https://github.com/PCRE2Project/pcre2, commit 6f9d7c1373262c541324a16a358785b33ef116cf; app/dependencies.lock.json; static UTF-8 snippet matching, JIT/tools/tests disabled |
| utf8proc 2.12.0 | MIT and included Unicode data terms, [local notice](licenses/utf8proc.txt) | https://github.com/JuliaStrings/utf8proc, commit 79cdd5ab40c79afa559f689ffc13b76812dee1ac; app/dependencies.lock.json; Unicode snippet casing |
| Monocypher 4.0.3 | BSD-2-Clause or CC0-1.0, [local notice](licenses/Monocypher.txt) | https://monocypher.org ; release archive and SHA-256 pinned in app/dependencies.lock.json; Ed25519 and SHA-512 for plugin packages |
| TinyXML2 | zlib, [local notice](licenses/TinyXML2.txt) | https://github.com/leethomason/tinyxml2, commit 9148bdf719e997d1f474be6bcc7943881046dba1; app/dependencies.lock.json |
| CyUI | [MIT license](https://github.com/MrMybal/CyUI/blob/ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3/LICENSE) ([local copy](licenses/CyUI.txt)) | [Official repository](https://github.com/MrMybal/CyUI), pinned by app/cyui-source.lock.json |
| JetBrains Mono Regular 2.304 | SIL OFL 1.1, [local notice](resources/fonts/JetBrainsMono-OFL.txt) | https://github.com/JetBrains/JetBrainsMono ; original font bytes embedded by CyUI and retained in app/resources/fonts |
| Dear ImGui | MIT | https://github.com/ocornut/imgui |
| TextEditor | MIT | CyUI snapshot of https://github.com/goossens/ImGuiColorTextEdit |
| stb_image | MIT or Unlicense | https://github.com/nothings/stb (app/dependencies.lock.json) |
| libwebp decoder | BSD-style and additional patent grant | https://github.com/webmproject/libwebp (app/dependencies.lock.json) |
| Lexilla / Scintilla | Included permissive notices | https://github.com/ScintillaOrg/lexilla ; https://github.com/ScintillaOrg/scintilla |
| SciTE language properties | Included permissive notice | CyUI third_party/SciTE-Properties-License.txt |
| Tree-sitter and Rust/Java/JS/TS/HTML/CSS grammars | MIT | https://github.com/tree-sitter |
| TypeScript | Apache-2.0 | https://github.com/microsoft/TypeScript |
| typescript-language-server | MIT | https://github.com/typescript-language-server/typescript-language-server |
| VS Code language servers (extracted) | MIT | https://github.com/hrsh7th/vscode-langservers-extracted |
| Rust toolchain, rustfmt and rust-analyzer | MIT / Apache-2.0; included component terms | https://github.com/rust-lang |
| Eclipse JDT LS | EPL-2.0 and included component terms | https://github.com/eclipse-jdtls/eclipse.jdt.ls |
| SDL3 | zlib | https://github.com/libsdl-org/SDL |
| nlohmann/json | MIT | https://github.com/nlohmann/json |
| CyToolsCore | MIT | Owner's CyToolsCore wheel/check-out; public repository not configured |
| MCP Python SDK | MIT | https://github.com/modelcontextprotocol/python-sdk |
| ripgrep | MIT / Unlicense | https://github.com/BurntSushi/ripgrep |
| libvterm | MIT | https://www.leonerd.org.uk/code/libvterm/ |
| CodeLLDB | MIT | https://github.com/vadimcn/codelldb |
| NetCoreDbg | MIT | https://github.com/Samsung/netcoredbg |
| csharp-ls | MIT | https://github.com/razzmatazz/csharp-language-server |
| Private Microsoft .NET SDK/runtime | Included Microsoft terms and component licenses | https://github.com/dotnet/sdk |
| Bundled LLDB/LLVM | Apache-2.0 with LLVM exceptions | https://github.com/llvm/llvm-project |
| CodeLLDB's embedded Python | PSF and included third-party terms | https://github.com/python/cpython |

The IDE-only libvterm lock is dependencies.lock.json. Shared editor dependency
revisions/hashes and widget patches now live in the CyCodeReader repository
selected by editor-source.lock.json.
TextEditor retains its MIT notice. CyCodeReader/editor/scripts/prepare_texteditor.py creates a build-local
copy with CyCode's lexical color roles/context callback, versioned semantic display
overlays and read-only token queries;
the original downloaded source is preserved.
The shared syntax parser's seven upstream MIT notices are copied to
licenses/tree-sitter*.txt. Language tool archives are pinned in
language-tools.lock.json and web npm package integrities in
web-languages/package-lock.json. Their runtime archives/package trees preserve
their upstream license files and transitive components. Node.js and Java are
external prerequisites, not redistributed runtimes. Public binary distribution
still requires a complete transitive license/notice audit of these packages.
The private Python environment also includes the dependencies declared by the real
CyToolsCore/MCP wheels. Their installed dist-info metadata and license files are
preserved by pip. They are not replaced by CyCode's project license.
The private environment and SDK wheel are not committed to this repository.

CyUI's repository is currently private; its GitHub license link requires repository
access. The MIT license is also included locally so reading it does not require
GitHub access. CyUI retains its MIT license within the GPL-licensed application.

The Windows 0.2.0 archive contains the IDE/CLI, documentation, notices and the
locked ripgrep executable under tools/, with its three upstream notices. It does
not redistribute the development runtime, private SDK/Python environment, language
servers, debug adapters, fonts or dependency source trees. The Microsoft Visual
C++ 2022 x64 runtime and an OpenGL driver are external prerequisites.

The build installs a checksum-pinned official ripgrep binary in runtime/tools.
Its exact version, archives and hashes are in runtime-tools.lock.json. The original
COPYING, LICENSE-MIT and UNLICENSE accompany it in runtime/tools/licenses/ripgrep.
Git, CMake, clangd and the C++ runtime remain external development prerequisites.
Segoe UI/Consolas/MDL2 are loaded from Windows when available and are not redistributed.
Other OSes currently use the library's font and icon fallbacks.

The C# server package is pinned in language-tools.lock.json; its own license is
copied to licenses/csharp-ls.txt. Its managed tool package, including Roslyn,
MSBuild, Ionide LSP, FSharp and ILSpy assemblies, is preserved under
runtime/tools/csharp-ls. Upstream dependency licenses remain applicable; a complete
transitive notice audit and distributable runtime package remain prerequisites for
public binary distribution. The private SDK archive's LICENSE.txt and
ThirdPartyNotices.txt are preserved in runtime/tools/csharp-dotnet and copied to
licenses/Dotnet-SDK.txt and licenses/Dotnet-ThirdPartyNotices.txt. Project-specific
SDKs/targeting packs remain development prerequisites.

The optional managed debug adapter is the official NetCoreDbg 3.2.0-1092 release,
pinned with SHA-256 hashes in debugger-tools.lock.json. Its MIT license is copied
to licenses/NetCoreDbg.txt; upstream package contents and component notices remain
in runtime/tools/netcoredbg. No Microsoft Visual Studio debugger is redistributed.

The selected Code module includes Lexilla/Scintilla and SciTE language properties.
Their notices are preserved in app/licenses/Lexilla.txt, Scintilla.txt and
SciTE-Properties.txt. CyUI and its bundled TextEditor notices are copied to
app/licenses/CyUI.txt and TextEditor.txt. The common syntax implementation and
its upstream snapshots remain in the pinned dependency, with no second copy here.

Image previews include the pinned stb_image decoder (MIT or Unlicense, notice
in licenses/stb.txt) and the decoder-only libwebp target (notices in
licenses/libwebp-COPYING.txt, libwebp-PATENTS.txt and libwebp-AUTHORS.txt).
The source archives remain under runtime/deps; they are not copied into app.

Optional Python analysis uses Microsoft Pyright 1.1.411 (MIT), pinned in
language-tools.lock.json. Its notice and typeshed fallback notice are retained in
licenses/Pyright.txt and licenses/Typeshed.txt; the complete upstream notices/stub
metadata remain in the private package. Node.js is an installed prerequisite.
Optional GLSL analysis uses nolanderc/glsl_analyzer 1.7.1 under GNU GPL v3, with
the original text in licenses/GLSL-Analyzer.txt and corresponding source at
https://github.com/nolanderc/glsl_analyzer/tree/v1.7.1. Language-server archives are
development/runtime prerequisites and are not added to the application ZIP.


Local help uses [CyDoc](https://github.com/MrMybal/CyDoc), MIT, pinned by
`app/.CyDocBuild/cydoc.lock.json`. The exported help includes its MIT notice
and the Marked, PrismJS and esbuild notices in `Documentation/DocData/licenses/`.
Only image/gallery runtime modules are distributed; no Three.js runtime is included.


## Microsoft Visual C++ runtime

Windows packages include app-local Microsoft Visual C++ runtime DLLs from the
Visual Studio toolchain. These Microsoft redistributables retain their separate
Microsoft terms and are not covered by CyCode’s GPL license. The update helper
stages verified copies of the same runtime.

## Portable interface resources

Roboto (Google, Apache 2.0) and JetBrains Mono (JetBrains, SIL OFL 1.1) are
distributed as offline fallback fonts in `app/resources/fonts`, with the original
license texts. Exact upstream revisions and file hashes are recorded there in
`font-resources.json`.

CyCode Controls is adapted from Microsoft Codicons 0.0.36 (Microsoft Corporation,
CC BY 4.0, https://github.com/microsoft/vscode-codicons). Its glyphs are subset,
mapped to CyUI's public icon codepoints and the family renamed. Codicons' original
license is retained in `app/resources/fonts/Codicons-LICENSE.txt`. This resource
does not redistribute Microsoft's Windows Segoe fonts. CyUI itself is unchanged.

## Bundled MCP environment (new Windows packages)

The official CPython 3.13.17 embedded distribution is pinned from python.org in
mcp-runtime.lock.json. Its complete LICENSE.txt ships in tools/python, covering
Python/PSF and the original bundled component notices. No system Python is copied.
CyToolsCore 0.10.1 is MIT at published commit
05352e1d2f58f423fa2ad57b8eec48d2609f2626. Its 53 package files match that source;
source checkout line endings are normalized. Original SDK/dependency license files
remain in Lib/site-packages/<distribution>.dist-info/licenses or the upstream
license paths. Generated RECORD files describe the actual installed locations.

The exact wheel hashes/URLs, source pin and installed-payload hash are recorded
in app/mcp-runtime.lock.json and tools/python/cycode-runtime.json. Only dependency
CLI entrypoint scripts and pywin32's unrelated GUI/service launchers, plus the
upstream SDK manual, are omitted; the application's own CyDoc help remains bundled.
Authoring setuptools/wheel and downloaded wheel archives are not distributed.
The separate licenses below remain applicable; CyCode's GPL does not replace them.

| Distribution | Version | Upstream declared license |
|---|---|---|
| annotated-types | 0.8.0 | MIT |
| anyio | 4.16.0 | MIT |
| attrs | 26.1.0 | MIT |
| certifi | 2026.10.23 | MPL-2.0 |
| cffi | 2.1.1 | MIT-0 |
| click | 8.5.1 | BSD-3-Clause |
| cryptography | 50.0.2 | Apache-2.0 OR BSD-3-Clause |
| cytools-core | 0.10.1 | MIT |
| h11 | 0.16.0 | MIT |
| httpcore | 1.0.9 | BSD-3-Clause |
| httpx | 0.28.1 | BSD-3-Clause |
| httpx-sse | 0.4.3 | MIT |
| idna | 3.20 | BSD-3-Clause |
| jsonschema | 4.26.0 | MIT |
| jsonschema-specifications | 2025.9.1 | MIT |
| mcp | 1.30.0 | MIT |
| psutil | 7.2.2 | BSD-3-Clause |
| pycparser | 3.12 | BSD-3-Clause |
| pydantic | 2.14.3 | MIT |
| pydantic_core | 2.50.4 | MIT |
| pydantic-settings | 2.16.2 | MIT |
| PyJWT | 2.15.1 | MIT |
| python-dotenv | 1.2.4 | BSD-3-Clause |
| python-multipart | 0.0.33 | Apache-2.0 |
| pywin32 | 312 | PSF |
| referencing | 0.37.0 | MIT |
| rpds-py | 2026.9.1 | MIT |
| sse-starlette | 3.7.0 | BSD-3-Clause |
| starlette | 1.10.0 | BSD-3-Clause |
| typing_extensions | 4.16.0 | PSF-2.0 |
| typing-inspection | 0.4.5 | MIT |
| uvicorn | 0.54.0 | BSD-3-Clause |
