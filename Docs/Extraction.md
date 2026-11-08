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

- Core runtime revision 84e4c7831afeab27406a958a957db23fe005eae8 passed the
  63-case native suite on Windows x64, Ubuntu x64 and macOS arm64. MSBuild is
  skipped on POSIX. The existing Core case includes 80 concurrent child ownership
  probes on POSIX; the formerly failing macOS replay passed and then passed
  three extra repetitions:
  https://github.com/MrMybal/CyCode/actions/runs/42492881492
- Reader runtime revision 671c588b924f9da2655843f64615b6b6bc7076e6 passed all
  17 service/editor/view cases on those systems plus its separate seven-case
  headless graph. Actual codecs, plugins, guarded edits, IPC/EOF, comparison,
  history, syntax, filtering and default/custom completion chords are covered:
  https://github.com/MrMybal/CyCodeReader/actions/runs/42493128377
- IDE runtime revision 6d9176c713af50c663667bb2d0837ab5eb7f333b passed all
  seven view cases on Windows x64, Ubuntu x64 and macOS arm64:
  https://github.com/MrMybal/CyCodeIDE/actions/runs/42496974728
  This workflow builds the real Core, editor, IDE and offline help, then runs
  workflows/session, templates, modules, images, extension/provider reviews,
  recovery and resource view contracts. All three jobs completed successfully;
  this is view-test acceptance in addition to the production builds.
- The extracted image fixture was restored. Shared Keymap accepts displayed and
  canonical strokes without losing contexts/chords. Session snapshots and
  restoration use initialized editor state; transient clipping no longer drops
  valid folds. The provider fixture advertises its real diff capability and
  produces a genuine unified diff; the capability guards remain enforced.
- New-window commands launch CyCodeIDE, retaining project/theme/locale/keymap
  arguments. The IDE has its own product version definition. Build staging
  derives its actual launch name while preserving Core operations and permissions,
  includes the logo/fonts/notices, and excludes local settings/project data.
  Its inert staging regression passed locally. The development update panel
  points to CyCodeIDE; the old updater executable is not built or staged. Its
  obsolete generated output was removed from the local build directory.
- Both final semantic workloads completed on actual clangd/CLI. The real Core
  CMake project has 121 translation units. Exact observations, raw reports and
  limits are retained in the Core guide:
  https://github.com/MrMybal/CyCode/actions/runs/42492881444
  Short synthetic responses improved after incoming-message wakeup. The real
  completion/analysis observations do not establish an overall speedup or complete
  index coverage. No CLion/IntelliJ comparison is claimed.
- Fresh Windows production builds passed for the CLI, minimal native components,
  Reader GUI and IDE with tests OFF and no native execution locally. Minimal
  component configuration builds no Engine/CLI/GUI or unused native libraries.
- Final offline CyDoc exports passed EN/FR, guide coverage, resources and aliases:
  Core/IDE 612 pages / 6328 routes / 91 guides / 26 topics; Reader 76 pages /
  462 routes. Both keep the actual local HTML reader and image/gallery modules.
- The original CyCode checkout's 154 changed/untracked source files remain
  byte-identical to their preservation snapshot; active Reader sources are untouched.

The current Core source pin 716947280c376b7f291d76f87f9625af67a98094 adds only
README/CyDoc/source-guide/benchmark-report updates to accepted runtime revision
84e4c78. The tracked diff was verified to contain no native/build/manifest change.
This keeps the final accepted manual in default source builds without changing
the native code validated by that matrix. Reader editor source API 1 is pinned
to d27e84f1097b68c12630dc4c10fcc406d4628760; its subsequent 671c588 change only
selects the same fixed Core for the standalone app.

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
updater code remains disabled in this build; its old CMake target is archived in
app/legacy/UpdaterHost.cmake.txt and is not included by production configuration.
No native application, CLI, helper, updater or test binary is run locally while
antivirus removals remain unexplained.
No exclusions, quarantine restores or hidden application launches are used.
