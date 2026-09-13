# NEAR-POLY audit checkpoint

This is a source-analysis checkpoint, not a native admission or performance
claim. Both routines still execute through the retained game code.

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
Z loads precede null tests (including reverse load `28E02` before `28E0A`);
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

## Still required before implementation

- Finish full source/literal and mutation contracts of `63A10C`, all
  rotations and reused matrix/vector helpers; reuse reviewed identities
  where they really cover the complete called mode.
- Preflight the entire eligibility closure, input lists, sentinel chains,
  output arrays, matrix storage and owner/callee stacks before the first
  owner mutation. Prove physical aliases and protected/observed mappings.
- Reject unsupported/cyclic/unproved inputs before writes; no ContinueOriginal
  after clearing either count. Preserve the actual 96/1024 success limits.
- Compare CPU/FPSCR, all changed RAM and store order with original bytes,
  then run a matched stage measurement before enabling the owner.

The fresh diagnostic Gamma profile still samples both owners, but it does
not establish the gain this proposed closure would deliver.
