# CPU work aligned to title progress

The private gameplay probe now samples the calling execution thread and the
whole process at its existing one-second observation boundary. Production
frames and guest instructions do not perform these clock queries. Source:
src/sonic_execution_clock.hpp and emit_sonic_native_gameplay_probe_sample.

The sampler uses the actual current thread, not an inferred busiest thread.
GetThreadTimes/GetProcessTimes provide cumulative kernel plus user CPU time
in 100-ns units. QueryThreadCycleTime is recorded independently as raw cycles;
it is never converted into elapsed time or simulation FPS. See Microsoft's
[GetThreadTimes contract](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadtimes)
and [cycle-count limitations](https://learn.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-querythreadcycletime).
Failed queries have explicit validity flags, not a fabricated zero-cost result.
The caller's LastError value is restored. No other thread is suspended.

Benchmark schema v3 requires a continuous, identical thread identity before
deriving thread CPU metrics. The existing external process sampler remains
available for older binaries. New in-process totals align with the exact
title sample endpoints instead of the external observer's half-second poll.
The execution-thread number includes native providers and submission work on
that thread; it is not an exclusive AOT function profile. Title-boundary and
new-draw counters still do not establish additional gameplay updates.

## First observation

runs/execution-clock-ec-01/result.json passes the existing 60-second hidden,
muted Emerald Coast probe, forward input profile 3, 1280x720 D3D11, VSync Off,
144 output target and copied saves. The first ten gameplay seconds are excluded.
Executable SHA256:
f61fba5ade73c0b544733fef430bd8b712c84adce6b6ff5da4be36506b0d9ba8.

| Metric | Observation |
| --- | ---: |
| New draws / second | 13.655 |
| Presentations / second | 143.475 |
| Execution-thread CPU ms / title boundary | 71.972 |
| Execution-thread CPU core equivalents | 0.983 |
| Aligned whole-process CPU ms / title boundary | 83.057 |
| Aligned whole-process CPU core equivalents | 1.134 |

This supports prioritizing work on the execution thread: it is almost fully
occupied, while additional worker CPU accounts for roughly 0.15 core in this
sample. It does not identify the responsible functions by itself, establish
a speedup, or prove low-end hardware behavior. The original clock witnesses
remain video 50 Hz, release 2, logical delta 2. Neither 30 nor 60 actual
simulation updates per second is claimed.

Build .local/menu-preview/build-execution-clock-01.log passes the native link
audit at 1,910,005,760 bytes, with no frozen AOT translation-unit rebuild.
The Python benchmark syntax check passes; the live probe verifies all clock
fields and continuous thread identity. Planned stop reason is 2, no recorded
contract or crash frontier. No extra level matrix or physical test was run.
