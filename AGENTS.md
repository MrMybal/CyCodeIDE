# CyCodeIDE development

Read README.md, Docs/Extraction.md and the pinned CyCode app/Docs/CoreExtraction.md.
This repository owns only the IDE presentation; reuse CyCode::Core for native
services and Reader editor API 1 for graphical editing. Never fork contracts,
semantic indexes or document transactions. Read Core's app/AGENTS.md too.
Use the pinned CyUI modules, palette roles and dp/style; no vendor source edits.
Preserve locales, keymaps, themes, multi-window behavior and native revisions.
Ordinary builds keep BUILD_TESTING=OFF. Do not launch GUI/CLI/helper/native test
executables locally while antivirus removals remain unexplained; no exclusions,
restores or rebuild/retry loops. Use source checks and production-only builds.
Do not publish an installer/release until its new identity and updates are migrated.
Never publish runtime/data, credentials, private sample projects or machine paths.
