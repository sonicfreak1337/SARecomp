# Recomp performance research: CPU work, simulation rate and interpolation

Date: 2026-09-12. Local branch: `enhancements/ingame-settings`; inspected HEAD: `d1e1a2246bd2258a09fb642667870af8ca767122`, with concurrent uncommitted enhancement changes. This is a report-only investigation. No build, game run, baseline modification, AOT regeneration or performance experiment was performed. Estimates below are engineering estimates, not measured speedups.

## Decision

Prioritize reducing the cost of **the existing game update**, beginning with an unambiguous update counter and exclusive CPU/wait measurements. The available measurements implicate generated execution and its helpers much more strongly than renderer submission. They do not establish that changing the renderer, enabling interpolation or removing a limiter will deliver real 60 Hz gameplay.

Keep interpolation **disabled**. The user's withdrawal and the documented whole-world matching failures are stronger evidence than successful isolated interpolation tests. Preserve r354, its AOT pack and original cadence. A real higher simulation rate is a separate, substantially larger gameplay-semantics project.

| Technique | What changes | What it does not prove |
|---|---|---|
| Optimize native/AOT execution | Less wall time for the same updates | Higher intended game-update frequency |
| Increase simulation frequency | More gameplay/camera/physics updates per second | Correct timers, speeds and collision behavior without adaptations |
| Interpolate scene transforms | Extra rendered states between game updates | Extra gameplay updates or reduced simulation cost |
| Repeat completed presentation | Output/display cadence | New world state or visual motion interpolation |

## Local evidence and measurement caveats

[`performance-2026-09-11.md`](performance-2026-09-11.md) records Emerald samples around 16–19 reported simulation FPS while output remains approximately 144 FPS. Broad AOT time dominates renderer submission; reported resource-fence time is small. The O3, register-inlining and SDK ThinLTO experiments were inconsistent or regressive and were reverted. Recommending blanket O3/LTO again would ignore this evidence.

[`vulkan-performance-2026-09-12.md`](vulkan-performance-2026-09-12.md) reports no consistent simulation improvement from Vulkan: Emerald D3D11/Vulkan 16.049/15.909 and Windy 18.223/17.603 reported simulation FPS. These are **historical sequential probes**, not new results against the current dirty source. They are not an identical deterministic replay comparison; GPU timestamps were unavailable. Summed process CPU milliseconds include multiple threads and are not simulation-thread wall time. Broad nested timing regions must not be added or used as an exact speedup ceiling.

There is an additional counter qualification requirement. `tools/benchmark-stage.py` derives simulation FPS from the report's frame delta. The adapter's presentation boundary increments `context.frame_index` even when presenting without a newly opened world frame. `service_frame_producer_until` uses 50/60 Hz video-slot deadlines and preserves original completion callbacks; those slots alone do not identify the gameplay-update rate. This does **not** prove every existing measurement is wrong. It means a real-60-Hz claim must correlate scheduler updates, new-world submissions and repeated presentations separately.

## What the other projects actually do

### UnleashedRecomp: actual frame-rate-dependent gameplay patches

The inspected revision is `cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c`. Its [FPS patches](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecomp/patches/fps_patches.cpp) address distinct units and ownership: force/camera delta time, smoothing, loading pacing, audio wait behavior, a boss counter maintained through fixed 1/30-second steps, and a wall-state per-frame constant. This is not simply a faster present loop. Even the smoothing correction contains game-specific choices, not a universally transferable formula.

Its [application update](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecomp/app.cpp#L43-L88) obtains swapchain pacing, establishes delta time and accumulated time, and updates audio. The portable method is to audit consumers of time and isolate fixed-rate subsystems. Sonic Adventure must establish its own PAL state/counter contracts; Xenon addresses, PPC ABI and Unleashed tuning constants are not evidence for them.

### XenonRecomp: reduce ABI bookkeeping where it is provably redundant

Revision `ddd128bcca99fe8bfbb99bea583c972351fa6ace` documents C++ recompilation and [register-localization/ABI options](https://github.com/hedge-dev/XenonRecomp/blob/ddd128bcca99fe8bfbb99bea583c972351fa6ace/README.md#L39-L63). Localizing ABI-preserved registers can remove context traffic and translated save/restore work. Its README reports substantial binary-size and frame-time benefits for Unleashed; those are author-reported results, not predictions for this port. [The actual Unleashed configuration](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecompLib/config/SWA.toml#L8-L15) enables these options.

SH-4 PR, delay slots, exceptions, hooks, floating-point state and indirect continuation visibility constrain any equivalent optimization. The useful idea is a proof-qualified ABI specialization, not deletion of observable context stores. This belongs in a future authored AOT/SDK change, not an adapter-only modification of the frozen pack.

Unleashed's [memory/function helpers](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecomp/kernel/memory.h#L19-L42) use a translated memory base and function-address lookup. Sonic already has guarded direct memory access. Its module generations, aliases and observer/lifecycle rules must survive any specialization; copying a permissive address lookup or mutable function map is not an admissible shortcut.

### Submission and iteration: transfer the ownership pattern, not another thread

Unleashed's [video submission](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecomp/gpu/video.cpp#L2773-L2850) gates swapchain waiting, submits work, rotates frame resources and waits for a fence before reusing the affected slot. Per-frame upload allocators are reset after that ownership boundary. The transferable lesson is precise pacing/fence accounting and safe resource reuse, not a general claim that more threads solve CPU-bound game execution.

[N64Recomp's README](https://github.com/N64Recomp/N64Recomp/blob/main/README.md) describes functionwise C output, direct calls, jump-table translation, overlay-aware lookup and patch objects linked ahead of generated archives. Its `main` README was inspected on the report date; unlike the revisions above, this link is not immutable. The overlay mechanism is a useful architectural comparison, not permission to discard Sonic's admission checks.

The local project already follows the fast-iteration principle: `CMakeLists.txt` imports the frozen generated archive and links a small `sonic_dispatch` override before it. `tools/prepare-motion-dispatch.py` verifies generated-manifest/source identities; currently it relocates includes and does not inject motion behavior. Preserve that authenticated boundary rather than manually editing generated code. Faster build iteration is valuable, but it is not a runtime FPS improvement by itself.

### Shipwright: matrix-history interpolation, not faster game logic

Shipwright revision `f04b570d2a08399a64d7cff294ad1c2e42871888` is a decompilation-based game port, not a Xenon-style recompiler. Its [interpolator](https://github.com/HarbourMasters/Shipwright/blob/f04b570d2a08399a64d7cff294ad1c2e42871888/soh/soh/frame_interpolation.cpp#L444-L540) records matrix operations, child paths and occurrence identity, carries recordings between frames, and produces replacement matrices. Camera epochs and actor-relative transforms matter. That increases visual cadence around the original update model; it does not make 20-Hz gameplay logic 60 Hz.

### Zelda64Recomp: broad identity tagging is a prerequisite

Revision `1a9c26613c6e0906140dc8bcca7362cbe00bf1eb` separates N64Recomp CPU translation from RT64 graphics. [Actor transform tagging](https://github.com/Zelda64Recomp/Zelda64Recomp/blob/1a9c26613c6e0906140dc8bcca7362cbe00bf1eb/patches/actor_transform_tagging.c) assigns actor/limb transform identities across opaque and translucent draws and accounts for initialization, deletion and skip conditions. [Camera tagging](https://github.com/Zelda64Recomp/Zelda64Recomp/blob/1a9c26613c6e0906140dc8bcca7362cbe00bf1eb/patches/camera_transform_tagging.c#L211-L267) handles camera continuity and both lists. These are targeted native graphics patches, not proof of higher physics frequency.

The relevance to Sonic is the amount of identity and discontinuity handling required. [`render-interpolation.md`](render-interpolation.md) already records partial world matches, missing/deformed draw coverage and mixed camera times. A callback address is not an instance identity; task identity alone is still not complete scene/deformation authority. Neither project justifies re-enabling the withdrawn prototype.

## Prioritized, concrete transfer plan

### 1. Establish authoritative update cost before changing cadence

**Scope:** existing benchmark/cadence diagnostics; adapter presentation boundary and `service_frame_producer_until`. **Estimate:** 0.5–1 day. **Risk:** low if bounded and capture-free in the timing window.

Correlate original scheduler/update completions, new geometry frames, repeated presentations and video slots. Measure exclusive main-thread execution versus pacing waits, plus separately aggregated worker CPU and GPU time where available. Use the same seeded save, same recorded inputs and same world interval. Frame-indexed forward input alone is insufficient when variants travel different distances.

Acceptance: counters reconcile; faster repeated output cannot inflate game-update claims; no extra guest callback or altered cadence. A sustained 30/60-Hz wall budget is 33.33/16.67 ms per relevant update, not summed CPU time across threads.

### 2. Optimize one measured hot dispatch/helper family at unchanged cadence

**Scope:** profile and exact native owner/helper boundaries first; current authenticated dispatch-preparation seam only if its structural contract supports the proposed change. **Estimate:** 0.5–1 day attribution, then 1–3 days for a bounded implementation. **Risk:** medium/high correctness risk, potentially strong relevance to the measured AOT hotspot.

Attribute `dispatch_native`, `runtime_only_call`, entry resolution and guarded memory operations separately. Distinguish lookup cost from the executed callee: a stack containing dispatch is not proof lookup dominates. The adapter's `SonicGuestReader` already acquires a direct linear-memory guard and retains scalar fallback; do not replace it with unchecked base-plus-address access.

For a confirmed frequently repeated whole-owner loop, prefer an exact native implementation that amortizes crossings while preserving all results, side effects and callback order. First check whether an existing native provider already covers it. No specific new collision/object owner is proven by this research alone. Require byte/owner/generation binding, PR/delay-slot/stop equivalence, memory-observer preservation and matched before/after measurements. Do not advertise a numerical speedup before that test.

### 3. Defer broad ABI localization to an explicit future AOT cycle

**Scope:** recompiler/SDK, not current frozen-pack adapter work. **Estimate:** several days or more, depending on proof coverage. **Risk:** high.

Xenon-style elimination of redundant context traffic is more plausible for the observed CPU bottleneck than renderer replacement, but requires SH-4-specific proofs and newly authored code. Compare CPU state, FP exception/rounding behavior, callbacks and discontinuities—not only output images. Previous broad compiler experiments provide no GO for a blanket optimization setting.

### 4. Verify pacing and resource ownership; do not assume a missing cache

**Scope:** `present_frame_after_title_cadence`, original title wait and per-slot fence accounting. **Estimate:** roughly one day to measure; implementation depends on the result. **Risk:** low for measurement, medium for changes.

The local code already bypasses generic pacing when title cadence owns the frame. A second wait is a hypothesis to measure, not a demonstrated bug. Likewise, the adapter explicitly disables its persistent mesh cache because synchronous create/evict operations publish and wake render prefixes, producing serialization. Simply enabling it is not a safe optimization. A useful cache would first need fence-owned create/retire behavior and measured upload reuse. Renderer submission is currently a lower priority than AOT execution.

## Gate for genuine higher simulation frequency

Only after the original cadence runs comfortably should a separate experiment audit every affected family: velocity/acceleration, per-frame increments, damping, animation sampling, collision/event callbacks, camera smoothing, fixed counters, audio scheduling and cutscene progression. Preserve fixed-rate islands where required, as Unleashed does, without treating its constants as Sonic authority. Verify real elapsed-world time and transitions across gameplay, menus and movies. There is currently no evidence for a safe global limiter change that yields correct Sonic Adventure 60-Hz simulation.

Interpolation remains off and outside this implementation sequence. Future reconsideration would require complete world/camera/instance/deformation continuity and explicit user authorization, not merely additional matching meshes.

## Boundaries and handoff

- No emulator, JIT, hardware replay or foreign runtime dependency is proposed.
- No edits to `.local/baseline/r354`, the old Katana tree, saves or generated AOT sources.
- UnleashedRecomp and Zelda64Recomp report GPL-3.0; XenonRecomp and N64Recomp report MIT. Shipwright's repository-level license API result was unresolved; inspect file/dependency notices before reuse. This report transfers methods, not foreign implementation code.
- Recommended next package: authoritative counters and exclusive hotspot attribution, then one proof-qualified CPU family optimization at unchanged cadence. Real higher simulation frequency is a separate decision. Renderer replacement and interpolation are not substitutes for that work.

Only this report was authored in this research task. Concurrent dirty source changes belong to the other ongoing tasks.
