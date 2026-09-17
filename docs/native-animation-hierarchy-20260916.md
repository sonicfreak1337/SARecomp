# Native animation hierarchy

The user's current goal is to replace expensive SH4-shaped execution with
whole native PC/Deck operations, preserving gameplay and authored timing.
The first new semantic owner is PAL `8C057B00`: one model node and all of its
descendants, including animation key selection, position/rotation/scale,
matrix stack and output. Its own sibling is returned, never traversed.

`sonic_animation_hierarchy.cpp` contains hand-written structured C++ rather
than generated instruction labels. One invocation captures typed node/track
inputs before mutation and processes the complete tree without guest calls.
Reusable invocation storage avoids per-node allocation. Both Windows and
Linux compile the same implementation. The internal environment variable
`SARECOMP_NATIVE_ANIMATION_HIERARCHY=0` retains the old owner for diagnosis.
The September 17 shared native CPU candidate enables the qualified implementation
by default; see `native-cpu-defaults-20260917.md` for combined measurements.

The preparation scripts authenticate the PAL source closure and the retained
AOT owner, then insert one call at its public entry. Direct callers inside
the same compilation unit also reach this entry; merely adding a dispatcher
hook would miss those callers. Resume entries and rejected inputs keep the
retained implementation. Only its containing AOT unit needs recompilation.

Important semantic details retained:

- The global counter is a visit budget, not recursion depth. Increment wraps;
  exhausted visits clamp and return without producing a matrix.
- Channel count is mask bit length, not popcount; only two/three-channel
  authoring is admitted. Unsupported masks remain with the original owner.
- Two-channel missing translation and three-channel missing translation have
  different skip-flag rules. Rotation is Z/Y/X with signed-16 angle deltas.
- Key search, final-key wrap, FSUB/FMAC order, source bits, FP scratch and
  exception state retain the existing arithmetic contract.
- Stack leftovers and every ordered memory write are preserved. Siblings
  reuse the same stack frame without zero-initializing untouched locals.
- SDK push writes two matrix slots; pop restores RAM-backed XF and its globals.
- All pointer, alias, immutable-code, mode and observer checks precede mutation.
  No fallback or retry is permitted after the first write.

The first admission covers bounded RAM model trees and ordinary nonnegative
authored frames below 2^31, no trapping arithmetic or observing reads, and
adequate SDK matrix-stack capacity. Other inputs retain the original owner.

## Verification

The independent oracle executes the original PAL bytes in the retained SH4
interpreter. Its existing separately qualified FTRC source-register correction
is reused; the SDK source is not changed. 108 differential cases and ten
mutation-free rejection cases pass on both Windows and the Linux VM. Checks
include complete architectural state, all 16 MiB of RAM, ordered writes and
host MXCSR restoration. Cases cover nested children/siblings, skip flags,
track combinations, frame wrap, visit exhaustion and wrap, address aliases,
NaNs in matrix data, denormals and both admitted rounding/bank modes.

The shared SDK matrix math now uses the same FSCA/FTRV epoch per rotation
axis as the retained AOT. It does not extend this scope over the whole owner.
An independent arithmetic trace exposed a QEMU/TCG numerical discrepancy:
Windows gives identical results with individual operations or an outer
epoch; the VM differs in one trace value (`bc03634f` versus `bc03633f`).
The Linux reference therefore follows the actual retained AOT's axis scopes.
All Windows cases also pass against the per-instruction reference. Do not
disable product SIMD/FMA or loosen tolerances to hide this VM behavior.
The initial failed comparison stopped before RAM comparison, so it did not
establish that differences were confined to scratch registers.

Component success is correctness evidence, not an FPS result. The initial
hierarchy-only Gamma pair was slower: 344.663 versus 330.398 execution CPU
ms/update in the VM, with 20 updates each and a three-tick intro offset.
The axis-scope follow-up measured 345.073 ms/update and also provides no CPU
gain. Those early variants were retained internally for the broader native-model
work; they were not promoted on the basis of that measurement.

## Complete pose blending and memory publication

`sonic_pose_blend.cpp` additionally replaces complete PAL owner `8C0417C8`:
two-pose blending, translation, authored ZYX/YXZ rotation and scaling.
It now defaults ON in the September 17 group; `SARECOMP_NATIVE_POSE_BLEND=0`
retains the previous owner for diagnosis. This is
the same implementation on Windows and Linux. Preserve the authored
mixed-scale reads from the position arrays; they are not a transcription
error. Unsupported callback selections are rejected before any mutation.

Together the two owners pass 184 positive differential cases and 18
mutation-free rejections on both platforms. The new pose cases cover all
blend switches, both supported rotation callbacks and the admitted FPU modes.

After whole-owner range/alias/code checks, `sonic_native_model_memory.hpp`
can retain the existing qualified RAM-write capability for the closed native
invocation. Direct writes require matching backing/generation and the actual
guard-only registered product observer. Arbitrary observers keep the public
write API and its ordered events. There are no guest calls, callbacks,
mapping changes or observer changes inside the admitted direct interval.
Linux product-observer component runs exercised all 108 hierarchy and 76
pose calls through this direct route with exact CPU/RAM equality. Windows
ordered-observer tests used the original memory API and also passed.

The current combined candidate is SHA-256
`108184766c86a313534fdc18452e9d6694ffdac2f431b22161f7d48c017a2023`.
Its Linux Gamma gameplay run reached 375 hierarchy calls and 5,282 pose
calls, all using the qualified direct writer and with zero owner fallbacks.
None of these experiments replaces the installed CPU update of 2026-09-16.

## Combined gameplay measurements

Fresh same-executable Gamma gameplay comparisons use isolated input, copied
or isolated saves, hidden/muted rendering and diagnostics/provider timers OFF.
Linux uses product corner reuse, 800x500 with 50% rendering in the TCG VM;
Windows uses 800x500 with Vulkan offscreen rendering. These are distinct
machines/measurement environments, not cross-platform speed comparisons.

| Platform/window | Retained CPU ms/image | Both native CPU ms/image | Retained images/s | Native images/s |
|---|---:|---:|---:|---:|
| Linux, images 5..25 | 372.623 | 345.554 | 0.5432 | 0.5584 |
| Windows, images 5..125, old corner override | 14.844 | 14.453 | 60.009 | 59.978 |
| Windows, images 5..125, installed settings | 14.193 | 13.802 | 60.026 | 60.009 |

The Linux pair saves 7.26% execution CPU and increases whole-image throughput
2.79%. Both perform 20 updates; boundary player XYZ/HUD bits match, while the
intro leaves a three-tick absolute counter offset. This is a short VM result,
not a qualified Deck gain or the requested large global improvement.

Windows uses identical boundary game ticks 126..246, timer values and player
XYZ bits. Its coarse process/thread CPU clock suggests 2.63% less CPU work;
execution cycles fall from 67.745M to 61.113M per image. Both are capped at
60 Hz, so there is no measured FPS increase. This first Windows pair had the
benchmark's old explicit `indexed-corners=off` override in both variants.
The harness now defaults to `installed`, so subsequent runs preserve the
actual product setting. This test override never changed product defaults.
The subsequent installed-settings pair also has exact matching boundary
ticks/XYZ/HUD: 2.75% less thread CPU time and 2.62% fewer execution cycles
(63.327M to 61.671M/image). Both remain capped at 60 output images/s.

Evidence: `runs/gamma-native-model-{on,off}-20260916.json` and
`runs/native-model-windows-{on,off}-a/result.json`. The Windows executable is
`230ae0bdb9ea854307302d3ced22bbca00a12d8f9edb47765f6adf15c7138515`.
At that measurement checkpoint both flags remained OFF. The subsequent
combined qualification and September 17 default policy are documented in
`native-cpu-defaults-20260917.md`.

The additional Linux Original-timing Chaos 4 pair covers the boss introduction
(main 4 / scene 4), not the complete fight. Original PAL50/release2/delta2
remain intact. In the same 5..25 image window, retained/native execute 66/68
updates: 728.223/638.602 CPU ms/image, 220.674/187.824 CPU ms/update and
0.5608/0.5930 images/s. Native hierarchy calls rise from 44 to 180 with zero
fallback; pose blending is not used on this particular path. This is 14.89%
lower normalized execution cost and 5.74% greater image throughput, with
different final game ticks (88/90) and expected differing character Y position.
It is positive evidence for this native hierarchy in the introduction, not
an exact-state pair or a full boss/Deck qualification. Summaries are in
`runs/chaos4-native-model-{on,off}-20260916.json`.

## Native execution boundary

The oracle compares guest architectural data and ordered RAM effects. It does
not assert equality of translated-instruction accounting (attempt/retired
counts, pending pseudo-cycles or last-instruction diagnostic provenance).
The new owner is a native operation, as are the existing native math hooks;
it does not fabricate retirement events for instructions it no longer runs.
The retained AOT wrapper still produces the ordinary return/exception boundary.
The product's `NativePortAotServices` explicitly has no Dreamcast scheduler or
interrupt model: its sequence counter budgets host lifecycle/deadline polls.
Game time remains owned by the unmodified title update/release path. This
distinction must stay explicit when interpreting the test or profiler counters.
The finite admission limits also bound the callback-free native interval.

The first hidden Linux gameplay run has reached Gamma Emerald Coast, scene 15,
with 357 admitted hierarchy calls / 14,243 nodes and zero owner fallbacks at
relative image 16. This establishes reachability, not a performance result.
Windows and Linux game builds both completed incrementally; Windows closure
and FPU link audits passed. Neither build replaces an installed Deck patch.
