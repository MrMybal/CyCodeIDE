# CyCodeIDE presentation sources

Follow ../AGENTS.md. This directory owns the graphical application and resources;
Core services and native public contracts come from the pinned CyCode dependency.
Do not add native document/semantic owners here or copy dependency implementations.
Keep explicit CyUI targets, locale generation, palette/dp and shared editor API 1.
Normal builds use BUILD_TESTING=OFF and never launch GUI/CLI/updater/test binaries.
The development build deliberately disables the previous CyCode release updater;
its repository/package identity must be migrated before enabling IDE distribution.
