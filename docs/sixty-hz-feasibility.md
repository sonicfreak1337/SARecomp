# Timing-faithful 60 Hz feasibility

Read-only audit for Eggman, 2026-09-12. Repository: Sonic Adventure Recompiled,
branch `enhancements/ingame-settings`; inspected HEAD
`d1e1a2246bd2258a09fb642667870af8ca767122` with concurrent Options/Audio/Input
changes. Only this report was authored. No game, build, interpreter, capture,
full analyzer, AOT regeneration or physical display/device change was run.
Interpolation remains withdrawn.

Integration follow-up: `runs/options-integration-after/result.json` now records
a fresh hidden Emerald Coast run with 50 Hz active video, release 2 and logical
delta 2 throughout its sampled gameplay. New-draw throughput was 13.336/s with
143.995 presentations/s. This closes the missing current-run witness for that
one path, not for every scene, save or boot sequence. See the
[paired measurement and limits](options-input-audio-completion.md).

## Decision

There is a real original **slot-count plus logical-delta contract**, including
one-unit and two-unit consumers. This merits a bounded feasibility experiment.
It does **not** yet prove an unused whole-game 60-update/s mode, nor justify a
global constant patch. First resolve the effective PAL/NTSC clock independently
of increasing the number of simulation updates.

The current bootstrap explicitly adopts a **50-Hz** active video clock.
Historical Emerald/Windy witnesses show two slots per gameplay frame, giving a
nominal **25 updates/s** when work fits the budget. The host configuration's
`simulation_rate_hz=30` is not evidence of actual title cadence. The newest
Options runs do not log the effective title rate, so they cannot establish that
50 Hz persists through every later initialization or that it changed to 60 Hz.

## Authority and limits

Installed `.local/baseline/r354/native-content/boot.bin` is 6,735,296 bytes,
loaded at 0x8C010000, SHA256
`b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af`.
Installed `postpal-main-ram-native-ready.bin` SHA256:
`b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c`.

Original instruction semantics below were checked against retained generated
AOT and original literal words read from that boot image. The local FunctionMap
is useful for locating candidates but also covers data in the 16-MiB RAM image;
its mere entry presence is not executable proof. The r354 evidence manifest
reports 249 preserved modules. This audit did not prove timing closure across
all those modules and did not compare a separately installed NTSC executable.
Addresses below identify private PAL witnesses, not public SDK rules.

Paths/line anchors refer to this inspection; concurrent edits can shift lines.
AOT paths below are relative to
`.local/baseline/r354/product/generated/code/`. Baseline files were read only.

## Bootstrap, effective clock, and video selection

- `src/native_title_adapter.cpp:28480-28491` checks the checkpoint's applied
  horizontal/vertical/border tuple (008D034B/0270035F/002C026C), then calls
  `bind_sonic_native_video_refresh(50)`.
  The nearby comment at 28364 claiming native time starts at 60 Hz is stale.
- `bind_sonic_native_video_refresh` at 2160 owns active Hz and resets the
  rational deadline phase. It is not a display-FPS preference.
- The applied-mode constructor at 24993-25008 can subsequently bind 60 or 50,
  based on the actual constructor tuple and only after the mode-active gate.
  Unknown tuples bind zero and leave the title-cadence path unavailable.
- Despite its name, `sonic_native_video_mode_60hz` at 25022-25060 preserves a
  PAL625 owner and binds **50**, not 60. Renaming a field or trusting this
  symbol is insufficient evidence.
- Development restore at 28044 restores a saved active Hz; the serialized
  value is checked against 0/50/60 at 26612-26614. A restored state is a
  separate clock authority from a fresh bootstrap.
- `src/sonic_legacy_video.cpp:5-25` intercepts the shared ADVERTISE Apply,
  Test and restore owner before the TV-mode RAM write and SDK reinitialization.
  It performs no cadence change. `docs/legacy-video-options.md` explains
  the existing crash-prevention contract. Therefore this menu cannot itself
  correct a currently active 50-Hz clock. This does not prove that no other
  constructor ran before or after the menu.
- The checkpoint words read in this audit were release=1, ready=1, delta=1,
  TV-mode word at 0x8C754B44=1. Do not reinterpret that persisted enum as the
  current rate; the applied constructor owns the rate.

Latest local evidence was checked explicitly:

| Evidence | What it actually proves |
| --- | --- |
| `runs/options-final-ui-01/stdout.log:1` (latest directory, 22:06) | 1,922 source frames and 5,700 presentations; configured 30/144. No effective-Hz witness. |
| `runs/options-native-02/stdout.log:1`, `options-transition-sound-01/stdout.log:1` | Configured 30/144 only; neither proves the rate after Options or VMU initialization. |
| `runs/legacy-frequency-continued/stderr.log:402,404` | Test mode=2 at frame 1905 and restore mode=1 at 2214 were intercepted with disabled=1. No effective-Hz sample around them. |
| `runs/motion-owner-emerald-01/stdout.log:1` | 398 source frames, 2,570 presentations, configured 30/144; not a 60-Hz simulation proof. This historical run predates withdrawal of interpolation. |
| `runs/perf-before-ec/stderr.log:894-896,1291-1293` | Gameplay main=4, release=2, active_hz=50; AccountWithoutWait consumes a slot before the final WaitForFrame. |
| `runs/perf-before-windy/stderr.log:1399-1402` | Same 50-Hz/two-slot gameplay contract, with threshold-2 and threshold-1 services before the final wait. |

A search including ignored local log/JSON/JSONL files found cadence witnesses
in the historical performance runs, not a newer effective-rate witness in the
listed Options/legacy runs. No Station Square or credits-specific live clock
proof was identified. Missing instrumentation is not evidence of 50 or 60.

## Original frame and task contract

The exact initializer 0x8C051760 independently publishes:

| Original operation | Destination | Meaning established here |
| --- | --- | --- |
| 0x8C05176E, value R4 | 0x8C754DFC | Producer release / slots per cycle |
| 0x8C051770, value R4 | 0x8C754E00 | Current ready count |
| 0x8C051774, value R5 | 0x8C754E04 | Logical delta consumed by title code |
| 0x8C051776, zero | 0x8C754E08 | Initial auxiliary state |

Original literals at 0x8C051880/884/888/88C bind these destinations.
See `unit-v8C050BE4-8C051E00-44b823a416a623f6.cpp:41617` onward,
especially the guest-PC comments for the four stores. The owner also registers
the completion callback through 0x8C605174; it is not merely a scalar setter.

A targeted scan of all retained `unit-v*.cpp` found seven resolved direct
call sites for this initializer (not a completeness claim for indirect or
overlay calls):

| Callsite | R4 slots / R5 delta |
| --- | --- |
| 0x8C0492AA | 2 / 1 |
| 0x8C049570 | 2 / 2 |
| 0x8C04E668 | 2 / 2 |
| 0x8C054738 | 2 / 2 |
| 0x8C0829F0 | 2 / 2 |
| 0x8C08F18A | 1 / 1 |
| 0x8C09A124 | 2 / 2 |

The two independent arguments and the 2/1 case rule out equating release with
a universal timestep. The 1/1 call proves an original configuration exists;
it does not prove normal Sonic physics is correct when every scene uses it.

`service_frame_producer_until` at adapter 19275-19418 advances one rational
1/active-Hz deadline per completion. Earlier AccountWithoutWait calls and the
final frame wait share the same phase. It rebases after missed slots and does
not invent task updates to catch up. `service_frame_producer_completion` at
19167 decrements ready and invokes the completion matrix only on zero.
The matrix contract in `src/sonic_frame_completion_contract.hpp:9-35` covers
44 x 8 entries; these callbacks are not the gameplay task scheduler.

`sonic_native_frame_begin` at 39729-39886 copies release to ready, completes
the current scene, increments the title frame counter once, and updates input.
`complete_native_frame` at 5525-5560 calls the title-cadence presentation API
when that clock already owned the wait. The manifest's 30/144/144 defaults
(`src/native_port_manifest.cpp:2841`) and synthetic pacing test
(`tools/test_pacing.cpp:21-52`) therefore cannot substitute for live title data.

The retained task scheduler invokes update from task+16 at 0x8C0986F6 and uses
distinct display/child-display consumers. See
`unit-v8C0979FC-8C099124-d2e05c26a52f659a.cpp:25290-25445` and historical
ownership proof in `docs/render-interpolation.md`.
Repeated presentation does not execute those consumers.
Moving display traversal between updates also needs proof: a display callback
can have ordinary-RAM effects, so it cannot simply be assumed pure.

## Delta, animation, physics, events and audio

A complete aligned-word scan of the 6.7-MiB Boot found two literal carriers of
release (0x8C051880, 0x8C109D38) and four of delta (0x8C04EADC, 0x8C051888,
0x8C08A5EC, 0x8C0BDB90). This inventory excludes synthesized addresses,
base-plus-offset access, copied pointers and loaded modules.

Concrete consumers show useful existing support and important limits:

- 0x8C08A4FE-0x8C08A50A reads delta and adds it to a title counter.
  AOT `unit-v8C08A3FA-8C08B1C4-e710e24b11dd7416.cpp:3444` onward.
  Two delta=1 calls can match one delta=2 counter increment, provided surrounding
  effects and branches are also equivalent.
- 0x8C0BDAB2-0x8C0BDACA tests delta==2 and selects 0x1000 versus 0x0800.
  AOT `unit-v8C0BD740-8C0BE498-9784a409eddec077.cpp:11432` onward.
  This is a real half-step branch, not proof of a global physics multiplier.
- 0x8C04EA84 compares a loop index with delta; 0x8C04EB2C reads delta again
  for remaining work. AOT `unit-v8C04DC58-8C04ECFA-c5709913296e7386.cpp:
  55732-55863,56910` onward. Existing discrete substeps must be preserved;
  replacing every numeric 2 would change control flow.
- Animation diagnostics already expose motion-state+60 frame bits and +64/+68
  motion pointers at adapter 34546-34588. They observe a display path, not the
  writer of animation progress. No complete animation-writer, player-physics,
  collision or event callback closure was proven here.
- Event/text diagnostics already observe delta and text timer
  (adapter 16145-16200). Frame counters also exist independently of delta.
  Halving one central delta therefore does not prove text, fades, credits or
  cutscene timing equivalent.
- Periodic title callbacks use monotonic elapsed time: adapter 16379-16427 and
  19470-19516, one-second interval, bounded backlog retained. The TMU0-facing
  unit remains 781,250 ticks/s at 16086-16101.
- Audio pumping is at most once per source frame at adapter 3140-3170;
  sample playback remains owned by the audio engine/endpoint. Audio-service
  epochs are separately published at 22436-22464. More frequent source frames
  change service opportunities, not the authorized pitch or sample rate.

The user's report of credits progressing too slowly relative to a song is
consistent with either a 50-versus-60 logical-clock mismatch or missed
frame-driven updates while sample playback advances. It is not yet attributable
to one cause: credits' actual delta/scroll writer and a matched live audio
position/clock sample are missing. Do not time-stretch audio to conceal it.

## Concrete next probe and gates

**Priority 1: settle the existing clock, before a 60-update enhancement.**
Estimated 0.5-1 day for a narrow diagnostic and source-bound review; low risk
when observation-only. At the existing cadence witness, record active Hz,
release, ready, delta, main/minor state, mode-constructor identity and actual
slot/update counts. Add one sample after VMU load, after each legacy
Apply/Test/restore, on gameplay entry, and at credits start. Reuse existing
timing/audio-position logs; sample per boundary, not per draw. One bounded
hidden/muted trace with copied saves and no monitor change can establish
whether selected 60 intent leaves active Hz=50. This run has not been performed.

Compare elapsed monotonic time with logical delta totals, periodic callback
totals, animation frame progress and audio playback samples. The nominal
relations are 50/2=25, 60/2=30, 60/1=60 updates/s, only if the observed scene
actually consumes that many slots. Switching 50 to 60 alone makes fixed
per-update content up to 20% faster; call it a regional-clock correction only
after the intended original mode and PAL compensation are proved.

**Priority 2: test the existing delta contract with retained AOT.**
Estimated 1-3 days for a bounded component proof, not a full game conversion.
Use isolated copied RAM and the existing compiled initializer/consumer entries,
with controlled host time and recorded provider effects. First reproduce all
seven original call tuples without mutation. Then compare one delta=2 title
step with two delta=1 steps over the same intended logical duration. Cover the
counter, half-step branch, substep loop, animation advance, player movement/
collision, event text/fade, camera and audio command order in one selected
gameplay family. This needs explicit component-test implementation later;
no AOT rebuild or use of the withdrawn interpolation code is required.

Require equal state at shared logical endpoints, identical ordered visible
writes/events/audio commands and exception outcomes. Preserve input transitions
by logical time, not by duplicating frame-indexed replay samples. Require
bounded intermediate motion/collision and no duplicated trigger events.
Do not assume calling A,A,B,B equals A,B,A,B across interacting task callbacks.
Keep periodic/sample clocks fixed. A mismatch blocks global promotion and
identifies the smallest additional owner contract.

A 30-to-60 reference uses two original logical units per 30-Hz step versus one
per 60-Hz step. Preserving current PAL50 walltime while producing 60 distinct
states is a different problem: integer delta 1/2 alone does not prove that
conversion. The intended timing reference must be explicit in the experiment.

**Priority 3: establish CPU capacity on the unchanged clock.**
Existing measurements in `docs/vulkan-performance-2026-09-12.md` report
16.049/15.909 simulation FPS for Emerald and 18.223/17.603 for Windy,
despite approximately 144 output FPS. These are whole-process CPU/path probes,
not exclusive simulation-thread timings or matched instruction traces.
They do not support a 16.67-ms update budget. Use matched path/clock samples
after Priority 1; target single-writer update p95 <=16.67 ms with no logical
time loss. The existing data cannot predict speedup from half-sized steps.

Full 60-Hz gameplay is therefore **NO-GO for immediate enablement, GO for the
bounded clock/delta proof**. A universal unused 60-Hz mode was not established.
Nor was the need for an indiscriminate whole-game frame-constant rewrite.
If the delta probe fails, owner-specific timestep/event contracts must be
completed before claiming timing-faithful 60 Hz. Such a conversion across
physics, animation and overlays is high risk and cannot be sized responsibly
from the current partial coverage.
