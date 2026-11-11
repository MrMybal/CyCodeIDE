# CyCodeIDE application updates

CyCodeIDE uses the native Core update library, compiled once with the immutable
consumer identity `CyCodeIDE`, repository `MrMybal/CyCodeIDE`, and the IDE version.
It does not subscribe to the historical CyCode or Reader release streams.

Open **View → Application updates** to check stable GitHub Releases, review release
notes, download or resume a verified package, and inspect download/history records.
Startup checks are optional in preferences. A check never downloads or installs.
**Later** and **Skip this version** retain their existing explicit behavior.

On Windows x64, downloaded assets require GitHub's SHA-256 digest and the exact
`CyCodeIDE-VERSION-windows-x64.zip` or `-setup.exe` name. Native services validate
the repository, size, product-bound metadata, file paths, managed-file hashes,
installation identity and update revision before replacement. The ZIP is stored,
not ZIP64, and contains a `CyCodeIDE-VERSION/` folder with `manifest.json`.

**Install update and restart** first follows normal document save/discard and
close handling. The staged, hash-verified `CyCodeIDEUpdater.exe` waits for all
instances using that installation. It uses the same native leases, transactions,
rollback journals and explicit recovery services. It does not forcibly close
other instances. User `data/`, settings and unknown files remain outside the
managed payload; local edits to managed files block replacement. Interrupted
updates retain their recovery records and backups.

Development checkouts and older/foreign manifests cannot be replaced. The first
update-capable copy must come from a complete CyCodeIDE package. Linux/macOS use
the release link; automatic transport and installation there remain unsupported.
The system accepts a newer stable version only, with no downgrade or prerelease.

Private releases read `CYCODEIDE_UPDATE_TOKEN`, `GH_TOKEN` or `GITHUB_TOKEN` from
the environment. Credentials are not saved in preferences, manifests or logs.
Keep all tokens out of command examples and package input directories.

## Authoring packages

The production build includes the IDE helper and the canonical EN/FR offline
manual. It keeps `BUILD_TESTING=OFF` and does not run an application or native test.
`--without-updates` explicitly disables release checks in development builds;
the package author refuses such a build for update distribution.

```sh
python app/scripts/build.py --fetch
python app/scripts/package_windows.py --build runtime/build --mcp-bundle <verified-bundle> --tools-root <verified-runtime-tools> --iscc <ISCC.exe>
```

The author reuses the exact pinned Core's pure package validation, PE resource
reader, archive writer, MSVC runtime staging and installer compiler. Prepare the
real isolated MCP bundle with the pinned Core's `prepare_mcp_runtime.py` and its
locked SDK wheel/source; this does not execute the payload. Runtime tools must
match the Core's locked ripgrep archive and binary hashes.

The package includes the native CLI, VCS providers, app-local MSVC runtime,
isolated Python/MCP, fonts, locales, notices and complete manual. Build trees,
credentials, downloaded source archives and user state are excluded. Each
manifest records the product/repository, IDE version, source commit and dependency
locks. `SHA256SUMS.txt` records the authored artifacts. `--portable-only` omits
Setup. `--allow-local-changes` is an explicit local review mode and records that
the package is not reconstructed solely from its source commit.

Creating packages does not upload them. A future release must publish a matching
stable tag and complete assets in **MrMybal/CyCodeIDE**, with accessible
corresponding sources. GitHub Actions remain disabled. Source pushes, release
publication and installation are distinct authorizations.

## Validation boundary

Production compilation, source/data checks, inert authoring checks and PE/ZIP/hash
inspection can run under the current local policy. They do not establish GUI,
clipboard, installer execution, restart or real online update acceptance. Do not
launch project GUI/CLI/helper/installer/native-test executables while the antivirus
investigation suspends those checks. Preserve the exact build/scan/package report
and list any checks not performed before a public binary release.
