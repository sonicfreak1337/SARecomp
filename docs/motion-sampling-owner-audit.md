# Original motion sampling: complete native owner closure

Complete native motion experiment, with its earlier source audit retained below.
Render interpolation remains withdrawn. These routines are the game's own
keyframe sampling, executed during the original animation update/draw path.

The matched post-NEAR profile is
`runs/near-owner-profile-gamma-d3d11-01/execution-ip-resolved.json`, executable
`a418080d31bba79b2e92763d06ff9f6c073b4f2fb997ec9c283872db34f88a4e`.
The retained unit `8C0400A0..8C04124E` has 55 of 1942 nearest-symbol samples.
Individual primary-function samples include 13 at `0400A0`, 6 at `040150`,
4 at `03FF90` and 3 at `03FEB8`. These are diagnostic samples, not exclusive
family CPU cost or a prediction of speedup. Optimizing only the two small
leaves would address little self time; a useful next experiment should cover
the common calling motion owners and their matrix closure together.

## Source and call context

All spans refer to the immutable r354 16-MiB RAM image, base `8C000000`, SHA
`b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c`.

| Owner/span | Size | SHA-256 |
| --- | ---: | --- |
| `8C03FEB8..8C03FEF2` | 0x3A | `30acd547629a38163256792165f65b9f8821244c4337702f622a56899f38e7bf` |
| `8C03FF90..8C040010` | 0x80 | `28e9dbccd008f14942cbb0b238b24de6d8864295257c59d69cab2823b5746a73` |
| shared literal `8C04007C..8C040080` | 4 | `fca303f275bd9e45e125971411e316f16331e938594c4bf17556602eea7d53f1` |

Both functions are leaves without guest callees. The shared literal points
to live frame float `8C88FD8C`; its value must not be frozen. `03FEF2` and
`03FF2C` are distinct routines outside these two audited owners.

Caller `040150` reads motion context `8C88FD80`: slot index +20, key-pointer
array +4, key-count array +8. A nonzero key pointer selects `03FEB8` and
then `03FF90`; otherwise it uses object +20/+24/+28 directly. It then calls
matrix rotation `639C34` and increments the context slot index. `0400A0`
has analogous position handling at object +8/+12/+16, but uses `03FF2C`.
The complete parent/position-sampler/matrix closure was subsequently audited
and implemented below.

## 03FEB8: key selection, not weighting

Inputs: R4 = base B of 16-byte records, R5 = exclusive upper bound N,
R6 = output-index address, R15 = S. It first stores old R14 at S-4,
loads live frame into FR3 and FTRCs it into FPUL/R1.

Binary search starts lo=0, hi=N. While unsigned `(hi-lo-1)>0`, mid is
unsigned `(hi+lo)>>1`; comparison is unsigned `R1 >= load32(B+mid*16)`.
True assigns lo=mid, false hi=mid. It writes lo to R6, then restores R14
from the stack in the RTS delay slot. It does not compute or store weight.
N=1 performs no key read. N=0 underflows and does not become a valid empty
result. Admission must exclude nontermination and arithmetic/address wrap
before the first stack write. Do not clamp negative/NaN/Inf frame results
or replace the original search with a sorted-data library operation.

Normal disjoint-input final state includes R1/FPUL=FTRC result, R2=R3=0,
R5=lo+1, T=false and FR3=frame bits. R0/R7 keep the last key/mid, or remain
unchanged for N=1. R14/SP restore; R4/R6/PR remain unchanged. Only stack4
and output4 are written. Output aliasing the saved R14 changes its restore;
the stack write can also alter a subsequently read frame/key. Either prove
disjointness before mutation or preserve that live order exactly.

## 03FF90: signed component sampling

Inputs: R4=B, R5=index i, R6=N, R7=three-int32 output O. Records contain
signed frame at +0 and signed components at +4/+8/+12. Current C is
`B+(i<<4)`. If unsigned `i+1<N`, Next=C+16; otherwise Next=B. It wraps to
the first record, without a fabricated loop-end time or last-key clamp.

Alpha starts `liveFrame - FLOAT(frameC)`, including the branch delay slot.
Delta is the wrapped 32-bit difference `frameNext-frameC`. Only when signed
delta>0 does it compute FLOAT(delta), FSRRA, square that reciprocal root and
multiply alpha by it. Beta=1-alpha. There is no FDIV substitution, angle
normalization, shortest-arc correction or 16-bit wrapping in these bytes.

For x/y/z in order: load current signed component and FLOAT; load next signed
component and FLOAT; multiply current by beta, next by alpha, add and FTRC;
store the int32 result. No FMAC substitution. Writes occur at `03FFDC`,
`03FFF4` and in the final RTS delay slot `04000E`. N=1 is not a direct copy:
its zero delta retains alpha and the original rounding operations.

Output can overlap future input components: e.g. O=C+8 makes the first
store alter current Y before the second component reads it. Never preload
all six components unless admission excludes this overlap. Current and Next
may legitimately be the same record. Frame values are read before stores.

Final state: R0=C, R2=8C88FD8C, R3/FPUL=last int32 Z result, R4=Next,
R5=delta bits, R6=N, R7=O; T is signed(delta)>0. FR2=weighted Next Z,
FR3=Z sum, FR4=alpha, FR5=beta. No stack or register-bank exchange.

## Admission contract

Prove the parent context/slot arrays, counts, source/literal identities,
stacks and output spans before mutation. Preserve live aliases or decline
before writing; never restart Original after partial execution. Reuse the
actual FLOAT/FTRC/FSRRA/arithmetic helpers under the existing conservative
PR0/SZ0/FD0/DN1/RM0-or-1/FP-enables-zero gate, with either FR bank.
Do not restore FPSCR wholesale: original causes and sticky flags are outputs.
Authenticate and audit the remaining position and matrix callees, then compare
complete CPU/RAM/store sequences before one matched gameplay measurement.

## Complete motion experiment, 2026-09-14

`sonic_motion_sampling.cpp` implements the four complete owners `0400A0`
(position), `0400F8` (scale), `040150` (ZYX rotation), and `0401A8` (YXZ
rotation), including key selection, float/angle sampling and the complete
register-only R4=0 SDK tails. No retained callback or runtime instruction
decoder runs inside this closure. Original authored animation updates retain
their cadence; this is unrelated to the retired render-interpolation feature.

`prepare_motion_sampling.py` authenticates the full RAM image, all seven
motion bodies, four SDK bodies and reachable literal islands. The four SDK
tails are `63A7B8`/12h, `63A5DC`/46h, `639C34`/86h, `639F38`/86h. Translation
uses all 16 matrix lanes; scale retains every fourth-lane multiply; rotation
retains full-u32 zero tests, skipped-axis delay-slot FPUL writes, both FTRVs
before XF writes, and exact raw pair transfers/FSCHG ordering. There is no
affine simplification, angle-wrap shortcut, FMA substitution or frozen frame.

One host FPU epoch owns each parent. Admission proves physical disjointness
of all external reads/code/literals from `[SP-40,SP)` and slot `8C88FD94`.
The intentional local index/output stack traffic stays live. Table additions,
whole key ranges, zero/oversized counts, object offsets and RAM aliases are
checked before mutation. Only the stable prevalidated-write observer contract
is admitted; writes preserve its ordered delivery. A failed admission resumes
retained AOT; a failure after admission aborts rather than replaying stores.

The four function-entry hooks have exact reviewed source/address/size tuples,
and the incremental provider tool only admits the complete four-hook family.
`SARECOMP_NATIVE_MOTION_SAMPLING=0` selects retained execution in the same
binary for comparisons. Original gameplay mode retains its existing route.

### Original-byte oracle correction

The pinned test interpreter mistakenly passes decoded destination `n` to
FTRC. Its decoder deliberately represents FTRC's FR register as source `m`
and FPUL as destination zero, so FTRC FR3 incorrectly read FR0. Retained
production AOT for `8C03FEC0` correctly calls `fpu_truncate_to_fpul(cpu,3u)`.
The generated test-only interpreter copy changes only that call to source
`m`, under an exact source hash. No pinned SDK, baseline or game interpreter
is edited. Sixteen independent register-selection checks protect this oracle
adaptation. It is not a production change or a speedup.

### Verification

96 complete original-byte cases compare CPU/FR/XF/FPSCR, all 16 MiB RAM and
ordered write tuples. Coverage includes both FR banks/rounding modes, default
object values, one/multiple keys, unsorted keys, negative/NaN frame, non-affine
and exceptional matrices, all skipped/executed axis combinations and high-bit
angles. Sixteen rejected configurations leave guest state/RAM untouched.
Native and reference paths preserve host MXCSR; additional all-zero rotations
preserve existing guest flags/causes under seeded host rounding/DAZ/FTZ/status.
Sage independently reviewed the actual admission and SDK sequence read-only.

Component log: `.local/menu-preview/motion-sampling-component-test.log`.

### Product measurement and decision

Incremental build `.local/menu-preview/motion-sampling-game-build.log` passes
both FPU audits and the native-port link audit in 94.107 seconds, with zero
retained AOT recompiles. Build SHA-256 is
`7a6a13c139314b243b67bdf3e25e0a32a8d01f73f1c484a61491e8865a2f1da2`;
provider is `1a8a9350bba29679f26308d071e2da0bbafca8b6ff2b4f0791c7e292749a2f2d`.

Same-executable Gamma/Emerald Coast, D3D11 3440x1440, Recompiled timing,
isolated forward input, hidden/muted 60-second runs, first 10 seconds excluded.
Order was native/retained then retained/native. All runs completed normally.

| `runs/motion-owner-` suffix | New game draws/s | Execution CPU ms/draw | Execution cycles/draw |
| --- | ---: | ---: | ---: |
| `native-gamma-d3d11-01` | 51.20456 | 17.69814 | 78,063,408 |
| `retained-gamma-d3d11-01` | 49.90861 | 18.34839 | 80,609,193 |
| `retained-gamma-d3d11-02` | 49.99905 | 18.26991 | 80,298,566 |
| `native-gamma-d3d11-02` | 50.44608 | 17.99017 | 79,500,590 |

Mean execution CPU decreases **2.54%**, cycles decrease **2.08%**, new draws
increase from **49.95383 to 50.82532/s**. Both individual pairs improve CPU
(3.54% and 1.53%); retain the native closure in Recompiled gameplay. Native
runs admit 1,315,478 and 1,273,903 calls, with zero retained declines. The
retained controls execute 1,249,955 and 1,257,739 original calls.

This is a modest global animation CPU reduction, not 60-fps completion.
Presentation stays around 60/s while real game draws remain below the target.
No new Vulkan performance or full-stage completion claim is made here.

`runs/motion-owner-sonic-01` is a separate hidden/muted 10-second startup
check with copied saves. Native frame 750 was inspected: Sonic, Tails,
Emerald Coast geometry and HUD display normally. The run ends at its own
deadline (armed at frame 416), without a contract failure; no test process
remains. This is startup/visual integration evidence, not stage completion.

### Next bounded source findings

Sage's subsequent read-only map identified three remaining real owners,
not merely sampled instruction continuations:

| Owner | Body including RTS delay slot | Role |
| --- | --- | --- |
| `8C040612` | 40h; SHA `4813ac7b770507282572180bc172d260934a37c7ca4abdf92336e47c8dd12450` | Two-channel motion record/context dispatcher; three mutable SRT callbacks, then cursor advance |
| `8C040784` | ACh; SHA `2009115049c0e5f3debb917ee05c7d6977d1171bb62b03340a04bb50422baa74` | Child/sibling traversal, matrix push/pop, motion and arbitrary object callbacks, model draw |
| `8C040EF8` | 82h; SHA `f1538070f22d301a7b88cd89812d322748a3a0aba964b03d4d9719ceb58ccf6c` | Indexed channel-context angle sampling, output triplet and selected-channel flags |

`040612` could consolidate repeated admission/context work around the existing
four native roots, but its targets are mutable. Normal constant alternatives
are `04057A`, `040588`, `040596`, `0405A4`; other target values need Original
fallback before mutation. `040784` has open transitive callback dependencies:
never flatten its graph or keep admission across callbacks that can mutate
links, matrices, code or mappings. Its original flag-40 rotation branch reads
arguments from the stack, which must not be rewritten as object rotation data.

`040EF8` offers a separate closed sampling family through only `040A60`/4Ah
(SHA `3cdea64921f19ef788309d04d64c984b8c71eae57605d9d9dc622cf835566a81`)
and `040D60`/90h (SHA `ffcfa7521f3ca710f63d02224d29461c8584066c14133f092c2a112aa3b51fb7`).
Neither helper calls further guests. It needs signed-byte channel offset
wrapping, word selector/flags, output/context/index/stack alias proofs, and
the existing exact FTRC/FSRRA behavior. This is a possible next complete owner,
not an implemented optimization. Sparse prior samples (Gamma: 8/15/7 for the
three owners; Windy: 2/7/0) do not predict a substantial performance gain.
