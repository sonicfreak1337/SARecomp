# Sonic Adventure: Recompiled

Private development project for the native Sonic Adventure PC port.

The accepted baseline is **r354 / 0.49.9** (2026-09-11). All seven stories
were completed by the user, who also accepted the current bug-fix batch.
The executable and its dependencies are preserved independently from the
experimental worktree. See `baseline/r354.json` for the exact file identities.

Development uses the existing Sonic title adapter and a pinned Katana runtime.
Katana core and SA2 development are outside this project. Experimental work
lives on `enhancements/widescreen` and `enhancements/camera-style`; `main` and the `r354-baseline` tag retain
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

At the title screen, **B / Circle / Escape** opens a localized quit prompt.
**A / Cross / Enter** confirms; **B / Circle / Escape** cancels. See
`docs/title-quit.md` for the title-only scope and verification.

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

The Adventure Field ring icon now shares its counter's left anchor; the user
confirmed this correction. Widescreen visibility also reaches the retail
BasicAttach pre-cull and 16 reviewed object display callers, including the
hint monitor. A reported station-hall monitor crash is still under
investigation; this experimental build is not a replacement baseline.
See `docs/widescreen-culling.md` for the scope, evidence and remaining limits.

The subsequent Speed Highway 2 capsule identified a separate finite-only SDK
color read. The experimental build now preserves the original FMOV/FADD color
behavior and converts exceptional colors at their output boundary. See
`docs/speed-highway-color-crash.md` for the exact source evidence and checks.

## CPU performance work

The performance batch found no reliable reduction in total CPU work. All
runtime, AOT and adapter experiments were discarded, including mesh-local
color reuse. The exact accepted widescreen executable and adapter sources
were restored. No gameplay work, memory guard or floating-point contract
has changed. This batch adds measurement tooling, not a claimed FPS upgrade.

The linker now uses a persistent ThinLTO cache and three worker threads. Build
logs count actual recompiled AOT objects instead of always reporting zero.
This improves development diagnostics; it is not a game-FPS claim.

Run `python tools/benchmark-stage.py --tag unique-name --timing` for one hidden,
muted, 60-second Emerald Coast probe with forward input and separate copied
saves. It measures simulation/presentation rates and process CPU milliseconds
per simulation frame, excluding the first ten seconds. It refuses to overlap
another game or compiler. `--exe` selects a preserved reference executable;
`--scenario sonic-windy-valley` selects the other measured scene.

The probe uses a diagnostic entry and a timed shutdown, not a completed stage
or story replay. Passing requires completed gameplay, the expected deadline
stop reason, process exit status and no reported runtime fault. CPU timing
includes all game threads and is not CPU temperature or a single-core duration.
The local logs live under `runs/`; the compact measurement report is in
`docs/performance-2026-09-11.md`.

## Experimental Vulkan

Open **Optionen > Renderer (Neustart)** and select **Vulkan (experimentell)**,
then restart. Direct3D 11 remains the default and selectable fallback. Both
backends support the existing widescreen modes. The selection persists in
`sonic-display.ini`; builds preserve the user's current configuration.
Vulkan also supports **Alt+Enter** for borderless fullscreen and return to the
previous window size and position.

This is a native Vulkan backend using the same scene and shader contracts as
D3D11. It requires a compatible Vulkan 1.3 driver; the SDK/compiler are not
needed to play. Linux host support and the replacement ingame Options screen
are subsequent work. See `docs/vulkan-renderer.md` for implementation,
requirements, source provenance and bounded validation results.

The benchmark helper accepts `--renderer d3d11` or `--renderer vulkan`.
The earlier measurements identify translated game execution as the main CPU
cost, so a new GPU backend alone does not establish a simulation-FPS gain.
The matched D3D11/Vulkan probes found no consistent CPU or simulation gain;
both maintained roughly 144 output FPS. See `docs/vulkan-performance-2026-09-12.md`.

## First-start configuration

The first interactive launch opens the English **`sonic-config.exe`** beside
`game.exe`. Save & start continues into the game; Cancel exits before starting
the game. Run `sonic-config.exe` again whenever settings should change.

Choose Direct3D 11 or Vulkan; windowed, borderless or exclusive fullscreen;
resolution; original 4:3 or widescreen; render scale; 30–144 output FPS; camera style;
text language, voice language and subtitles. The original game cadence is
independent of output FPS. Exclusive fullscreen falls back to borderless when
the driver cannot acquire it. Alt+Enter returns to the saved window rectangle.
Settings apply on the next launch and builds preserve an existing INI.

Text supports Japanese, English, French, Spanish and German. Voices support
Japanese and English. **Use game setting** leaves that original setting alone.
Explicit choices are applied when loading a save and merged into its language
options on the next normal game save, using the original checksum and existing
VMU persistence. Loading does not undo the configuration. The config program
does not edit save files or force a story-progress save during loading.

Direct launches use `out/experimental/sonic-display.ini`; `tools/start.ps1`
retains separate settings per run profile. `SARECOMP_DISPLAY_CONFIG` selects
an explicit config file (also understood by `sonic-config.exe`). Background
tests skip the popup and use their own settings and copied saves.

## Experimental camera

Choose **Camera style: Recompiled** in `sonic-config.exe` and restart.
The right stick rotates freely around the character and adjusts camera
elevation; the view follows the character during movement. **Original** is
the default and keeps the original camera. Scripted camera overrides and
reviewed fixed/path/timed sections retain their original control, including
in-level event mechanisms. See `docs/camera-style.md` for the exact policy,
hidden gameplay verification and current collision limitations.
