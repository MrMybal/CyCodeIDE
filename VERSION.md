# CyCodeIDE version notes

[README](README.md) · [Français](VERSION.fr.md)

## CyCodeIDE 0.2.4 · Windows x64 · 2026-11-20

**In development.** This release adds the integrated assistant and native
automation to the existing IDE. [Downloads](https://github.com/MrMybal/CyCodeIDE/releases/tag/v0.2.4)
· [Release notes](Docs/Releases/v0.2.4.md) · [Previous release](Docs/Releases/v0.2.3.md).

## Changes

- **View → Assistant** opens the native chat: provider/model selection, progressive
  responses, cancellation, attachments and private conversation restoration.
- Chat, CLI and external MCP share the current window’s Core services, open
  documents, revisions and guarded transactions. Read access is the default;
  editing/build commands and editor preferences require separate grants.
- Eight IDE tools expose tabs, selection, supported panels and shared editor
  preferences. Dirty documents cannot be silently closed or discarded.
- CyMCPSDK replaces CyToolsCore and the Python MCP server. Existing CLI commands
  and explicit legacy-job migration remain available. See the
  [assistant and automation guide](Docs/AssistantMCP.md).
- The source snapshot also includes the recent project-tree, menu, search,
  shared zoom/minimap and breakpoint-gutter presentation improvements.

## Components

| Component | Version / source selection |
| --- | --- |
| CyCodeIDE | 0.2.4 |
| [CyCode Core](app/core-source.lock.json) | 0.1.5 · `5da07d2256b6035ee26425987e6238b647fe8334` · source API 1 |
| [Reader editor](app/editor-source.lock.json) | `889dd5fd8db8bfabdba4601f5d2a955723c91301` · graphical editor source API 1 |
| [CyUI](app/cyui-source.lock.json) | `6ef01cfcdb197f3a89d64618ce5ef8818c704728` |
| CyMCPSDK | 0.7.0 · `791d4d7d7680213b282ec8529d248a10f8494d77` |

Source API 1 is not a stable C++ binary ABI. Build these components together.

## Validation and documented limitations

Windows x64 Release compilation and ten Python staging/contract checks passed.
Isolated production CLI/MCP/chat/hidden-GUI acceptance passed with an explicitly
authorized local simulated HTTP provider: actual IDE tool calls, rights, unsaved
edits, explicit save, cancellation/restoration and complete chunked Unicode reads.
This does not validate real provider accounts or interactive attachment dialogs.

- Installer execution, clean-machine acceptance, real online replacement/restart,
  Linux/macOS and complete desktop interaction remain unverified. Binaries are unsigned.
- Language servers, compilers, VCS clients and an OpenGL driver remain external
  requirements. Syntax coloring alone does not establish semantic analysis.
- Ordinary GUI/LSP buffers retain their 4 MiB limit. Paged UTF-8 editing has no
  LSP overlay, encoding conversion, folding, multi-cursor or snippets; save or close
  dirty paged buffers before compiler queries. Contextual syntax is unavailable above 32 MiB.
- Native history retains up to 256 transactions. Closing/rebinding a document
  invalidates path-bound entries. Explicit paged checkpoints remain separate from Save.
- Third-party application MCP relays and supervised local inference are not
  included. Native provider filesystem/terminal permissions remain separate from IDE tools.
- GitHub Actions remain paused. Normal builds keep `BUILD_TESTING=OFF`; the
  authorized isolated production checks do not lift the general local execution restrictions.

Read the [update guide](Docs/Updates.md) and retain the
[third-party notices](app/THIRD_PARTY_NOTICES.md). No bug-by-bug list is maintained
for this snapshot; report issues through the [IDE tracker](https://github.com/MrMybal/CyCodeIDE/issues)
or the [Core tracker](https://github.com/MrMybal/CyCode/issues) for shared services.
