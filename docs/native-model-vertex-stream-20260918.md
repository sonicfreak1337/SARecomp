# Native model vertex stream, September 18, 2026

Status: private experiment, OFF by default. The delivered September 18 patch
and all test installers remain unchanged. This follows the complete render /
model group in `native-groups-next-profile-20260918.md`; it does not re-enable
the previously rejected consumer-side expanded-packet experiment by itself.

## Connected boundary

The title retains the authored point/UV index topology, material state and an
immutable attribute snapshot for the ordered renderer. The Vulkan consumer
decodes each model point's position, normal and two colors once under its
captured host rounding mode. A vertex shader gathers these 64-byte point
records through 12-byte authored corners, replacing the repeated construction
and transfer of 76-byte expanded corner records. Original projection/palette
RAM effects, hierarchy callbacks and simulation timing remain on the CPU.

This covers the existing admitted TitleBasic shared-corner path across scenes.
Flat, near-clipped, environment-mapped, observed/diagnostic and exceptional SDK
material paths retain their existing handling. Windows Vulkan and Linux use
the same shader/consumer; D3D11 currently keeps the expanded fallback.

`SARECOMP_NATIVE_MODEL_VERTEX_STREAM=1` enables the whole optional transition.
An explicit `SARECOMP_NATIVE_MODEL_PACKETS=0` still overrides producer admission.
There is no product menu setting. Both benchmark tools have an explicit
`--native-model-vertex-stream on` selection so measurements cannot silently
inherit an unrelated shell setting.

## Ownership and GPU contract

- Point reuse is keyed by an owned immutable attribute generation and its host
  rounding mode; topology reuse is keyed by an owned immutable geometry.
  Neither uses a guest address or caches mutable guest state across calls.
- Caches are recording-local and bounded to 4,096 entries. Source owners are
  released when recording is sealed; copied upload bytes and descriptors stay
  alive until that submission's Vulkan fence completes. Overflow retains normal
  uncached submission instead of dropping work.
- The gather constants/storage descriptors use a separate set 1. Existing scene
  and Type2 capture descriptors remain set 0. Dynamic offsets respect device
  alignment/range requirements and bound descriptor sets are never rewritten.
- Queue budgets conservatively include retained host sources and worst-case
  point/corner/index GPU uploads, even when actual immutable sources are shared.
- Original host FPU state is restored. In particular, upward rounding on Linux
  retains the runtime white-color selection before conversion; folding an
  unconditional literal would change its RGB bits. No comparison tolerance was
  introduced to hide that discrepancy.

## Qualification so far

Windows and Linux each pass 256 exact retained-vertex comparisons across host
rounding, denormal policies, UV seams, palettes, normals and material flags.
Actual Vulkan serial and parallel renderer tests each compare 20 captured
frames byte-for-byte, with 19 confirmed new-path draws. They cover ownership
release, ordered resource prefixes, reused point uploads, different snapshots,
opaque/Type2 materials, lighting, vertex fog and producer-abort recovery.
Windows D3D11 passes the same image cases with zero new-path GPU draws,
confirming its deliberate fallback. Captures contain varying rendered pixels;
they are not pairs of empty frames.

The Linux parallel fixture also passes with Khronos Vulkan validation enabled,
without validation messages. The first added Type2 fixture was rejected because
it supplied a forward-depth contract; the fixture now uses the renderer's
required reciprocal-depth convention. Product validation was not weakened.

Evidence: `runs/model-vertex-stream-components-windows-final-20260918.log`,
`runs/model-vertex-stream-linux-serial-20260918.log`,
`runs/model-vertex-stream-linux-parallel-20260918.log`,
`runs/model-vertex-stream-linux-validation-20260918.log`, and corresponding
Windows Vulkan / D3D fallback logs and frame directories.

Incremental game builds pass for Linux and Windows. The isolated Windows
output is `out/model-vertex-stream-windows-20260918`; the Linux executable is
`build-linux/game`, SHA-256
`7d76509b3ec3cdb16e87ea3de26b7c8bfe0f73de80e09dfa1cf66ff301611e11`.
The Windows game SHA-256 is
`efa0002a158c4cea8bf5e6802865ee7133e8818d14f51e405b952fb10ec13e11`.

## Whole-game decision: remain OFF

One Gamma pair per platform is enough to reject promotion of this version;
do not repeat the unchanged pair or expand the level matrix.

On the real Windows Radeon GPU, the same-executable Original pair matches all
16 endpoint fields and 48 updates. New images remain at the original cap
(25.029 versus 24.989/s). Execution cycles per image change +0.38%, coarse
thread CPU time +4.55%, and process CPU time -3.23%. That is no meaningful
reduction in the serial game bottleneck. The new path actually handled 13,531
draws, reusing 5,536 point uploads and 5,614 topology uploads during the window.
This is not an accidentally disabled feature or an inferred Deck speedup.

The Linux software-rendered pair also completes, with 68 updates per measured
window, but differs by one starting update and small endpoint position/counter
differences. It therefore fails the exact-work gate. It trends substantially
slower: execution CPU/update 160.849 -> 193.227 ms, process CPU/update 763.533 ->
1,145.961 ms and new images 1.239 -> 0.770/s. These are adverse VM observations,
not exact percentages attributable to the new path or measured Deck GPU costs.
The first launch in the isolated Linux directory lacked the existing menu
assets and stopped before the level; that setup failure is excluded. Linking
the existing read-only test assets resolved it, without a product code change.

Evidence: `runs/model-vertex-stream-gamma-comparison-windows-20260918.json` and
`runs/model-vertex-stream-gamma-comparison-linux-20260918.json`, with their
underlying `model-vertex-stream` result/summary files. Legacy expanded-vertex
upload telemetry excludes the optional gather buffers; dedicated model draw /
point / topology counters prove activation. Neither is used as a substitute
for measured CPU time.

Keep the qualified private implementation for future connected work. Do not
extend it to D3D11, change product defaults or build a new patch on the strength
of these results. The next useful serial scope is the full movement/NEAR/
TOUCH/contact family in the existing profile, sharing admission and ownership
across the whole operation rather than enabling its previously rejected
individual switches.
