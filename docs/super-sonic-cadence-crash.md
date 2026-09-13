# Super Sonic story: native cadence boundary — 2026-09-13

User crash session `1789303678884-39032`, frame1604:

- NativePortContractError4, PC8C051790, PR8C05399E.
- The version5 provider transcript records
  `standard-gameplay-cadence-contract`, not a missing compiled callback.
- Read-only inspection of the still-existing failed process and its exact
  link-map / compiler record layout establishes: standard60 inactive,
  configuring false, original release2/delta1, frame_open true, applied rate50
  owned by `postpal-checkpoint` at frame0. This inspection did not start a game,
  replay input, mutate process memory, or touch saves.

The cadence configurator rejected an open image before checking whether the
original pair was eligible for conversion. Original2/1 scripts and1/1 menus
were supposed to return unchanged, but could abort first if a turnover
callback had opened the next image.

The ownership/no-op check now precedes that boundary check. Actual promotion
or restoration is deferred while an image is open, retaining the guest's
current state until a closed boundary. This applies to the whole scheduling
family, without a Super Sonic address/level exception. Existing validation of
mode-constructor results, setter results and an active1/1 contract remains.

Failures now include a `sonic-gameplay-cadence-v1` transcript with the specific
failed phase, main/scene/request, current and original release/delta, video flag,
applied Hz and active/open-frame flags. Subsequent capsules need no live-process
inspection to recover these values. The second reported attempt (session
1789303740546-29356) records the same original failure.

Per the user's instruction, no additional game/test run was made. Verification
is source/capsule analysis, compilation and required link audits; an actual
post-fix Super Sonic story run is not claimed.
