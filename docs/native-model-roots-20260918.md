# Complete direct model operations

The private model-submission group now covers direct model roots as well as
models called from the native render hierarchy. This closes all three PAL
owners `03700C`, `037098` and `037108` over visibility/material selection,
projection, palette lighting, drawing and renderer-context capture/commit.
The existing projection and palette loops already have native batch kernels;
this change connects their complete callers and shared memory ownership.

`SARECOMP_NATIVE_MODEL_SUBMISSION=1` remains private and default OFF. Global
native CPU/model overrides and diagnostics retain the previous path. The
delivered September 18 Deck update is unchanged.

## Operation lifetime

A direct root authenticates the complete source set and acquires one admitted
read/write capability. Hierarchy children continue borrowing the enclosing
operation. Palette and renderer-context owners reuse that capability and its
source proof, while still admitting their live operands, write ranges and
aliases. Both also check the enclosing operation's additional source fences.

No source proof survives a retained guest call, root return, scene change or
queued renderer boundary. A declined closed child revokes the operation and
clears the active model capture before calling the retained implementation.
Interrupted children keep the actual continuation frontier. The model's
points/normals remain per-call snapshots; this revision introduces no mutable
asset cache, parallel guest execution, timing change or reduced draw distance.

## Functional evidence

Windows and Linux each pass 94 new direct-root cases and the 73 affected
shared-operation cases. The new oracle executes actual PAL wrapper and cull
instructions, comparing all architectural state, all 16 MiB of RAM and child
entry states. Transform/draw are fixture children; palette and render-context
owners exercise their real borrowed and independent native paths. This is
not a claim that every rendered vertex was checked against original AOT.

Coverage includes all three roots, odd/even models, both supported rounding
modes, FR bank, the normal aliased GBR and a separate GBR, both mask states,
widescreen/4:3 culling, source modification, code/input/output aliases,
observer replacement, retained-child boundaries and interrupted children.

The hidden Windows Gamma gameplay pair enables both movement/contact and
model-submission together. All sixteen selected state fields match at the
two measurement boundaries. Captured gameplay frame 270 is byte-identical:
SHA-256 `f351cb258cecce1622bc275cc6a06b47b9aeeef6eac57f85f2df24616c846498`.
The capture was also visually inspected. Its readback excludes this pair
from performance qualification. There are 2,280 additional direct root
operations and 20,780 closed model children, with no render/contact resumes
or model revocations in the measured interval.

Evidence:

- `runs/model-roots-components-windows-20260918.log`
- `runs/model-roots-shared-windows-20260918.log`
- `runs/model-roots-components-linux-20260918.log`
- `runs/model-roots-gameplay-comparison-windows-20260918.json`
- `runs/model-roots-executables-20260918.json`

Both actual game targets build incrementally. All 27 allocated ELF sections
are identical between the unstripped Linux executable and its stripped VM
copy. The latter's SHA-256 is
`4282c491d95e32fa3b07ce7c52eb2153cae5a871f9b662f8f4b64d0cce81f280`.
The Windows executable remains at the reused private
`out/model-submission-windows-20260918/game.exe`; its new hash is in the
manifest, not the earlier visibility report.

## Performance qualification

The bounded Linux comparison uses both private groups OFF against both ON,
with Original timing, Deck aspect, diagnostics/telemetry OFF and explicitly
two software-raster workers. Both scenes match all sixteen selected endpoint
fields and 68 game updates. Neither has a model revocation or render/contact
resume. The reference and candidate use the same executable.

| Scene | Execution CPU/update | Process CPU/update | New game images/s |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | -7.04% | -1.31% | -0.80% |
| Knuckles Lost World | -2.71% | -1.15% | +1.98% |

Gamma closes 2,295 additional direct model roots and 20,966 children; Lost
World closes 244 roots and 5,944 children. This proves the new route is used,
but the throughput results do not establish a substantial global gain.
Keep both private groups OFF. Do not repeat these unchanged pairs or component
suites, promote the defaults, or package another patch based on these results.

These are short four-vCPU QEMU/TCG measurements with software Vulkan, not
Steam Deck FPS. They combine both groups and are not incremental percentages
on top of the older contact or visibility tests. In particular, the previous
visibility pair used the default raster-worker setting, while this pair
explicitly uses `LP_NUM_THREADS=2`; their absolute times are not interchangeable.

Evidence with raw metrics, selected endpoint values, group configuration,
executable hashes and counters:

- `runs/model-roots-gamma-comparison-linux-20260918.json`
- `runs/model-roots-knuckles-lost-world-comparison-linux-20260918.json`

A separate execution-thread call profile with both groups active is complete;
see `native-model-roots-profile-20260918.md`, including its recorder-status
limitation. It selects the common land display/list/transform family as the
next connected scope. Its instrumented run is excluded from these comparisons.
