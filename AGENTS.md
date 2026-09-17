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
  The one-shot thread heartbeat `sarecomp-patch-am-17-09-um-05-20` was removed
  after the completed delivery below. Do not schedule a duplicate delivery.
  This scheduled delivery was fulfilled at 05:09 on September 17:
  `out/patches/SonicAdventureRecompiled-CPU-Update-2026-09-17.run`, source commit
  `4309d3a`. Actual Linux installation and both timing modes are checked.
  Matching policy-only ON/OFF packages are also delivered; see
  `docs/cpu-update-20260917.md`. Do not rebuild this completed delivery on wake.

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

- The post-delivery RAM extension is isolated; see
  `docs/native-ram-extended-20260917.md`. It adds exact GPR/PR stack operations,
  T tests and fixed shifts to the existing callback-free regions. Both Windows
  and Linux pass 3,004 component cases; 41 units cover 1,413 prefixes. Matched
  Linux Gamma/Chaos-4 endpoints save 5.04%/2.43% execution CPU, but images/s
  change +3.13%/-1.50%. Do not claim a large global or Deck FPS gain.
  The Windows counterpart passes map ownership for 894 entries. Original
  Gamma/Chaos-4 pairs save 8.92%/10.47% CPU, with matching endpoint state.
  Final ordinary-start Original and Recompiled Gamma also pass; Recompiled
  produces 59.996 new images/s on this desktop. The Windows comparison includes
  the whole RAM prefix path, which was absent from its control, rather than
  just the Linux extension. New options still default OFF. The development
  caches explicitly retain Linux EXTENDED=ON / Windows EXTENDED, and the new
  Windows test executable enables the compiled path at normal startup while
  respecting internal override 0. The delivered binaries and Sep17 patch are
  unchanged. Treat `build-linux/game` and `out/ram-extended-windows-20260917`
  as experiments, never as implicit release-promotion candidates. Continue
  from the documented comparisons; do not rerun all component cases unchanged.

- The user confirms the September 17 native CPU approach improves Deck
  performance and requests further replacements; no new exact Deck numbers
  were provided. The complete model-projection batch is a new internal-OFF
  experiment: see `docs/native-projection-batch-20260917.md`. Both platforms
  pass 4,096 exact component cases, plus real-game dual-path verification.
  Matched Linux Gamma saves 18.84% execution CPU/update, but Chaos-4 entry
  costs 1.44% more. Windows results are also mixed. Do not promote or advertise
  a global/Deck gain from the favorable scene alone. The development Linux
  game and `out/projection-batch-windows-20260917/game.exe` contain this
  OFF path plus the retained extended RAM experiment; the Sep17 patch is
  unchanged. Continue from these reports without repeating unchanged tests.

- Whole-matrix bulk push/pop remains internal-OFF after exact 588-case tests
  on Windows and Linux but mixed/negative paired gameplay results. See
  `docs/native-matrix-bulk-20260917.md`. Its development Linux game and
  `out/matrix-bulk-windows-20260917/game.exe` include this OFF path and the
  projection OFF path, plus the retained extended RAM experiment. Final tests
  include reserved-FPSCR normalization; performance-run binary identities are
  recorded separately. Do not promote this experiment or repeat unchanged
  pairs. The next static inventory must use `extended_regions` from preparation
  metadata: its largest remaining gaps are FMOV predecrement and unattempted
  FPU instruction groups. Original group accounting must remain exact.

- The subsequent extended RAM/FPU prefix experiment is qualified functionally
  (5,040 comparisons per platform), but remains OFF in source defaults. See
  `docs/native-ram-fpu-prefixes-20260917.md`: same 41 units, 1,490 prefixes,
  23,853 instructions; Windows Gamma gameplay saves 3.36% CPU. Matched Linux
  Chaos-4 entry saves 2.75%, but Gamma costs 11.97% more CPU / loses 18.26%
  image throughput. Do not replace its fresh control with an older slower run,
  claim a global/Deck gain, expand ALL or ship it. Current `build-linux/game`
  and `out/ram-fpu-windows-20260917/game.exe` explicitly contain this extended
  candidate; source defaults and delivered Sep17 patch remain unchanged.
  Original and Recompiled Windows probes pass, and the latter retains exactly
  one update per image at 60-Hz authored timing. Continue from the recorded
  results; inspect register pressure and whole FPU epoch boundaries before
  further code-shape expansion. No repeated unchanged test suite is needed.

- Projection layout/padding follow-up: see
  `docs/native-projection-layout-20260917.md`. Four-vertex SIMD transposes
  replace scalar temporary-array scatter/gather; one unobserved buffer capture
  preserves padding. Both platforms pass 4,096 exact cases and actual-game
  dual-path verification; Original/Recompiled timing is preserved. Current
  same-executable ON/OFF pairs save only 0.48%/0.76% Windows CPU and
  2.59%/2.49% Linux CPU (Gamma/Chaos 4), with +0.83%/-0.31% Linux image
  throughput. The older 18.84% Gamma result is not reproduced or additive.
  This does not isolate new layout versus old batch; no such gain is claimed.
  The experiment remains OFF; delivered binaries/patch are unchanged.
  Current development artifacts are `build-linux/game` and
  `out/projection-layout-windows-20260917/game.exe`, with the separate EXTENDED
  RAM/FPU experiment retained. Do not promote, expand ALL, or repeat unchanged
  suites from these small results. Investigate repeated RAM-capability setup
  and CPU state publication next; preserve callback/generation boundaries and
  distinguish that work from the rejected observer-permission cache.

- Function-local prepared RAM capabilities remain compile-time and runtime OFF;
  see `docs/native-ram-prepared-access-20260917.md`. Both platforms pass 5,377
  comparisons. In matched Original runs, Linux Gamma CPU/update changes -1.28%
  and Chaos 4 +1.93%, with -0.45%/+0.04% new-image throughput; Windows is also
  marginal/mixed. Do not ship, expand or repeat this unchanged experiment.
  OFF generation is proven identical to the previous source across 41 units;
  only development caches enable the pilot, with runtime default OFF. Final
  `build-linux/game` and `out/ram-prepared-windows-20260917/game.exe` are separate
  from the measured hashes documented in that report. The final Windows
  Recompiled Gamma probe passes; the final Linux relink has no repeated gameplay
  claim. September 17 delivery stays unchanged. Next inspect shared native
  collision stores and repeated source identities. Guard generation does not
  cover module range removal/addition, and tracks_address is overlap rather
  than complete coverage; do not build an identity cache on those assumptions.

- Closed collision RAM remains an internal-OFF experiment; see
  `docs/native-collision-memory-20260917.md`. Cross/normalize, triangle contacts
  and contact candidates share authenticated direct accesses between calls.
  Windows and Linux each pass 1,667 original-byte/access/observer cases. Linux
  Gamma/Chaos-4 exact-state pairs save 1.95%/0.77% CPU, with +1.41%/+0.03%
  images/s. Windows Gamma costs 4.08% more CPU; its Chaos-4 pair is not comparable
  because the start tick and ending game state differ. Do not promote, ship,
  call this a global gain or repeat unchanged pairs. Artifacts are
  `build-linux/game` and `out/collision-memory-windows-20260917/game.exe`; the
  Sep17 delivery stays unchanged. Next investigate fusing specifically reviewed
  cross/length/normalize children into their fully admitted collision parents:
  every short child currently releases/revalidates/recaptures the context.
  Prove child addresses/arithmetic/live outputs and preserve external bridge
  boundaries and arbitrary observers. No generic cross-call capability cache;
  the retired NEAR-POLY experiment remains retired.

- Closed collision child fusion is a separate internal-OFF experiment; see
  `docs/native-collision-closure-20260917.md`. Reviewed cross/length/normalize
  bodies share their triangle/contact parents' admitted RAM/FPU context;
  external bridges and arbitrary observers retain the previous boundaries.
  Windows and Linux each pass 1,644 exact/observer/rejection cases. Matched
  Linux Gamma/Chaos-4 pairs save 4.75%/2.30% execution CPU, with -0.45%/+0.47%
  images/s. Windows Gamma saves 2.56%; Windows Chaos-4 has mismatched state and
  provides no qualified gain. Final Windows Recompiled Gamma passes at 59.998
  new images/s with exact 60 updates. This is not a large global/Deck FPS gain;
  keep `SARECOMP_NATIVE_COLLISION_CLOSURE` OFF and do not ship or repeat unchanged
  suites/pairs. Artifacts are `build-linux/game` and
  `out/collision-closure-windows-20260917/game.exe`; Sep17 delivery stays intact.
  Move next to the complete model transaction/shared capture described in
  `docs/native-model-pipeline-next-20260916.md` (037098, then 037108), preserving
  all projected/palette RAM and live effects. This is distinct from the rejected
  model-packet offloading/resource-cache path. Do not flatten mutable callbacks
  in 040612/040784, repeat their rejected register ABI, or ignore the documented
  QEMU arithmetic issue by broadening model FPU epochs.

- The complete model transaction is implemented but remains internal-OFF;
  see `docs/native-model-pipeline-20260917.md`. Both platforms pass 145 cases,
  Windows has exact image/19-field matches and a Recompiled 4:3 check, and
  Linux Gamma and Chaos-4 entry pass. Initial shared capture saves no meaningful
  CPU; the direct-output follow-up costs 12.48% more Linux Gamma CPU and loses
  7.15% image throughput with matched state. Do not ship, promote or repeat
  unchanged pairs. The Sep17 delivery is intact. Next implement the distinct
  common object activation/lifetime family in
  `docs/native-object-activation-next-20260917.md`; preserve mutable callback
  boundaries and exact distance/activation semantics. Its benefit is unproved.

- Native object activation/lifetime/distance is now implemented as a shared
  internal-OFF family; see `docs/native-object-activation-20260917.md`. The
  finite-kernel revision E passes 1,250 cases on both platforms. Its matched
  Linux Gamma pair saves 8.12% CPU/update / gains 5.82% images, but Chaos-4
  entry costs 5.15% CPU / loses 2.13% images. Windows saves only 1.57% CPU
  with essentially unchanged cycles. Do not present E as a global win or
  promote it. Revision F folds the two proved read-only stage/character
  queries into the activation owner, retaining real constructor/release
  bridges and their exact mutable boundaries. F passes 1,262 Windows cases
  plus all 242 affected Linux cases. F's matched Windows Gamma pair saves
  6.30% CPU time / 3.96% cycles; Original and Recompiled checks pass. Linux
  F Gamma saves 1.71% Linux CPU but loses 1.58% image throughput. Chaos-4's
  raw -22.49% CPU/+30.50% images is NOT qualified: it starts one update apart
  with different player Y and animation counters. No global/Deck gain is claimed.
  The user explicitly requests a new patch for a Deck test. Revision F now
  joins the normal native CPU group; private override 0 and diagnostics retain
  original owners. For this delivery, Linux EXTENDED and PREPARED_ACCESS are
  disabled, preserving previously shipped BASE RAM/transfer paths. Windows
  returns to retained RAM policy. All other unqualified experiments stay OFF.
  See `docs/cpu-update-native-objects-20260917.md` for actual delivery evidence.
  The earlier September 17 package, baseline and personal saves stay intact.
  The F arithmetic is unchanged from E; do not repeat the unchanged distance
  and lifetime suites. If F is unhelpful, the next substantial owner scope is
  `docs/native-movement-resolver-next-20260917.md`, not an unchanged repetition.

- Subsequent user steering explicitly requests testing related optimizations
  together. Complete model ownership and SIMD projection now form the default-ON
  `SARECOMP_NATIVE_MODEL_GROUP`, with global/per-feature zero overrides retained.
  On the release BASE-RAM configuration, their shared Gamma Linux run saves
  8.29% execution CPU/update versus the same object-enabled executable with both
  model features OFF. Versus the delivered Sep17 CPU patch, the combined object
  and model groups save 10.64% CPU/update and gain only 0.78% VM images/s; all 19
  endpoint fields match. This supersedes the separate model/projection OFF
  conclusions above for this jointly tested configuration, not for arbitrary
  combinations. Collision closure was additionally measured with the model
  group: exact endpoints, -0.68% execution CPU, +1.09% process CPU and -0.80%
  images/s. It remains OFF without a clear additional end-to-end gain. Never
  report any of these VM results as measured Steam Deck FPS. See
  `docs/cpu-update-native-objects-20260917.md` for the follow-up patch. Its final
  incremental Windows/Linux builds and installed Linux update pass both timing
  modes; Recompiled dual-path checks verify 23,130/20,003 projection batches.
  The update is `out/patches/SonicAdventureRecompiled-CPU-Update-2026-09-17-v2.run`,
  with separately named matching ON/OFF diagnostics switches. Actual application,
  idempotence, original-program backup and Story/Chao/settings preservation pass.
  Do not confuse this target with the intermediate 901962 test executable or
  overwrite the completed morning Sep17 update. The 20–25 ms Deck goal stays open.

- The user explicitly requested a full Steam Deck test installer for friends
  containing all current patches. It is delivered separately as
  `out/installers/SonicAdventureRecompiled-SteamDeck-Test-2026-09-17.run`;
  see `docs/steam-deck-test-installer-20260917.md`. Its stripped game has identical
  allocated runtime sections to the v2 CPU update and excludes the in-progress
  movement replacement. Full GDI installation, payload/content hashes, defaults,
  installed Gamma gameplay and matching diagnostics toggles are checked in Linux.
  A subsequent Deck crash without a capsule/reproduction remains unresolved;
  do not present the test installer as its fix or as a final release promotion.

- Current full test installers for all three platforms are dated 2026-09-17
  under `out/installers/`, with a shared English `INSTALLATION.txt`. See
  `docs/test-installers-20260917.md`: real full installation and installed Gamma
  gameplay pass on Windows and Linux; all original/payload hashes and reinstall
  settings preservation pass. Linux/Deck use the same stripped v2 runtime;
  Windows includes the newer movement code with its default OFF. Do not claim
  byte-identical cross-platform source snapshots or a new Deck performance gain.
  At the user's request, obsolete published installers/patches and reproducible
  staging/test-installation duplicates were moved to a desktop deletion folder,
  not deleted by the agent. `runs/cleanup-installer-moved-20260917.json` records
  25 entries / 9.56 GiB and their original paths. Older artifact paths in these
  historical reports may no longer exist. Current v2 patches and matching
  diagnostic switches are retained; user archives, baseline and saves are intact.

- The complete movement/contact owner `073018/0730A0..074214` is implemented
  as an internal-OFF group, including the full candidate/contact/position path.
  See `docs/native-movement-resolver-20260917.md`: Windows and Linux pass 216
  component cases and ten actual-AOT bridge cases each. Local restarts preserve
  the sparse global entry table, exact instruction/exception boundaries and
  callback-visible CPU/RAM. Windows gameplay ON/OFF and Linux ON/OFF pass.
  A matched Linux Gamma pair has the same 19 endpoint fields and 68 updates;
  execution CPU/update falls 2.52%, but process CPU rises 0.67% and new images/s
  fall 4.56%. Keep it OFF; do not claim a global gain, ship it enabled or repeat
  the unchanged pair. Related contact classification is inventoried but has low
  historical exclusive weight; no blind tiny-child expansion is justified.
  The next substantial alternative is the collision-list/hierarchy/polygon
  construction owner group described in the movement scope document.
  `out/native-objects-windows-20260917/game.exe` now contains this default-OFF
  code and is no longer the former Windows v2 comparison executable. The new
  Windows test installer explicitly records that distinction. Linux/Deck test
  installers and delivered v2 patch retain their previously qualified runtime.

- The complete collision-world group now joins the default-ON native CPU
  group on both platforms. See `docs/native-collision-world-20260917.md`:
  11 owners / 1,492 instruction PCs, shared admitted RAM and native indexes for
  active membership / first eligible occurrence. Both platforms pass 78 full
  CPU/RAM reference cases and eight actual-AOT fallback cases. Private restarts
  preserve the sparse global entry table and the root's exact return marker.
  Short matched Original Linux Gamma / Knuckles Sky Deck / Knuckles Lost World
  pairs each match all 19 endpoint fields and 68 updates. Execution CPU falls
  3.64% / 6.45% / 8.10%; new images/s change +0.62% / +3.63% / +9.54%.
  These are entry-window VM measurements, not Deck FPS or full-level guarantees.
  Global/per-feature zero overrides and diagnostics retain the original path.
  Existing installers, delivered v2 patch, baseline and saves stay unchanged;
  the 20-25 ms Deck goal remains open. Do not repeat these unchanged pairs or
  enable unrelated rejected experiments. The next related boundary is the
  complete matrix/transform SDK group described in the report, preserving
  arithmetic epochs and real callback boundaries. The retired NEAR-POLY path
  remains retired. Latest user scope: include both Knuckles stages, but keep
  testing short rather than expanding the full matrix.

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
