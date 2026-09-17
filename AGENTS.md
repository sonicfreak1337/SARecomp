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
- Linux PGO remains OFF. The compiler/profile fixture round trip is qualified,
  but two 119-unit game training builds stalled at Gamma loading, including
  counters-only instrumentation. No valid game profile or USE gain exists.
  See `docs/linux-pgo-experiment-20260916.md`; do not repeat or expand this
  instrumentation without new evidence explaining the guest-call stall.
- Use small relevant visual/boot checks, no full level matrix by default.
- Whole semantic native owners `057B00` (animation hierarchy) and `0417C8`
  (pose blending/SRT) now default ON for Windows and Linux, as part of the
  September 17 native CPU group described below.
  See `docs/native-animation-hierarchy-20260916.md`: 184 positive and 18
  rejection cases; short paired Linux results save 7.26% Gamma / 14.89%
  Chaos-4-intro CPU per update, but image throughput gains are only 2.79% /
  5.74%, with documented start/update differences. These initial results alone
  did not qualify a Deck gain. The next model-pipeline boundary is documented separately;
  do not discard guest-visible projection/color RAM or revive synchronous
  GPU mesh creation as a supposed new optimization.
- Generation-bound observer permission caching remains OFF. The Linux component
  cases and Gamma/Windy stage runs passed, but a 2–4% execution CPU reduction
  accompanied worse whole-frame throughput. Do not ship or expand it based on
  the micro-cost alone; see `docs/linux-write-observer-guard-20260916.md`.
- Dispatch TLS constinit remains OFF. The 41-unit candidate reduced code size
  by 2%, but Linux Gamma/Windy execution CPU per update improved only 0.3%/2.6%,
  without a useful global throughput gain. See
  `docs/linux-aot-code-shape-20260916.md`; do not expand it based on code size.
- Prepared transfer plans cover the shared indirect dispatcher across all
  modules; see `docs/linux-prepared-transfers-20260916.md`. Two Linux pairs
  measured about 4–5% less execution CPU, with a small Windy state difference.
  Keep the tested experiment for further global work, but it is not the large
  gain required for a patch. Its CMake default is OFF; the isolated build-linux
  candidate is ON. Future benchmark tools default provider telemetry OFF;
  specify `--telemetry on` to reproduce the older instrumented measurements.
- The direct comparison with the pre-22:00 build establishes no positive net
  gain; see `docs/linux-net-progress-20260916.md`. Do not report the earlier
  dispatcher-toggle percentage as overall progress. The old Windy control
  also aborted during shutdown after the measured window; its cause is open.
- Qualified scalar RAM writes remain OFF. The 792-case Linux component test
  passes and Gamma completes, but +1.82% image throughput with a one-update
  workload difference is insufficient. See `docs/linux-scalar-writes-20260916.md`.
  Do not expand ALL scope or ship this experiment based on its 3.80% normalized
  execution-CPU result alone.
- Complete stack-sequence fusion remains OFF; see
  `docs/linux-stack-frames-experiment-20260916.md`. The first gameplay pair was
  marginal and predates a scheduler admission correction. The corrected helper
  passes 1,876 component cases, but has no qualified gameplay speedup.
- Mixed RAM/ALU/FP regions remain experimental and default OFF; see
  `docs/linux-ram-regions-experiment-20260916.md`. The isolated C772 build
  covers 967 regions and passes 1,022 component cases. Fresh Gamma/Windy pairs
  show 5.77%/9.16% lower execution CPU/update, but workload differences and only
  1.75% Windy image throughput do not qualify a large global/Deck FPS gain.
  Do not ship or expand blindly from owner membership or static region counts.
- The B04B follow-up uses per-region RAM page proofs and batched memory counts.
  All 1,440 component cases and Gamma/Windy probes pass. The 27.1% synthetic
  saving becomes only -1.42% Windy / +0.61% Gamma execution cost versus C772;
  this is not a new qualified global win. Latest pre-22 control comparisons
  show -5.20%/-10.45% CPU/update with workload differences, not net FPS gained.
  Keep it within the default-OFF RAM experiment; no ALL expansion or shipping.
  Fixed-literal call dispatch was also audited: the 41 units already have
  2,886 direct callee sites and zero remaining static_call sites. Do not repeat
  that pilot; remaining dynamic dispatch selects functional hooks/overlays.
- Page-proven code-address translation remains OFF. The independent mapping
  oracle passes 253,912 cases, but a 41-unit Linux game comparison shows no
  useful throughput gain (Gamma -3.79%, Windy -0.50% images/s, with recorded
  workload differences). Its synthetic lookup and 2.44% code-size gains are
  not grounds for ALL expansion. See `docs/linux-code-address-pages-20260916.md`.
  The active isolated executable returns byte-for-byte to B04B afterward.
- The private shared-register ABI now has a six-procedure game pilot and 71,681
  component comparisons; see `docs/linux-procedure-registers-20260916.md`.
  Its fresh Gamma comparison costs 5.41% more execution CPU/update with differing
  update counts. It remains OFF and is excluded from the requested patch. Do
  not expand it based on the synthetic improvement or the older slower control.
- On 2026-09-16 the user explicitly requested a new Deck update for yesterday's
  installed patch. This authorizes that update despite the still unmet 20–25 ms
  target. Use the previously qualified B04 RAM/transfer/memory-comparison paths
  with normal-start defaults, preserve installed saves/settings and diagnostic
  policy, and test the actual self-extracting patch. This is not a claim of a
  large Deck gain or permission to promote the rejected private-register pilot.
- Latest scheduling instruction: continue autonomous native CPU work, then
  build the next patch around 05:20 Europe/Berlin on 2026-09-17. This explicitly
  authorizes that patch; ship qualified improvements only and preserve saves.
  The one-shot thread heartbeat `sarecomp-patch-am-17-09-um-05-20` is registered
  for that time. Avoid a duplicate patch if the active goal already built it.

- The September 17 shared native CPU group now defaults ON independently of
  timing: animation hierarchy, pose blending, whole-owner direct model memory,
  render-context capture/commit, bounded SIMD palette publication, asynchronous
  audio status and scene-independent admitted collision owners. Functional code,
  memory, FPU and callback admission remains. `SARECOMP_NATIVE_CPU_PATHS=0`
  disables this new group internally; per-feature `0` overrides remain.
  See `docs/native-cpu-defaults-20260917.md` and the component reports.
  The exact matched Windows Original comparison saves 6.64% execution CPU /
  7.57% cycles with the same 720 updates, player state, HUD and PAL cadence.
  This is not a Deck FPS guarantee; the 20–25 ms target remains open.
  Windows model-memory capability and Linux RAM/transfer/performance-default
  CMake switches default ON, preserving the September 16 release paths on a
  fresh configuration. Older OFF descriptions above are historical experiments.
  Model packets, sound metadata caching and deferred MIDI remain OFF after
  unhelpful/negative end-to-end results. Do not enable them with this group.
  The actual September 17 Linux patch passes installation, save/settings
  preservation, idempotence, Gamma Original and Chaos-4 entry checks. A quiet
  two-vCPU Gamma comparison to September 16 shows 47.90% more new images/s,
  1.03% less execution CPU/update and 24.41% less process CPU/update, with 66
  versus 68 updates and differing end positions. These VM numbers are not
  exact-work or Deck guarantees. Recompiled Gamma also passes in a four-vCPU
  function check, excluded from that comparison. The VM now has four vCPUs;
  benchmark summaries record CPU count and LP_NUM_THREADS. See the delivery
  report before comparing to older two-vCPU data.

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
