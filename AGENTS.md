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

- The complete collision-world SDK extension remains internal-OFF after full
  Windows/Linux qualification; see `docs/native-collision-world-sdk-20260917.md`.
  Ten SDK owners share the world's RAM and preserve exact partial-fault private
  continuations. Both platforms pass 78 world / 450 SDK comparisons and 8 world
  / 18 SDK actual-AOT cases. The first Linux arithmetic discrepancy was fixed
  by preserving helper-local SDK FPU epochs; no tolerance was weakened.
  Matched Linux Sky Deck improves 7.32% CPU / 20.63% images, but Lost World costs
  5.72% CPU / loses 9.27% images. Gamma has differing endpoints and supplies no
  qualified gain. Keep the extension OFF and its parent world group ON; do not
  ship it or repeat unchanged tests. Current development artifacts are
  `build-linux/game` and `out/world-sdk-windows-20260917/game.exe`; the delivered
  v2 patch and installers remain unchanged. Next is the distinct complete
  live render/motion hierarchy group, retaining mutable callbacks/draws.

- The live render/motion tree `040784` and its complete SRT/sampling/matrix
  closure are implemented as an internal-OFF group; see
  `docs/native-render-hierarchy-20260917.md`. Both platforms pass 198 exact
  CPU/RAM cases and ten actual-AOT continuation cases, including the completed
  tail-callback invalidation correction. Windows Original endpoint state matches
  and Recompiled retains 60 Hz; Linux gameplay qualification is still running.
  Do not promote from component success or the small Windows gain alone. The
  next coherent scope is the distinct two-pose render tree described there.

- The first render group now has matched Linux Gamma/Sky Deck/Lost World
  comparisons: -6.78%/+9.66%/+3.34% execution CPU, with mixed/worse throughput.
  It stays OFF. The entire two-pose tree and 16 connected owners now extend
  the same closure to 39 owners. Both platforms pass 306 CPU/RAM cases and
  19 actual-AOT continuation cases. These include ordinary-call PR aliasing
  and pending original dynamic tails; preserve these distinctions. Combined
  gameplay qualification is pending. See `docs/native-render-hierarchy-20260917.md`;
  do not promote from component success or rerun the unchanged first-group pairs.

- Latest delivery request: continue broad native function-group work and build
  the next Deck patch on September 18, 2026 around 05:20 Europe/Berlin. The
  one-shot heartbeat `sarecomp-deck-patch-18-09-05-20` is already scheduled in
  this thread. Do not create a duplicate. Deliver qualified changes only,
  briefly verify actual Linux patch installation and preserve all saves,
  settings and diagnostic ON/OFF switches. Delete that automation after
  delivery. This authorizes the next patch; the previous Sep17 v2 remains.

- The complete 39-owner render/motion group now defaults ON in both timing
  modes, following scoped-proof revision H. See the final section of
  `docs/native-render-hierarchy-20260917.md`: 310 exact CPU/RAM + 19 real-AOT
  cases on each platform, and matched Linux Gamma/Sky/Lost endpoint states.
  Execution CPU/update changes -8.48%/+0.37%/-6.93%; images/s -0.19%/-1.20%/
  +7.24%. Sky is neutral and these are not Deck FPS. Reuse proofs only within
  one root operation and revoke them at every foreign callback. Preserve exact
  source-write rejection and original partial-fault continuations. Do not
  repeat unchanged pairs. Revision E's Lost World result overlapped a build
  and is excluded. Inverse-trig qualification is recorded below.

- The complete seven-parent inverse-trig group now defaults ON, together with
  its four existing children. See `docs/native-inverse-trig-20260918.md`:
  1,319 exact instruction cases and 92 actual-AOT cases pass on both platforms.
  An existing old-child host-FPU-scope mismatch was reproduced and corrected
  without relaxing assertions. Preserve the exact original local epochs.
  Matched Linux Gamma/Sky/Lost pairs with the other native groups active save
  6.39%/6.01%/2.47% execution CPU per update; images change +0.19%/+0.04%/+4.53%.
  All 16 selected endpoints and 68 updates match. These are VM measurements,
  not Deck FPS. Global/per-feature OFF and diagnostics keep original owners.
  Do not repeat unchanged component or gameplay comparisons. The subsequent
  complete rigid/render-family qualification is recorded below.

- The corrected complete 40-owner render family now includes the rigid tree
  by default after the matched three-scene revision-C Linux comparison. See
  `docs/native-rigid-hierarchy-20260918.md`: Gamma/Sky/Lost execution CPU per
  update changes -10.62%/-3.63%/-1.32%, images -0.76%/+0.57%/+2.04%, all 16
  endpoints and 68 updates match. The isolated revision-D rigid addition had
  regressed Lost World and must not be confused with this complete-family
  policy. Root admission now checks only owners actually entered; all source
  proofs are still revoked across foreign callbacks. Static range coalescing
  reduces 211 checks to 118 with proven identical byte coverage and unchanged
  generated bodies; it is included in the final net comparison, not C's hash.
  The additional 14-owner morph group remains private-OFF: it passes 502 total
  exact / 33 actual-AOT cases on both platforms but the two reviewed discovery
  windows have zero morph calls. Do not infer a speedup or repeat zero-call runs.
  See `docs/native-morph-hierarchy-20260918.md`. The inverse-trig closed-memory
  extension remains private-OFF after matched Gamma/Lost comparisons: execution
  CPU -1.94%/+0.26%, images -0.61%/-0.85%. Its 1,518 exact and 158 actual-AOT
  cases pass on each platform, but there is no useful whole-frame gain. Do not
  repeat these unchanged pairs. The complete qualified groups with that extra
  extension OFF save 13.37% execution CPU/update in the preliminary matched
  Gamma comparison against delivered Sep17 v2; images gain 1.77%. This is not
  a Deck FPS claim. The final package/installed checks are recorded separately
  in `docs/cpu-update-native-groups-20260918.md`.

- September 18 update delivered ahead of the requested 05:20 deadline. Runtime
  source is commit `2e745a5`; see `docs/cpu-update-native-groups-20260918.md`.
  The exact tested package is
  `out/patches/SonicAdventureRecompiled-CPU-Update-2026-09-18.run` (262,349,464
  bytes), with matching dated diagnostics ON/OFF switches. Program SHA-256 is
  `55889c92ee5c5ab6a34f1d8733a8698bab6da3b20eaa416fa48b593c0b335f71`.
  Actual Linux application passes both supported bases, Story/Chao/settings
  preservation, policy preservation, backups and repeated installation. Both
  timing modes pass actual installed Linux and hidden Windows checks. Final
  direct-v2 Gamma/Sky/Lost pairs match all 16 endpoint fields and 68 updates:
  execution CPU -11.55%/-5.73%/-8.05%, new images +0.33%/-0.71%/+8.94%.
  Use these final installed results, not the preliminary 13.37% Gamma result.
  These are short VM measurements, not Deck FPS. No further unchanged tests
  or full matrix are needed. The uncaptured Deck crash and 20-25 ms target
  remain open; do not claim the overarching native-work goal complete.
  `docs/native-groups-next-profile-20260918.md` records the next full
  model-submission and movement/contact boundaries. The related model-packet
  follow-up with the newly qualified hierarchy has byte-exact Windows output
  but only -2.81% execution CPU and unchanged Linux image throughput; it stays
  OFF. Do not repeat that unchanged comparison or revive isolated micro-work.

- The model-to-GPU vertex-stream experiment remains private-OFF; see
  `docs/native-model-vertex-stream-20260918.md`. It replaces expanded corners
  with immutable indexed points across the ordered Vulkan consumer, retaining
  original guest projection/palette effects and callbacks. Windows and Linux
  each pass 256 exact vertex cases plus 20 exact GPU image cases in serial and
  parallel modes; Linux Khronos validation is clean. D3D11 deliberately retains
  its expanded fallback. The matched Windows Gamma pair changes execution
  cycles only +0.38% (neutral); Linux is substantially slower with a one-update
  start difference and is not exact-work evidence. No useful serial game gain
  exists. Do not repeat unchanged pairs, broaden this test matrix, implement a
  D3D11 gather path or enable it in a patch. Current isolated outputs are
  `build-linux/game` and `out/model-vertex-stream-windows-20260918/game.exe`;
  the delivered Sep18 patch is unchanged. Next is the connected movement /
  NEAR eligibility / TOUCH candidate / contact-classification group, rather
  than re-enabling any old isolated movement or NEAR switch.

- The connected 37-owner movement/contact family is private-OFF; see
  `docs/native-movement-contact-20260918.md`. Windows and Linux each pass 164
  full CPU/RAM comparisons and twelve actual-AOT continuation cases. Matched
  Windows Gamma saves 4.08% execution cycles; matched Linux Gamma saves 4.67%
  execution CPU/update, with neutral (-0.04%) image throughput. No Deck gain
  or promotion is established. The active window still crosses 12,022 foreign
  calls; review the complete remaining math/copy closure before another test.
  Do not repeat unchanged pairs or enable the retired isolated movement path.
  The delivered September 18 patch and saves remain untouched. A subsequent
  complete 61-owner revision adds the connected inverse-angle, matrix/vector
  and whole-record-copy operations. It passes 276 CPU/RAM and twenty actual-AOT
  cases on each platform, including overlapping copies and code-write stops.
  Matched Windows Gamma saves 6.69% execution cycles but no thread CPU and
  costs 10% more process CPU. Matched Linux Gamma saves 7.03% execution CPU,
  with neutral process CPU and -2.96% images/s. This is not a whole-frame win.
  The bounded Knuckles pairs are complete with all 16 endpoints / 68 updates
  matching: Sky saves 4.08% execution CPU but loses 2.89% images/s; Lost saves
  15.96% execution CPU / 11.17% process CPU and gains 18.03% images/s. No Deck
  FPS is measured. Keep the group private-OFF while the next full model-submission
  composition is qualified; do not repeat these unchanged three scene pairs.
  Do not rerun the unchanged Gamma pair or component cases. Current artifacts
  are `build-linux/game` and `out/movement-contact-closure-windows-20260918`;
  see the report for exact hashes. Three replaced Linux VM experiment programs
  were removed to free 5.25 GB; their reports/assets/installations remain.

- The whole model-submission composition remains private-OFF after bounded
  qualification; see `docs/native-model-submission-20260918.md`. It connects
  the complete hierarchy to 03700C/037098/037108 and native renderer-context
  capture/commit, sharing admitted RAM/source proofs only until a real guest
  call. Preserve the reciprocal source-write fences under narrow immutable
  guards. Windows passes 218 model CPU/RAM and 19 hierarchy cases; Linux
  passes the new 73 model and 19 hierarchy cases. Windows Gamma is pixel exact
  with the 61-owner contact group also ON. Separate no-capture Windows timing
  reduces execution CPU 5.88% and process CPU 8.25%. Linux Gamma reduces
  execution CPU/update 6.51%, but image throughput is neutral (+0.05%). Those
  are combined-group results, not additional gains over contact alone.
  Lost World completes, but its starting game tick differs by one; reject its
  timing percentages. Do not rebase its counters or repeat unchanged Gamma
  pairs/component suites. No new patch or default promotion is warranted yet.
  Current outputs are `out/model-submission-windows-20260918/game.exe`,
  `build-linux/game` and its staged Linux copy. Delivered binaries stay intact.

- The private model-submission follow-up now includes the complete visibility
  and material owner, using the shared RAM/FPU operation in 4:3 and widescreen.
  See `docs/native-model-visibility-20260918.md`. Parent admission now covers
  all six GBR publications; the normal GBR=8C8FFE00 intentionally aliases the
  mask inputs, which must stay live and ordered. Windows/Linux each pass 253
  actual-cull CPU/RAM cases; the Windows 4:3 frame is byte exact and all 5,811
  model calls stay closed. Linux Gamma with contact ON in both sides saves
  18.73% execution CPU/update and gains 23.70% new images/s with exact selected
  endpoints. This pair uses the default software-raster worker setting, not
  the earlier LP_NUM_THREADS=2 setting; do not compare absolute costs across
  them or sum toggle percentages. Lost World matches but is neutral in CPU
  (-1.02%) and slower in images (-3.78%). Sky Deck has a one-HUD-tick start
  offset and different positions/palette work; exclude its percentages.
  Keep the groups OFF pending a useful broader composition. No unchanged
  reruns or full matrix. The actual game outputs are build-linux/game and
  out/model-submission-windows-20260918/game.exe, plus the stripped Linux copy
  under out/model-visibility-linux-20260918. The earlier Windows candidate
  filename is reused; use the new visibility executable manifest for its SHA.
  The delivered September 18 patch is unchanged. The remaining measured Gamma
  hierarchy callback 040784 -> 0D209C has real character-node transform work;
  it must not be deleted or treated as an unnecessary diagnostic callback.

- The private model-submission group now also composes all three direct model
  roots and lends its admitted memory/source proof to palette/context children.
  See `docs/native-model-roots-20260918.md`. Windows/Linux each pass 94 new root
  cases and 73 affected shared cases; the Windows Gamma capture is byte exact.
  All 27 allocated ELF sections match the staged Linux program. Both groups
  OFF/ON Linux pairs match all 16 endpoints and 68 updates, with no resumes or
  revocations: Gamma execution CPU -7.04%, images -0.80%; Lost World CPU -2.71%,
  images +1.98%. These explicitly use LP_NUM_THREADS=2; do not compare absolute
  times to the preceding default-worker visibility pair or sum percentages.
  This is no substantial throughput win. Keep both groups private-OFF; no
  unchanged reruns or new export. Current SHA bindings are in
  `runs/model-roots-executables-20260918.json`, staged Linux under
  out/model-roots-linux-20260918; the private Windows filename is reused.
  The delivered September 18 patch remains untouched. The separate current
  profile is complete; see docs/native-model-roots-profile-20260918.md. Its
  1,457 samples have exact resolved-leaf cross-checks, but perf record exited 1;
  disclose this limitation rather than claim a clean full-duration recording.
  Next is the common land task/list/display/transform family 0519C0, 051E56,
  051F64, 052048 and 0520C8. Preserve initialization, live list order and mutable
  callbacks; do not repeat small model-copy changes or the already-native
  collision-world producer. The original camera is also present in this profile.

- The common land display/model composition is implemented, private-OFF;
  see `docs/native-land-render-20260918.md`. Its twenty added owners connect
  0519C0, static/animated lists, visible-list preparation, transforms and the
  existing shared model/motion operation (74 total owners). Preserve live list
  order and foreign callbacks. The matrix restore has temporary paired FMOVs;
  the final bridge also preserves actual retained return/tail provenance.
  Windows/Linux each pass 73 final CPU/RAM/AOT cases. The initial hidden Gamma
  frame is byte exact; both game targets build incrementally. Initial/final
  executable manifests are runs/land-render[-final]-executables-20260918.json;
  the current Windows filename is reused. Staged final Linux is under
  out/land-render-final-linux-20260918, SHA a59a814b9f074036b02ed85ed5a195c4faac7c65d1a4323c83f299fff03ee2cb.
  The combined movement/contact + model + land OFF/ON Linux pairs use Original
  timing and LP_NUM_THREADS=2. All 16 endpoints and 68 updates match, with no
  resumes/revocations: Gamma execution CPU -9.48%, process CPU -2.96%, images
  +5.56%; Sky Deck execution CPU -8.16%, process CPU -1.02%, images -0.45%.
  These are combined VM measurements, not incremental percentages or Deck FPS.
  Keep the groups OFF pending a broader throughput result. Do not rerun the
  completed pairs/component suite unchanged or build another patch for this
  result. Continue substantial connected native work; the September 18
  delivered patch/installers, baseline and saves remain untouched.

- The complete object-collision pass is now private-OFF under
  SARECOMP_NATIVE_OBJECT_CONTACT=1; see docs/native-object-contact-20260918.md.
  Its 53 additional owners connect the live category lists, broad/narrow
  shape tests, transforms and hit registration (114 movement/contact owners
  total). Preserve live list ordering and real callbacks. Source proof bits
  now cover the full inventory; public-entry selection is distinct from
  retained-continuation membership. Actual return/tail provenance is retained.
  Windows/Linux each pass 105 CPU/RAM/AOT cases; hidden Windows Gamma is
  byte-exact. Both games build incrementally. The current SHA binding is
  runs/object-contact-executables-20260918.json, staged Linux under
  out/object-contact-linux-20260918; the private Windows filename is reused.
  The combined movement/contact + model + land + object OFF/ON Linux pairs
  use Original timing and LP_NUM_THREADS=2. All 16 endpoints and 68 updates
  match, without resumes/revocations: Gamma execution CPU -15.34%, process
  CPU -3.28%, images +0.05%; Knuckles Lost World execution CPU -13.37%, process
  CPU -7.23%, images +11.59%. These are useful combined CPU reductions, not
  incremental percentages or Deck FPS. Keep the groups internally selectable;
  do not discard the CPU improvement because Gamma VM image throughput is
  neutral. No default promotion/new export or unchanged reruns from this
  bounded comparison. Continue substantial connected native work. The
  September 18 delivered patch/installers, baseline and saves are unchanged.

- The common original camera operation is now private-OFF under
  SARECOMP_NATIVE_CAMERA_OPERATION=1; see docs/native-camera-operation-20260918.md.
  Its 33 additional owners connect 019F4A, the measured camera handlers,
  adjustment/transition modes and shared geometric helpers (147 contact/camera
  owners total). The existing Recompiled pre-update/publication hooks and real
  scene callbacks are preserved. Windows/Linux each pass 50 CPU/RAM/AOT cases;
  the hidden Windows Gamma frame is byte exact. Both games build incrementally.
  Current binaries are bound by runs/camera-operation-executables-20260918.json;
  Linux staging is out/camera-operation-linux-20260918, SHA
  9ff104baff27941026fa52ff8197d8ff845920e4fd692764e538d2a1f3822866.
  The private Windows filename is reused. The combined five-group Gamma Linux
  pair matches all 16 endpoints and 68 updates: execution CPU -17.59%, process
  CPU -5.01%, new images +4.21%. Sky Deck completes but has a one-game-tick
  initial offset and different palette work; exclude its raw percentages.
  Both scenes run 68 native camera updates without resumes/revocations.
  These are combined TCG results, not incremental gains or measured Deck FPS.
  Retain the work internally; no default promotion/new export or unchanged
  reruns. Continue substantial native groups. The older profile is not an
  updated distribution after these changes; computed-dispatch actor families
  remain unconverted and require reviewed original local targets. The delivered
  September 18 patch/installers, baseline and saves remain untouched. Only an
  older verified duplicate VM game was removed; its local copy is preserved.

- Fresh all-five-group profiling and actor-state author preparation are recorded
  in docs/native-actor-state-preparation-20260918.md. The Linux Gamma recording
  is clean (perf exit 0, 1,226 samples, 1 unresolved game leaf, exact raw-leaf
  cross-check). Use this distribution, not the older pre-land/object profile.
  Actor roots 0CBD40/0FDC20 have 318 inclusive samples, but 209 already contain
  native descendants; their 25.94% is not a predicted gain. A complete 32-owner
  0FDC20 state closure now authors with its actual 13-state table, twelve reviewed
  BRAF/BRA transfer sites, exact FPU scopes and original local resumes. Inventory:
  runs/native-state-transfers-20260918/actor-scope.json. Branch mechanics pass 50
  cases on each platform. This is preparation, NOT a gameplay-integrated actor
  feature or measured speedup. Next integrate the larger connected actor/state
  work with the existing model/contact operations, including the 0CBD40 chain.
  Preserve PR/frame across state transfers and do not replay a completed delay
  slot on child fallback. No unchanged profile/retest or new export for this
  intermediate result. All 231 existing generated group files and both private
  continuation C++ files are byte-identical; released artifacts remain unchanged.

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
