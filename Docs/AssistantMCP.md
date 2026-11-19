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

Read access is the default. **Enable editing, builds and plugin commands** grants
the existing Core permissions. **Allow editor preference changes** separately
grants `settings.write`. Rights are checked on each tool call, including calls
from a connected external client. Provider-native filesystem/terminal permissions
remain separate and are denied by default. No unrestricted agent mode is selected.

## CLI and external MCP

The existing Core CLI and its legacy commands remain available. The IDE executable
also accepts `--sdk` before SDL/window creation. For the state of an already open
window, use the private descriptor referenced by **Copy MCP configuration**:

```text
CyCodeIDE.exe --sdk --help
cycode_cli.exe --sdk --bridge-config <private-descriptor.json> list-operations
cycode_cli.exe --sdk --bridge-config <private-descriptor.json> describe-operation ide_state
cycode_cli.exe --sdk --bridge-config <private-descriptor.json> invoke ide_state --params-file query.json
cycode_cli.exe --sdk --bridge-config <private-descriptor.json> --access edit invoke ide_open_document --params-file open.json
```

`query.json` may contain `{ "offset": 0, "limit": 50 }`; `open.json` may contain
`{ "path": "src/main.cpp", "line": 0, "character": 0 }`. Positions are zero-based
UTF-16. JSON results go to stdout, diagnostics to stderr; errors return a nonzero
exit code. Without a connection descriptor the Core CLI can work headlessly on
an explicitly selected workspace; `ide_*` operations require an open IDE instance.

The copied MCP configuration starts the native CLI over stdio and attaches to this
window's authenticated loopback endpoint. Descriptors and bearer credentials stay
private. Closing the window invalidates its endpoint; obtain a new configuration
after reopening. The compact profile exposes the five SDK discovery/resource
tools. Search, describe, then call the original operation. `--mcp-profile direct-full`
keeps full discovery available. Complete Core contracts are preserved; only these
presentation operations are appended:

| Operation | Purpose | Permission |
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
CyMCPSDK **0.7.0**, revision `791d4d7d7680213b282ec8529d248a10f8494d77`.
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


## Version 0.2.4 validation

Isolated production acceptance exercised chat with a local simulated HTTP
provider, actual IDE tools, CLI and an external MCP client. Reads, authorized
edits, rights/revision denials, explicit save, cancellation/restoration and
complete chunked Unicode reads passed. Real provider accounts and interactive
attachment dialogs remain unverified. See the [version notes](../VERSION.md).
