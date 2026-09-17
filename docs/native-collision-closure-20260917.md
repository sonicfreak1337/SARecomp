# Native collision closure

Private experiment, disabled by default. `SARECOMP_NATIVE_COLLISION_CLOSURE=1`
enables the reviewed triangle/contact owners to execute their cross product,
vector length and normalization children inside one admitted native context.
This is separate from `SARECOMP_NATIVE_COLLISION_MEMORY`, which remains OFF.
The delivered September 17 patch and the user's installed saves are unchanged.

## What changed

Previously each short math child released the parent's RAM capability and host
FPU epoch, repeated full public admission, and then forced the parent to repeat
its mode, mapping and writable-footprint checks. The three callback-free bodies
now share the parent's authenticated RAM capability and FPU epoch. Ordinary
entry and closed entry use the same source-bound C++ arithmetic body.

The parent still checks its full source closure and all dynamic list/input and
writable ranges before the first mutation. A closed child checks its dynamic
vector addresses (alignment, 16 MiB bounds, P0/P1/P2 translation policy) and
requires its complete output to fit an already-proved parent writable span.
Read/write aliases remain legal because the original bodies latch their inputs
before the first store. No vector reassociation, approximation, changed FSRRA
semantics, lost live registers, or changed simulation cadence is introduced.

Only the exact registered product observer can acquire this capability.
Arbitrary or revoked observers use the public path with ordered store events.
All matrix owners and retained angle/rotation/copy calls still discard both
capabilities before calling, validate the original return contract, revalidate
the parent and recapture. No mapping/observer/guest callback is allowed inside
the fused interval. The authenticated retained source closure restricts those
bridges to the original reviewed bodies; this is not a generic cross-call cache
or a source-identity cache. Post-mutation failures still abort without restarting
the original owner.

`collision_fused_cross`, `collision_fused_length`, and
`collision_fused_normalize` provide positive game-run evidence. Batched access
counters are committed at the original external boundaries and final return.

## Qualification

The component tests compare the original SHA-bound SH4 routines with the native
implementation, including all CPU/FPU live outputs and all 16 MiB of RAM. The
authenticated product mode must exercise all three fused children; the revoked
observer mode must exercise none and preserve the ordered public-store events.
Twelve additional closed-entry rejection cases cover invalid aliases, unaligned
or wrapping vectors, unsupported entries, absent capability, and incomplete
output coverage without modifying guest state or access counts. Existing
interrupted-return/mapping tests still check that external boundaries release
the capability. The test interpreter remains excluded from the game.

The intended measurement is same-executable OFF/ON, with every other setting
held fixed, using Gamma Emerald Coast gameplay and Chaos 4 entry. Exact endpoint
state, cadence and native-work counts must match before claiming a CPU saving.
The four-vCPU TCG/Linux VM is a comparison environment, not a Steam Deck FPS
prediction. The user's 20–25 ms Deck target is not yet established.

Both platforms pass 1,644 component cases each: 659 math, 88 triangle-contact,
43 candidate, 25 rejected-candidate and one interrupted-candidate case in each
of the two observer modes (1,632), plus the 12 new closed-entry rejections.
The existing narrow FTRC correction in the triangle/candidate test executor is
unchanged; no interpreter is linked into the product. No baseline AOT unit was
recompiled by either incremental build. Windows link ownership checks pass.

Measured executables:

- Windows `out/collision-closure-windows-20260917/game.exe`:
  `cb26f63f10488e721d4bab03a39fe931243c1e43fe1e2b30421b90481fdf9806`.
- Linux `build-linux/game`:
  `7db2fe2177fb809c6e94b581a5d5efced457a23d2b559737fb3c28389ed56551`.

Windows Original Gamma frames 5..305 match all 28 checked endpoint fields,
including player/HUD state, native-call counts and authored cadence. CPU per
new image falls from 22.395833 to 21.822917 ms (2.56%); execution cycles fall
1.38%. Both runs remain at the authored 25 images/s. The Windows Chaos 4 pair
starts one game tick apart and reaches different ending states. Its raw CPU
reduction is not a qualified performance gain. All four runs complete their
requested windows and expected stop without an integration failure.

Linux Original frames 5..45 match all 28 endpoint fields in both scene pairs,
with 40 new images and 136 game updates per run:

| Scene | CPU ms/update OFF | CPU ms/update ON | CPU change | New-image rate change |
| --- | ---: | ---: | ---: | ---: |
| Gamma Emerald Coast | 221.967639 | 211.434779 | -4.75% | -0.45% |
| Chaos 4 entry | 226.089482 | 220.900149 | -2.30% | +0.47% |

The ON Gamma window executes 4,781 fused cross products, 11,461 lengths and
9,559 normalizations. Chaos 4 executes 1,938 / 4,692 / 3,876 respectively.
OFF records zero. The samples retain the same existing native-owner counts.
This is a modest reduction in execution CPU work, without a useful overall
VM image-throughput gain. It does not establish the user's Deck target.
Keep the experiment OFF in source defaults; no new patch or installer is
produced from this change alone.

A separate final Windows Recompiled Gamma check also passes: frames 5..65,
ticks 126..186, 60 Hz video, release 1, logical delta 1, 59.99847 new images/s,
with positive fused-child counts. This validates integration and unchanged
timing; it is not a before/after performance result or a Linux Recompiled run.

Evidence: `runs/collision-closure-{windows,linux}-comparison-20260917.json`,
the correspondingly named run directories, component logs, and
`runs/collision-closure-artifacts-20260917.json`.

## Next structural boundary

Continue the complete model transaction described in
`native-model-pipeline-next-20260916.md`: shared capture through transform,
palette and draw for `037098`, followed by `037108`. Preserve all projected
RAM/palette/clip and live CPU effects; do not assume buffers are dead after a
draw. This is still unimplemented and has no promised speedup. Do not revive
the rejected GPU-resource cache or model-packet offloading as a new proposal.

The motion audit already identifies `040612` and `040784` as mutable-callback
owners. Flattening their trees or holding capabilities across those callbacks
would be invalid. A generic shared-register ABI was already measured there and
rejected. Likewise, the model arithmetic's documented QEMU rounding discrepancy
for larger FPU epochs is still relevant; do not repeat that scope expansion
or relax exactness as an apparent shortcut.
