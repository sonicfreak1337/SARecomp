# Original camera operation and the combined native CPU groups

The common camera update now has an internal native implementation under
`SARECOMP_NATIVE_CAMERA_OPERATION=1`. It remains OFF by default. This applies
to the original game's camera computation in both timing modes; it does not
change the optional Recompiled camera's controls, ownership or return policy.

## Connected scope

The original update owner `019F4A` connects the live camera parameter table,
history, update callback, three pose/angle adjustment modes and transition
callback. The group also includes the camera handlers `020D60` and `02535E`
observed in the profile and their position, direction, constraint and matrix helpers.
The inventory adds 33 authenticated owners, 3,286 reachable instruction PCs,
140 call sites and 68 original lexical FPU scopes. Together with the existing
movement/contact owners this is 147 owners sharing one admitted operation.

Original dynamic callback pointers remain live. Scene-specific handlers and
the two unconverted helpers `023960` and `01B5E0` still execute through real
original calls. The former has a computed local dispatch; the latter contains
a sparse continuation beyond its annotated entry body. Neither is flattened,
stubbed or skipped to make the group appear closed. Foreign callbacks revoke
the borrowed memory and source proofs before the native parent continues.

The existing `sonic_recompiled_camera_original_step` entry hook still runs
before this update. Camera publication at `01A100` remains outside the new
group, including the Recompiled camera's scripted-event ownership checks.
The original adjustment, collision queries and event logic are retained.

The new root uses the same original-fault continuation bridge as the contact
operation. Selection remains distinct from membership during an original
resume. No persistent camera-data cache or cross-callback permission cache is
introduced. All new owners are bound to their retained source hashes and
original code/literal bytes; source/data alias fences stay active.

## Verification

Windows and Linux each pass 50 cases: 38 original-instruction comparisons,
eight actual retained-AOT executions and four actual unaligned-access fault
continuations. These cover absent callbacks, the three adjustment modes,
history restoration, special/event mode 5, transitions, live foreign writes
and observer/FP-mode changes across callbacks. Nearest rounding and the
alternate FP bank with round-to-zero are included. Every CPU register and
all 16 MiB of RAM are compared, including foreign-call boundaries. This is
a targeted root/transition suite, not an exhaustive playthrough of every
scene-specific camera mode. An initially missing camera-area table pointer
was corrected in the fixture; both final suites pass.

The hidden, muted Windows Gamma Emerald Coast pair enables all five groups
together: movement/contact, model submission, land display, object collision
and the common camera operation. All sixteen selected endpoint fields match.
Frame 270 is byte-identical on both sides:
`f351cb258cecce1622bc275cc6a06b47b9aeeef6eac57f85f2df24616c846498`.
The interval executes 48 native camera updates and 101,463 internal contact/
camera calls, with zero contact-family resumes. Capture/readback makes this
a correctness check, not performance evidence.

Both game targets build incrementally. The executable binding is
`runs/camera-operation-executables-20260918.json`; all 27 allocated ELF
sections match between the build and its stripped Linux copy:

- Windows: `out/model-submission-windows-20260918/game.exe`, SHA-256
  `bb62433b3cccb0579212fb7a15c4047f77d33b27c6bef7667691b3cbe0b8a1df`.
- Linux build: `build-linux/game`, SHA-256
  `3d18f7283ebbeead9ad7060dc9cb607c87b22b453eaf2f6d40b5543f217fb138`.
- Linux staged and VM: `out/camera-operation-linux-20260918/game`, SHA-256
  `9ff104baff27941026fa52ff8197d8ff845920e4fd692764e538d2a1f3822866`.

The Windows private candidate filename is reused; this manifest supersedes
its previous binding. The delivered September 18 patch and installers,
baseline and personal saves remain unchanged.

## Bounded Linux comparison

Both pairs use the same binary, Original timing, Deck aspect, two software-
Vulkan workers, diagnostics/telemetry OFF and frames 5 through 25. The five
groups are disabled together in the reference and enabled together in the
candidate. Every run completes without forced termination and performs 68
game updates in the measurement window.

Gamma Emerald Coast matches all sixteen selected endpoint fields and all
non-feature settings. Execution CPU per update falls from 197.593 to 162.837
ms in this TCG VM: **17.59% less execution CPU**, **5.01% less process CPU**
and **4.21% more new game images per second**. There are 68 native camera
updates and 134,183 internal contact/camera calls. Contact/hierarchy resumes
and model-proof revocations are all zero.

Knuckles Sky Deck also completes, with 68 native camera updates, 158,585
internal contact/camera calls and zero resumes/revocations. Its reference
starts one game tick later, however, and its palette work differs. Position,
HUD timer and timing policy match, but this is not an identical workload.
The recorded raw percentages are **excluded from performance conclusions**.
Do not rerun the unchanged pair simply to obtain a favorable measurement.

These are combined five-group results, not an isolated camera percentage,
not percentages to add to earlier comparisons and not measured Deck FPS.
Keep the CPU improvement and the groups internally selectable. The camera
path remains private-OFF; this bounded comparison does not promote defaults
or authorize replacing the already-delivered patch/installers.

Evidence:

- `runs/camera-operation-tests-{windows,linux}-20260918.log`
- `runs/camera-operation-gameplay-comparison-windows-20260918.json`
- `runs/camera-operation-{gamma,sky-deck}-comparison-linux-20260918.json`
- Per-run summaries, raw logs and display configuration in the corresponding
  `runs/camera-operation-*-linux-*-20260918` folders.

The remaining older profile includes the actor family `0FDC20` and its
display children `0FC0A0` / `0FC240`. Its computed local dispatch, like the
main Gamma actor's, is not supported by the current group author. This is a
candidate for coherent follow-up investigation, not evidence of a new CPU
share after these five groups. The old profile must not be treated as an
updated measurement, nor its inclusive task costs as removable overhead.
