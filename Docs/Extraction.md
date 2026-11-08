# Core / IDE / Reader separation

The presentation was extracted from CyCode af0c149e1366c3b50d5e509153dbab380dc3f368.
Exact current dependencies are pinned in app/core-source.lock.json and
app/editor-source.lock.json; the development branches are published and default
resolution now works without an explicit local checkout. Core and Reader are
public. CyCodeIDE is currently private.

## Ownership and preserved behavior

66 C++ presentation/host units, 12 UI helper headers and 10 UI fixture sources
belong to this repository. The IDE links CyCode::Core, not a duplicate Engine.
The project Engine and Reader TextSession use the same native BufferState,
codecs, guarded I/O, process/protocol and endpoint implementation. Project and
standalone text policies remain explicit profiles; Reader does not acquire
semantic analysis merely by opening a coloured file.

Reader supplies shared graphical editor source API 1. The target graph has one
native Files/Snippets owner and one compatible ImGui/backend. The IDE retains its
themes, locale catalog, context help, keymaps, multi-window docking and editor
Undo/Redo; project operations retain Core-issued revisions, previews, disk checks,
explicit saves and coherent transactions. Project and text plugin C ABIs remain
distinct and unchanged. There is no stable C++ binary ABI promise.

The native CLI/stdin service and socket adapter reuse the same persistent Engine.
The canonical manual and public operation schemas remain in Core. Build staging
copies only the facade/manifest, selected assets/notices, CLI and bundled providers;
it copies no local workspace, settings, runtime data or credentials.

## Actual checks on 2026-11-08

- Core d0b1791 passed 63 native cases on Windows/Linux/macOS runners; MSBuild is
  Windows-only. macOS replay also passed three repetitions, with earlier
  intermittence still retained for diagnosis:
  https://github.com/MrMybal/CyCode/actions/runs/42485288416
- Reader c4a013b passed seven native integrations on those systems, including
  encodings, data/native plugins, revision/disk conflicts, Git environment handling,
  actual IPC and EOF cleanup:
  https://github.com/MrMybal/CyCodeReader/actions/runs/42485861029
- Semantic benchmarking used actual clangd and both synthetic and real configured
  Core CMake sources. Source d0b1791 / 121 translation units / 227 owned files:
  https://github.com/MrMybal/CyCode/actions/runs/42485288360
  Raw reports and their memory/coverage caveats are retained in Core's architecture
  guide. No complete-index or CLion/IntelliJ parity is claimed.
- Windows production CLI, standalone native components, Reader GUI and IDE builds
  passed without executing their application/helper/test binaries locally.
- The actual moved IDE view suite is now enabled on three runners. Initial runs
  exposed session-fold capture, displayed shortcut normalization, an omitted image
  fixture and alternative-provider review failures. The fixture and shared native
  keymap are corrected; session/provider diagnostics are being resolved. Follow
  the exact companion run, rather than treating compilation as acceptance:
  https://github.com/MrMybal/CyCodeIDE/actions/runs/42486970514
- The genuine offline CyDoc export passed EN/FR, guide coverage, resources, aliases
  and moved UI help routes: 612 pages / 6328 routes / 91 guides / 26 topics.
- The original CyCode checkout's 154 changed/untracked source files remain
  byte-identical to their preservation snapshot; active Reader sources are untouched.

## Build and validation commands

```sh
python app/scripts/build.py --fetch
# Only on an explicitly opted-in runner while local native execution is suspended:
cmake -S . -B runtime/build -DBUILD_TESTING=ON
cmake --build runtime/build --config Release --parallel 4 --target cycode_workflow_ui_tests cycode_template_ui_tests cycode_module_tests cycode_image_tests cycode_extension_ui_tests cycode_recovery_ui_tests cycode_resource_ui_tests
ctest --test-dir runtime/build -C Release --output-on-failure --no-tests=error -R "^(workflows_ui|templates_ui|ui_modules|images|extension_ui|recovery_ui|resources_ui)$"
```

The workflow builds the actual pinned native services, shared editor, CyUI and
offline help before those headless view cases. Theme/locale/DPI loops belong to
specific cases; they do not validate OS windows, GPU drivers or multiple monitors.

## Release boundary

This separation is a development delivery. No main branch, tag or release has
been replaced. Before a CyCodeIDE installer is published, migrate its update
repository/asset identity and validate portable/installed layouts. Historical
updater code remains disabled in this build. No native application, CLI, helper,
updater or test binary is run locally while antivirus removals remain unexplained.
No exclusions, quarantine restores or hidden application launches are used.
