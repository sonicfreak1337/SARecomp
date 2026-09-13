# Complete TOUCH-POLY collision owner

2026-09-14, Sonic port only. Original timing and the protected r354 product
remain available. The admitted native owner is enabled for Recompiled
gameplay; the private control retains a direct comparison with Original.

The shared PAL `8C029B00..8C02A66C` routine consumes candidate triangles,
computes contacts, writes the selected/averaged result and manages its matrix
stack. It is not a pure distance helper. The implementation preserves the
original calls, FPU operations, RAM writes and return state instead of dropping
collision work or changing the game's simulation rate.

`tools/prepare_collision_candidates.py` authenticates the complete retained
16 MiB RAM image and expands 1,121 reachable original words into native C++.
There is no runtime instruction decoder in this path. The owner SHA-256 is
`744ca095c47e07d9b87ed43cf54e013c45349cd472852cf44cee85dd7b946832`.
Its 19 source spans also authenticate the normal callees, literal islands and
libm coefficient table. Authoring output stays under the build directory.

The wrapper is optional and guarded. Before any guest mutation it checks:

- Legal scalar FPU/CPU modes, RAM mapping and stable observer contracts.
- Signed debug byte `8C752B1C < 0`; rendering/text debug branches use Original.
- The complete selected candidate pointer array and all 64-byte records.
- Query, contact/export arrays, errno, stack, matrix depth/pointer and matrix
  slot write ranges, including physical overlap across P0/P1/P2 aliases.
- Source/literal identity and the copier's discarded read beyond its 88 bytes.
- A valid matrix stack with spare capacity and at most 96 input candidates.

Unsupported input declines without changing CPU, RAM or observer state. The
96-candidate admission limit never truncates an original list. Interrupted
retained calls abort explicitly; they cannot fall through to Original after
partial mutation. RAM/FPU contexts are released and revalidated around calls.
Existing exact native vector, transform, stack and inverse helpers are reused.

At exactly 16 contacts the original branches past its matrix pop. The native
owner preserves that result, including the outstanding push; it does not add
an apparently convenient cleanup that would change later game state.

## Verification

`sonic_collision_candidates_tests` passes 43 full original-byte comparisons:
registers/FPSCR, complete 16 MiB RAM including stack residues and ordered guest
store address/size/source/changed events. Coverage includes 0/1/2/16 contacts,
both winding orders, scalar FPSCR modes, 96 candidates, tilted/distant geometry,
motion, absent selectors, P0/P2 data aliases and zero/negative/NaN radius.
Six cases actually take the original 16-contact skipped-pop branch.

Another 25 checks verify mutation-free rejection, and one verifies explicit
abort after an interrupted retained call. The earlier component-only raw body
also matched 27 cases using actual original callees. These are semantic tests,
not an assertion that every gameplay collision has been visited.

Sage independently reviewed the admission, identity spans and callee closure
read-only and found no blocker. No additional game runs were delegated.

The private Gamma gameplay gate probe `touch-gate-gamma-d3d11-01` observed -1
throughout its 60-second run, so the RAM-only admission is relevant in-game.
Private controls: `SARECOMP_NATIVE_COLLISION_CANDIDATES=1` and benchmark option
`--collision-candidates native`; `0` / `retained` preserves the comparison path.

## Game measurement

First matched pair: hidden/muted Gamma Emerald Coast, 60 seconds of walking,
first ten seconds excluded, D3D11 3440x1440, Recompiled timing, VSync off/output
60, indexed corners and matrix-vector helpers on. Both runs use EXE SHA-256
`45358e1f27868e61338d0a335f39f7da45bda7e91373330771f66d193c88cf1e`.

| Run | Actual draws/s | Execution CPU ms/draw | Cycles/draw |
| --- | ---: | ---: | ---: |
| `touch-owner-retained-gamma-d3d11-01` | 49.19194 | 18.62393 | 81,902,427 |
| `touch-owner-native-gamma-d3d11-01` | 50.25413 | 18.14195 | 79,848,855 |

Both completed normally. The native run recorded 36,891 native owner calls,
zero admission fallbacks. This single matched pair shows 2.6% less CPU per
draw and 2.2% more actual draws, while presentations remain 60. It does not
establish stable 60 gameplay FPS. The prior Vulkan corner-reuse pair is a
different renderer/binary comparison and is not combined into these figures.

An additional arithmetic-specialization experiment was tested and removed.
It reused the existing source-authenticated `NontrappingSingleBody` for binary
arithmetic/equality. All semantic tests passed, but a matched three-way game
comparison did not establish a useful advantage over the simpler owner.
That experiment added a runtime branch to each selected arithmetic site, so
the first and second binaries are not interchangeable controls.

All three runs below used EXE SHA-256
`d828be3a2cec976e831b6fd4e80f932083bf7cdb4244afe01b6271b848e352ad`:

| Run | Actual draws/s | Execution CPU ms/draw | Cycles/draw |
| --- | ---: | ---: | ---: |
| `touch-owner-retained-gamma-d3d11-02` | 49.14494 | 18.60726 | 82,144,357 |
| `touch-owner-native-gamma-d3d11-02` | 49.60343 | 18.42059 | 81,359,543 |
| `touch-owner-specialized-gamma-d3d11-01` | 49.81558 | 18.43561 | 80,905,875 |

The final implementation restores the simpler, independently audited body.
No new arithmetic-specialization option is shipped. All five measurement runs
completed normally; the 60-FPS-with-headroom objective remains open.

## Delivered product

Final `out/experimental/game.exe` SHA-256:
`0c9556893c3e57bf785c258167e95cd6d4d0783835ae9a8e3ff63bc87e073f11`.
Provider implementation identity:
`39a6aff2f1adebf4fcab765b29faa9a5a39bbb8139e8c4f30befbd456ea0b86c`.
The final incremental build took 73.890 seconds, with zero AOT recompiles;
provider closure and both FPU link audits passed.

`touch-owner-final-sonic-01` then started Sonic Emerald Coast in the actual
final product, hidden/muted D3D11, with Recompiled timing and copied saves.
The captured frame shows ordinary gameplay, Sonic/Tails and the stage/HUD;
the run ended at its intended deadline, frame 411, without a contract crash.
This is a startup/integration check, not an entire stage playthrough or a new
performance comparison. The existing Amy hammer-tail correction is retained.
