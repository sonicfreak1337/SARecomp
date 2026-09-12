# Next CPU performance audit — 2026-09-12

## Decision for Eggman

Prioritize **one fused RAM-access preparation in the measured AOT hot units**,
then **reuse of an already admitted generated dispatch entry**. Investigate
the independently costly **per-collection QSound execution** as a bounded
audio optimization, not as permission to discard tails. Only then prototype
operation-specialized FPU regions. There are four proposals below.

The September 9 estimate of 10–25% less CPU work was a planning hypothesis,
not a result. The September 11/12 Sonic measurements do not validate it.
Do not carry it into an r354-derived delivery claim. No substantial CPU
improvement has been established by the rejected adapter/FPU/compiler
experiments or the new Vulkan backend.

This audit performed **no builds, tests, game starts, devices changes,
baseline changes, or implementation edits**. Only this report is authored.
Interpolation remains withdrawn/compiled out. Original game cadence, all
stage coverage and executable fallback paths remain in scope for preservation.

## Scope and provenance

Repository: C:/Users/ultim/Desktop/Sonic Adventure Recompiled.
Branch inspected: enhancements/ingame-settings.
HEAD at audit start: d1e1a2246bd2258a09fb642667870af8ca767122.

Root was editing Options/audio/input/CMake and adapter identity/source.
Those shared edits were read only, not replaced. This is a source snapshot
plus historical traces, not an assertion that the current dirty sources
have been built or measured. The old Katana workspace was not edited or
used as the development target. Frozen .local/baseline/r354 was read only.

Important local references (paths relative to this repository):

- docs/performance-2026-09-11.md
- docs/vulkan-performance-2026-09-12.md
- docs/startup-performance.md
- docs/render-interpolation.md
- runs/perf-before-windy/ip.txt and ip-resolved.txt
- runs/perf-before-windy/stderr.log and stdout.log
- runs/perf-reference-final-ec/result.json and stderr.log
- runs/perf-final-ec/result.json
- runs/vulkan-{control,candidate}-{ec,windy}/result.json
- CMakeLists.txt, cmake/PinnedSdk.cmake and tools/prepare-motion-dispatch.py

Generated-source locations below refer to the retained working copy under
.local/working-product/generated/code/. They are **inspection inputs**, not
files to hand-edit. Any approved optimization must prepare a separately
bound build copy or explicit port-local source, preserving the original
archive and source manifest. Keep the original implementation selectable.
Line numbers are from the inspected source and can drift with Root's work.

SDK leaf copies .local/sdk-leaves/{memory,native_port_runtime,block_table}.cpp
were SHA-checked against the identities recorded in the existing extraction
recipe .local/performance-experiments/sdk/prepare-sdk-leaves.py:

- memory.cpp: 56806312c7d8fcdd5d5d33c0678397757ba0c52830566333f872cc8ba63db6f5
- native_port_runtime.cpp: 0ca5290d78f5e85c661377b52220376c9745ceab2c1251bcc169fcf09e39d3b8
- block_table.cpp: 563427eba975b85c4a62fde67c116601194dc477cdd32211ba666edeb8c60d09

The archived FPU and coverage source were read directly in memory from
.local/baseline/r354/katana-source-178448be.zip without extraction.
.local/analysis/native_port_sound_bank.cpp is text-identical to the archive
after CRLF normalization. Its on-disk SHA is
516a23e2ce99bdf9cac1dc47d6636c76896f11f6ca38d935c2287764832cae3b;
the archive bytes bound by tools/prepare-audio-buses.py have SHA
645484cf8e751a9a85a73607e21817f62ff6107211ac4dcaf8ebeb79011e0655.
These are different byte encodings, not two different DSP implementations.

## What is actually integrated

| Earlier recommendation | Current evidence/status |
| --- | --- |
| Local registers and grouped RAM work | Already present in frozen AOT: NativeAotRegisterFile, guarded direct reads/writes, write batches and FPU epochs. The next step is removal of duplicated preparation, not introduction of a first fastpath. |
| Dispatch caching | Already 16,384 dispatch entries and a 4,096-entry coverage cache. Primary static hits skip movable generation work; loaded hits compare mutable stamps rather than SHA strings. Do not just enlarge either cache again. |
| SIMD/FPU fast paths | Already in the pinned SDK and native model-transform provider. September 11 alternative FPU/ThinLTO/AOT flags were rejected, not delivered as wins. |
| Bulk model reads | src/native_title_adapter.cpp:3455 and 33810–33827 already stage finite point/normal arrays once per model invocation. Scratch capacity and output scratch are reused. |
| Persistent meshes | Implementation exists but enable_native_persistent_mesh_cache=false at src/native_title_adapter.cpp:779. Its comment records roughly 30 creates and nearly as many evictions per EC title frame under the synchronous API. Blindly enabling it would resurrect producer fences. |
| Startup/code-page preparation | Implemented, with real cache-cold/warm startup improvements. This is not proof of steady-state simulation improvement. No hard-fault-only/OS-cold measurement. |
| Vulkan | Implemented as optional backend; measured mixed CPU results, no consistent simulation gain. |
| FPR-local bodies / reduced native code footprint | Not established as integrated by these sources. Frozen AOT still calls common FPU helpers extensively. Compiler-only O3/ThinLTO/inline experiments were reverted. |
| Interpolation | Withdrawn on user request; prepare-motion-dispatch.py retains current hook bindings but injects no motion-owner tracing. It must stay off. |

## Existing measurements: what they do and do not show

All rows below used hidden/muted probes at 3182x1332, render scale 100%,
target output 144 Hz. CPU ms/frame is **sum of all process threads**, not
exclusive simulation-thread duration. First ten gameplay seconds are
excluded from steady metrics. Inputs are frame-indexed, so faster versions
can travel farther; host activity/clock variance is another confounder.

| Existing run | Sim FPS | CPU ms / sim frame | Interpretation |
| --- | ---: | ---: | --- |
| perf-before-windy | 19.602 | 61.668 | Reference, with detailed timing/IP sampling |
| perf-final-ec | 16.470 | 70.427 | Rejected adapter candidate |
| perf-reference-final-ec | 18.665 | 61.491 | Restored accepted reference |
| vulkan-control-ec | 16.049 | 73.465 | D3D11 control on newer build |
| vulkan-candidate-ec | 15.909 | 72.278 | Same build, Vulkan |
| vulkan-control-windy | 18.223 | 63.961 | D3D11 control |
| vulkan-candidate-windy | 17.603 | 66.147 | Same build, Vulkan |

Reference SHA: c15acd3199022ac41cac3721e2cfb7175e0c8fa5415524c3a2de67cdb343c9f4.
Rejected final candidate SHA: d6362d53276c40900a61e82990cafd5112941b3f61351a2d7291399608c5e8aa.
Vulkan comparison SHA: 18ab07aeff19775c6b00d1b0db4987008c91cc87273a72377c9d0c13fd2b325f.
These are historical build identities, not the current dirty executable.
Detailed adapter timing was enabled in the older comparisons, not the
Vulkan comparison; do not compare them as one controlled series.
The older initial result schemas omit the later passed field.

Windy IP trace: 835 samples in 13,089.698 ms, 29.0302 ms total suspension,
1.415 ms maximum suspension. The resolved text contains 780 counted samples.
Selected object totals calculated from that existing text:

| Object or unit | Resolved samples |
| --- | ---: |
| SDK fpu.cpp | 74 |
| AOT unit-v8C029400-8C029B00-9bed0201322da5d9.cpp | 44 |
| SDK native_port_runtime.cpp | 35 |
| Native title adapter | 35 |
| Generated native-port-dispatch.cpp | 35 |
| AOT unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp | 29 |
| AOT unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp | 26 |
| AOT unit containing shared ExplicitGuestInstructionAttempt | 26 |
| SDK memory.cpp | 24 |
| SDK native_bringup_coverage.cpp | 20 |
| SDK block_table.cpp | 16 |

These are location counts from one selected thread, not exclusive whole-game
percentages or call counts. Linker-attributed shared helper objects are not
proof that the corresponding guest address owns all sampled work.
No performance percentage is inferred from this table.

In late Windy 120-frame windows, existing work timing reports approximately
0.97 ms/model-transform work per title frame, 2.38–2.42 ms/model-draw work,
and 0.81–0.90 ms/audio-pump scope. Corresponding title intervals are about
49–53 ms. In late reference EC windows model-transform is about 0.79–0.81 ms
and model-draw 2.13–2.18 ms/frame. These bounded provider scopes do not cover
all translated object/collision work. Their aggregate timings may overlap.

Windy terminal render-producer wait is 43.40 ms and resource-fence wait
10.19 ms across the run, versus a broad AOT wall scope of 64.84 s.
This makes those measured waits a poor first target in Windy; it does not
exclude other graphics work. AudioServiceTotal is 8.70 s and EC's newer
D3D11 control reports 9.66 s, but these service wall scopes are not exclusive
CPU or pure QSound time and must not be added to AOT/provider/GPU scopes.

## Station Square and original-rate limitation

There is **no matched Station Square CPU profile** in the inspected reports
and trace inventory. The report in docs/render-interpolation.md is a user
motion-glitch report, not CPU attribution. Station Square occurs in
src/sonic_private_stage_scenarios.inc:287ff as selector inventory;
src/sonic_private_scenario_launcher.cpp:13–42 explicitly separates that
inventory from 32 launchable action stages. Passing its inventory name to
benchmark-stage.py does not establish a valid Station Square replay.

Use a copied real adventure save and normal story entry for the next
authorized Station Square measurement, preserving district/act, camera,
character and task state. Compare the same milestones, including one
transition and one settled movement window. Do not fabricate story flags
or a new direct loader just to obtain a benchmark. This audit starts none.

launcher/main.cpp:788–789 takes simulation cadence from definition.frame_timing,
separately from presentation. Original-rate investigation must establish
actual guest updates, animation, movement, timers, input and audio elapsed
time. Changing an output setting to 60 or reviving interpolation proves none
of these. At current EC control throughput, a purely proportional path
would need roughly 46.5% shorter intervals for 30 updates/s, or 73.3% for 60;
that is a budget illustration, not proof that the original title requires
60 identical update workloads. All-process CPU ms/frame is not interchangeable
with either deadline.

## Four bounded proposals

### 1. Fuse RAM preparation and consumption in the two measured AOT units

**Priority:** highest execution-cost target. **Effort:** medium/high.
**Risk:** medium with read-only fastpath first; high if broadened to stores
or across host callbacks. **Station Square:** candidate general mechanism,
not measured there.

Exact sources:

- .local/working-product/generated/code/unit-v8C029400-8C029B00-9bed0201322da5d9.cpp:
  native body starts at 102; guard acquisition at 119; resolve/can-read/can-write
  at 140–166; actual readers at 168–206; write helper at 226–278.
  The caller preflight at 1206ff is followed by instruction bookkeeping and
  another helper preparation. Source text is about 3.85 MB for this partition.
- .local/working-product/generated/code/unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp:
  second sampled partition, same generated helper shape.
- .local/sdk-leaves/memory.cpp:779, 917, 1284.
- .local/sdk-leaves/native_port_runtime.cpp:268, 368–395.

**Concrete change:** in separately prepared Sonic-local copies, replace the
boolean can-read result with a tiny prepared access carrying the resolved
backing offset/address and applicable state stamp. Consume it directly if
nothing observable occurred between preparation and use. Current generated
reads translate/validate once in can-read and again in the actual reader;
writes additionally call guest_write_observer_allows_prevalidated_linear_writes
during both preflight and consumption. Begin with ordinary aligned RAM reads
within one no-callback region; preserve the fallback reader unchanged.

For a later store variant, recheck after a flush/provider call and preserve
old/new byte comparison, ordered observer notifications, executable-alias
detection, exact partial-fault state and instruction/guest-cycle accounting.
The immutable guard already has a 256-byte page mask index; adding a first
page cache is not a new proposal. Do not remove the observer or assume that
a general data address can never become executable.

**Why not another O3 experiment:** the prior two/twelve-partition experiment
changed compiler flags. This changes redundant source operations with a
specific before/after native-code expectation. A source-level duplication
can still have been optimized away, so verify the bound native hot region
before retaining a rewrite. Avoid forced inlining everywhere.

**Authority/build boundary:** port-local prepared copies, source SHA manifest,
unchanged function signatures and original archive retained. AOT-body
replacement needs Eggman's explicit local adaptation scope; it is not a
request to edit Katana's emitter or rebuild the frozen SDK. Apply the general
transformation to all matching members in the selected scope, not a crash-PC
skip. Initial unit selection controls cost, not execution admission.

**Gain:** unmeasured; plausible leverage in 44+29 sampled unit counts and
RAM-helper samples, but no valid percent forecast. Reject if native calls/
checks are unchanged or paired runs regress.

### 2. Reuse the immutable generated entry after coverage admission

**Priority:** best smaller, independently buildable candidate.
**Effort:** medium. **Risk:** low/medium if ONLY the generated index lookup
is memoized; high if source/owner admission is bypassed.

Exact sources:

- .local/working-product/generated/code/native-port-dispatch.cpp:253932
  find_exact_entry, 253944–254052 existing 16,384-slot dispatch cache.
- Same file:258033 coverage admission, 258069–258098 selected_entry lookup/
  owner check, 258100ff primary/static return and movable selection.
- tools/prepare-motion-dispatch.py: current manifest-bound preparation.
- CMakeLists.txt: sonic_dispatch replaces the dispatch member before the
  frozen AOT archive. Motion annotation is already disabled.

**Concrete change:** after every existing coverage preflight, resolve
admission.dispatch_source through a bounded immutable-source-to-entry
memoization instead of repeating dispatch_index().find on every successful
preflight. Prefer reuse of the existing mapping machinery or a compact
last-entry fast hit inside find_exact_entry; do not accidentally interpret
source coordinates as runtime addresses through find_entry_from_source.
Retain owner-class checks and current coverage preflight on every transfer.
The generated array and its function pointers are immutable; only that
lookup is reusable. Any result that includes runtime-owner data must retain
binder/context/epoch invalidation. Measure conflicts before making a wider
cache, not after arbitrarily increasing TLS footprint.

**Evidence:** dispatch object has 35 sampled locations and coverage 20.
The source explicitly performs the lookup even after coverage-cache success.
No trace separates that lookup's cost from other dispatch work, so the
whole 55-sample group is not an expected saving.

**Gain:** small/unknown until measured. This can use the existing small
dispatch build and should not recompile 1,000+ AOT objects. No dispatch
target, native hook, Options binding or error path may disappear.

### 3. Reduce per-collection QSound overhead without cutting audible/state tails

**Priority:** independent CPU-load candidate, potentially important on cheap
hardware; not established as the main-thread FPS limiter.
**Effort:** small/medium for routing overhead; high for exact idle acceleration.
**Risk:** low for invariant hoisting; high for state sharing or pausing.

Exact sources:

- .local/analysis/native_port_sound_bank.cpp:3944 render_block; 3947–3953
  clears each collection's sends; 4069–4073 renders every live effect kernel;
  4082–4131 process_effect_block converts 16 buses and routes output.
- src/sonic_qsound_reverb_medium.hpp:35 State, 83 encode_ring_word,
  98 decode_ring_word, 308 initialize, 326 render, 369–439 snapshot lifecycle.
- src/sonic_qsound_reverb_medium_program.inc:128 statically specialized steps.
- tools/prepare-audio-buses.py currently changes bus/master gain only;
  Root owns this dirty source-generation boundary.
- tools/test_audio_buses.cpp contains the existing synthetic PCM/restore
  path; no test was executed by this audit.

**Confirmed multiplication of work:** each loaded collection with a kernel
gets a separate render call even without an active voice. Late Windy log
windows with five collections and two voices show audio_effect_frames
increasing by 1,353,875 while audio_rendered increases by 270,775
(about five kernel frames per audio frame; snapshot timing is not atomic).
The implementation executes 128 specialized steps per sample, so five active
kernels imply 28,224,000 such steps/s at 44.1 kHz. This is operation-count
arithmetic, NOT host instructions or a measured CPU percentage.
AudioServiceTotal includes more than this DSP work.

**Small safe first hunk:** decode each collection's 16 output pan pairs and send levels
from effect_outputs outside the per-sample loop (keep factors separate);
routing values are unchanged
during synchronous render_block. Keep scalar multiplication order and final
mix accumulation order. Track whether any send was added to the current
collection block; when none was added, use a pre-zeroed integer input span
instead of per-value llround/clamp. Still render the kernel and mix its tail.
Do not suppress sequence events, phase advance or output for a muted bus.
No cross-collection state sharing is needed for this first optimization.

**Why naive pausing is unsound:** decode_ring_word(0) evaluates to 4,194,304,
not zero. encode_ring_word(0) is 0x6000. initialize value-initializes ring
words to zero, and therefore starts a nonzero internal trajectory.
The reported PCM tail after three synthetic seconds is Eggman's existing
test finding, not newly reproduced here. No voices, zero current sends,
master mute, or one silent output block proves quiescence.

**Sound pause contract to investigate, not claim completed:** only a certified
zero-input state with all relevant temporary/memory state and decoded ring
history in a proven invariant can advance analytically. The 128-step
transition, delayed reads, rotating addressing, memory_decrement phase,
future impulse response, snapshots and effect_frames accounting must remain
equivalent. The cursor cannot simply freeze. The currently observed
startup/tail state does not satisfy a demonstrated certificate.
A threshold/timer-based fade or zeroing the initial ring to 0x6000 would be
an audio semantic change, not this optimization.

Identical kernel bytes permit shared immutable program data, which already
exists; they do not permit merging different collections' ring histories.
A later exact-state grouping could compute one block only for bit-identical
semantic states (including phase) with identical inputs, then materialize
each successor and mix each collection in the original order. It must cover
independent load times, note events, gain changes, unload, capture/restore;
state comparisons/copies may cost more than they save. Do not merge by hash
alone, omit a startup tail, sum inputs through nonlinear saturation, or
defer catch-up until the next note and call the resulting hitch a win.

**Gain:** no retained FPS estimate. Eliminating three of five truly equivalent
kernel evaluations would cut kernel-evaluation count by 60%, not total CPU
or game frame time. Equality/quiet-state incidence and DSP-only CPU cost are
not measured. Start with the small routing/conversion hunk; do not promise
that quiet collections can all be paused.

### 4. Specialize one FPU region, not the whole FPU runtime

**Priority:** after the first two execution changes; do not repeat a rejected
global compiler/FPU experiment. **Effort:** high. **Risk:** high.

Exact sources:

- .local/working-product/generated/code/unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp:
  HostFpuExecutionEpoch at 3690 and repeated fpu_binary Multiply calls at
  3724–3770; another Divide sequence at 3042–3109.
- Archived src/runtime/fpu.cpp:668–696 epoch enter/restore; 738–783 dynamic
  fpu_binary operation dispatch with existing normal-single fast paths.
- src/native_title_adapter.cpp:33244–33455 already has a native transform,
  one owner-scoped FPU epoch, and try_fpu_transform_vector_simd.
  Do not count that implementation as a missing replacement.

**Concrete change:** in one source-bound local hot-region adaptation, expose
constant operation/register operands to a specialized implementation of the
existing arithmetic, using local values and exact state publication. This
targets dynamic operation selection and repeated cpu.fr loads/stores at
known callsites, not weaker floating-point semantics. Keep fallback to the
current helper for exceptional modes/values. End the region on memory/
provider/exception/FPSCR/bank boundaries; preserve precise causes, enabled
traps, partial writes, delay-slot owner, PR/SZ/DN/RM and FPUL/T interactions.

Do not globally skip epoch restoration because entry MXCSR happened to
match: arithmetic flags and callbacks can change observable state. Do not
substitute fused arithmetic, blanket fast-math, or assume Xbox skip-LR/MSR
rules apply. Operation specialization and local dataflow are the meaningful
difference from the rejected plain ThinLTO/inlining-threshold experiments.

**Evidence/gain:** 74 SDK FPU samples and 26 in this partition identify work
but do not distinguish useful arithmetic from removable overhead.
Native model-transform scope was only about 1 ms/frame in sampled Windy;
even eliminating that whole provider would not solve the 50-ms interval.
No defensible total speedup forecast yet.

## Acceptance and coordination for the next authorized batch

1. Root finishes current Options/audio/input work. Reserve disjoint hunks;
   dispatcher, AOT-local copies and audio provider preparation have distinct
   owners. No further work is authorized by writing this report.
2. Small source/native-code check per candidate, followed by one coordinated
   product build and existing relevant hidden/muted checks when authorized.
   Retain original archive/body fallback and r354 snapshot. No baseline
   promotion, new full matrix, extra replay route, physical device/monitor
   switch, or interpolation.
3. Existing semantic fixtures must cover observer/provider mutation,
   executable aliases, exact fault/register state, generation retire/rebind,
   FPU exceptional cases and audio PCM plus post-resume snapshots as relevant.
   A fallback must continue current AOT, not manufacture a new admission stop.
4. Reuse benchmark-stage.py only for valid action-stage scenarios. Preserve
   EXE identity, settings, warmup, input and milestone alignment; do not turn
   tool flags or output FPS into original-clock proof. Compare a settled
   Station Square save route separately once available.
5. Measure CPU time/frame AND frame intervals/actual progress, not total
   machine CPU percentage. Current results are on a high-end RX7900XTX host;
   they do not validate inexpensive hardware, OS-cold behavior or 60-Hz
   timing fidelity. Keep gains below repeat variance unclaimed.

## Prior XenonRecomp rationale (not fresh upstream research)

The prior audit examined XenonRecomp ddd128bcca99fe8bfbb99bea583c972351fa6ace
and UnleashedRecomp cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c.
Their local-register, direct-call/load and native-library approach motivated
this review; it does not override Sonic's measured regressions or SH4 rules.

- https://github.com/hedge-dev/XenonRecomp/blob/ddd128bcca99fe8bfbb99bea583c972351fa6ace/README.md#optimizations
- https://github.com/hedge-dev/XenonRecomp/blob/ddd128bcca99fe8bfbb99bea583c972351fa6ace/XenonUtils/ppc_context.h
- https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecomp/misc_impl.cpp

**Handoff state:** report only. Stop after reporting to Eggman.
