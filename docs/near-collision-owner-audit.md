# Native NEAR-POLY and eligibility closure

**Not enabled or linked in the game.** The complete native NEAR-POLY and
eligibility closure passes differential correctness checks, but its measured
CPU improvement is too small and inconsistent to promote. The implementation,
authoring tool and explicit component-test target remain for future work on
a larger owner. Production hooks, provider registration and benchmark options
were restored. The protected r354 baseline was never modified.

Source: immutable r354 `postpal-main-ram-native-ready.bin`, 16 MiB, base
`8C000000`, SHA-256
`b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c`.
The actual source hash is recorded by the existing collision authoring
generator; the individual spans below were independently hashed from that file.

## Candidate owner 8C028BFE

Code returns at `28E2C` with its delay at `28E2E`; the final used literal
ends at `28E44`. SHA-256 of `[8C028BFE,8C028E44)`:
`1039a254e1a32bfd40deedc30c20cb7e4cb8c082732c926160ba58a95c6992c8`.

R4 points to radius at +0, position xyz at +4/+8/+12 and motion xyz at
+16/+20/+24. The owner has a 40-byte save frame. Its first nonstack mutation
clears `[8C73E1BC]`. Length at `8C63A69C` expands the radius, but the
subsequent `8C0522C0` call receives the **original** radius in FR7.

Objects start at `[8C02D548]`: next +0, eligibility key +8, forward/reverse
node heads +16/+20, candidate output pointer +28, count +36 and Z bound +40.
Visited objects reset +28/+36 before eligibility matching. Nodes have next
+0, previous +4, xyz +8/+12/+16 and squared bound +20.

The signed-16-bit eligibility count is at `8C754E30`; matching records at
`8C754E34` have stride 12 and key at +4. Z-slab pruning uses query Z,
object bound and expanded radius. The distance test preserves x*x followed
by two FMACs, against `(expandedRadiusSquared + node.boundSquared) *
float(3FA66666)`, with ordered strict comparisons.

The output array `[8C73E1C0,8C73E340)` holds 96 pointers. The 96th
accepted entry commits its object pointer/count, array pointer and global
count before the original early return; later objects are not cleared.
This must not become truncation or a mutation-free fallback. Some head/node
Z loads precede null tests (including reverse load `28E04` before `28E0A`);
adding an early null exit changes the original contract.

Debug byte `[8C752B1C]` is signed: negative bypasses the overflow diagnostic
call `8C640862`, which otherwise receives R4=00020015 and R5=8C1123C4.

## Eligibility producer 8C0522C0

This callee was missing from the initial delegated audit. Local inspection
now covers its complete control flow, including the late second input list.
The callee has a 72-byte stack frame (48 saved +24 local), returns at
`52514`/delay `52516`. SHA-256 of `[8C0522C0,8C052518)`:
`38a058ff1b5ebddd17fe736c6834d581a645b0d4625be8aad6b71f97249dff28`.
Its late literal `[8C052600]` is `8C63A904`; SHA-256 of those four bytes:
`c91dfa5500b65da9672adce5738a90005507a3b8f75f2d76fafdace998a741c2`.
The literal lies beyond the return and must be authenticated separately.

It clears the signed halfword count, pushes the SDK matrix with R4=0,
then scans the first 12-byte input list backwards:
`8C757E34`, count `[8C19E8A4]`, record flags +0, object pointer +4,
auxiliary +8. Accepted records are copied into the 12-byte output list
beginning at `8C754E34`; count 1024 is the original early completion limit.
The matrix is popped with R4=1 on both this limit and normal first-list exit.

First-list object position is +8/+12/+16; model pointer +4 references bounds
center +24/+28/+32 and radius +36. Flag 10000000 selects a transformed
bounds center. That path loads the unit matrix (`63A820`), optionally calls
`63A10C` for object+28, `639E08` for object+20 and `639E9C` for
object+24, then transforms model+24 into stack+12 through `638E0C`.
The strict distance test uses original FMUL/FMAC ordering and adds literal
30.0 (`41F00000`) after model radius + query radius, before squaring.

The second list runs only when `[8C19E8B8]==1` and `[8C759634]!=0`.
It starts at `8C758434`, stride36, count low signed16 of `[8C19E8A8]`.
Entry+32 must intersect mask `00400003`. Position +0/+4/+8 and radius +12
are checked using three squares, ordered adds and `63A904`, then subtraction
of the query radius and strict comparison. Accepted output is flags, pointer
from entry+24, and zero auxiliary. The same 1024 output limit applies.

`63A820` with R4=0 reads the 64-byte identity matrix at `8C67C580` through
FSCHG paired FMOVs into the opposite bank. `63A904` has no callees: it
computes signed sqrt (negative input: FABS, FSQRT, FNEG), preserving the
original FCMP/T and delay-slot behavior. These are not host math substitutes.

## Admission and SDK closure

`src/sonic_near_collision.cpp` authenticates the original owners, all used
literals, and each SDK leaf before mutation. The authoring generator emits
493 reachable instruction/delay words as native C++; it performs no runtime
decoding. The existing TOUCH generator keeps its original defaults, and its
three generated outputs remain byte-identical after the shared refactoring.

The five RAM-only leaves are X/Y/Z rotation (`639E08`, `639E9C`, `63A10C`),
unit-matrix load (`63A820`), and signed sqrt (`63A904`). They preserve the
full angle/FPUL value, active/opposite register banks, SZ toggles, comparison
flags, original FPU operation ordering and live unit-matrix data. Existing
reviewed matrix push/pop, point-transform and vector-length owners are
called synchronously; no retained game callback occurs inside this closure.

Admission covers privileged direct RAM with FD=0, PR=0, SZ=0, DN=1,
RM=0/1 and either FR bank, with no enabled FP exceptions. Unsupported modes,
observers, aliases, debug-render paths and invalid graphs retain Original
before any write. All seven write spans are writable, outside protected
source regions and physically disjoint. Read spans cannot alias any writer.
The 512-byte stack reservation covers both owners and their stack-free leaves.
Memory mapping, backing, exception state and FPU epoch are revalidated at
helper boundaries. A failure after mutation aborts; it never restarts Original.

The actual pools contain 192 object slots at `8C6BB1BC` and 8192 node slots
at `8C6BE1BC`, each 64 bytes. Their bases end in BC: alignment is relative
to the pool base, not absolute 64-byte alignment. The allocator reserve slot
is not an active-list sentinel. There is no per-object contiguous node arena
or authoritative node count. Each query therefore proves complete finite
object/node chains, unique nodes, reciprocal previous links and exact tails.
It excludes an empty head only when either input list could emit its key.
The original bucket sort is coarse, not an exact monotonic Z ordering.

These invariants justify the original reverse search stopping at the head
without reading the head's null predecessor. All original comparisons,
including NaN/equal-boundary behavior, remain intact. Eligibility input bounds
of 4096 are conservative admission limits; the original 96 candidate and
1024 eligibility early-success exits remain unchanged, including which later
objects are left untouched. Full graph validation and source comparisons run
per query; this cost must be included in any claimed improvement.

## Verification

`sonic_near_collision_tests` passes 44 comparisons against execution of the
original instruction bytes, checking full CPU/FPSCR, all 16 MiB of RAM and
ordered guest-store events. Cases cover both rounding modes/register banks,
all rotation combinations, live nonidentity matrices, both input lists and
their 1024 saturation exits, 96 candidates, multiple objects, P0/P2 aliases,
unsorted/reverse/equal Z, NaN/Inf and original early-exit side effects.
Fourteen unsupported/corrupt/aliased cases decline without mutation.

The existing TOUCH test also passes: 43 differential cases, 25 safe declines
and an interrupted retained-bridge case. A separate read-only source review
found no concrete counterexample under these admission boundaries. This is
component evidence, not a claim that an entire story or stage was completed.

## Stage measurement

All four matched runs used executable
`a418080d31bba79b2e92763d06ff9f6c073b4f2fb997ec9c283872db34f88a4e`,
Gamma/Emerald Coast, D3D11 3440x1440, Recompiled timing, VSync off, isolated
hardware input and the same 60-second forward probe. The first ten seconds
are excluded. Matrix-vector, indexed-corner and TOUCH owners were held fixed.
Each run completed normally, hidden/muted, without a forced stop.

| Run under `runs/near-owner-` | Real draws/s | Execution CPU ms/draw | Cycles/draw |
| --- | ---: | ---: | ---: |
| native-gamma-d3d11-01 | 50.40108 | 18.12773 | 79,542,118 |
| retained-gamma-d3d11-01 | 50.16636 | 18.24790 | 79,998,112 |
| native-gamma-d3d11-02 | 50.17606 | 18.20837 | 79,902,125 |
| retained-gamma-d3d11-02 | 50.03458 | 18.18432 | 80,177,244 |

Mean CPU cost improves by only about 0.26%; the second pair instead costs
about 0.13% more. Mean cycles improve by about 0.46%. Both native runs
executed the owner (5334 and 5294 calls, zero Original fallbacks), so this
is not a failed-admission comparison. Presentation remained about 60 Hz;
real game draw progress remained around 50 Hz. The 60-simulation-FPS target
with headroom is still unmet.

The separate instrumented run `near-owner-profile-gamma-d3d11-01` collected
1942 samples over 30 seconds. Only four were attributed to the native NEAR
object, while larger sampled groups remain in native model rendering, FPU
helpers, retained motion functions and dispatch. This IP profile perturbs
timing and does not provide exclusive inclusive-family CPU percentages.
It does not support spending another iteration removing already-inline
reader guards; that repeats an earlier low-yield approach.

The measured runtime integration is archived for reference in
`.local/research/near-owner-runtime-integration.patch`. Reusing it requires
fresh provider/source identity review, explicit linkage and new measurement;
it is not a shipping enable switch. The existing TOUCH generator output is
still byte-identical. Only its authoring API became parameterized.

## Retained product and incremental retirement

The provider refresh previously accepted additions only. The withdrawn hook
now has an explicit retirement contract: exact address/size/symbol/source SHA,
FunctionEntry/Required/MayContinueOriginal and StaticImage. No other removal
is authorized. It proves the frozen Original entry remains, removes the hook
row, declaration, switch, both chain gates and audit token, and validates both
declared array counts against their contents, including canonical replay.
All existing neutral-definition/provider checks and transactional writes stay
in effect. Ten regression cases pass, including wrong policy, duplicates,
missing frozen code and stale array counts after interrupted refresh.

Restored game build: `.local/menu-preview/near-retirement-game-build.log`,
94.840 seconds, zero AOT recompiles; both FPU audits and the final link audit
pass. Executable SHA-256:
`0a75abc6ce1516f16795faa9579b2562cabf4fc781bcd93b330ac0c70a1f8968`.
The provider identity changed to authenticate the backwards-compatible
authoring refactor; the native NEAR body is not linked or registered.

`runs/near-retired-sonic-01` reached Sonic in Emerald Coast in a hidden,
muted ten-second D3D11 capture. Frame750 was visually inspected; the game
stopped at its requested deadline, frame415, with no contract violation.
This is a startup integration check, not a completed stage or FPS measurement.
The already committed Hedgehog Hammer callback fix remains included.
