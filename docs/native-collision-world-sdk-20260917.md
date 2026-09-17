# Complete collision-world transform group

This extends the eleven-owner collision-world group in `2c6e1c1` with its ten
related SDK owners. Original and Recompiled select timing only; the native
implementation and its internal switch are shared by Windows and Linux.
The September 17 v2 update and published installers are not replaced here.

## Scope and original behavior

All addresses below have the `8C` prefix; ends are exclusive.

| Owner | Entry | End |
|---|---|---|
| Point transform | 638E0C | 638E64 |
| Matrix push | 639BB0 | 639C30 |
| Matrix pop | 639AD8 | 639B18 |
| Rotate X | 639E08 | 639E98 |
| Rotate Y | 639E9C | 639F32 |
| Rotate Z | 63A10C | 63A1A2 |
| Scale | 63A52C | 63A5D8 |
| Translate | 63A744 | 63A7B4 |
| Identity/load | 63A820 | 63A886 |
| Signed square root | 63A904 | 63A918 |

These complete owners include the non-null RAM-matrix branches, full-stack and
pop-underflow behavior, mutable identity source, scratch stack and all live
register clobbers. The group contains no further calls or recursion. It is
authored into finite native C++ at build time, with no runtime opcode decoder.
The combined 21-owner inventory has 1,850 ordinary instructions and 195 delay
slots, authenticated against 4,926 original instruction/literal bytes.

SDK children share their parent's admitted RAM view. Their reads and writes
remain in original order: point output may alias later matrix reads, and the
identity source's second half must be read after the first destination stores.
64-bit FMOV checks the complete pair before changing RAM or a register; an
unsafe access resumes the original complete instruction, which retains any
partial fault effects. SZ/FR pair selection and reserved-FPSCR normalization
use the existing CPU helpers. MOVCA keeps its StoreQueue source contract.

The previous external call's index lifetime is retained by incrementing the
index serial after a native SDK child. This avoids stale object/eligibility
indexes when unusual output aliases modify their source RAM, without repeating
RAM capture or code validation. Real alternative-selection/debug callbacks
still release the view and revalidate on return. Diagnostics and the global
native-CPU zero override continue to retain original execution.

Private continuation copies contain only the ten exact retained SDK bodies,
bound to the retained manifest and three original unit hashes. Their 69 added
restart routes are local, never new public dispatcher entries. Original
register masks, exception handling and safepoints remain in those copies.

## Arithmetic correction and qualification

The first Windows implementation passed, but Linux caught one ULP in the
world fixture combining three rotations. A broad SDK-child FPU epoch changed
the result at return PC `8C0290E8` (FR4/XF8 `3F1A8279` instead of `3F1A827A`).
The correction retains the original helper-local rounding scopes inside SDK
owners. Parent/core epochs still close at child and callback boundaries.
No arithmetic tolerance or reference weakening was introduced.

Revision B passes on both platforms:

- 78 complete collision-world cases, comparing CPU and all 16 MiB of RAM at
  each real callback and each private SDK entry/return, plus the final state.
- 450 standalone SDK cases covering both rounding modes, FR banking, RAM
  matrices, input/output/source/stack aliases, P2 aliases, full push, wrapped
  pop subtraction, zero count, NaN/infinity/negative zero and reserved FPSCR.
- Eight actual retained-AOT world continuations, plus eighteen SDK partial
  fault continuations. Full CPU/RAM and exact original exception state match;
  each SDK fault resumes once. The original table still has its 1,023 relevant
  public entries; private restart addresses are not added to it.

Logs: `runs/world-sdk-windows-test-20260917-b.log`,
`runs/world-sdk-windows-aot-test-20260917-c.log`, and
`runs/world-sdk-linux-test-20260917-b.log`.
The earlier failed Linux revision A is excluded from performance qualification.

The initial Windows Recompiled Gamma gameplay check passed hidden/muted with
20 updates and 20 new images (59.882 images/s, 60-Hz/one-slot/one-delta cadence).
Its binary `ab876d4d2dc77c6b33f8fabde2d2f854ce0a8732d1ee755dc7c6d4eaff0975f3`
predates the Linux arithmetic correction; this is not a final-build gameplay
claim or a Windows performance comparison. Revision B's CPU/RAM qualification
above includes the arithmetic correction on both platforms.

## Measurement policy

One short Original-timing pair each is used for Gamma Emerald Coast, Knuckles
Sky Deck and Knuckles Lost World. The collision-world parent stays ON in both
halves; only `SARECOMP_NATIVE_WORLD_SDK` changes. Frames 5..25, isolated state,
four VM vCPUs, two llvmpipe threads, 800x500 at 50% rendering, diagnostics and
provider telemetry OFF. VM throughput must not be presented as Deck FPS.

The legacy `matrix_push_native_calls` counter changes because those SDK calls
are now internal to the world. It is implementation telemetry, not an exact
workload invariant. Compare the other eighteen previously recorded endpoint
fields, player bits, HUD timer and exact game-update count, and report the
changed external/internal world call counts separately.

## Result and delivery policy

Revision B's Linux executable is
`7950256145ce202fca550b53e63ea96f7ffa87225d97a762c9a9e57e90d447a9`;
Windows is `cd66e320d76f146fa4d3fc43ed43494b0563de5403a3ad79453e2703f26138cf`.
Both retain the release BASE RAM configuration. All six Linux probes complete
through their expected stop with zero world resumes; each measures 68 updates.

| Original timing, SDK ON vs OFF | Execution CPU/update | Process CPU/update | New images/s | Exact endpoints |
|---|---:|---:|---:|---|
| Gamma Emerald Coast | -4.20% | -1.29% | +2.08% | No: start/end one tick apart |
| Knuckles Sky Deck | -7.32% | -18.51% | +20.63% | All eighteen fields match |
| Knuckles Lost World | +5.72% | +21.83% | -9.27% | All eighteen fields match |

Gamma's player X/Z and animation/pose/palette counts also differ at the end.
Its raw result is not an exact-work gain. Sky Deck and Lost World have matching
binary, configuration, player bits, game/HUD clocks and update count at both
window endpoints. SDK ON removes all measured world external calls in these
windows, but that alone does not establish a speedup. Per-stage reports are
`runs/world-sdk-<scenario>-comparison-linux-20260917-b.json`; raw summaries and
logs have the same prefix with `-off-`/`-on-` and `-linux-20260917-b`.

The SDK extension remains internal-OFF (`SARECOMP_NATIVE_WORLD_SDK=1` enables
it explicitly). The qualified eleven-owner collision-world parent remains ON.
This mixed result is not promoted into the scheduled Deck patch and is not a
global or Steam Deck FPS claim. Do not repeat these unchanged pairs or component
suites. The next distinct group is the complete live render/motion hierarchy:
`040784`, its five SRT dispatchers, key sampling, static SRT tails and matrix
children. Preserve mutable object/draw boundaries and the original live reads;
do not revive the rejected shared-register ABI or immutable tree capture.
