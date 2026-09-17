# Native model transaction and shared inputs

The complete PAL model owners `8C037098` and `8C037108` now have structured
native implementations shared by Windows and Linux. This is a private,
default-OFF experiment (`SARECOMP_NATIVE_MODEL_PIPELINE=1`), not a promoted
performance improvement or a new user patch. The September 17 delivery and
personal saves are unchanged. The latest paired Linux result is slower.

## Implementation

One model scope authenticates the retail wrapper/cull/child bytes, supported
CPU/FPU mode, product memory observer and complete RAM/alias footprint. It
preserves the fixed 48-byte save frame, cull exits, clip count, material prelude,
GBR fields, final render flags, live registers and actual child continuations.
Unsupported inputs decline before mutation. Interrupted children retain their
failure frontier; the original wrapper is never restarted after mutation.

Point and normal arrays, including the authored extra reads, are copied once
for the scope and shared by transform, palette and draw. Reusable capacity is
not an asset cache: every invocation replaces the contents. A retained guest
child clears the shared scope before it runs. The original cull remains usable
in 4:3. FPU epochs retain the existing child boundaries and exact arithmetic.

The second iteration additionally lets the native transform write its admitted
projection records directly into RAM. Padding remains untouched. It replaces
the generic transaction's temporary vectors, byte-change mask and final copy;
GBR+60 and unobserved byte accounting are published at completion. The exact
product observer has no side effects for these non-code ranges. Arbitrary
observers, diagnostic execution and unsupported aliases keep the original
transaction. This direct-output path is included only inside the OFF model
experiment; its negative measurement does not change the released path.

This does not fuse the actual projection/lighting arithmetic, move geometry
to a new GPU format, or flatten animation callbacks. Palette admission and
most child work still execute. Initial shared capture alone was insufficient
to produce a useful CPU improvement.

## Validation

Both platforms pass 145 component cases. The reference runs the original
wrapper bytes, with deterministic transform/cull/draw child contracts and the
actual native palette child. Comparisons cover architectural state, all 16 MiB
of RAM, child entry state, odd/even counts, alias forms, both admitted rounding
modes, cull/clip exits, material masks, observer/code rejection and interrupted
children. The direct-output extension compares publication against a normal
RAM transaction, preserves padding and rejects mismatched model/count/output
borrows. Final counters: 97 normal reuses and 114 direct output publications.
These component tests do not independently replace the renderer's own oracle.

The final Windows Gamma Original ON/OFF gameplay pair completes 5..65 with
the same captured frame 270, SHA-256
`4f13863a4b3c1058e93a3b8fd6f93cfba380fe2c093e2120162dd65af1f56c60`.
Its ON run uses 27,455 direct outputs. These capture runs are correctness
checks, excluded from performance qualification. A separate final Recompiled
4:3 run passes at 60.061 new images/s: ticks 126..186, video 60, release/delta
1, exercising the retained original cull. Both tests are hidden and muted.

The final Linux Chaos-4 entry check also completes its 5..25 window and expected
diagnostic stop: 20 images, 68 updates, 4,175 direct projection outputs and no
declined model owners. This is a functional check, not a paired performance
claim. Both Gamma ON/OFF comparisons and the Windows pair match all 19 checked
game/cadence/native-owner fields at both boundaries. The fresh delivered-control
comparison has the same endpoint fields as well.

## Linux measurements

Four-vCPU / 6-GiB TCG VM, llvmpipe with `LP_NUM_THREADS=2`, Gamma Emerald Coast,
Original, 800x500 / 50% internal resolution, serial quiet runs. Every listed
window has 40 new images and 136 updates. Endpoint state is checked separately
in the comparison report. VM rates are not Steam Deck rates.

| Candidate | Model scope | Execution CPU ms/update | New images/s |
| --- | --- | ---: | ---: |
| Initial shared capture | OFF | 212.407746 | 0.859383 |
| Initial shared capture | ON | 212.313810 | 0.878908 |
| Final direct output | OFF | 211.673036 | 0.895496 |
| Final direct output | ON | 238.092223 | 0.831463 |
| Delivered September 17 control | absent | 231.177668 | 0.877732 |

The initial same-executable CPU change is only -0.04%. The final same-executable
candidate costs 12.48% more CPU and produces 7.15% fewer new images. It remains
OFF. The development executable also contains the earlier EXTENDED RAM/FPU
experiment; comparing its OFF result with the delivered control cannot be
credited to this model work. Earlier isolated savings are not additive.

Final development executables:

- Windows `out/model-pipeline-windows-20260917/game.exe`, 1,913,812,480 bytes,
  SHA-256 `5e14f8bfcfb44aa38324ec74bb7f641ca1f10966ca23ba140c5d8401f4ec6214`.
- Linux `build-linux/game`, 1,744,522,688 bytes,
  SHA-256 `f28fb37e384efc2d48b95ad4aeff8f33fcee986fac84f0d0cbe51bb01a2b58eb`.
- Initial Linux shared-capture comparison used
  `dc54dea5b1efff2a9f3ae884723e37a991ad3e8e0836b8f17a5d8c3a0f4cf9a9`.

Evidence: `runs/model-pipeline-direct-*-20260917*`,
`runs/model-pipeline-linux-gamma-{on,off,delivered}-20260917.json`,
and `runs/model-pipeline-comparison-20260917.json`.

## Incremental build identity

The two exact owner bindings are explicitly admitted by the provider refresh
tool. Existing retained AOT bodies remain available. The scalar-write and
FPU-register preparation checks now pin the path/size/SHA records for all 1,157
retained units instead of the entire mutable provider manifest generation.
The manifest's own checksum and individual source bytes remain checked. This
avoids a circular identity dependency when only native provider metadata
changes; it does not authorize an arbitrary new AOT source generation.

The transfer-plan dispatcher pin was refreshed only after verifying its entire
original dispatch body was unchanged. Windows link audits retain 894 prepared
owners / 41 units, with zero original selected archive members. Neither build
regenerated or fully recompiled the retained AOT pack.

Do not promote this experiment or rerun unchanged pairs. The next distinct
semantic boundary is the common object activation/lifetime family, documented
in `native-object-activation-next-20260917.md`. It is still an implementation
task, not a measured improvement. The overall 20–25 ms Deck target stays open.
