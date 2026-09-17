# Scene-independent collision owners

The internal `SARECOMP_NATIVE_COLLISION_ALL_SCENES=1` experiment removes the
historical gameplay-state gate from the already-native candidate and triangle
contact owners. The retained diagnostic override and original private fixture
remain respected. All code, input, CPU/FPU, alias, capacity, callback and memory
admission stays inside the owners. It now defaults ON with the September 17
native CPU group; `0` retains the historical gameplay-only gate.

The earlier suspicion that direct AOT calls bypassed native collision hooks
was disproved. `native_chainable_entry` excludes both owners correctly. The
real policy gap is the early inactive-scene return, which used to occur before
the original-call counters. New `*_scope_blocked_calls` counters make that
explicit. The later Gamma active-chain profile reaches main 4 / scene 4 and
the old gate is false; its translated collision owner is genuinely active.
The earlier short Gamma gameplay window, where the gate was true, did not
exercise this condition. These observations do not conflict.

Read-only review of the entire owner and callback closure found no dependency
on scene ID, cadence or camera style. The invoked helpers perform retained
RAM/math/copy work; no arbitrary title callbacks are introduced. Negative
debug admission, 96-candidate bound, stack/FPU checks and the original
16-contact path that skips Pop remain unchanged. Any incomplete retained
call after mutation still aborts rather than continuing the original owner.

This change never drops logic updates, reduces original timing or broadens
gameplay activation. It only selects the equivalent native implementation
where the existing complete-owner admission proves it safe.

Windows Gamma Recompiled 5–1605 passes with all-scene admission. Compared with
the same executable/render-owner ON and old gameplay gate, 1,688 updates and
final XYZ/HUD are exact. Execution CPU is 15.410156 versus 15.419922 ms/image:
this is not a meaningful additional performance win. The new path finishes
with 25,210 native candidate calls, 3,239 triangle calls and zero scope blocks.
Original Gamma 5–125 also passes with PAL 50 / release 2 / delta 2.

Linux Gamma Original 5–25 passes in a combined render-owner/scope comparison.
Its differing update counts and state make it insufficient to isolate this
switch's benefit; see the render-context report. The policy was never a bypass
of native math inside already-admitted gameplay.
