# Shared native prefixes across FPU stack operations

This development continuation extends the existing internal extended RAM
experiment on both Windows and Linux. Its source defaults remain OFF; the
separate development caches explicitly enable it. The shipped September 17
patch and accepted product remain unchanged.

The native prefix can now cross scalar FMOV predecrement stores and FLDI0/FLDI1
constant loads. Previously an FPU stack save interrupted a run of adjacent
integer stack saves, even when all addresses were ordinary writable RAM.
The same transformation applies to all 41 already selected source units;
there are no character, stage or timing-mode special cases.

## Behavior retained

The entire retained generation and each source unit remain SHA-bound. New
shapes require their exact original instruction bodies. FD/SZ and, for FLDI,
PR legality are checked before entering the prefix. Paired FMOV and illegal
FPU modes use the original code. Raw register payloads are not converted to
host floats. Predecrement publishes the pointer only after the store succeeds.

Existing mapping, scheduler, read-guard, observer and immutable-code admission
still govern every accepted access. A miss commits only the completed prefix
and resumes the original failing instruction. Exact instruction counts,
cycles, memory counts, provenance, T/PR and register values are retained.
Interior resume labels continue to bypass the new prefix.

Unattempted FPU instructions inside retained host epochs are deliberately
unchanged. Their surrounding group carries counters and final instruction
metadata; treating the individual lines as ordinary one-cycle ALU operations
would silently change the contract. They require a separate whole-group owner.

## Qualification

Both Windows and Linux pass 5,040 component comparisons. Seven authenticated
original/transformed game witnesses cover 116 instructions, including mixed
GPR/FPU stack stores, a constant followed by dependent stores and FPU stack
saves followed by arithmetic. Every witness executes both a complete prefix
and partial completion. The new cases include raw NaN/denormal/signed-zero
payloads, SZ/PR/FR combinations, FD, exception enables, reserved bits, scheduler
mutation, invalid addresses, code writes, stale guards and observer changes.
The complete CPU state, RAM, access counters, fault provenance and callback
logs match the retained instructions.

The same source scope grows from 1,413 to 1,490 prefixes and from 22,030 to
23,853 enclosed instructions (+8.28%). These are static coverage numbers,
not measured frame-rate improvement. Evidence:
`runs/ram-fpu-coverage-20260917.json`,
`runs/ram-fpu-component-{win,linux}-20260917.log`.

## Windows gameplay

The incremental build and map audits pass: 894 selected entries come from the
41 prepared units, with no retained selected library members also linked.
The executable is `out/ram-fpu-windows-20260917/game.exe`, SHA-256
`2ab7029600b65fd8c53c23586b7279ef5f603d92e492af4f7f1da3b0a464fa50`.
Control is `out/matrix-bulk-windows-20260917/game.exe`, SHA-256
`68436e67494fdee8fb06556410f2a3af2179008dd441ddca3c5509882bacea86`.
Projection batching and matrix bulk remain OFF in both executables.

Hidden/muted probes use Original timing, isolated input, Vulkan offscreen,
800x500 at 50% rendering and a 300-image window (5..305). Gamma explicitly
waits for gameplay before measuring; both boundaries are scene 15. No compiler
or other timing benchmark runs concurrently. Both runs complete normally,
match game ticks 136..856, XYZ/HUD, native-call and memory counters, and retain
PAL 50 Hz with release/delta 2.

| Gamma gameplay | Control | Candidate |
|---|---:|---:|
| Execution CPU ms/new image | 23.22917 | 22.44792 |
| Execution cycles/new image | 102,073,678.67 | 100,550,612.80 |
| New images/s | 24.99833 | 24.99841 |

This pair saves 3.36% execution CPU and 1.49% cycles. Its image rate is capped
by Original cadence, so this is CPU headroom, not increased output FPS.

Both Chaos 4 entry probes also complete, but their initial game ticks differ
by one and their subsequent scene/respawn states diverge. Their apparent CPU
change is not a speedup comparison. No claim is made from that pair.
Evidence: `runs/ram-fpu-windows-comparison-20260917.json` and its source runs.

A separate short Recompiled function check also passes in Gamma gameplay:
both boundaries remain scene 15 with native gameplay math enabled, 60 Hz,
release 1 and logical delta 1. Sixty new images advance game ticks 126..186.
It uses the ordinary compiled candidate with no RAM-enabling environment
override. This is a mode/behavior check, not a performance pair (the VM binary
transfer ran concurrently). Evidence:
`runs/ram-fpu-win-recompiled-functional-20260917/result.json`.

## Linux gameplay and disposition

The four-vCPU TCG VM uses LP_NUM_THREADS=2, Original timing, isolated input,
800x500 at 50% software Vulkan rendering, telemetry/diagnostics OFF, installed
native RAM/transfer defaults and image boundaries 5..45. Control is the previous
extended binary `d6af269a24900cb9a56f762dc3b1c225fce6b6ac158b524e3cc151c5723d70c3`,
preserved at `out/ram-fpu-control-linux-20260917/game`. Candidate is
`build-linux/game`, SHA-256
`b78bf72fe4543d1a970ab304dbbedf7ac8ee842df03b6d7a005bf1d9d78f98fc`.
Generated source outputs, region intervals and preparation SHA match Windows.

Order is Gamma control, Chaos 4 control, Chaos 4 candidate, Gamma candidate.
No compiler, other timing benchmark or file transfer runs during these probes.
Every run completes 40 new images and 136 updates and stops at its expected
deadline. Both pairs match boundary XYZ, scene, HUD ticks, game clock,
native-call counts and closed-memory counts. PAL release/delta 2 are unchanged.

| Scene | Control CPU ms/update | Candidate CPU ms/update | Control images/s | Candidate images/s |
|---|---:|---:|---:|---:|
| Gamma Emerald Coast gameplay | 197.30116 | 220.92166 | 1.106164 | 0.904220 |
| Chaos 4 entry | 224.51079 | 218.34105 | 0.899989 | 0.903118 |

Chaos 4 saves 2.75% execution CPU with only 0.35% more new images/s. Gamma
costs 11.97% more execution CPU and loses 18.26% image throughput. These are
single paired VM observations, not confidence intervals or Deck predictions.
The earlier matrix-OFF Gamma control was slower than this fresh control; it
must not be substituted to manufacture a favorable comparison.

The extension therefore remains experimental and OFF in source defaults.
Its functional qualification and positive Windows Gamma result do not justify
a release change in the face of the Linux Gamma regression. No installer or
patch is produced. Development caches explicitly contain the new extended
path; they must not be mistaken for the delivered product. The next native
work should examine generated register pressure/publication and whole FPU
group boundaries, not broaden this transformation to all units blindly.

Evidence: `runs/ram-fpu-linux-comparison-20260917.json`, the four source
`runs/ram-fpu-linux-{gamma,chaos4}-{control,candidate}-20260917.json` files
and corresponding game logs. The delivered Sep17 CPU patch is unchanged.
