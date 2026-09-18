# Complete inverse-trigonometry group

The seven PAL parents now execute through one authenticated native closure,
reusing the four already-native atan, quotient, polynomial and scaling children.
This is seven new owners, not eleven. The parents cover 265 instruction/delay
PCs and 680 source/literal bytes, and are called from 254 retained units.

| Parent | Source range, end excluded | Nested stack bytes |
| --- | --- | ---: |
| acos wrapper | 8C10CF48..8C10CF98 | 100 |
| asin wrapper | 8C10CF98..8C10CFE8 | 96 |
| atan2 wrapper | 8C10CFE8..8C10D038 | 92 |
| alternate atan2 wrapper | 8C10D038..8C10D088 | 92 |
| acos core | 8C10E4D0..8C10E4EC | 92 |
| asin core | 8C10E4EC..8C10E5E0 | 88 |
| atan2 core | 8C10E5E0..8C10E638 | 84 |

Source words, live constants, stack bounds, errno output, FPU mode and observer
contracts are checked before mutation. Wrappers preserve the exact finite,
infinite and NaN error classification and writes to 8C7AC048. Stack/errno and
source aliases decline before mutation. Real original callers are preserved;
the private closure only covers the authenticated fixed call graph. Its return
marker is the actual parent RTS site, not the last child's RTS site.

Two retained units receive exact root hooks through an authenticated preparation
step, composed with their current BASE RAM preparation. Original bodies remain
available on decline, internal OFF or diagnostics. No AOT regeneration is used.

## Exact arithmetic qualification, revision F

Windows and Linux each pass 1,319 original-instruction comparisons: 512 old
child cases and 807 parent cases. These compare complete architectural CPU,
16 MiB RAM, ordered writes and ambient host FPU state. They include boundary
values, signed zero, denormals, NaN/infinities, both legal rounding modes,
stack/source/errno aliases and enclosing opposite-rounding epochs.

Both platforms also pass 92 comparisons against the actual retained AOT bodies
and authenticated sparse entry table. This independently checks compiled FPU
scope behavior. The original-instruction oracle retains its previously documented
correction for two FTRC decoder operand bugs; no new tolerance or skipped case
was introduced. Actual-AOT execution owns its own paired memory observers;
rebinding a fixture observer after AOT admission would invalidate that contract.

The first Linux run exposed an existing child issue at atan input 0x3EBFFFFF:
the owner-wide host FPU scope included the polynomial FMAC iterations, while
the retained AOT runs those outside a scope. FR3/FR6 differed by one ULP.
The actual-AOT oracle reproduced the mismatch before the fix. All eleven
original child scope ranges are now preserved exactly; instruction-local
arithmetic outside them cannot leave a lazy scope active across later FMACs.
The parent asin's two original scopes likewise exclude its delayed divide.
Both oracles pass the unchanged assertions after this correction. This is an
observed Linux VM discrepancy, not proof of a particular QEMU defect.

Evidence: `runs/inverse-trig-{windows,linux}-*-20260918-f.log` and
`runs/inverse-trig-linux-build-20260918-f.log`.

The new seven-parent group joins the default native CPU group after the
matched Linux gameplay comparisons below. Global/per-feature OFF and runtime
diagnostics preserve the original path. The corrected existing four-child
implementation is shared by both timing modes. Earlier revision-A Windows
probes and revision-C Linux
84-case tests predate the child-scope correction and are not final evidence.

## Matched Linux group measurements

The same revision-F executable is used for all six short, quiet comparisons,
with collision-world and the qualified render/motion group already active.
All 16 selected endpoint fields, 68 game updates and the Original PAL cadence
match in each 5..25 image window. Four VM vCPUs and two llvmpipe threads are
used; no compiler overlaps the measured windows.

| Scene | Execution CPU/update | Process CPU/update | New images/s |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | -6.39% | -1.34% | +0.19% |
| Knuckles Sky Deck | -6.01% | -1.13% | +0.04% |
| Knuckles Lost World | -2.47% | -2.05% | +4.53% |

The reductions concern execution work, not a corresponding Deck FPS guarantee.
Gamma/Sky image throughput is essentially unchanged. Native parent admission
has zero declines in these runs. The totals include startup, so cumulative
call counts are not attributed solely to the measured window.

Executable SHA-256:
`9b5dd70b08774c0f4d6a28eb87258f96e222272ad51a85d1ac4010742784c5fc`.
Reports: `runs/inverse-trig-*-comparison-linux-20260918-f.json`.
The final normal-start executable has a different identity after changing
the default; it requires the normal final installed-program check.

## Whole-family memory transaction, revision D

The eleven-parent/child closure still routed each admitted stack/output store
through Memory separately. A new private `SARECOMP_NATIVE_INVERSE_MEMORY=1`
path captures the authenticated runtime's immutable-only observer capability
once for the complete callback-free operation. The existing source, literal,
mode, stack, errno, alias and write-range checks still run before mutation.
Loads use the admitted RAM directly; stores occur immediately in original
order, and memory accounting is published at function return. No stack/output
write is deferred or dropped, and no permission persists beyond the call.

An arbitrary stable observer does not qualify merely because it is stable.
It retains the original per-store path and ordered events. Replacing the
registered observer invalidates the capability. The complete original CPU,
RAM, flags, host arithmetic state and source protection remain unchanged.

Windows and Linux each pass the previous 1,319 cases plus 199 new closed-memory
comparisons, and 158 actual-AOT cases. The new checks require positive closed
admission, compare complete state and exact store counts, include P0/P2 aliases
and verify observer replacement returns to the ordinary path. Existing ordered
event assertions are retained for arbitrary observers. Evidence:
`runs/inverse-memory-d-{windows,linux}-{components,aot}.log`.

The candidate also includes the qualified complete rigid/render family and
its static dependency coalescing. Its Linux SHA-256 is
`d90b6b3474a897413f042fb0afdc7b58374831ebf9d8090d18d3df275a0c84af`.
The same-executable comparisons keep the related native groups active and
match all 16 endpoint fields and 68 updates. Gamma execution CPU/update changes
-1.94%, process CPU -0.25%, and new images/s -0.61%. Lost World changes +0.26%,
+0.11%, and -0.85%, respectively. The windows positively exercise 12,343 and
6,900 closed operations, with no declines or resumptions. Reports:
`runs/inverse-memory-*-comparison-linux-20260918-d.json`.

This small mixed result does not qualify an additional end-to-end improvement.
The extension remains private-OFF in the delivery; the already-qualified
inverse-trigonometry group remains ON. Do not repeat these unchanged pairs.
