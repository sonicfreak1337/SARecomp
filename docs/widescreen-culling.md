# Widescreen object visibility and Adventure Field HUD

This change is local to the experimental Sonic port. r354, the pinned SDK,
the frozen game archive and personal saves are not modified.

## Status

The user confirmed the Adventure Field ring HUD correction. The monitor
visibility correction is built, but a subsequent user report of a crash
when the station-hall monitor enters view remains unresolved. Do not promote
this build or describe the whole object-culling problem as visually verified.

A later Speed Highway 2 crash supplied a complete capsule and identified an
SDK constant-color read, separate from the new culling leaves. Its correction
and verification are documented in `speed-highway-color-crash.md`; the monitor
report remains unproven and must not be treated as the same root cause.

## Cause and implementation

The wider projection alone was too late: retail code rejected objects against
the 640-pixel viewport before submitting their meshes. The BasicAttach leaf
at 8C03718C now uses expanded horizontal comparison operands in widescreen
mode. The sphere query at 8C038D00 also gets wider X comparisons, restricted
to the source-reviewed display callers below. Shared guest viewport values,
vertical and depth comparisons are preserved. Original mode continues through
the original AOT functions.

Both leaves reuse FR8 in a later vertical comparison. The native replacements
restore that register immediately after each widened X comparison. They retain
the original register clobbers, FPU behavior and BasicAttach material writes.
Unsupported FPU modes continue through the original implementation.

The hint monitor has an earlier, independent sphere query at return PC
8C061400. Its display owner 8C0613BC draws the model cloned by 8C060E86
from object 8C1B9560 / BasicAttach 8C1B9538. Its separate proximity/state
routine 8C061300 remains unchanged. Omitting this display caller caused the
monitor to keep disappearing even after the lower model leaf was widened.

| Return PC | Display owner or scope |
| --- | --- |
| 8C0366B6 | 8C036690, render task |
| 8C048318 | 8C0482F2, drawing |
| 8C048436 | 8C048414, drawing |
| 8C0599E0 | 8C0599C0, drawing and presentation motion phase |
| 8C05A7BC | 8C05A7A0, lighting, matrix and model |
| 8C05AC82 | 8C05AC66, texture, matrix and model |
| 8C05AD5E | 8C05AD40, alpha and model |
| 8C05AE62 | 8C05AE40, existing state gate before drawing |
| 8C05D4E6 | 8C05D4AE, drawing and material selection |
| 8C05DBC6 | 8C05DBA0, model traversal and alpha callbacks |
| 8C061400 | 8C0613BC, hint monitor |
| 8C06399E | 8C063982, motion drawing |
| 8C064500 | 8C0644E0 and 8C064600, drawing and sprite phase |
| 8C0650E0 | 8C0650C0, material/model traversal |
| 8C0676EA | 8C0676B8, drawing after the existing proximity gate |
| 8C069B1E | 8C069B00, model/motion/sprite drawing |

The review followed custom helpers and consumers, not just task registration.
In particular, update roots republish shared scratch at 8C05D0D2,
8C05D95C, 8C064EA2 and 8C069862 before using it for gameplay. The cyclic
phases advanced by 8C0599C0 and 8C064600 feed presentation only.

Unknown callers retain retail behavior. 8C046854 updates velocity/position.
Character callers 8C0AE0E0, 8C0BA794 and 8C0C4620 are not admitted: their
visible branches also publish held-object attachment vectors or collision
centers. Those paths need a separate rendering/state split before extension;
this batch does not claim to solve every character popout.

The Adventure Field icon uses carrier 8C1BF4F4, texlist 8C1C3250,
frames 8C1BF4E0 and return PC 8C08A332. It now shares the left HUD anchor
with its digits, instead of staying centered in the former 4:3 viewport.

## Frozen build integration

The two new hooks bind these exact retail spans:

- 8C03718C, 0x108 bytes:
  `df39afabfbfdce25d7c3bd0cd59008959ec7e365320dd19a9603857401e67137`.
- 8C038D00, 0xA0 bytes:
  `1f573f535bbc2d5e67ba50eca018c42cab9736a60bc89ec7542df1a88e10d551`.

The port-local provider refresh admits only this pair of reviewed hooks.
It extends the dispatch and link audit and prevents the frozen direct-call
index from bypassing either hook. The original compiled leaves still serve
ContinueOriginal. Other structural changes remain rejected; provider and
artifact identities are regenerated normally. No AOT partition is rebuilt.

## Verification and limitations

`sonic_render_culling_tests` executed 4,536 cases against the retail SH-4
bytes, including original, 16:9 and 21:9 bounds, supported rounding/denormal
modes, vertical/depth rejection and material-state publication. Forty-eight
sideband cases were accepted at widened X bounds. The oracle compares CPU
registers, FPU status, GBR material scratch and shared viewport memory. Its
interpreter is linked only into this test executable, never the game.

Presentation checks passed for 16:9, 64:27, 43:18 and original mode. A hidden,
muted Emerald Coast capture in `runs/cull-family-ultrawide` reached gameplay
and its planned 15-second deadline with no reported runtime fault. It does
not exercise the station-hall monitor. The earlier `cull-field-ultrawide`
capture used a non-launchable selector-inventory ID and is not valid field
evidence. The capture helper now rejects those IDs before launching.

User run 2576, recorded at 21:08 on 2026-09-11, reported the station-hall
monitor crash. Its capsule contained only ARMED. The journal had 1,536 durable
records and 1,666 contiguous complete records. A recovered copy retained the
original file and reused a byte-identical copy of the user's VMU. It reached
the station hall, then hit input exhaustion. A second run with a documented
neutral tail reached its host deadline without reproducing the reported
crash. These are diagnostic probes, not proof that the crash is fixed.

The launcher now writes capsules for controlled runtime failures before task
cleanup, as well as for exceptions. It records normal/deadline stop reasons
in the session file. The culling hooks use a distinct failure code 53415743
and retain exception text; they no longer share the graphics-budget code.
This closes the missing-evidence path, not the still-unidentified crash.

Both incremental game builds retained all 1,157 AOT partitions; the object
family build took 67 seconds and the diagnostic update 70 seconds. The native
link audits passed. Baseline executable/metadata verification passed.
