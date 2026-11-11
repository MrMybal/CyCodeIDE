# CyCodeIDE version notes

[README](README.md) · [Français](VERSION.fr.md)

## Current source version

**CyCodeIDE 0.2.3 · Windows x64 · 2026-11-11**

This is the IDE version declared by the root and application CMake projects,
independent of the linked Core runtime. The presentation was extracted from the
historical CyCode 0.2.3 desktop application. This is the first separate IDE release.
[Release assets](https://github.com/MrMybal/CyCodeIDE/releases/tag/v0.2.3) · [Release notes](Docs/Releases/v0.2.3.md).
Production compilation and package checks pass; installation/restart remain unverified.

| Component | Source selection / contract |
| --- | --- |
| CyCodeIDE | 0.2.3 Windows x64 release |
| [CyCode Core](app/core-source.lock.json) | `8b8b7cefef47272871afe7774999f8b838ba1ab1` · native/common source API 1 · `cycode-core/1` |
| [Reader editor](app/editor-source.lock.json) | `889dd5fd8db8bfabdba4601f5d2a955723c91301` · graphical editor source API 1 |
| [CyUI](app/cyui-source.lock.json) | `0ba1ba663b4b2bf46d066a32829bc91ea2743cba` |
| CyDoc reader | 0.1.0 · inherited from the pinned Core documentation lock |

Source API 1 is not a stable C++ binary ABI. The IDE, Core and Reader must share
a compatible dependency graph.

## Changes in this source snapshot

- The analysis/inspection view exposes native batches, pause/resume, durable
  traversal checkpoints, historical/current evidence and actual clang-tidy profiles.
- Native change history and atomic commit receipts come from Core. Multi-file
  or annotated Undo/Redo is prepared/reviewed in Refactoring; single-file paged
  edits use that same native transaction owner.
- Shared paged editing uses asynchronous native pages, guarded UTF-16/byte mapping,
  whole-document publication and explicit checkpoint preview/restoration. Sessions
  retain the profile/path/caret, not an implicit unsaved paged backup.

- CyCodeIDE owns presentation; Core retains native services and guarded
  transactions; Reader supplies the shared graphical editor.
- Build time limits use the CyUI Inspector's bounded integer editing and
  commit/cancel behavior, with English/French validation messages.
- Diagnostic navigation follows manual caret movement. Filters distinguish no
  matches from no diagnostics and preserve selection identity.
- Code-action search retains Core action identities and revision checks.
  Search fields and popups keep their keyboard input; Preview retains its intent.
- CyUI supplies embedded JetBrains Mono 2.304, updated syntax colors and
  independent Ctrl + wheel zoom. Each editor's percentage button resets to 100%.
- `--build-dir` selects a separate production build directory. Default builds
  include the bilingual offline manual and never launch application/test binaries.
- Complete English/French READMEs follow the CyDoc convention, with version notes
  and dependency-license references.

## Documented limitations

- Ordinary GUI/LSP buffers retain their 4 MiB limit. Paged UTF-8 has no LSP overlay,
  encoding conversion, folding, multi-cursor or snippets. Save or close dirty paged
  buffers before compiler queries. Contextual syntax is unavailable above 32 MiB.
- Native history retains at most 256 transactions; document close/path rebinding
  invalidates affected entries. In particular, paged Save As resets history bound
  to the previous path. Explicit checkpoints are separate from Save and history.
- Cancelled typing and rejected proposals must be copied/recovered and acknowledged
  explicitly before editing/saving; they are never automatically replayed.

- Updates now use the CyCodeIDE repository/version and a dedicated helper linked
  to Core's immutable product profile. No legacy CyCode or Reader package is accepted.
  Installer execution, real online replacement/restart and clean-machine acceptance
  remain unverified. The release records these limits explicitly.
  See [application updates](Docs/Updates.md).
- Current sources have a successful Windows Release production build. Earlier
  native/view CI results cover specific revisions on Windows, Linux and macOS.
  They do not establish full desktop, multi-monitor or installer acceptance for
  this snapshot. Exact evidence and unexecuted checks remain in the
  [extraction guide](Docs/Extraction.md).
- Local native execution remains suspended during the existing antivirus
  investigation. GitHub Actions remain paused by the owner. Ordinary builds use
  `BUILD_TESTING=OFF`; recent native regression sources have not been run locally.
- Completion, diagnostics and code actions depend on configured language servers
  and supported Core capabilities. Syntax coloring alone does not provide semantic
  analysis. Read the [manual's limits](https://mrmybal.github.io/CyCode/Documentation.html#/en/limits).
- Public binary distribution still needs a complete transitive license/notice
  audit of selected language/debug/SDK packages. Preserve the
  [third-party notices](app/THIRD_PARTY_NOTICES.md).

## Bugs, feedback and release references

These notes do not maintain a bug-by-bug list for this development snapshot;
this does not mean it is free of bugs. Report IDE problems through the
[CyCodeIDE repository](https://github.com/MrMybal/CyCodeIDE), which currently
requires access. For shared engine problems, use
[CyCode Core's issue tracker](https://github.com/MrMybal/CyCode/issues).

[Historical CyCode releases](https://github.com/MrMybal/CyCode/releases) concern
the former application, not this extracted CyCodeIDE snapshot.
