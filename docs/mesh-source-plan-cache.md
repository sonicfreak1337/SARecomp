# Authored mesh topology and UV plans

The Recompiled gameplay path reuses decoded CPU-side BasicAttach topology and
TitleBasic UVs. Original timing retains the existing path. This is independent
of the retired render-interpolation feature and the disabled persistent GPU mesh
cache. It does not cache transformed geometry, lighting, materials or GPU objects.

## Source and lifetime contract

`sonic_mesh_plan.cpp` owns copies of the consumed polygon and UV bytes, decoded
triangle winding, global authored corner identities and signed-16 TitleBasic UVs.
Every hit checks the complete current 24-byte descriptor, point count, corner
budget and exact live source bytes. A mapping generation or a hash alone is not
an identity check. Reloads at the same address and in-place edits rebuild the plan.

The adapter admits only the current unobserved direct-reader scope, with no guest
writes or callbacks between lookup and expansion. A fresh descriptor must still
match the local type/count/stream/UV arguments captured by the retained decoder.
No guest pointer is stored; the borrowed plan ends before the submit callback.
Missing ranges, invalid topology, arithmetic overflow, resource limits and
allocation failures fall back to the retained decoder before emitting geometry.

The gate excludes environment mapping, flat shading, normal/cache diagnostics,
and SDK exceptional/header/constant/float/vertex-color contracts. Live positions,
normals, primary/secondary colors, transforms, clipping, materials and fog still
use the existing per-draw path. Authored UV seams, per-polygon corner reuse,
triangle order, provoking order and the existing output budgets are preserved.

UV conversion uses the existing `sonic::model_uv::decode` operation and its exact
TitleBasic literal. The plan key includes relevant guest FPSCR and host MXCSR
rounding/denormal modes. Unmasked host FP exceptions decline. The existing UV
execution epoch restores full host MXCSR; skipping a repeated decode must not
change guest FPSCR or host status. No approximate reciprocal or FMA is introduced.

The default LRU allows 512 entries and 16 MiB of retained plan payload, with a
512 KiB per-plan payload limit. These limits do not include map/list overhead or
temporary cold-build allocation. Owner reset clears the cache; move operations
also reset the moved-from accounting, so both objects remain reusable.

## Controls and verification

The product enables the cache only through the existing Recompiled gameplay
gate. `SARECOMP_MESH_SOURCE_PLAN=0` selects the retained control without rebuilding.
`SARECOMP_MESH_SOURCE_PLAN_VERIFY=1` runs the original polygon/UV decoder and
compares its output with the cached plan. The benchmark exposes these choices as
`--mesh-plan cached|retained|verify`; they are development controls, not menu items.

`sonic_mesh_plan_tests` passes 26 cases: triangle/quad/forward/reverse-strip order,
UV seams and bits, source/descriptor mutation, same-address reload, unreadable
ranges, malformed counts/indices, overflow/budgets, guest and host FP modes,
unmasked-host fallback, LRU limits, moves and owner reset. Test log:
`.local/menu-preview/mesh-plan-component-test.log`.

The real-game run `runs/mesh-plan-verify-gamma-d3d11-01` also enables the existing
indexed-corner oracle. It completes without a mismatch, comparing 26,117,903
triangles, 77,543,532 UV values and 27,788,252 reused complete vertices. It records
1,875,604 source-plan hits, 2,790 misses, 2,404 source changes, zero declines and
zero evictions. This run is a correctness check, not a performance measurement.

## Matched performance, 2026-09-14

Gamma Emerald Coast, D3D11, 3440x1440, Recompiled gameplay, isolated forward-input
fixture, hidden/muted, 60 seconds with the first 10 seconds excluded. Both paths
use the same executable and active native motion/matrix/collision/corner paths.
The run order is cached 01, retained 01, retained 02, cached 02. All four complete
normally; no compiler or second game runs during measurement.

| Run under `runs/mesh-plan-` | Real draws/s | Execution CPU ms/draw | Cycles/draw |
| --- | ---: | ---: | ---: |
| cached-gamma-d3d11-01 | 51.3183 | 17.7596 | 77,781,412 |
| retained-gamma-d3d11-01 | 50.8601 | 17.9081 | 78,760,407 |
| retained-gamma-d3d11-02 | 50.5998 | 18.0312 | 78,925,650 |
| cached-gamma-d3d11-02 | 51.1679 | 17.8152 | 78,140,056 |

Mean execution CPU decreases from 17.9697 to 17.7874 ms/draw (1.01%); cycles
decrease 1.12%. Real draw throughput increases from 50.7299 to 51.2431 (+1.01%).
The improvement repeats in both orderings, so the cache is retained as a small
global saving. These measurements do not establish 60 simulation FPS or reserve,
and nominal 60 output FPS must not be substituted for real draw throughput.

Executable SHA-256:
`c48e046d7f7b50087161f8d5fc896ef16643eaf4bb510ba087e003f573fb5858`.
Provider SHA-256:
`3111430648cee10205e93e343da9c40387ed9448b1d7148412422a686a2e2228`.
Build `.local/menu-preview/mesh-plan-game-build-03.log` takes 73.369 seconds with
zero retained AOT recompiles; native-port and both FPU link audits pass.

Hidden/muted Sonic startup `runs/mesh-plan-sonic-01` stops at its intentional
deadline, frame 417, with zero runtime contract failures. The viewed native
capture `frames/frame-750.bmp` shows Sonic, Tails, Emerald Coast and the HUD
without a visible geometry or UV regression. The cache records 29,625 hits and
94 changes. This is a startup/visual check, not a completed Sonic stage.

The shared adapter change has runtime evidence on D3D11 only. Vulkan performance
and visual equivalence are not claimed by this batch. The r354 snapshot, pinned
SDK, original game timing and personal saves remain untouched.
