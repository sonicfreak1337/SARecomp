# Camera style

Select **Original** or **Recompiled** in the English `sonic-config.exe`.
The native window menu also has **Camera style (Neustart)**. Selection persists
as `camera_style=original|recompiled` and applies on restart. Missing settings
default to Original; an incremental build does not change the user's INI.

Original immediately continues the original camera publisher without guest
memory access. Recompiled uses P1's right stick to orbit around the character:
horizontal movement has no yaw stop; vertical movement changes elevation.
The chosen angle follows the character instead of returning to stage-camera
directions each frame. There is a radial stick deadzone and neutral handling
for disconnect/suppression. Movement, buttons and other controller slots keep
their existing mapping. No additional platform input poll is performed.

The first orbit inherits the original eye position, with distance limited to
24–65 world units. Pitch ranges from a 75-degree downward view to a 5-degree
upward view. The low limit keeps the orbit above the player's foot height on
flat ground. This is not swept wall/ceiling collision: tight or sloping geometry
can still intersect this experimental camera. Original remains available.

## Original camera ownership

This is a Sonic PAL v1.003 feature, not a Katana-wide camera heuristic. The
hook is bound to the original `8C01A100`, size `AC`, SHA-256
`f88a14755daffb71dc3b9490f35660e768605e121e8f83e5df2dd2a8f541d002`.
Only the normal state-2 camera task's call with PR `8C01991C` may be changed.
It runs after the original callback/adjustment and before original NINJA
view publication, so rendering, culling and the guest camera share the view.
The frozen AOT partitions and pinned SDK are retained.

The only guest write is the camera task's contiguous `+14..+2B`: pitch,
yaw, unchanged roll, then eye XYZ. The original publisher still runs. The
angle convention matches the original `01A680/01A718` conversion. CPU
registers, floating-point flags, player work and other camera fields are
preserved.

The active control record is `U32[8C111F88]`: **byte +6** is the camera type,
**byte +7** its output format, **byte +8** its active level, and word +12
the callback. A word read at +6 would incorrectly include the format byte.
Only level 0/1 and the reviewed P1 follow/area type-and-callback combinations
in `sonic_camera_policy.hpp` admit manual control. Level 1 includes ordinary
spatial camera areas; higher levels retain their complete original camera.
Unknown types or a changed callback also retain original ownership.

This preserves explicit event/path registrars `01AD2C` and `01ABDC`, which
raise the active level to 2. STG01 and STG07 use these paths; further explicit
registrars use levels 4/5. Area priority 3 is also excluded. Level alone is
insufficient: some fixed and timed sequences use ordinary priorities.
The conservative type policy additionally excludes:

- 26/27, 28/29: external or fixed target/eye data.
- 48/49: fixed world coordinates or a multiphase authored sequence.
- 51–57: external actors, timed phases or module-owned callbacks.
- 62/63, 65–70: authored task poses, special area sequences, path/entry
  selection, external positions or timed camera state.

The policy contains no stage-specific trigger or whale/boulder coordinates.
The source checks establish these mechanisms in Emerald Coast and Lost
World, but do not establish a completed playthrough of either chase.

Manual control also suspends outside running state 15, for pause (16),
outstanding transitions, the original pause/input-block predicates and the
special-view path. It reacquires the original eye after a suspension,
stage/act/character change, changed task owner, teleport or quicksave restore.
A restored state can retain all pointer values; an explicit host-only reset
handles that case after the existing RAM restore validation.

## Timing and verification

The camera integrates host elapsed time once per game frame, capped at
250 ms after a stall. Duplicate publishers reapply the pose without either
integrating again or discarding elapsed time before the next frame. This is
independent of repeated 144 Hz presentation. Camera input is not guaranteed
to reproduce identical angles from a replay executed at a different speed;
diagnostic traces include the actual integration interval and pose.

`sonic_camera_tests` checks a full revolution, vertical limits, target follow,
raw-stick normalization, frame-rate independence, duplicate publication,
Original passthrough, pause/script guards, restore reset and guest ABI/write
boundaries. `sonic_presentation_tests` covers settings persistence and the
retained aspect/HUD transformations. The config control test covers the new
selection without opening a visible window or editing personal saves.

For the required actual gameplay check:

```powershell
./tools/capture-stage.ps1 -Tag camera-check -CameraStyle recompiled -CameraTest -Renderer d3d11 -Seconds 20
python tools/check-camera-capture.py runs/camera-check
```

`-CameraTest` provides a bounded right-stick sequence only inside an owned,
hidden, muted test process. It uses the production normalization/orbit path
and never injects operating-system input. The checker requires a continuous
360-degree orbit, both vertical extremes, a character-aligned view, captured
frames and the expected diagnostic deadline. Captures must also be inspected
visually. Test saves and configuration are private copies under `runs/`.

Verified on 2026-09-12 with the actual native game, hidden and muted:

| Backend / local run | Continuous rotation | Vertical view | Gameplay samples |
| --- | ---: | --- | ---: |
| D3D11, `runs/camera-orbit-d3d11-02` | 632.72 degrees | -75 to +5 degrees | 280 |
| Vulkan, `runs/camera-orbit-vulkan-04` | 566.65 degrees | -75 to +5 degrees | 187 |

Both reached the expected diagnostic deadline without a runtime fault. The
captured side/front/rear, low and overhead views keep Sonic in the center;
HUD placement remains fixed. These are stationary orbit checks, not a new
stage matrix. Character-follow translation and 39 protected-state cases
passed the component test. Config persistence, Original passthrough, camera
menu selection and restoration also passed.

The source audit covers the original in-level event mechanisms. The whale
and boulder chase routes themselves have not been played in this test.

Two intermediate hidden Vulkan launches failed before any camera hook at
`vkGetPhysicalDeviceSurfaceCapabilitiesKHR` with result -13. Their cause is
not established; the subsequent run above passed. The renderer now records
window validity, client bounds and owning thread if this error recurs; it
does not suppress or retry an unexplained failure. Logs are retained under
`runs/camera-orbit-02` and `runs/camera-orbit-03`.

The final build changes only numeric type formatting in the camera trace
relative to the successful orbit runs. The final Vulkan Original-mode probe
(`runs/camera-original-final`) also reached its deadline, with zero manual
camera callbacks and no surface/runtime failure. The final config control
test and 39 guard cases passed again. All owned game processes exited.

Final build: `.local/camera-build-final.log`, 61.673 seconds, zero recompiled
AOT partitions, native link audit passed. The r354 executable/metadata hash
check passed. The user's current Vulkan/borderless/German INI was preserved.

- `out/experimental/game.exe` SHA-256:
  `bc52c8afb21500d11c4a98ff8cd4aab88337b7c9f7100ae4a792a2d7ee634cbe`
- `out/experimental/sonic-config.exe` SHA-256:
  `116a8e7d7442153df8aff4dc3d0d41c446268e8cdebe32299c604b552c24728a`
