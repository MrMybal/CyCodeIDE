# CyCodeIDE version notes

[README](README.md) · [Français](VERSION.fr.md)

## Current source version

**CyCodeIDE 0.2.3 · IN DEVELOPMENT**

This is the IDE version declared by the root and application CMake projects,
independent of the linked Core runtime. The presentation was extracted from the
historical CyCode 0.2.3 desktop application. These notes describe current sources,
not a newly published binary, installer or release date.

| Component | Source selection / contract |
| --- | --- |
| CyCodeIDE | 0.2.3 development source |
| [CyCode Core](app/core-source.lock.json) | `5dc05f4bffe5d38e871250c9de23651c33d47221` · native/common source API 1 · `cycode-core/1` |
| [Reader editor](app/editor-source.lock.json) | `d27e84f1097b68c12630dc4c10fcc406d4628760` · graphical editor source API 1 |
| [CyUI](app/cyui-source.lock.json) | `6173630ad5eb1e585eee1a0e7d3617285563e774` |
| CyDoc reader | 0.1.0 · inherited from the pinned Core documentation lock |

Source API 1 is not a stable C++ binary ABI. The IDE, Core and Reader must share
a compatible dependency graph.

## Changes in this source snapshot

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

- Installer/update identity and distribution layouts need migration and validation
  before the first CyCodeIDE release. The historical updater is disabled; its old
  helper is neither built nor staged. IDE identity/version are independent of Core.
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
