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
