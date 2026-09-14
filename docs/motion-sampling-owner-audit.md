# Original motion sampling: bounded next-owner audit

Analysis only; no motion or game-timing changes are implemented here.
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
The complete parent/position-sampler/matrix closure is not yet audited.

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

## Required before a native implementation

Prove the parent context/slot arrays, counts, source/literal identities,
stacks and output spans before mutation. Preserve live aliases or decline
before writing; never restart Original after partial execution. Reuse the
actual FLOAT/FTRC/FSRRA/arithmetic helpers under the existing conservative
PR0/SZ0/FD0/DN1/RM0-or-1/FP-enables-zero gate, with either FR bank.
Do not restore FPSCR wholesale: original causes and sticky flags are outputs.
Authenticate and audit the remaining position and matrix callees, then compare
complete CPU/RAM/store sequences before one matched gameplay measurement.
