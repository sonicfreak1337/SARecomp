# Sonic Adventure: Recompiled

Private development project for the native Sonic Adventure PC port.

The accepted baseline is **r354 / 0.49.9** (2026-09-11). All seven stories
were completed by the user, who also accepted the current bug-fix batch.
The executable and its dependencies are preserved independently from the
experimental worktree. See `baseline/r354.json` for the exact file identities.

Development uses the existing Sonic title adapter and a pinned Katana runtime.
Katana core and SA2 development are outside this project. Experimental work
lives on `enhancements/widescreen`; `main` and the `r354-baseline` tag retain
the initial standalone baseline.

The independent, read-only snapshot lives under `.local/baseline/r354`.
`python tools/verify-baseline.py` checks its contents without starting a game;
`--quick` checks the executable and AOT metadata. Experiments use `out/` and
separate save directories. No feature automatically replaces r354.

The private GitHub release `r354-baseline` also contains a split development
archive. `baseline/development-bundle.json` records each part and every restored
file's SHA-256. A fresh clone can use `python tools/restore-baseline.py` with
authenticated GitHub CLI access to restore the pinned SDK, compiled AOT,
baseline product and installed content. This helper refuses to replace an
existing snapshot. Archive upload and remote part identities were verified;
a complete fresh-clone restore has not yet been exercised.

Original disc images are excluded. Installed content may be used privately
during development; a later end-user installer will require original media.

Build with `./tools/build.ps1` (Windows x64, PowerShell 7, Python 3.12+,
Visual Studio clang-cl, CMake and Ninja). It uses the pinned runtime libraries
and retains all 1,157 compiled game partitions. Adapter changes refresh only
the provider contract and small dispatch archive, then relink. The original
native link audit remains mandatory. The final widescreen build took 106
seconds with zero AOT recompilations.

The development build is **`out/experimental/game.exe`**. Keep its DLLs,
configuration files and the installed content at their current locations.
Double-clicking the executable resolves content through `katana-content-root.txt`.

Run `./tools/start.ps1` for experiments or
`./tools/start.ps1 -Mode baseline` for an independent r354 run copy.
Both seed separate profiles from the local save backup. Directly launching
the experimental EXE defaults to `%LOCALAPPDATA%/SARecomp/experimental`.
The immutable snapshot is never itself a writable run directory.

## Experimental widescreen

Open **Optionen > Bildformat (Neustart)** in the native window menu:

- Original (4:3), which remains the default.
- 16:9 at 1920 x 1080.
- 21:9 at 2560 x 1080.
- An Monitor anpassen, using the monitor's actual aspect and fitting the window
  into its available desktop area.

Choices persist in `sonic-display.ini` and apply on the next launch. The window
is fixed to the selected dimensions until restart, keeping output, rendering
and camera aspect consistent. Custom resolutions can also be set in the INI.
Builds preserve an existing output INI. The launch helper keeps per-profile
settings; explicit `-Aspect`, `-Width` and `-Height` arguments override them.

The world gains horizontal field of view without stretching. Supported HUD
owners keep their original proportions and margins at the physical edges:
time, rings, lives, alternate main counters, boss health and the animal row.
Other interface elements remain centered. Movies keep their aspect. Source-bound
full-screen fade owners cover the added width. Only host rendering/culling
copies change; guest gameplay activation, collision, simulation cadence and
save semantics remain unchanged. Presentation stays at the title's 144 Hz
default. Original mode leaves presentation packets unchanged.

Verification on 2026-09-11 used hidden, muted Emerald Coast captures at 4:3,
16:9 and ultrawide, plus projection/edge/culling checks for 16:9, 64:27 and
43:18. The final ultrawide capture confirms the stage-entry fade now covers
the full width and the left HUD retains its margins. An in-process integration
check exercised all four Options commands and restored the test configuration.
The baseline's full hash check and subsequent executable/metadata check passed.

This is a bounded visual verification, not a new full-story or level-matrix
pass. Character-specific HUD extras and every scene transition have not all
been independently checked. Captures and logs are local under
`runs/widescreen-final`; the inspected final frame is `emerald-coast-ultrawide.png`.
Personal saves and original disc images are excluded from Git and the remote
development archive.
