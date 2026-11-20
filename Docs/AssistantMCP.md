# Assistant and native automation

Open **View → Assistant**, or **Open assistant** in the MCP panel. The native
CyMCPSDK chat shares the Core Engine that owns this window: open documents,
unsaved changes, the project index and guarded transactions remain in that Engine.

Choose **Detect providers** or select a provider and its real executable/service.
An API key is read from the named environment variable; it is never saved in the
connection preferences. Remote APIs require a model from their actual catalogue.
**Connect / New conversation** does not send a message. Streaming, cancellation,
model selection, attachments and provider errors use the SDK's native panel.
**Restore conversation** restores the selected private checkpoint or reconnects
its exact native session; a missing session is an error, not a new conversation.

Local MCP defaults to **Free**. Open the MCP panel to enable/disable local HTTP
and choose its access profile. A client can connect with the displayed `/mcp` URL
alone in Free, without a token or a second operation confirmation. All registered
operations are available; argument schemas, document revisions, workspace
boundaries, explicit saves and existing business rules still apply.

| Profile | HTTP token | Operations |
| --- | --- | --- |
| Free (default) | No | All registered operations |
| Local controlled | No | Host permission grants |
| Token only | Required | All registered operations |
| Protected | Required | Host permission grants |

In controlled profiles, **Enable editing, builds and plugin commands** grants
the existing Core rights. **Allow editor preference changes** separately grants
`settings.write`. The host checks these rights on every call. Changing the
profile or enabling/disabling HTTP restarts its endpoints and disconnects the
chat; reconnect explicitly. The current Engine and open documents stay intact.

Preferences without an explicit choice migrate to Free. Named profiles and
explicit older `requireToken` / `controlOperations` choices are preserved.
The canonical choice is saved as `automation.localAccessProfile`, with `enabled`,
`allowEditing` and `allowSettings` kept independently. Provider-native filesystem
and terminal permissions remain distinct from the application’s MCP policy.

## CLI and external MCP

The existing Core CLI and its legacy commands remain available. The IDE executable
also accepts `--sdk` before SDL/window creation. For the state of an already open
window, use **Copy MCP address** in the MCP panel. With Free, the URL alone is sufficient:

```text
CyCodeIDE.exe --sdk --help
cycode_cli.exe --sdk --connect <MCP-URL> list-operations
cycode_cli.exe --sdk --connect <MCP-URL> describe-operation ide_state
cycode_cli.exe --sdk --connect <MCP-URL> invoke ide_state --params-file query.json
cycode_cli.exe --sdk --connect <MCP-URL> invoke ide_open_document --params-file open.json
```

`query.json` may contain `{ "offset": 0, "limit": 50 }`; `open.json` may contain
`{ "path": "src/main.cpp", "line": 0, "character": 0 }`. Positions are zero-based
UTF-16. JSON results go to stdout, diagnostics to stderr; errors return a nonzero
exit code. Without a connection descriptor the Core CLI can work headlessly on
an explicitly selected workspace; `ide_*` operations require an open IDE instance.

**Copy MCP address** copies the HTTP URL; **Copy MCP configuration** copies a
direct HTTP configuration, adding an Authorization header only for token profiles.
**Copy CLI configuration** keeps the native stdio connection available, using
`--bridge-config <private-descriptor.json>`. For token profiles, address-only CLI
connections read `CYMCP_TOKEN` from the current process environment. Its private
descriptor selects the real window. Endpoints bind only to `127.0.0.1`; descriptors
and any bearer credentials stay private. Closing or reconfiguring the window
invalidates its endpoint; obtain a new configuration after reopening.
The compact profile exposes the five SDK discovery/resource
tools. Search, describe, then call the original operation. `--mcp-profile direct-full`
keeps full discovery available. Complete Core contracts are preserved; only these
presentation operations are appended:

| Operation | Purpose | Controlled-profile permission |
| --- | --- | --- |
| `ide_state` | Current document, paginated tabs, panels and editor preferences | Read |
| `ide_commands` | Paginated command catalogue and shortcuts; discovery only | Read |
| `ide_selection` | Live editor selection and observed revision | Read |
| `ide_open_document` | Open/reveal a workspace document | `workspace.write` |
| `ide_activate_document` | Focus an open document | `workspace.write` |
| `ide_close_document` | Close a clean tab at its expected revision | `workspace.write` |
| `ide_show_panel` | Focus a supported tool window | `workspace.write` |
| `ide_editor_preferences` | Shared code zoom, font size and minimap | `settings.write` |

Closing dirty or unsettled documents, discarding changes, arbitrary command
execution, application shutdown and changing host roots/credentials are excluded.
Use the existing Core edit/preview/apply/undo/save contracts for document mutations.
UI calls are marshalled to the rendering thread; Core owns cancellation and durable
job receipts. Mutations are never cached or automatically replayed.

## History and independent options

Completed jobs and conversations are private under `data/automation/<workspace>/slots/`.
Each live window leases a separate slot; inactive slots are reused after restart.
SDK result expiry never removes durable Core job receipts. Interrupted work is not
replayed. Existing old job databases remain intact; use the Core's shipped
`app/scripts/migrate_legacy_jobs.py --state-dir <old-state>` to explicitly import an
inactive legacy state, then query that state with the native CLI's `--data-dir`.
See the selected Core's `app/Docs/NativeAutomation.md` for recovery and ownership.

The provider editor controls deferred tool discovery and the API context budget
independently. The budget is bytes, not a token estimate; full history remains
stored, tool/result groups stay together, and native CLI agents manage their own
context. No automatic summarizer is used.

Trusted host configuration `automation` may separately set `inlineBytes`,
`resultTtlSeconds`, `cacheTtlMs`, `diskResults`, `notifications` and `telemetry`.
Disk spooling, notifications and metadata-only performance JSONL default off.
Resources use bounded chunk reads and private expiring handles; retrieve all chunks
before expiry. Recover a durable mutation receipt rather than repeating an action.
Subscription delivery depends on the transport; TTL and explicit refresh remain
available. Project search uses Core's index and its existing bounded fallback;
no second SDK filesystem search root is exposed. Relaying third-party application MCP tools is not included in this integration.
The local inference-engine supervision module is disabled.

## Source integration

The Core revision is recorded in `app/core-source.lock.json`. Its SDK lock selects
CyMCPSDK **0.7.2**, revision `e562c731a146d599e3404718ea8414bb5b96b66d`.
The SDK is built from clean local sources, with Core, Platform, Http, MCP, AI and
ChatUI. Local engine supervision, example applications and SDK tests are disabled
in the production graph. SDK code is not copied or modified. CyUI and the Reader
editor share the host's one ImGui/backend graph.

```text
python app/scripts/build.py --core-root <clean-Core> --cymcp-source <clean-CyMCPSDK> --cyui <locked-CyUI> --fetch
```

Local SDK changes are preserved: select a separate clean checkout of the pinned
revision. Git retrieval is a fallback only when local SDK sources are absent.
The new package contains no CyToolsCore or Python MCP runtime. Its optional stdlib
Python compatibility runtime is retained only for old CLI uses and job migration.


## Version 0.2.5 validation

Fourteen isolated production checks exercised the real IDE: Free reads/writes by
URL alone, Protected token rejection and grants, all profiles, argument validation,
migration/persistence, CLI and MCP stdio, then chat streaming, cancellation and
restoration without mutation replay with a local simulated provider. Real provider
accounts and interactive attachment dialogs remain unverified. See the [version notes](../VERSION.md).
