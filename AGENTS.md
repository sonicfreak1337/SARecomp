# Sonic Adventure: Recompiled

This is the separate, private Sonic port project. Remote:
https://github.com/sonicfreak1337/SARecomp. The former KatanaRecomp repository
is not the development target. SA2 work is stopped.

The user accepted r354 / 0.49.9 on 2026-09-11 as the new working baseline.
All seven stories are complete; currently reported bugs are user-confirmed
fixed. Preserve that product, its AOT pack, native semantics and save data.

## Baseline protection

- `.local/baseline/r354` is an independent byte-verified snapshot, never a
  build/output directory. No edits, cleanup, hardlinks, regeneration or
  automatic replacement there. `baseline/r354.json` binds every saved file.
- The original r354 under the old Katana workspace remains another copy.
- Katana is a pinned SDK dependency. Do not edit/rebuild the old Katana tree
  as a side effect of Sonic enhancements. Any needed port-local adaptation
  must be explicit and preserve the original behavior when disabled.
- Enhancements are experimental and opt-in. Use new output/build directories
  and separate `KATANA_USER_DATA_ROOT` roots. Never write experiments into the
  baseline or the user's original save namespace. Seed test saves by copy.
- Baseline promotion is an explicit user decision, never a consequence of a
  successful compile. Keep the original presentation mode available.

## Current feature

Real 16:9 and ultrawide 21:9: Hor+ world rendering at the actual pixel aspect,
consistent render culling, undistorted HUD anchored at the actual screen
edges (explicit latest user requirement), and complete screen fades. Keep
movie aspect and title simulation cadence intact. Do not stretch the final
4:3 image or broaden gameplay activation/collision/event logic.

## Work and verification

- Development continues on `main`; the integrated enhancement branches were
  removed at the user's request. Camera style remains opt-in; Original is the
  unchanged default. Preserve scripted/event camera ownership inside levels
  as well as cutscenes. PAL camera-control +6 and +7 are separate bytes, not
  a 16-bit type. See `docs/camera-style.md` for the reviewed type policy and
  actual-game orbit/collision check. Recompiled temporarily overrides OG;
  after three idle-stick seconds, walking returns to OG. The sphere sweep
  covers target-to-eye collision, not the entire temporal orbit arc.
  The port-local DualSense camera mapping uses Z/R; retain Xbox and Original.
- Startup shows English progress, prefetches bounded read-only program pages
  and caches D3D shader bytecode / Vulkan driver data and pipeline recipes.
  Cache failures must remain optional. Keep OS-cold/reboot measurements
  distinct from an empty application cache. See `docs/startup-performance.md`.
- Title quitting is host-owned: B/Circle/Escape opens; A/Cross/Enter confirms;
  B/Circle/Escape cancels. Preserve exact ADVERTISE title-state guards,
  neutral release, modal guest freeze and normal shutdown. No OS input
  injection in tests. See `docs/title-quit.md`.
- The optional native Vulkan renderer lives in `src/renderer/` alongside the
  retained D3D11 path. Selection is in the native Options menu and requires a
  restart. Keep D3D11 and the accepted baseline available. See
  `docs/vulkan-renderer.md`; native Linux support is being integrated. Never
  claim that Vulkan fixes the measured guest-execution bottleneck without
  matched measurements.
- `sonic-config.exe` is the English first-start settings dialog. Keep display
  and output FPS independent of original game cadence. Explicit language
  choices merge into the loaded record and the original normal-save path;
  Use game setting leaves original semantics untouched. See
  `docs/configuration-language.md`. Never patch personal VMU files directly.

- Work autonomously within the user's authorization; no repeated permissions.
- Prefer incremental performance builds; retain the r354 compiled AOT pack
  for adapter-only changes. Do not regenerate AOT for display settings.
- Latest user output policy (2026-09-13) replaces the previous 144-FPS default:
  Recompiled targets 60 simulation FPS and outputs 60 without VSync; VSync
  follows the display. Original retains both original scene cadence and
  title-requested output, without autonomous repeats. Game timing is a
  restart-only Original/Recompiled selector. No numeric FPS or anisotropy UI;
  old INI values are ignored. Interpolation remains retired.
- On 2026-09-14 the user promoted current main `57210a0` to the replacement
  1.0 candidate and resumed Linux/Steam Deck work. The old Windows RC1 is
  superseded; r354 remains immutable. Linux must ship an installer accepting
  original GDI + tracks, never requiring a Katana export or end-user compile.
  Steam Deck is always fullscreen: native 1280x800 / 16:10 in handheld mode,
  supported external display resolutions/aspects when docked, and safe return
  to the internal display on undock. Original 4:3 remains an explicit choice.
- Until Deck performance is resolved, new Deck installations default to
  Original game timing at the user's request. Recompiled stays selectable;
  reinstalling must preserve explicit existing settings and all saves.
- Original/Recompiled must select timing only. Both use the authenticated
  native gameplay math and mesh paths. A release/defaults change must check
  Original as well as Recompiled, including native-call evidence and the
  original release/delta/video-clock values. Never infer Deck performance
  from VM throughput. The withdrawn v5 installers must not be distributed.
- Latest Deck feedback after the policy patch: Emerald Coast about 17 SIM FPS,
  Windy Valley 10–15, Egg Hornet 23–26, subsequent cutscene about 20. Continue
  performance work autonomously; no further patch/installer until a large
  measured frame-rate improvement. Preserve original game speed and saves.
- Runtime diagnostics now default off. The user explicitly requested separate
  internal ON/OFF patches and Linux testing; see `docs/internal-diagnostics.md`.
  Never conflate the diagnostic switch with disabling functional memory,
  module-lifetime, executable-invalidation or timing behavior. The first Gamma
  VM comparison shows no useful gain; the 20–25 ms performance target stays open.
- The closed hardware-FPU-region experiment also remains OFF. Its native
  arithmetic kernel improved, but matched Linux Gamma/Windy gameplay did not;
  see `docs/linux-fpu-regions-experiment-20260916.md`. Do not expand it across
  the whole AOT pack based on the kernel result or repeat the per-op MXCSR path.
- Use small relevant visual/boot checks, no full level matrix by default.
- The user currently uses the PC: all tests hidden and muted using
  KATANA_PORT_BACKGROUND_TEST=1; no focus, keyboard/mouse injection or visible
  game window. Obtain visual evidence through native frame capture instead.
- Before any edit, inspect the file. Source edits use the synchronous local
  `codex.exe --codex-run-as-apply-patch` endpoint. Read-only inspection may be
  delegated to existing Sage; no automatic fan-out.
- Before a retained Ninja build use the maintained Ninja recovery helper and
  its absolute bound binary. Never falsify timestamps, objects or build logs.
- Local git commits belong here. Push only this Sonic project to its verified
  private remote; never original disc images. Installed assets are authorized
  for development, but the eventual end-user package requires installation
  from the user's original media. Personal saves stay local.
