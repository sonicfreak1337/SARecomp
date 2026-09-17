# Closed native collision memory

Private experiment, disabled by default with
`SARECOMP_NATIVE_COLLISION_MEMORY`. `1` enables it in both timing modes and on
both platforms. It is not part of the delivered September 17 update.

## Implementation and boundaries

The already-native cross/normalize leaves, triangle-contact owner and contact
candidate owner now support direct RAM reads and immediate, ordered writes
inside their admitted intervals. The read-only three-load length leaf retains
its small existing path. This removes the general Memory operation from each
access; it does not replace gameplay rules or alter the simulation clock.

`sonic_native_collision_memory.hpp` captures the same registered product
observer capability as the qualified model-memory path. Entry still proves
the complete source bytes, CPU/FPU mode, addresses, write permissions and
aliases. Arbitrary stable observers, revoked observer generations, unsupported
memory and diagnostics retain the existing access path. Per-access addresses
are not cached between owners, and no source-identity cache is introduced.

The capability commits its exact memory counters and is discarded **before
every native or retained callee**. Existing post-call validation runs before
recapture. It is also destroyed on normal return and exception unwind; no
post-mutation failure can restart the original owner. No callback, mapping or
code-invalidation behavior has been removed.

## Qualification

Both Windows and Linux pass 1,667 cases each:

- 659 collision-math cases, 88 triangle-contact cases, and 43 candidate cases
  plus 25 candidate rejections and one interrupted candidate call, each in two
  observer configurations (1,632 checks).
- 35 access-width, alias, immediate-store, exact-counter and revoked-capability
  checks.

The product configuration must actually enter the direct path. The second
configuration replaces the registered observer and must retain ordered
observer events. Full CPU/FPU state and all 16 MiB of RAM are compared with the
installed original SH4 routines. Unchanged stores, arbitrary physical aliases,
special floating values, maximum contact counts, and interrupted/malformed
bridges are included. Existing, narrowly source-bound FTRC corrections in the
test interpreter remain documented in those tests; they are not game changes.

The Linux oracle uses the same original executor and source-hash checks as the
Windows tests. Test sources and decoders do not enter the game. The 3 original
test targets now link the prepared product Memory so a passing test cannot
silently exercise only the slower fallback.

## Artifacts and measurements

Both incremental builds retained the compiled AOT pack (zero AOT recompiles).
The Windows link audit also confirms the retained 894-entry RAM-region pilot
and the existing FPU provider ownership.

- Windows `out/collision-memory-windows-20260917/game.exe`:
  `5cb66914db2df1aaa9f31d8c282f161b1bad0430383506972eb31d77a800325f`.
- Linux `build-linux/game`:
  `13899c5718ce3d9500c85381706f24d47aca38ad434257cf54081e1ef882b3de`.

Pairs use these same executables with only the private collision-memory flag
changed. All other development experiments/settings are held fixed. Original
timing, hidden/muted isolated input, Gamma Emerald Coast gameplay and Chaos 4
entry are used. Windows measures frames 5..305; Linux measures 5..45 in the
four-vCPU TCG VM, with two software-raster threads. VM throughput is not Deck
performance. The user reports a qualitative Deck improvement from the delivered
native CPU approach but has supplied no new exact frame rate for this change.

Windows Gamma has matching endpoint state and native-work counts. Execution
CPU/image changes 21.718750 to 22.604167 ms (+4.08%), cycles +1.36%; output
remains capped at 25 images/s. This is not an improvement. The Windows Chaos 4
pair is not a qualified performance comparison: it starts one game tick apart
and reaches different ending scene/player states. Its lower raw CPU result must
not be presented as a speedup.

The two Linux pairs match all recorded initial/final scene, player, HUD,
cadence and native-call fields, with 136 game updates for each 40-image window:

| Scene | CPU ms/update OFF | ON | CPU change | New images/s change |
| --- | ---: | ---: | ---: | ---: |
| Gamma Emerald Coast | 219.489918 | 215.220451 | -1.95% | +1.41% |
| Chaos 4 entry | 227.502632 | 225.740634 | -0.77% | +0.03% |

The path demonstrably executes: Gamma's ON window has 102,942 intervals,
566,801 reads and 295,294 writes; Chaos 4 has 41,175 / 170,735 / 109,413.
OFF has zero direct accesses. All four Linux runs complete their exact window
and expected diagnostic stop. There are no concurrent builds, other gameplay
runs or large transfers during these measurements.

These small Linux differences and the Windows regression do not qualify a
global improvement or a new patch. The experiment remains OFF; do not promote
or repeat the unchanged pairs. The 20–25 ms Deck target is still unproved.

A final hidden Windows Recompiled Gamma check with the flag ON passes in the
same measured executable: 60 new images and 60 game ticks (126..186), with
60-Hz video, one release slot and logical delta 1. Its 59.98 new images/s is an
integration/timing check, not another improvement claim or a Linux/Deck result.
The owned VM executable/component uploads and input copy were removed after
evidence collection, reclaiming 1,812,417,252 bytes. The VM has 2,484,854,784
bytes free. Delivered binaries and the existing patch were not replaced.

Evidence: `runs/collision-memory-{windows,linux}-comparison-20260917.json`,
`runs/collision-memory-artifacts-20260917.json`, the corresponding per-run
`result.json` / `summary.json` and game logs, and
`runs/collision-memory-{win,linux}-*-mode{1,2}-20260917.log`.

## Next closure boundary

The Windows Gamma direct window records 533,442 capability intervals for
5,664,375 reads and 1,815,229 writes. That is evidence of frequent, short
intervals, not a measurement of capture cost. Both larger owners already
authenticate the complete cross/length/normalize code in their entry source
spans, but then release/revalidate/recapture around every such tiny callee.

A stronger follow-up can fuse these specifically reviewed, callback-free math
leaves into the admitted parent operation, retaining their exact arithmetic
order and live CPU results. It must prove every child's addresses against the
parent's ranges, preserve arbitrary-observer semantics, and still release the
capability/FPU epoch at every retained external bridge. Do not turn that into a
general cross-call cache or assume immutable-guard generation covers module
range removal. The retired NEAR-POLY owner remains retired; its old weak result
does not justify relinking it as another supposed new optimization.
