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

## UI follow-up and Core coordination — 2026-11-11

The build time limit is now a bounded integer property in the existing CyUI
Inspector. It participates in English/French filtering and the shared
draft/commit/cancel lifecycle; only a committed edit changes preferences.
Validation reuses Core's BuildLimits helper. The inspector's text-size, clangd
and build-timeout validation messages have English/French catalogue entries.

Source/data checks passed for seven Python and seven JSON files. Regenerating
the UI catalogue preserved all 1,659 existing entries and added four entries.
The existing Windows Release x64 cycode_ui production library compiled with
BUILD_TESTING=OFF and BuildProjectReferences=false, reusing existing dependency
outputs. This check used explicit development sources: Core a34db1ddf413b29cd86c856ce2a58c4c636adc43,
Reader 00daef69fc59bbefd0424098e96c23c4f10b9613 and the locked CyUI c5b0f8ad818a.
It is presentation compilation evidence, not a full link/distribution build,
default-pin acceptance, desktop interaction test or Linux/macOS validation.
No application, CLI, helper or native fixture was launched.

The coordinated Core integration target a34db1d is committed locally but not
published. Keep the versioned Core pin at 5dc05f4 until publication is authorized;
the Reader and CyUI locks are unchanged too. New analysis.inspect presentation
and additional clangd transformations belong to that later integration. The
inspector change requires no new Core API. GitHub Actions remain paused.

## Diagnostic and context-action interactions — 2026-11-11

Current-file diagnostic navigation now rebases on the caret line after a manual
move instead of continuing from an unrelated old selection. Filtered-out
diagnostics keep their selection identity without showing an unrelated detail.
The panel distinguishes no diagnostics from no filter matches, offers filter
reset and disables navigation when no visible location is navigable.

Context actions use a persistent CyUI text filter for titles, kinds and
unavailability reasons, with result counts and clipped table rows. Filtering
retains original Core action indices and existing availability/revision guards.
Search fields and popups own their keyboard input; Preview activation keeps
preview intent rather than also requesting Apply. Action titles render as text,
including literal ImGui label markers.

Five additional EN/FR entries were generated without changing previous entries.
Regression cases were added to the existing problem-navigation and module-view
suite sources for manual caret movement, filter selection and Enter during text
input. Those native fixtures were neither built nor executed locally.
The updated Windows Release x64 cycode_ui library compiled with zero warnings
and zero errors, BUILD_TESTING=OFF and dependency builds disabled, using the same
explicit development sources recorded above. Source/data and diff checks passed.
This remains a presentation-only compilation check; desktop interaction,
full-link/default-pin and Linux/macOS acceptance are unverified for this change.
Core/Reader/CyUI pins, pre-existing local changes and paused Actions are preserved.

## CyUI code presentation — 2026-11-11

The IDE now pins published CyUI 6173630ad5eb1e585eee1a0e7d3617285563e774.
Core 5dc05f4 and Reader d27e84f remain unchanged. The existing locked Reader's
build-local backend preparation accepts this CyUI without editing either
dependency or copying another editor implementation.

Code, editor and diff views use CyUI's embedded JetBrains Mono Regular 2.304,
shared syntax roles, local 50–300% wheel zoom and brace-scope guide correction.
Each IDE editor shows its current zoom percentage; clicking it resets that view
to 100%. Portable control icons merge into interface fonts only. The existing
font resource and SIL OFL notice match CyUI's original bytes and remain staged.

The build helper accepts --build-dir for an independent configuration. A fresh
Windows Release production build with BUILD_TESTING=OFF rebuilt the locked Core,
CLI/providers, Reader, all selected CyUI modules and the IDE together, including
the 612-page EN/FR offline manual. It used a clean explicit CyUI source at the
new lock; default dependency resolution selects the same published revision.
This avoids mixing libraries built against different native Palette layouts.

Source/data and distribution checks passed, including the font/license hashes,
the staged Core operation manifest and complete offline-reader resources.
Earlier local edits and the previous build remain preserved; Core, Reader and
CyUI sources were not modified. No application, helper or native fixture was
launched, no release was published, and GitHub Actions remain paused. Actual
desktop wheel/font/theme/DPI acceptance and Linux/macOS builds remain unverified.

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
