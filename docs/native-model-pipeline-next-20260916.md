# Next complete native model boundary

The animation/pose owners in `native-animation-hierarchy-20260916.md` are
implemented. This document records the next audited structural boundary;
the pipeline below is not implemented or included in an installer.

Title owners `8C037098`, `8C037108` and `8C036FFC` surround native transform,
palette and draw operations. Replacing only their wrapper with three native
calls mostly removes wrapper overhead. A larger replacement needs a shared
capture and different geometry submission, preserving title-visible effects.

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
