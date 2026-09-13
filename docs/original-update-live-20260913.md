# Original update cadence: live evidence and callback ownership

The current hidden Emerald Coast fixture executes about **59.63 original task
traversals/s**, while producing **17.74 new images/s** and **143.73 output/s**.
These are three separate counters. This is not a completed 60-frame rendering
enhancement, nor proof that every character/animation/event callback runs at
60 Hz. The original scheduler already performs multiple passes per image.

## Scope and source

Source is the installed PAL boot, SHA256
`b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af`.
The small port-only diagnostic observes existing authenticated native leaves;
no original AOT unit, timer arithmetic, callback, register effect or save is
changed. The INVERSE arithmetic candidate remains selected; its default is OFF.
Interpolation remains withdrawn. The immutable r354 product is untouched.

`SARECOMP_UPDATE_TIMING_TRACE=1` additionally requires the existing hidden
background and gameplay-probe flags. Observation starts only after the private
stage fixture becomes active. All reads use the existing guarded RAM reader.
No memory watchpoint, per-instruction hook, physical input or image capture is
added. A fixed 4,096-event prefix is printed after the measured window; totals
continue for the entire window. Disabled normal play does not collect it.

The first timer sample at PR8C04E73C follows coroutine scheduling8C099990 and
the task traversal8C0987E4 inside body8C04E714. The second at PR8C04E782 follows
8C09DEA4 in the same body; it is a consistency witness, not another update.
Both calls target the existing periodic-sample provider8C06C0C2. The saved
return word at the body's unchanged stack pointer identifies the actual
wrapper. The actual elapsed FR0/FR15 result and wait argument are observed at
the existing elapsed/wait leaves. Nothing is inferred from output FPS.

## Measured route and correction to the earlier component

Live gameplay uses **8C04E95A**, returning from the common body to8C04E9A0.
It uses threshold1900 (31.667 ms), wait return8C04E9FE and elapsed return8C04E9DE.
It calls post8C08A3FA and then8C08A664 once per outer wrapper. The previously
tested adjacent8C04EA4A uses threshold1850 and only the first post service.
They must not be treated as interchangeable.

`tools/test_legacy_video.cpp` now executes both SHA-bound original wrappers.
The alternate PAL phase cycle below threshold is2,2,3,2,3 passes over five
wrappers; at/above1900 it is3,3,4,3,4. Its extra post call, pressed-button union,
callee-saved registers, stack, return, threshold and wait arguments are checked.
Existing five video-option, four persistence and two constructor cases still
pass. Log: `.local/menu-preview/update-loop-contract-02.log`. This remains an
orchestration component with deterministic callee fixtures, not full physics.

The initial live run `original-update-live-01` established the alternate caller
and60.04 task traversals/s, but only the primary wrapper's elapsed/wait returns
were instrumented. Its zero elapsed counters were **missing coverage**, not a
zero timer or absence of extra work. The final trace covers both callers.

## Final live trace

`runs/original-update-live-02/result.json`, `update-timing.json` and
`update-contract-analysis.json` retain the evidence. Executable SHA256:
`f923b9c064d8706d8d17088d8415b1fcc148da8e31fe0d6b07c19a706f7b2161`.
Build: `.local/menu-preview/build-update-timing-04.log`; retained ten-entry
ownership and native-link closure passed. Only the adapter/launcher were
incrementally rebuilt; no full AOT compile.

The hidden/muted run used copied saves, isolated physical devices with normal
remapping and the existing forward fixture, D3D11,1280x720,100%,VSync off,144
output target. Sixty gameplay seconds, first ten excluded from steady rates.
It completed its planned stop, without a runtime fault or forced termination.

| Steady result | Measured |
| --- | ---: |
| New images / title boundaries per second | 17.7449 |
| Original task traversals per second | 59.6254 |
| Traversals per image | 3.3601 |
| Output presentations per second | 143.7280 |
| Execution CPU milliseconds per image | 54.8379 |
| Extra-update threshold taken | 96.04% |
| Timer setup calls / outer boundaries | 883 / 883 |
| Host periodic callbacks during those boundaries | 0 |

All sampled clock witnesses retain PAL50,TVword1,release2,delta2 and the original
post-PAL checkpoint owner. There is no newly applied50/60-Hz preference.

The retained prefix contains367 complete wrappers: four with two passes,218
with three and145 with four. The script `tools/analyze-update-timing.py` checks
their exact event order, PAL phase, actual threshold, wait arguments, caller,
iteration and following image boundary against the executed original contract.
All pass. Its three final partial events are explicitly excluded;7,933 later
events exceed the bounded prefix capacity. No guest read failed. Full-window
counters still reconcile task samples, second samples and wrapper ownership.

Elapsed time at the second-pass test is28.824/35.867/88.443 ms minimum/median/
maximum in that prefix. The source-authored extra pass is consequently expected
under the measured CPU load. It must not be removed as supposed duplicate work.
This run is instrumented and the PC is in use: it does not establish an extra
performance gain over the separately measured INVERSE comparison.

## Original render callback was misclassified

One bounded Astra/high helper audited this independently and finished; root
checked its source anchors and the local Flycast interrupt definition.

1. Setup8C06C032 registers8C06C118 through8C64127E, which supplies slot zero to
   8C641202. The registration destination is `(*8C67BF44)+0x230`; initialization
   8C651000/1002 establishes base8C8A2C94, giving callback slot8C8A2EC4.
2. IRQ owner8C652150 reads ISTNRM at8C652160/2162. Instructions2192:2398 and
   219A:E107/219C:2C12 test mask4 and acknowledge the render bits. Code226E..2274
   sets pending bit zero. Dispatch23D4..23F6 selects slot8C8A2EC4 through the
   FP-preserving bridge8C652494.
3. Local Flycast `core/hw/holly/holly_intc.h:15` identifies bit2 as TSP render
   completion. The reference resides under the old Katana workspace at
   `reference/flycast-oracle-v27-1-src`; it was read only.
4. Original8C06C118 samples TCNT1 into8C78C544 and invokes8C604486. The latter
   publishes flags8C88F710=0 and8C88F718=1 and optionally invokes`*8C6733B8`.
   Neither inspected callback resets TMU1.
5. Setup writes TCR1=0 at8C06C01C, so TMU1's interrupt-enable bit0x20 is off.
   TCOR1/TCNT1 receive12,500,000 atC024/C02C. Flycast TMU mode0 uses200MHz/16,
   so the counter has a one-second period. **Its period is not the originating
   event for the registered render callback.**

The title reset owner8C053940 loads8C06C000 at5395C, calls it at5396A and can
branch back at539A2. The final live counters now prove one setup/reset per
outer gameplay boundary on this route. Native `service_periodic_host_time`,
however, schedules604486 by whole seconds since that repeatedly reset epoch.
The measured zero periodic callbacks follow from that mismatch. The current
comments describing a one-second title callback are incorrect.

This is a verified callback-source defect; its visible impact is not established
by the update count. A safe correction must associate the original notification
with its own completed guest render submission, preserve ordered RAM/callback
effects, and avoid treating host-only Options/movie/repeated images as new guest
render completion. Neither an arbitrary callback per update nor resetting the
timer again at image publication follows from this evidence. Production callback
behavior remains unchanged pending that complete submission/retirement contract.

## Remaining work

The immediate engineering targets are now concrete: reduce the measured work
per original update/image, correct the separately proven render-completion
notification owner, and determine how original rendering can publish suitable
states between existing updates. A60-output setting alone cannot do this.
Original PAL compensation, gameplay inputs, post services, scene boundaries
and saves remain part of the timing contract; no universal60-Hz claim is made.
