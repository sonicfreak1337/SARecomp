# Development setup

[← Back to the project](../README.md)

This page is for contributors. Players use the prebuilt installers and do not
need a compiler, Katana export or development bundle.

## Project layout

- `src/` and `launcher/`: Sonic-specific runtime, rendering, input, configuration and setup.
- `tools/` and `cmake/`: generation, incremental builds, packaging and focused checks.
- `assets/`: project presentation and interface artwork.
- `third_party/`: dependency provenance and license notices.
- `docs/`: design notes and dated implementation/verification reports.
- `baseline/`: identities for the preserved r354 / 0.49.9 product.

Dated reports describe the state at the time they were written. They are not
necessarily the current feature list; the root README describes the player-facing scope.

## Required development material

The port consumes a pinned Katana runtime and retained compiled game partitions.
It is not a standalone generic CMake checkout: a clone alone does not contain
the local SDK, generated AOT build inputs or original disc data.

Authorized development access can restore the private `r354-baseline` bundle
using `tools/restore-baseline.py` and authenticated GitHub CLI access. Its exact
contents and hashes are recorded in `baseline/development-bundle.json`.
The archive transfer was verified; a complete clean-machine restoration is not
yet claimed to be tested. Do not present this recovery archive as a player download.

Original disc images and personal saves are excluded from Git. The frozen
`.local/baseline/r354` snapshot is never a writable build, run or cleanup target.
Use `python tools/verify-baseline.py` to verify it without launching the game.
Read [AGENTS.md](../AGENTS.md) before changing runtime semantics or retained builds.

## Builds

Windows development uses x64 Visual Studio clang-cl, PowerShell 7, Python,
CMake 3.25+ and Ninja, together with the pinned dependencies. The maintained
entry point is `tools/build.ps1`; its parameters select targets and a separate
output directory. Preserve the existing retained build and Ninja recovery state
rather than forcing a full rebuild for adapter-only changes.

Linux uses the port-local configuration in
[cmake/SonicLinux.cmake](../cmake/SonicLinux.cmake), native Linux dependencies and
the original authenticated title inputs. The Linux test VM and packaging helpers
are under `tools/`. A general end-user source-build bootstrap is not yet provided.

Development outputs belong in `out/` and build directories inside this project.
Raw profiles and machine-specific test evidence in `runs/` stay local and ignored.
Do not force-add them to Git; retain concise findings in `docs/` and bind published
installers with release manifests and checksums.
Run experiments with a separate `KATANA_USER_DATA_ROOT`; never overwrite the
baseline or the player's save directory. Use focused checks for the changed path.

## Release preparation

Player installers reconstruct the required original game data from the supported
GDI. Package only the runtime and dependencies needed to play, setup resources
and their licenses. Do not include original discs, installed gameplay assets,
personal saves, development bundles, crash reports or local build caches.

Keep test releases distinguishable from a final release. Record source commit,
artifact hashes, supported update bases and the checks actually performed.
Performance measured in the Linux VM is not a Steam Deck FPS measurement.
