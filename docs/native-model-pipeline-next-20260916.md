# Next complete native model boundary

The animation/pose owners in `native-animation-hierarchy-20260916.md` are
implemented. The first compact model submission below is now implemented,
internally OFF and excluded from the installed update. Complete transaction
capture across transform, palette and draw remains work in progress.

Title owners `8C037098`, `8C037108` and `8C03700C` surround native transform,
palette and draw operations. Replacing only their wrapper with three native
calls mostly removes wrapper overhead. A larger replacement needs a shared
capture and different geometry submission, preserving title-visible effects.
`036FFC` is only the final five-instruction render-state setter, not a whole
model owner. Start with `037098`, then `037108`; `03700C` also copies SDK
render context through `605CEC` / `605D4A`.

## State that cannot be discarded

- Transform `037294` writes through GBR+60: rounded-even point count times
  16 bytes, projected X/Y, reciprocal depth and retained fourth word. It
  also reads the authored extra source point and publishes clip/FPU state.
- Palette `037350` writes primary colors into each projected record +12,
  secondary colors into `8C03D760` and the latter base into GBR+64.
- Native draw recognizes PR `037060`, `0370E2` and `03715E` and consumes those
  colors. It is not redundantly recalculating the same palette lighting.
  Its no-host-matrix fallback still reads the projected positions; mixed
  near-clipping consumes clip flags.
- `037108` prepares material globals GBR+52/+44. `037098` finishes through
  `036FFC`, which writes `[8C89004C] |= 0xC0`. Preserve their separate effects.

No proof makes those RAM buffers dead after owner return. Leaving RAM stale
because a particular GPU shader does not need it is not a valid replacement.
Hierarchy `040784` also has callback slots that can change node/model/child
data. Capturing its entire tree before executing callbacks is not equivalent.

## Concrete architecture

1. Qualify a full title-model transaction, including cull, transform, palette
   and draw. Capture point/normal arrays and projection/material data once.
   Preserve guest publication; reuse captured/generated data inside the draw.
2. Extend the existing topology/UV plan with immutable corner geometry and
   point-index mapping. Submit current point attributes/colors and per-draw
   matrix/material separately, avoiding complete 76-byte vertex rebuilding.
3. Create/retire each immutable package generation in ordered frame commands.
   Submitted frames own their referenced generation. Do not synchronously
   create/evict resources on the producer: that made the old GPU cache slower.
4. Preserve content comparison or a proven complete write-generation scheme
   for mutable assets. A model pointer alone is not an asset version.
5. Initially admit smooth TitleBasic, no ENV, no exceptional SDK colors and
   no clipped triangles. Existing geometry submission handles all other cases.

The current profile has 52/903 exclusive samples in the main native draw and
its two largest lambdas, about 5.8%. Reader, transform, memory and copy costs
also appear, but their full ownership is not established. The 33 memcpy
samples must not simply be added to that estimate. This is a concrete next
structural change, not evidence of a promised large performance gain.

Sources: `src/native_title_adapter.cpp` transform/draw owners and
`src/renderer/pinned/native_port_graphics.cpp` create_mesh /
submit_resource_sync. Read-only audit and source checks were done against the
current local candidate; the pinned Katana SDK and r354 remain unchanged.

## Implemented compact model submission

`SARECOMP_NATIVE_MODEL_PACKETS=1` retains immutable topology/UV generations
and captures the current points, normals and palette colors once per model.
Eligible smooth, unclipped TitleBasic meshes enqueue this owned data plus
their own material state. The render consumer expands vertices immediately
before the unchanged D3D11/Vulkan draw. The producer no longer rebuilds and
serializes every 76-byte corner. GPU vertex expansion/upload is not eliminated.
Ineligible meshes and diagnostic capture keep their previous path.

No guest RAM write, title cadence or object lifetime is removed. The existing
plan's content comparison determines immutable topology identity. Both queues
charge the larger of expanded and retained storage; no empty span can bypass
limits. Frame/resource-prefix ordering, release/acquire ownership and abort
cleanup cover the sidecar references. The reusable pending-draw pool drops
those references when its frame is flushed. The consumer releases them before
acknowledging completion. This is port-local and does not change SDK commands.

`SARECOMP_NATIVE_MODEL_PACKETS_VERIFY=1` additionally builds the retained
vertices and compares every byte, including the original ARGB expression and
host rounding mode. This mode is for correctness, never performance.

Windows D3D11 and Vulkan, each with serial and parallel consumers, pass 32
vertex cases (four rounding modes, eight material combinations) and four
exact image comparisons. The same Vulkan cases pass on Linux. Tests cover
producer mutation/destruction, two model generations/material states split by
a synchronous resource prefix, invalid submission/abort and subsequent frame
recovery. The ARGB oracle uses the retained source expression, including its
compile-time opaque-alpha behavior; forced dynamic arithmetic is not the same
reference under upward rounding.

Windows Gamma gameplay verifies 77,383 submissions / 1,874,720 vertices with
zero differences. A separate installed-settings 5..125 image pair saves
1.90% producer CPU time (13.671875 to 13.411458 ms/image) and 3.84% execution
cycles. Both remain capped at 60 output images/s; ticks 126..246 and both
boundary XYZ/HUD values match exactly. This is a small incremental CPU gain,
not the user's requested large global/Deck improvement.

The first Linux Gamma verification run ended at the diagnostic deadline
before its requested complete window: 18 new images, 13 in the partial
measurement. No vertex mismatch occurred, but it is NOT a passed full stage
window or a throughput comparison. Producer CPU was 517 ms/update; process
CPU was 21.39 seconds/update. Main-thread sampling primarily found waiting;
audio synthesis and llvmpipe dominated other threads. Repeated presentations
rose to about 57 per new image versus five in the previous native-owner run.
The cause needs the normal on/off comparison; neither the verify overhead nor
the small Windows result establishes Linux performance.

Evidence: `runs/model-packets-*-latest.log`, Windows component logs,
`runs/model-packets-windows-verify/result.json`,
`runs/model-packets-windows-{on,off}/result.json`, and
`runs/gamma-model-packets-verify-20260916.json`.
Linux game SHA-256:
`6bf2d8ac3491d306a43cafbeeb75446a2ba5ca1eeafaff87fa6977e20ed35e2c`.

After introducing the nonblocking ADX status publisher, the Linux Chaos-4
stage-entry/cutscene probe completes its full 5..25 window with packet
verification enabled and no vertex mismatch. It retains Original cadence
(50 Hz video, release/delta 2), produces 20 images / 65 updates and exits
through the expected diagnostic stop. Candidate SHA-256 is
`a944a94bbd9aeab3f62ac6db68d9e148067be9e2a602fd4b9a56838e46054fac`;
evidence is `runs/native-memory-linux-chaos4-intro-command.log`.
Verification rebuilds both vertex forms: its throughput is not a performance
comparison.

The subsequent normal Linux Gamma pair on SHA-256
`a5b78b0ba71f00be09a955368fcbbc17664b59416f5511ae6be2e793bf7da579`
does not qualify promotion. Packets ON costs 418.6231 producer CPU ms/update
versus 423.5076 OFF (1.15% less), but image throughput is 0.240460 versus
0.268100 images/s (10.31% worse). Both complete 20 updates. Audio status,
animation, pose and closed-memory owners are ON in both; sound metadata and
deferred notes are OFF. This remains internally OFF and excluded from the
requested patch. Evidence: `runs/model-packets-linux-on-gamma-command.log`
and `runs/sound-command-linux-off-gamma-summary.json`. This VM result makes
no claim about Steam Deck throughput.
