# Execution-thread profiling and legacy joystick work

The Options package closure is committed as `f0d7f32`. The subsequent
performance investigation uses the actual execution-thread ID published by
the game, not a guessed main thread or process-wide CPU total.

## Private measurement tools

`sonic_execution_sampler` is an `EXCLUDE_FROM_ALL` executable. It is not linked
into the product and adds no normal-game instrumentation. The stage harness
starts it only for its own game process and the recorded execution thread,
after ten gameplay seconds. Sampling is bounded to at most 30 seconds.

The sampler validates process/thread ownership, uses the Windows SDK's aligned
x64 `CONTEXT`, and resumes after each observation. It preserves any pre-existing
suspension count. It performs no input injection, device change or target-code
execution. Module names and code addresses are recorded; stack memory and saves
are not exported. Optional stack traces use Windows `StackWalk64`, at most 32
traces of 16 frames outside the game module. Symbol setup is outside suspension;
the handler uses local image/unwind data and exports with symbol-server search
and prompts disabled. DbgHelp calls stay on one sampler thread.

The native self-test checks a real worker resumes, mismatched process/thread
ownership is rejected, external suspension counts survive, and stack unwinding
returns more than the initial instruction. It caught and corrected the first
unwind iteration returning the initial frame before any game stack capture.
The map resolver's fixture covers aliases, exact boundaries, later static
symbol sections, and exclusion of data symbols. It streams the large LLD map
and checks executable SHA, PE/map timestamp, image size and execution-thread
identity. Folded symbol aliases remain explicit. DLL names are nearest exports;
they must not be mistaken for exact private-function names.

Primary API contracts:
[GetThreadContext](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext),
[SuspendThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-suspendthread),
[StackWalk64](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-stackwalk64),
[SymSetOptions](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symsetoptions).

## Observed execution costs

Both traces used copied saves, hidden/muted D3D11, Emerald Coast, 1280x720,
100 percent rendering, 144 output, existing forward diagnostic input profile 3.
Both completed at their planned 60-second gameplay deadline without a runtime
failure or forced termination. Executable SHA:
`57afb7a7cbece36c7b5e08ba1029282b49a4942c22c2e2bb75bce67761c44380`.

| Run | Samples | Sampling window | Total suspension | Longest suspension |
| --- | ---: | ---: | ---: | ---: |
| execution-profile-ec-01 | 1,296 | 20,000.8 ms | 57.8988 ms | 0.1589 ms |
| execution-io-ec-01, 21 stack traces | 965 | 15,000.9 ms | 67.1495 ms | 1.5753 ms |

The first trace attributed 1,024 IP samples to game.exe and 243 to ntdll.dll.
Game samples include FPU helpers, AOT bodies, instruction scopes, immutable
write guards and dispatch/coverage work. The execution thread used about
0.978 CPU-core equivalents. These are statistical observations, not exclusive
CPU percentages, invocation counts or a throughput comparison. Both traces
perturb timing; their FPS values must not be used to claim an optimization.

The flat ntdll file/registry samples alone did not prove input ancestry. The
second trace did: its first real unwind reaches `Impl::poll_gamepads` at RVA
`0x3b20d`, followed by the platform wrapper at `0x3a970`. Disassembly verified
`callq *%rsi` at `0x3b20b`, returning at the sampled RVA. This is the first,
capabilities query in the authored WinMM loop, before the second position
query. Other captured Windows stacks share the device/registry ancestry;
some were depth-truncated. Separate samples reach audio queue waits and frame
payload copies. Those are not classified as input costs.

Important scope: hidden tests deliberately disable the real Sony SDL backend.
They therefore exercise WinMM/XInput fallback. Normal successful SDL startup
already sets the WinMM device count to zero. The expensive fallback observed
here is not evidence of the same input cost in normal SDL operation. Historical
hidden-stage FPS measurements retain this backend limitation.

## Bounded experiment — not promoted

The experimental WinMM loop queries current position/attachment first and asks for
capabilities only after that succeeds. Both queries must still succeed before
the connected ID or candidate is published. No metadata, state, device identity
or negative result is cached. The identity worker, disconnect/reconnect policy,
slot retention, SDL, XInput, analog conversion and rumble paths are unchanged.
The private environment option `SARECOMP_WINMM_POSITION_FIRST=1` enables the
candidate. The original capabilities-first order remains the normal default.
The first paired build used the inverse diagnostic switch; both runs below
explicitly selected their order in that same executable.

Microsoft documents that
[joyGetNumDevs](https://learn.microsoft.com/en-us/windows/win32/api/joystickapi/nf-joystickapi-joygetnumdevs)
counts supported driver slots and recommends a position query to establish
physical attachment. [joyGetPosEx](https://learn.microsoft.com/en-us/windows/win32/api/joystickapi/nf-joystickapi-joygetposex)
reports unplugged/unavailable devices, whereas
[joyGetDevCapsW](https://learn.microsoft.com/en-us/windows/win32/api/joystickapi/nf-joystickapi-joygetdevcapsw)
queries capabilities. Stable-state admission remains the conjunction of both
successful queries. A physical hotplug between them can still shift observation
by one poll, as with the prior order; no new throttling delay is introduced.

The source recipe matches the entire original query block and the pinned SDK
source hash. Only its generated port-local copy changes. The shared query
policy test covers success/failure combinations in both orders and proves an
unplugged position result skips the capabilities call. The actual platform
test passed in `.local/enhancement-tests/input-position-first-01`, including
its existing host-poll isolation, recording count and owner-scope checks.
The incremental game/native closure audit passed with retained AOT; r354 and
the old Katana repository were not changed.

## Direct comparison

The profiler was disabled for both clean paired runs. Each completed its
60-second gameplay window; first ten seconds were excluded. Same executable:
`ed482f64cce97ab5ea57d32ee884f03e188f6a8bbc4d9572b2534070225445c0`.

| Run | New draws/s | Output/s | Execution CPU ms/title boundary | Raw cycles/title boundary |
| --- | ---: | ---: | ---: | ---: |
| winmm-capabilities-first-ec-01 | 13.304 | 143.862 | 73.698 | 324,474,044 |
| winmm-position-first-ec-01 | 13.347 | 143.913 | 73.317 | 323,388,535 |

Both report active video 50 Hz, release 2, logical delta 2, no runtime error,
no forced termination and the expected planned stop. The approximately 0.5%
thread-CPU and 0.3% cycle differences are not a defensible performance win
from this moving fixture on an actively used host. Windows can do related
metadata work inside the position query too; the query-order change does not
eliminate that. No extra matrix or repeated pairs were run to chase the delta.
**Decision: preserve the original query order by default.** Keep only the
bounded diagnostic candidate for reproducibility; do not claim a delivered
FPS improvement or change normal hotplug behavior on this evidence.

The useful next measurement is a hardware-isolated execution profile with
the same existing synthetic movement. Current hidden tests force a fallback
that successful normal SDL initialization skips. Isolating input must also
preserve the same input transformation/deadzone; merely launching a neutral
replay would bypass `sonic::input::transform` and change the forward probe's
effective analog value. No such changed fixture or 60-SIM implementation is
claimed here. Render interpolation remains disabled and the game clock has
not been patched.

Restoration evidence: `.local/menu-preview/build-winmm-control-restore.log`
passes the native closure audit; the actual platform test passed again in
`.local/enhancement-tests/input-control-restored-01`. The normal output is
`out/experimental/game.exe`, 1,910,015,488 bytes, SHA-256
`c856fa9b84f6f4083ed250b24b85f97d9894a1131c2cb053dc5f75fd014cd27d`.
Prepared RAM reads remain OFF. All game, sampler, build and helper work from
this comparison has completed; nothing is polling or holding a device open.

## Hardware-isolated follow-up

`tools/benchmark-stage.py --hardware-input isolated` now excludes physical
controller discovery/polling only in the existing hidden forward fixture.
Four exact environment values must agree: the private isolated-input flag,
background-test flag and gameplay-probe flag are `1`; input profile is `3`.
The platform snapshots this policy per construction and refuses combination
with recording, replay or an initial-state trace. Normal runs are unchanged.
The synthetic forward input is still applied above the platform, followed by
the ordinary remapping/deadzone transformation. No replay flag is enabled.
Guest poll sequences/telemetry advance once per guest poll; host menu polls
remain separate. Results require an explicit runtime isolation witness.

The actual linked platform test passed in
`.local/enhancement-tests/input-isolated-02`. Its first attempt exposed a test
setup issue: Windows environment setters did not update the CRT environment
read by the policy. The corrected fixture uses `_putenv_s`, checks positive
admission, and covers visible-session rejection, wrong-profile rejection,
recording conflict, neutral snapshots, retained per-instance policy and host
poll accounting. Existing recording/replay checks also passed.

`runs/execution-isolated-ec-01` completed its planned hidden/muted 60-second
Emerald Coast window without runtime failures. Executable SHA-256:
`469a2c24555d9eceeb196a7e3720151a562d62e44ec9dab1da015b0a91d43268`.
Same 1280x720, D3D11, 100%, VSync off, 144 output target and forward profile 3.
The 20-second sample has 1,297 observations, zero errors, 80.789 ms aggregate
suspension and 1.790 ms maximum suspension. Results are instrumented:
17.199 new draws/s, 143.837 outputs/s, 56.969 execution-thread CPU ms per
title boundary and 249,608,948 raw cycles per boundary. All cadence witnesses
remain 50 Hz / release 2 / logical delta 2.

1,244 samples are in game.exe; 36 in ntdll, 12 in VCRUNTIME and 5 in UCRT.
No WinMM/DINPUT/registry sample remains. This confirms the old hidden fallback
was a substantial measurement contaminant; it does **not** mean normal SDL
gameplay received the same improvement. Native/guest execution remains the
principal cost. Top unambiguous object counts: FPU 117, native runtime 82,
selected 8C638FF0 AOT unit 54, generated dispatch 49, memory 45. Another 134
samples retain folded multi-object attribution. Off-module stacks now mostly
show audio-command acknowledgements and graphics payload copies.

The FPU forwarding overload accounts for 26 samples, 25 at its stack-copy
instruction. Actual disassembly confirms an extra non-tail call/frame and a
byte store followed by an overlapping qword load. A store-forwarding penalty
is a hypothesis, not a measured fact. The next bounded experiment removes
425 forwarding calls in the selected unit while retaining the exact
five-argument arithmetic implementation. The experiment is now complete:
the game comparison did not show a gain, so OFF is restored. See
`docs/fpu-call-experiment.md`.

## Restored control and clock provenance

The final retained-AOT build, `.local/menu-preview/build-clock-provenance.log`,
passes the native link audit at 1,910,017,024 bytes. SHA-256:
`e00c1e5c765aff5db26d8ef5212dc6b81016a82c7ffc9bff0ce68e307777088c`.
Both FPU-call and prepared-RAM experiments are OFF; default WinMM order is
unchanged. The actual platform test passed again in
`.local/enhancement-tests/input-isolated-03`, including recording/replay and
hardware-isolation guards. Its background setting now also uses the CRT
environment setter before constructing any platform.

`runs/clock-restored-ec-01` completed the same hidden, muted, hardware-isolated
60-second Emerald Coast fixture without a sampler or runtime failure. First
ten seconds excluded: 17.534 new draws/s, 143.570 outputs/s, 55.530 execution
CPU ms/title boundary, 245,074,991 raw cycles/boundary. This is restoration
evidence, not a paired optimization win. Host activity and differing distance
travelled prevent attributing small differences between these separate runs.

All gameplay samples retain 50 Hz / release 2 / logical delta 2. The new
read-only provenance witness is `postpal-checkpoint`, bound at frame 0;
TV-mode word remains 1. No later video constructor owns this sampled route.
The enum is not reinterpreted as a refresh rate. See the timing report for
the checkpoint contract and the remaining timing-faithful 60-Hz work.

## Audio snapshot reuse rejected

A follow-up read-only review checked the apparent duplicate snapshot in
`sonic_native_adxt_time`: `synchronize_native_adx_streams` reads the voice,
then the time getter requests it again. These are separate synchronous
audio commands. Between their acknowledgements the consumer may service an
independent WOM_DONE wake. The pinned SDK's `native_port_audio.cpp:396-410`
requests that service; `native_port_audio_execution_domain.cpp:1909-1936`
services an idle queue. Playback/decode counters and completion state can
therefore advance without another producer mutation.

Reusing the first result would change the observation point. No exact local
reuse was established and no cache was implemented. In addition, the eight
captured voice-snapshot stacks belong to pump/synchronization, not the time
getter, so they cannot establish a duplicate-getter hotspot. The helper
performed no game run or build and is finished. Preserve command ordering
and audio tails; do not repeat this suggestion without a new equivalence
contract and relevant measured cost.
