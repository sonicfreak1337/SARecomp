# Experimental render interpolation

**Withdrawn on 2026-09-12 at the user's request.** The user confirmed that
the whole-environment motion glitches disappear with interpolation disabled.
The prototype is retained only for reference and is compiled out. Display no
longer offers it; old `interpolation=1` settings are ignored and omitted on save.
The current generated dispatcher retains native hook bindings without motion
instrumentation; the frozen r354 AOT pack remains reused. Normal independent presentation
up to 144 FPS and the original simulation cadence remain unchanged.

The following notes are historical prototype work, not current functionality.

## Current implementation

The producer attaches port-local metadata beside the frozen SDK command
stream; SDK diagnostic fields are not repurposed. Frames retain owned copies
of transient vertices and indices. Persistent mesh/texture handles retain
their original generation identity. The render consumer can redraw a complete
scene at intermediate object/view transforms without executing guest code or
polling input. This introduces approximately one source-frame of visual delay.

Matching currently covers unchanged local Basic model geometry with the same
task/model/mesh ownership. Duplicate identities, topology/deformation changes,
large rotations and translations are rejected. UI remains at its authored
screen position. Pretransformed world geometry and other unproven ownership
are not independently interpolated. This is not yet a complete camera and
animation interpolation implementation for Sonic's full renderer.

Only the reviewed normal gameplay camera types may submit frame metadata.
Events, cutscenes, menus, pause, actor/stage/camera changes, large camera or
player jumps, reversed clocks and gaps above 100 ms break history. Quicksave
load advances an independent epoch. Source frame intervals are measured;
the implementation does not assume every scene produces exactly 30 frames/s.
Output alpha is clamped; it never extrapolates.

Texture updates and texture/mesh destruction first drain any deferred draws
that reference old resources, then invalidate the retained scene. The actual
resource operation still follows the original backend contract. Images/movies,
command errors and buffer exhaustion also leave the ordinary rendering path
in control. Each owned frame is limited to 32 MiB and 32,768 entries.

## Visual regression and correction

The first game integration allowed only the matched subset of a scene to
interpolate. This was incorrect: the model transforms include camera motion,
so unmatched terrain, sprites and shadows used a different camera instant.
The user reported glitchy movement, especially in Station Square. The hidden
Emerald Coast orbit in `runs/motion-emerald-before-01` demonstrated incomplete
coverage (commonly 172–175 matched draws out of 266 total), intermittently
falling to zero. The successful isolated triangle test had not covered this
whole-scene requirement.

World coverage is now explicit, including unproven world draws. If any world
draw cannot be matched, the entire frame uses the current original pose.
The fallback remains latched for that camera/scene epoch, preventing a moving
scene from alternating between delayed and undelayed images as visibility
changes. Such scenes currently have no motion interpolation. This protects
visual consistency; it does not complete the requested enhancement.

## Evidence

`tools/test_motion.cpp` checks analytic intermediate positions, immutable
geometry, duplicate ownership, source timing, angle wrap, teleports, scene and
quicksave epochs, bounded storage and complete-scene fallback. A mixed known
model plus unproven world draw must render both at current pose.

`motion-coherent-vulkan-01` and `motion-coherent-d3d11-01` under
`.local/enhancement-tests/` each pass real GPU image assertions: eight native
frames, 27 presentations, 15 intermediate images and 20 distinct positions.
HUD positions are unchanged; an in-frame texture update preserves earlier
users of the old texture, and subsequent destruction cannot replay a stale
handle. Image tests use an explicit hidden-test clock because synchronous GPU
readback can exceed a simulation interval. These are not FPS measurements.

Whole-game acceptance and complete world/camera coverage remain pending.
The corrected game build `build-motion-coherence-game-01.log` completed in
68.697 seconds with zero AOT recompiles and passed the native link audit.
The 20-second hidden/muted Vulkan orbit `runs/motion-emerald-after-01` then
finished normally at source frame 383. Source frames 250 and 251 both rendered
at alpha 1 with zero interpolated draws; the second frame rejected 412 of 671
world draws and latched the complete scene onto ordinary rendering. Subsequent
camera movement therefore used no partial intermediate poses. The inspected
captures at output frames 1670 and 2300 retain a coherent character, shadows
and terrain. This verifies the conservative fallback in Emerald Coast, not
completed interpolation or a Station Square playtest. The quick r354 audit
also passes; the user's own PID 32392 exited normally without intervention.

`tools/capture-stage.ps1 -MotionInterpolation` enables private capture, while
`tools/benchmark-stage.py --interpolation 1` enables isolated timing without
readback. User saves and the normal configuration are not rewritten by these
helpers. Station Square is currently a selector inventory entry, not an
implemented direct scenario launch in the helper.

## Actual task ownership

The next diagnostic pass found a separate identity error: 8C1C1C80 contains
the scheduler's callback, not the current task. Different instances of the
same object therefore collided in the interpolation index. The retained
guest scheduler confirms this with the store of R14 at 8C0986F4.

The port-local central call dispatcher now scopes ownership from the real
post-delay-slot R4 argument at the three scheduler callback return sites:
8C0986FA (update), 8C098768 (display), and 8C09879C (child display). Original
instruction bytes and the task's callback are checked before annotation.
Nested callbacks restore their parent owner, including exception unwinding.
Ordinary helpers inherit the owner; unverified task boundaries clear it.
No guest register, task memory, callback, or AOT unit is modified.

tools/prepare-motion-dispatch.py verifies the authoritative generated-source
manifest and writes a separate annotated build copy. The working generated
source and its manifest remain untouched. The identity includes real task,
work pointer, model, mesh, and callback. This does not yet distinguish every
repeated model inside a single landtable task, or pretransformed world draws.
The complete-scene fallback therefore remains required and enabled.

The incremental build build-motion-owner-01.log took 70.376 seconds, compiled
zero AOT units, and passed the native link audit. motion-owner-tests-01.log
checks distinct instances with identical callbacks, nested/exception scope,
unchanged guest registers, invalid scheduler bytes, bounds, and the existing
whole-scene coherence contract.

The hidden, muted 20-second Vulkan camera orbit in
runs/motion-owner-emerald-01 ended normally at source frame 398. Actual draw
owners now contain task pointers rather than callback addresses. At source
frame 251, 294 of 667 world draws still lack a safe match; all world draws
therefore use alpha 1 together, with zero partially interpolated draws.
Output captures 1130 and 1580 were inspected. No diagnostic game remains.
This is a verified ownership correction and conservative visual safeguard,
not full gameplay interpolation or a Station Square acceptance test.

## Shared camera across the complete world

The next implementation reads the original NJS camera publication rather
than guessing a view from arbitrary object transforms. The retail 01A100
publisher calls 640480, which constructs 7888C4 and publishes that pointer at
8E9E40. Capture verifies the pointer, a rigid basis and agreement with the
original camera eye before accepting it. Projection uses the original logical
raster constants, including signed vertical scale and the selected Hor+ factor.

The renderer interpolates a rigid camera pose and reprojects all supported
world draws through that same pose. Object-space Basic geometry separates
model motion from camera motion. Pretransformed PVR world vertices are
unprojected with reciprocal depth and reprojected, retaining UV/color contracts.
This covers camera motion even when repeated terrain has no unique object
identity. Independently moving unmatched geometry still uses its current model
pose; this does not interpolate all animation. HUD coordinates remain fixed.

Projection mismatches, unsupported world vertex formats and possible near-plane
crossings reject shared camera interpolation for the whole scene. The existing
coherent current-frame fallback remains; no partial camera pose is presented.
No game task, camera memory or simulation tick is modified.

CPU tests cover translation, rotation, fixed handedness, stale/nonrigid views,
near-plane safety and unchanged HUD. The analytic GPU scene combines near 3D
geometry, a far pretransformed world primitive, depth occlusion and HUD. Its
interpolated midpoint exactly matches an independently rendered midpoint on
both Vulkan and D3D11: zero changed pixels in motion-shared-vulkan-02.log and
motion-shared-d3d11-01.log. The first Vulkan attempt exposed an invalid test
packet submission order; that fixture was corrected before the passing runs.

The combined build-casino-camera-01 link includes this path. Actual-game camera
binding and a Station Square movement acceptance check remain pending: the
next hidden orbit was declined by the harness when the user started PID 17444.
No test process was launched alongside that run, and it was left untouched.
Do not present the analytic GPU comparison as a completed native-game test.
