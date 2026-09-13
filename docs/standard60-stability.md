# Standard gameplay cadence and stability — 2026-09-13

The user accepted the current performance and explicitly requested the native
60-frame path as the default. Direct `out/experimental/game.exe` now uses it;
no fixture launcher, replay, altered save namespace or probe flag is necessary.
The accepted r354 snapshot remains unchanged.

## Cadence ownership

Normal gameplay and pause in main states4/5/9, scene15/16 with no pending
transition can promote original2/2 scheduling to1/1. The complete original
PAL051760 setter installs callbacks and resets its iteration/ready state.
PAL658744 installs the authenticated 60-Hz video registers when required.
Presentation remains separately configured at144; interpolation stays disabled.

The initially omitted main4 caused Action Stages to stay at30 while Adventure
Fields reached the new path. Original main4 and main9 both dispatch04CA40;
main5 uses04DA20. The final guard includes all three. This is a dispatcher
family correction, not a per-level address exception.

Original2/1 script and1/1 menu/minigame requests retain their meaning. Leaving
the eligible gameplay frame restores the last original request through the
complete setter. `SARECOMP_ORIGINAL_CADENCE=1` retains the old scheduling path.
Unknown-rate legacy quicksaves retain their previous cadence until a real
video-mode owner supplies evidence.

Palette lighting, vertex normals, matrix push/pop, vector collision math and
matrix inversion use the same authenticated native leaves as the user's
accepted manual run. Triangle contacts, atan and matrix store batching remain
private experiments. No new performance optimization is claimed here.

## Transition crashes

Private live stage selection used to jump to retail selector state12. That
publishes/loads the target but does not destroy a running gameplay scene.
Same-character, different-major selections now request the original04F794
transition and pass through scene17/04EE60 cleanup before target adoption.
Active events and incomplete transitions reject a debug launch. Cross-character
and same-major debug requests remain unavailable during live gameplay rather
than bypassing those ownership rules.

Overlay task retirement now traverses child lists as well as bucket siblings,
validates the full recursive destructor family, and takes a fresh graph after
each original destructor. It rejects current-task deletion, stale ancestry,
duplicate/cyclic links and unsafe callbacks. Inner native callback exceptions
also retain their original diagnostic message and instruction context.

## Quicksave and personal save

Schema6 stores standard60 ownership and the original requested release/delta.
Restore uses the snapshot fields, never the unrelated currently running level.
Preflight checks the saved video mode and RAM pair; incompatible opt-out loads
are rejected before commit. Existing exact legacy compatibility rules remain.

The hidden roundtrip exposed a second restore error: applying audio gains
between audio restore and frame commit rebound the audio cursor to the old
live frame. Unpause then saw a frame regression. Gain refresh now happens
after committing the saved frame and checking the restored RAM. The SDK's
monotonic audio guard remains intact.

The user's requested full-clear save was found in r354, byte-identical to the
old Katana personal container, and imported into the active `default` profile.
It contains both `SONICADV_INT` and `SONICADV_ALF`, generation262, 19,842bytes,
SHA256 `88a84d519974dd3209d24669d38021cf383d1c8b5e55e415bae64709c83d977c`.
Previous primary/recovery files were versioned in the local profile backup
library. Test runs use copies; no personal save is committed to Git.

## Chao symbols

The PAL64FBA8/64FA28 filled-contour emitters alternate front/back corners:
`0,last,1,last-1,...`. The native path incorrectly treated the contour as an
already sequential strip. For a four-corner billboard this overlaps one
triangle and leaves a wedge uncovered. The corrected ordering keeps each
vertex's position, UV and color together, including after near-plane clipping.

Authenticated AL_MAIN code constructs a rectangle with perimeter UVs and calls
63D354 with count4/flags80000060 through this exact path. The unrelated sprite
emitter already has a correct strip and is unchanged. The geometric regression
checks cover22 convex/clipped contours, both winding directions and complete
area without overlaps. A new in-game Chao emotion capture is not yet recorded;
the specific photographed states are therefore not visually re-verified.

## Verification and limits

- Earlier explicit Sonic matrix: ten action stages,60seconds each with held
  forward input, no crashes. This used the private fixture and is not a claim
  that every complete level, story transition or cosmetic timing is verified.
- Standard product policy: Emerald Coast→Speed Highway passes via original
  cleanup and reacquires native1/1 gameplay. `runs/standard60-level-switch-02`.
- Standard Pause→Quit: D3D11 and Vulkan pass through cleanup and60stable
  destination frames. `runs/standard60-pause-d3d-01` and
  `runs/standard60-pause-vulkan-01`. The debug exit destination is stage0:0;
  these tests do not assert a return to the title screen.
- Task-graph component:23cases pass. Contour geometry:22cases pass.
- Quicksave roundtrip after the audio fix: `runs/standard60-quicksave-02` saves
  at frame600 and loads twice. All three RAM digests match; both restores
  preserve `standard60=1` and original2/2. The final executable is bound in
  that run's result, separate from the preceding transition-test executable.

All live tests are hidden/muted, use copied saves and no OS input or replay.
Cadence activation logs prove the1/1 policy and native leaf execution, not a
universal sustained60FPS hardware result. User-reported stable60 performance
and earlier instrumented measurements remain distinct. The prior consumer
timing audit (ring blink, hint monitors, outer fades) is not a completed
whole-game original-speed certification.
