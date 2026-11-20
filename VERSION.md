# CyCodeIDE version notes

[README](README.md) · [Français](VERSION.fr.md)

## CyCodeIDE 0.2.5 · Windows x64 · 2026-11-20

**In development.** Local MCP now defaults to Free, with direct URL connections
and preserved explicit access choices. [Downloads](https://github.com/MrMybal/CyCodeIDE/releases/tag/v0.2.5)
· [Release notes](Docs/Releases/v0.2.5.md) · [Previous release](Docs/Releases/v0.2.4.md).

## Changes

- **Free by default:** connect an external MCP client with the local URL alone,
  without a token or another operation confirmation. Argument schemas, document
  revisions, workspace boundaries and explicit saves remain enforced.
- **Four access profiles:** Free, Local controlled, Token only and Protected.
  Protected still requires a token and host permission grants. Local MCP can be disabled.
- **Migration and persistence:** unset choices migrate to Free. Explicit profiles,
  SDK aliases and older token/control choices retain their behavior. Saving editor
  preferences preserves the complete MCP configuration.
- **MCP panel:** copy the address, direct HTTP configuration or existing CLI stdio
  configuration. HTTP token headers are included only for token profiles.
- **Shared services:** chat, CLI and external MCP continue to use the current
  window’s Core Engine, open documents and guarded transactions. Changing the
  transport disconnects chat; reconnect explicitly. Documents remain open.
- Updated to Core 0.1.6 and CyMCPSDK 0.7.2 built from local source. See the
  [assistant and automation guide](Docs/AssistantMCP.md).

## Components

| Component | Version / source selection |
| --- | --- |
| CyCodeIDE | 0.2.5 |
| [CyCode Core](app/core-source.lock.json) | 0.1.6 · `b902736b0edb9a76365a47778af8a223d50e5b83` · source API 1 |
| [Reader editor](app/editor-source.lock.json) | `889dd5fd8db8bfabdba4601f5d2a955723c91301` · graphical editor source API 1 |
| [CyUI](app/cyui-source.lock.json) | `6ef01cfcdb197f3a89d64618ce5ef8818c704728` |
| CyMCPSDK | 0.7.2 · `e562c731a146d599e3404718ea8414bb5b96b66d` (includes `34d4bca`) |

Source API 1 is not a stable C++ binary ABI. Build these components together.

## Validation and documented limitations

Windows x64 Release compilation and ten Python staging/contract checks passed.
Fourteen isolated production checks exercised the real IDE with a local simulated
provider: all four MCP profiles, token-free reads/writes, token/permission denials,
argument validation, profile migration/persistence, CLI, chat and cancellation/restoration.
Gitleaks and package/hash/link checks passed. This does not validate real provider
accounts or interactive attachment dialogs.

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
