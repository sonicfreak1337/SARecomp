# Sonic Adventure Recompiled

## Scope and current state

- This is the Sonic port project. Work on `main` and use only its private remote:
  https://github.com/sonicfreak1337/SARecompZZZZZ. Keep the repository private.
- Development was discontinued at the user's request. Do not resume development,
  builds or scheduled work; perform only explicitly requested maintenance.
- KatanaRecomp is a pinned SDK dependency, not the development target. Do not
  modify or rebuild the old Katana workspace as a side effect. SA2 is out of scope.
- All seven stories are playable from start to finish. The release is a
  work-in-progress beta; known issues remain. Read `VERSION`, `README.md` and
  `docs/INSTALLATION.md` for the current version, features and player guidance.
- Follow the current user request. Completed optimization batches and old
  delivery schedules are not standing instructions to resume work or rebuild.
- Work autonomously within established permissions; respect explicit stop,
  build and export instructions without asking for the same permission again.
- This file contains durable instructions only. Keep implementation history,
  experiment results and detailed evidence in `docs/` or local `runs/`.

## Protect the working game

- `.local/baseline/r354` is the immutable, byte-verified r354 / 0.49.9 snapshot.
  Never edit, clean, hardlink, regenerate or use it as an output directory.
  `baseline/r354.json` records its identity; the original in the old Katana
  workspace is also protected. Baseline promotion requires a user decision.
- Keep original presentation, timing and camera modes available. Isolate new
  experiments in separate outputs and preserve the existing fallback paths.
- Tests use a separate `KATANA_USER_DATA_ROOT`. Seed saves by copying; never
  modify personal VMU files or run experiments in the player's save namespace.
- Preserve Story and Chao progress together, including their separate save data.
  Reinstallation and updates must retain existing saves and explicit settings.

## Product behavior

- Windows supports D3D11 and Vulkan; Linux and Deck use Vulkan. Renderer and
  timing changes require a restart. Preserve both Windows renderers.
- Original timing preserves the game's scene-specific cadence. Recompiled
  targets 60 simulation FPS at the original game speed; output is 60 without
  VSync and follows display refresh with VSync. Do not confuse output with SIM FPS.
- Both timing modes use the qualified native CPU paths. Changing timing or
  disabling diagnostics must not disable functional optimizations or safety guards.
- Widescreen uses Hor+ world rendering, matching render culling, undistorted
  edge-anchored HUD and full-screen fades. Preserve movie aspect and gameplay
  activation, collision and event boundaries; do not stretch a 4:3 final image.
- Original camera is the default. Recompiled camera remains optional, with wall
  collision and configurable return to Original after idle-stick walking
  (three seconds by default). Preserve scripted cameras and controller mappings.
- Setup/configuration UI is English-only. In-game Options follows text language,
  preserves the original Options music and Sound Test, and supports controller,
  mouse and keyboard. Explicit language choices persist through save loading.
- Keep display-change rollback and startup progress. Optional shader/cache
  failures must not prevent launch. Title quit remains B/Circle/Escape with
  confirmation through A/Cross/Enter and cancellation through B/Circle/Escape.
- Interpolation is retired. Do not restore removed HUD, subtitle-style, rumble,
  numeric FPS or anisotropy controls. Render scale is capped at 100%.
  Keep debug access on Ctrl+F10 and the FPS toggle on Ctrl+F1, not Share.

## Linux and Steam Deck

- Linux installers target x86-64, glibc 2.31+, X11/XWayland and Vulkan 1.3.
  Linux Mint/Cinnamon and other compatible distributions are expected to work;
  distinguish compatibility expectations from environments actually tested.
- Deck defaults: Vulkan, fullscreen, 1280x800 / 16:10, Original timing, VSync Off.
  Preserve existing choices. Support compatible docked displays and safe undocking;
  retain explicit Original 4:3. Do not expose unsafe handheld display modes.
- On Deck, install in Desktop Mode but launch the game only through Steam Gaming
  Mode. Starting the game in Desktop Mode can freeze the device; keep this warning
  prominent. No sudo password or Proton is required for installation or play.
- Performance work remains open. Further limitations and the confirmed Tails
  opening-cutscene widescreen issue belong in the player-facing known issues.

## Builds and verification

- Inspect files before editing. Use the synchronous local
  `codex.exe --codex-run-as-apply-patch` endpoint for source edits.
- Prefer incremental builds through the maintained scripts. Reuse retained AOT
  partitions for adapter changes; do not regenerate them for UI/display edits.
  Before retained Ninja builds, use `tools/sonic-ninja-recovery.ps1`, its bound
  Ninja executable and `Save-SonicNinjaRecoverySnapshot`. Never falsify build state.
- Keep tests hidden and muted with `KATANA_PORT_BACKGROUND_TEST=1` and an isolated
  save root. Do not steal focus or inject OS input. Use native frame capture.
- Run focused checks for changed behavior, then stop. Do not repeat unchanged
  suites, full level matrices or zero-call comparisons without a new reason.
- Use the existing Linux VM for Linux checks. Record its CPU/render-worker
  configuration; VM throughput is not measured Steam Deck performance.
- Identify release candidates by source and executable hashes, not reused output
  filenames. Packaging unchanged verified runtimes does not require a full compile.

## Native performance work

- When requested, prioritize complete connected function groups and their combined
  end-to-end cost. Avoid isolated micro-optimizations as the main workstream.
- Movement/contact, model submission, land, object contact, original camera,
  actor and player operations default ON together in both timing modes.
  Preserve internal `SARECOMP_NATIVE_GAMEPLAY_GROUP=0`,
  `SARECOMP_NATIVE_CPU_PATHS=0` and individual fallback overrides.
- Current code/defaults and the latest qualified composition supersede historical
  OFF/ON conclusions in dated reports. Other experiments stay disabled unless
  explicitly qualified; do not broadly enable old pilots or repeat rejected work.
- Preserve original arithmetic/FPU boundaries, live callback order, observable
  CPU/RAM effects, module lifetime and code-write invalidation. Revoke admitted
  proofs across foreign callbacks. Local computed labels are not global functions;
  fallback resumes the actual PC without replaying completed prologues/delay slots.
- Diagnostics default OFF. Their internal switches must never remove functional
  memory/module guards or timing behavior. Release crash reports are opt-in and
  must not automatically include personal saves.
- Use matched starting state, workload and endpoints for comparisons. Distinguish
  CPU/update, new game images and presented frames; do not add toggle percentages
  or infer a global gain from a favorable scene alone.
- Relevant demanding scenes include Gamma Emerald Coast, Knuckles Sky Deck and
  Lost World, plus bosses/cutscenes when affected. Keep probes short and purposeful.
  The requested 20–25 ms Deck frame target has not been established globally.

## Packaging and repository hygiene

- `VERSION` is the release version; CMake uses its numeric portion. Package through
  `tools/package-installers.py --game` with a verified runtime. Keep manifests and
  checksums tied to the actual installer payload and checks performed.
- Ship only the game runtime, required dependencies/setup resources and licenses.
  The installer requires the supported original PAL GDI and tracks. Never upload
  disc images, installed retail gameplay data, personal saves or development bundles.
- The removed `r354-baseline` release contained retail data. Never re-upload it;
  recovery uses local backup parts only. Preserve hash-only baseline manifests.
- `runs/`, build caches, crash reports and generated packages stay local and ignored.
  Do not force-add them. Development settings use `cmake/development-display.ini`.
- Preserve the legacy installed-directory prefix for update compatibility; it is
  not the public version. Test actual installation/defaults/settings preservation
  when packaging changes, including non-root Linux installation.
- Public release copy describes player features and known issues, not internal
  patch names or experiment history. Retain the SEGA/non-profit disclaimer,
  SoNiCFReaK / KatanaRecomp credits and disclosure of AI coding assistance.

## Focused references

- Build layout and prerequisites: `docs/DEVELOPMENT.md`.
- Camera/input and widescreen: `docs/camera-style.md`, `docs/widescreen-culling.md`.
- Rendering/startup: `docs/vulkan-renderer.md`, `docs/startup-performance.md`.
- Options, language and quit: `docs/options-finalization.md`,
  `docs/configuration-language.md`, `docs/title-quit.md`.
- Native composition and continuation rules: `docs/native-player-operation-20260918.md`;
  earlier group details are in the related `docs/native-*.md` reports.
- Diagnostics policy: `docs/internal-diagnostics.md`.

Dated reports are historical evidence, not new task assignments. Check current
source defaults and release identities before reusing any old command or artifact.
