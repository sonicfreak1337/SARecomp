# Camera style

Select **Original** or **Recompiled** in the English `sonic-config.exe` or the
native Camera style menu, then restart. `camera_style=original|recompiled`
persists in the display INI; Original remains the default.

Recompiled keeps the original gameplay camera until P1 moves the right stick.
Horizontal input orbits without a yaw stop; vertical input raises or lowers
the view. The first input inherits the original eye and distance. Merely
moving horizontally does not clamp the inherited elevation. Each stick axis
has a 12% deadzone, preventing small vertical noise from lifting the camera.
The manual pitch range is -75 to +5 degrees; speeds are 240 degrees/second
horizontally and 120 vertically. A stall contributes at most 250 ms.

Releasing the stick while stationary holds the chosen view. After three
seconds without right-stick input, **walking** smoothly returns ownership to
the original camera. Stick input immediately retakes control. Simulation
cadence and repeated 144 Hz presentation do not change.

## Original camera and scripts

The original camera continues evaluating while the manual view is active.
Before that evaluation, a source-bound hook restores its saved pose only if
the current task still contains our last published pose and has the same
owner/state. This prevents our view from feeding back into the original
camera's height/history. After evaluation, the manual view can replace the
published pose; the original NINJA publisher still runs.

The PAL v1.003 hooks are:

- `8C019F4A`, size `158`, caller PR `8C019918`: original-pose restoration.
- `8C01A100`, size `AC`, caller PR `8C01991C`: final view publication.

Both are bound to verified original bytes in the manifest. Writes stay in
the camera task's 24-byte pose (`+14..+2B`). CPU state, player work and other
camera fields remain intact. Original bypasses both hooks without guest
memory reads or writes. No frozen AOT partition or pinned SDK is regenerated.

Control-record byte +6 is the camera type, byte +7 the output format, byte
+8 its active level, and word +12 its callback. Only reviewed follow/area
types at levels 0/1 admit manual control. Unknown or changed callbacks,
higher priorities, explicit event/path registrars, fixed/timed sequences,
pause, cutscenes, transitions and input-blocked states retain original
ownership. The policy uses no whale/boulder coordinates or stage trigger
list. Their event registration paths were reviewed in STG01/STG07; neither
full chase was replayed in this verification.

A stage/act/character change, changed owner, teleport, suspension or quicksave
restore resets the manual state. Restore explicitly resets host-only state
even when guest pointer values are unchanged.

## Wall collision

The camera sweeps a radius-2 sphere from its target to the desired eye with
0.35 units of clearance. It retracts immediately on collision and recovers
its distance smoothly. It uses actual registered static LandTable and dynamic
object collision triangles, including transforms and same-address geometry
updates. Solid geometry participates unless marked NoCam. Faces, edges and
vertices are tested from both sides through a cached BVH. Invalid geometry
or an overlapping starting position leaves the original camera in charge.

This is a target-to-eye boom sweep, not a temporal sweep of the entire orbit
arc. Invisible original collision boundaries can also shorten the boom;
complete behavior in every tight/sloping location is not established.

## DualSense

The pinned WinMM path assigned Sony right-stick input to R/U. On DualSense,
right stick is Z/R and U is a trigger; its released value falsely produced
full camera input. A source-verified **port-local** platform object now maps
Z/R only for DualSense plus Recompiled camera. Xbox, Original, other Sony
controllers, movement, buttons and replay packets retain their prior paths.
There is no extra input poll and no edit to the pinned SDK.

Neutral live DualSense input was captured as approximately (-257, 1) and
normalized to zero. The user then physically tested and accepted the fix.
This is a camera-axis correction, not a redesign of legacy trigger mapping.

## Verification

`sonic_camera_tests` checks full orbit, vertical control, target follow,
DualSense neutral/endpoints, deadzones, no height drift, original pose shadow,
three-second idle/walking return, collision geometry/registry updates,
restore, ABI/write bounds and 40 protected-state cases.

The hidden/muted actual-game D3D11 run `runs/camera-wall-d3d11-02` completed
its 25-second gameplay deadline. It captured 1,003.64 degrees of rotation,
-75 to +5 degree pitch, 37 images and 19 collision samples. The shortest boom
was 18.66 instead of 36.00 units; quiet stationary samples had no height drift.
The hit was original Emerald Coast collision geometry, not necessarily a
visible wall. Captured views kept Sonic centered.

For an owned gameplay check without desktop input or personal-save writes:

```powershell
./tools/capture-stage.ps1 -Tag camera-wall-check -CameraStyle recompiled -CameraCollisionTest -Renderer vulkan -Seconds 30
python tools/check-camera-capture.py runs/camera-wall-check --collision --return
```

The helper supplies stick/walking input only inside that hidden test process.
The checker requires a full orbit, vertical extremes, actual collision,
return while walking after the idle interval, subsequent manual reacquisition,
frame captures and the expected diagnostic deadline. Inspect the images too.
This is a bounded Emerald Coast check, not a level matrix.

The final startup build's `runs/startup-camera-vulkan-04` also reached its
deadline with 184 manual samples, a full orbit and both vertical extremes.
Its combined collision/return checker did **not** pass: the final quiet
section was shorter than 30 samples and the return walk did not establish a
completed OG handoff. The component return test passed; this run must not be
presented as additional end-to-end proof of that handoff. Further owned runs
were deferred because the user had started a separate game instance.

Earlier hidden startup diagnostics recorded two Vulkan surface-capability
errors (-13) and one frame-zero Windows heap exception (`c0000374`) before
camera execution. Their causes are unproven; neither is a demonstrated
camera-controller fault. Exact platform source/header/ABI checks passed and
a later live-controller launch reached gameplay without those exceptions.
Do not erase this distinction when describing successful camera tests.
