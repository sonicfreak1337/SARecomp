# Complete stack sequence experiment

Private Linux candidate, **not enabled in product defaults or installers**.
`SARECOMP_LINUX_STACK_FRAMES` defaults OFF. The retained r354 pack and installed
builds are unchanged. This is a larger optimization unit than the rejected
per-store prefix: it replaces all instruction wrappers inside a successfully
admitted sequence while preserving the actual guest stack traffic.

## Scope and contract

The source-bound preparer recognizes complete ordinary-instruction envelopes
for integer MOV.L pushes/pops and PR saves/restores. It verifies the original
opcode/operands, two-cycle cost, exception owner and remaining envelope via
four normalized hashes. The full retained artifact manifest is also verified.
Consecutive sequences must have uninterrupted, contiguous PC/resume labels.
All original source remains intact; only a reversible success-prefix is added.

The current PROFILE scope has 41 common units, 670 sequences (357 push, 313
pop), and 2,628 original instructions. Actual groups contain 2–8 operations;
the helper supports and tests up to 16. This is source coverage, not a measured
CPU percentage or a claim that every group is reached during gameplay.

Admission requires owned registers, no pending guest-write exit, the original
address translation and an aligned contiguous RAM range without physical or
backing wrap. Pushes additionally require the original function-entry read
guard and observer preflight, exclude active deferred write batches, and
use the qualified NativePort observer-pair capability. Code, read-only image
and current dynamic executable ranges are queried at each group admission.
No arbitrary observer contract is sufficient. Access sinks, watchpoints,
trace handlers, stale mappings/observers and diagnostic mode retain the
original path. No R15 destination/source, SR/FPSCR operation or delay slot is
combined.

On admission failure, no register, RAM byte or counter changes. Original
instructions then retain partial progress and exact fault provenance. On
success, no fallible callback remains: the group preserves bytes, register/PR
values, attempted/retired counts, pending cycles and final instruction-PC
provenance. It does not change `cpu.pc` or exit metadata. Interior resume
labels continue to enter the untouched scalar sequence.

## Qualification

`tools/test_stack_frames.cpp` compares the group against scalar guest memory
operations plus the original attempt/provenance implementation and scheduler
boundaries. The final corrected Linux component result is:

```
SONIC_STACK_FRAMES_OK cases=1876 push_hits=185 pop_hits=289
registers=exact ram=exact counters=exact faults=exact observers=exact scheduler=exact
```

Cases include 2/3/8/16-word groups, P1/P2/physical aliases, RAM mirrors, backing
wrap, unaligned/unmapped addresses, protected code/data, newly added dynamic
code, stale guards, observer changes, watchpoints/traces, user/MMU modes,
counter wrap, physical-PC fallback, access sinks and diagnostic disablement.
Scheduler cases change R14, PR and SP before the first scalar access and prove
the group rejects both a newly captured read-watchpoint guard and a stale one.
Evidence: `runs/stack-frames-scheduler-component-20260916.log`.

Peer review found that the first prototype admitted pushes using only a fresh
write view. A read-only watchpoint invalidates the original read preflight but
can leave writes available, so that version could skip a required cycle flush.
Its 1,588-case oracle also omitted preflight flushes; those passes did not
qualify scheduler semantics. Both helper and oracle were corrected, and the
source generation is now pinned in addition to individual envelope hashes.

## Gameplay comparison

Exploratory build log: `runs/stack-frames-build-20260916.log`. Both modes used
SHA-256 `6253e095449d60ece727803ac3af3f3031d803a638cdcbf3550580af0a545e73`.
This is the first prototype, before the scheduler correction; it is not a
qualified delivery candidate. No installer was created. The old scalar
experiment remained disabled in both modes.

The Linux harness exposes `--stack-frames original|fused`, recording the
selection in its result. Tests use isolated empty saves, hidden/muted Vulkan,
Original game timing, 16:10 culling, telemetry/diagnostics OFF and a fixed
5–25 gameplay-boundary window. No builds run during timed windows.

| Gamma Original timing | New images/s | Updates | Execution CPU ms/update |
| --- | ---: | ---: | ---: |
| Original sequences | 0.409157 | 65 | 276.246 |
| Grouped sequences, unqualified prototype | 0.414981 | 66 | 262.967 |

Both runs completed the requested window and expected host deadline without
a forced stop. The image-rate difference is only +1.42%, with different update
counts; the prototype also lacks the corrected admission. It establishes no
useful qualified gain. Evidence: `runs/stack-frames-gamma-{original,fused}-20260916.json`.
Do not spend another full pair or expand the scope based on this result alone.
The corrected helper has component qualification only, not a new gameplay A/B.

Keep the CMake option OFF and restore the private comparison slot to the
existing 6344 prepared-transfer control. Source and component evidence remain
available for a future larger owner. No product defaults or installer change.
A VM result does not predict Steam Deck frame times or satisfy the 20–25 ms
target by itself; there is still no positive net-gain claim since 22:00.

The retained Linux cache is restored to STACK_FRAMES=OFF. Reusing the original
objects required three archive operations and one link, without AOT compilation.
The stripped local output matches control SHA-256
`6344e07c5c9090c15f163f9a4ba6b303d91499d4baedc627460936d524bcd8bb`.
