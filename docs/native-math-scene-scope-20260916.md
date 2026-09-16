# Native math across scene transitions

The user's current Deck report distinguishes roughly 45-55 in-game SIM FPS
in Recompiled from lower Original image throughput, and reports especially
poor cutscenes and Chaos 4. The Original-camera report rules out the optional
free-camera collision cache as their common cause.

## Found policy defect

The previous gate admits native math only during the configured gameplay
cadence: main states 4/5/9, scenes 15/16, and a qualified timing pair. Scripts
and boss introductions can therefore return to translated math even though
their arithmetic and memory contracts are the same.

The eight leaf families now use their existing code, CPU/FPU, memory,
observer and alias guards independently of scene selection: palette lighting,
vertex normals, animation sampling, atan, inverse/determinant, matrix vectors,
collision vector math and matrix stack operations. The topology/UV source-plan
cache and within-draw corner reuse follow the same policy. These caches do not
retain a previous pose, world position or light result.

The callback-bearing triangle-contact and collision-candidate owners keep
their narrower gameplay gate. Individual overrides, the retained-math override
and the older private 60-Hz fixture retain their behavior. The internal
`SARECOMP_NATIVE_MATH_GAMEPLAY_ONLY=1` switch reproduces the previous gate for
a same-executable comparison. There is no new end-user setting.

## Timing remains independent

This change does not modify the game scheduler or reduce logical updates.
The SIM display counts new game images, not every task traversal. The PAL
Original wrapper can perform 2/3 consecutive updates per image, and 3/4 under
its authored overload compensation. Recompiled gameplay uses its qualified
60-Hz, 1/1 cadence. Dropping the extra Original updates while limiting output
to 25 images/s would drop time progression rather than remove duplicate work.
See `original-update-live-20260913.md` for the executed wrapper evidence.

## Chaos 4 probe

A separately reviewed diagnostic row uses the existing resident stage loader,
not a direct call into a PRS image. The PAL boot SHA256 is
`b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af`.
Table row 37 at 8C1C3E86 selects 17:0; context row 0 at 8C1C3F10 selects Sonic.
Case 8C09D21E loads B_CHAOS4 through 8C09D984 and retains its ordinary 1700/0000
resource loads. Runtime base is 0C900000, entry 0, callback 60.

Encoded: 215545 bytes, SHA256
`9dd44867ea3e1881bdf11a6ade50869f3e4749f5440f7ce8fd10cfbb8f8719c3`.
Decoded: 383276 bytes, SHA256
`4e5da90b871011e4406654c634bf6a706723779df753f12c94a48a2d671efbf0`.
The generated 32-stage catalog and mechanical 438-selector inventory stay
unchanged. No boss victory, story flag or personal save is synthesized.

The first completed hidden Linux run reaches main 4 / scene 4 in the boss
introduction. Its old gameplay gate is false while native leaves execute,
confirming the policy gap on this real path. PAL50, release2 and delta2 remain
unchanged. Four runs now compare the two gates with the same executable
`f2bd6531ae4cb95fef8fa3c04d9e7c0dfa6bf5d56aaa5e177d6e441fe7081532`.
Window: images 5 through 25, original camera, diagnostics off. Both introductions
remain main 4 / scene 4 and PAL50 release2/delta2, even with the Recompiled setting.

| Introduction | Old CPU ms/image | New CPU ms/image | Old images/s | New images/s |
|---|---:|---:|---:|---:|
| Chaos 4, Original | 929.88 | 737.97 | 0.5433 | 0.5809 |
| Gamma Emerald Coast, Recompiled setting | 1027.58 | 787.76 | 0.4179 | 0.3761 |

Execution CPU per image falls 20.64% / 23.34%; whole-image throughput is mixed
(+6.91% / -10.00%). These are not qualified overall or Deck FPS gains. Chaos 4
has 67 versus 68 updates with a one-tick start difference. Gamma has 68 in both
runs, one-tick start difference, and identical boundary XYZ/HUD bits. All four
complete their expected host deadline without forced stop. The summaries are
under `runs/native-math-scene-scope-20260916/`. No further introduction pair is
needed to establish the scope defect; the larger native pipeline remains work.

## Review follow-up

The four older transform-stream hooks remain in the pack, but the normal Basic
model composites already bypass that owner. Existing B04 sampling does not
establish those four hooks as a hotspot. The proposed coarse transform-stream
kernel is deferred. `sonic_model_projection.cpp` is only an excluded test; its
previous game integration was removed after no useful FPS gain. Neither is
being reintroduced as a new optimization on the basis of this review.

Build and measurement are incremental in the Linux test candidate. The
installed CPU update, user saves and immutable r354 baseline are unchanged.
QEMU TCG/llvmpipe results are not Steam Deck FPS estimates. Two initial probe
attempts omitted Xvfb and failed at window creation before running the game;
the harness now rejects a missing display before launch.
