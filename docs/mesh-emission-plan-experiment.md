# Indexed emission order experiment, 2026-09-14

Decision: retired from the game. Preparing the complete first-use/index order
did not establish a repeatable improvement over the accepted topology/UV source
cache. The original source cache from `b3d3468` remains active; no emission-plan
vectors, extra admission checks or batched vertex emission remain in the product.

`tools/experiments/mesh-emission-plan.patch` preserves the audited implementation
and its tests against `b3d3468`, before retirement. It is a historical experiment,
not a patch to apply blindly to later providers. The generator's source identity
and provider metadata must be reviewed/refreshed when reproducing it.

## What was implemented and verified

The plan retained first-use `(point, global authored corner)` pairs and complete
relative output indices, respecting the retained `CornerIndices` reset per
polygon, reversed-strip order, UV seams and provoking order. Each live draw
still invoked the same original corner builder in the same sequence; current
position, normal, color, UV/fog handling and output queue budgets stayed live.

Admission required the actual indexed-corner gate, validated current source plan,
no extra indexed-corner reconstruction diagnostic, sufficient full logical index
budget and zero current clipping flags for every referenced point. Missing flags
or clipped points used the existing triangle loop. Destination index storage was
reserved before live corner work. Append/allocation failures never retried after
partial work. Extra cache vectors counted toward the existing LRU payload limit.

The component suite passed 69 cases, including the retained CornerIndices oracle,
explicit reversed-strip first-use tuples, 32 mixed multi-polygon/seam fixtures,
live clip/OOB admission, full logical-index budgets, append failure and injected
reserve failure before any live append. Log:
`.local/menu-preview/mesh-emission-component-test.log`.

`runs/mesh-emission-verify-gamma-d3d11-01` ran the ORIGINAL polygon and UV decoder
with its original CornerIndices implementation. It compared every semantic
append argument, final first-use count, output vertex count and complete index
vector. The run completed without mismatch: 37,029,445 first-use vertices,
63,020,085 indices, 26,154,662 triangles and 50,098,192 UV values. Both verification
flags selected the original decoder; the extra indexed-corner reconstruction
flag excluded the batching oracle rather than hiding its additional calls.

Candidate game SHA-256:
`efbb6a746c0b635cc715085ff39d3c210d46640da2ee624cafdadd84b60e6c59`.
Provider: `b1271e31d741aeed9451f620ca146072795783568ad6e79e4c6781bb4202c384`.
Build `.local/menu-preview/mesh-emission-game-build-01.log` took 74.302 seconds,
zero retained AOT recompiles, with native-port and both FPU audits passing.

## Performance decision

Same executable, Gamma Emerald Coast, D3D11, 3440x1440, Recompiled, hidden/muted,
isolated forward input, 60 seconds excluding the first ten. Existing native
motion/matrix/collision/corner and source-plan optimizations stayed enabled.

| Run under `runs/mesh-emission-` | Passed | Real draws/s | CPU ms/draw | Cycles/draw |
| --- | --- | ---: | ---: | ---: |
| batched-gamma-d3d11-01 | yes | 51.0390 | 17.7529 | 78,340,257 |
| retained-gamma-d3d11-01 | NO, exit fault | 50.9839 | 17.8914 | 78,454,001 |
| retained-gamma-d3d11-02 | yes | 50.8337 | 17.8307 | 78,882,810 |
| batched-gamma-d3d11-02 | yes | 50.7789 | 17.8686 | 78,721,334 |

The failed run is not an accepted timing comparison. In the fully passed second
pair the candidate uses slightly more execution CPU and produces slightly fewer
real frames; its small cycle-count advantage is insufficient evidence of useful
headroom. No further tuning of this loop was promoted. Stable 60 simulation FPS
with reserve remains unachieved.

## Host fault encountered while measuring

The first retained-emission run completed its 60-second fixture and intentional
deadline, then exited with `0xC0000005`. Windows Application Error event 1000 at
04:57:52 +02:00 identifies the same game PID 13060 (`0x3304`), `amdxx64.dll`,
fault offset `0x177E05`. Batched emission was disabled in that run. This identifies
the fault location, not a proven driver bug or a complete causal diagnosis.

`runs/mesh-emission-exit-trace-gamma-01` repeats that configuration with the new
private exception tracer. It attaches to the benchmark-owned process and exits
normally (game's intentional-deadline exit 1, tracer exit 0). The subsequent
untraced retained/batched runs also exit normally. The isolated AMD-module fault
was not reproduced or claimed fixed; the traced run is not timing evidence.

The EXCLUDE_FROM_ALL `sonic_exception_tracer` target is optional via benchmark
`--trace-exceptions`. It verifies the supplied process image, captures a bounded
host stack/module list on an access violation, heap corruption or second-chance
exception, and lets the original handler run. It neither terminates the game nor
writes a memory/save dump. Normal performance runs have no debugger attached.
The benchmark records attachment/exit success separately and stops its owned
game before retiring a failed helper.

## Restored product

The final game restores the accepted model/mesh implementation. Its model source
is text-identical to b3d3468 after normalizing line endings; the patch endpoint
normalized six context-line endings, so the actual source/provider hash changes.
The emission experiment is preserved only in the offline patch above.

Final executable SHA-256:
`a348e0f10c05a8bbc6f44af1e3a9277b97057a17399c3eb3ad59516ce3c7e0d9`.
Provider: `815e03f86d65ed325654b2688297d51bb34dcd673f42369204b29bc0118d5ee1`.
`.local/menu-preview/mesh-emission-retired-game-build.log` passes the native-port
and FPU link audits in 73.882 seconds, with zero retained AOT recompiles.

The hidden/muted Sonic check `runs/mesh-emission-retired-sonic-01` stops at its
intentional deadline (frame 415), with zero runtime contract failures. The viewed
frame-750 capture shows Sonic, Tails, Emerald Coast and the HUD without a visible
regression. The exception tracer attached to owned PID 6008 and observed normal
deadline exit 1, returning 0 itself, without a host exception. No completed stage
or cure of the isolated AMD-module shutdown fault is claimed by this short check.
