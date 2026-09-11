# Sonic Adventure: Recompiled

Private development project for the native Sonic Adventure PC port.

The accepted baseline is **r354 / 0.49.9** (2026-09-11). All seven stories
were completed by the user, who also accepted the current bug-fix batch.
The executable and its dependencies are preserved independently from the
experimental worktree. See `baseline/r354.json` for the exact file identities.

Development starts from the existing Sonic title adapter and a pinned Katana
runtime. It does not continue analyzer work or SA2 bring-up. First enhancement:
real 16:9 and 21:9 world rendering with correct UI placement.

`tools/verify-baseline.py` checks the frozen snapshot without starting a game.
The snapshot lives under `.local/baseline/r354`; experiments must use `out/`
and their own user-data directory. No feature automatically replaces r354.

Original disc images are excluded. Installed content may be used privately
during development; a later end-user installer will require original media.

Build with `./tools/build.ps1` (Windows x64, Visual Studio clang-cl, Ninja).
This links the retained AOT archive; it never recompiles the game's 1,157
AOT partitions. The initial standalone build took 74 seconds and passed
the original native link audit. This is build evidence, not a new playtest.

Run `./tools/start.ps1` for experiments or
`./tools/start.ps1 -Mode baseline` for an independent r354 run copy.
Both seed separate profiles from the local save backup. Directly launching
the experimental EXE defaults to `%LOCALAPPDATA%/SARecomp/experimental`.
The immutable snapshot is never itself a writable run directory.
