# Shared native CPU paths, 2026-09-17

The same source implementations now activate on normal Windows and Linux
startup, independently of Original/Recompiled game timing. The immutable r354
baseline is unchanged. These are whole-operation native implementations with
the original functional admission and fallback, not removal of simulation work.

Enabled paths:

- Animation hierarchy and pose blending: complete native matrix/SRT owners.
- Closed model memory: one admitted writable snapshot for complete operations.
- Render context: complete capture/commit operations instead of SH4 dispatch.
- Palette lighting: bounded AVX2/FMA arithmetic and complete color publication.
- Audio status: coherent worker-published status instead of synchronous polling.
- Collision owners: valid complete owners also operate outside gameplay scenes.

`sonic_native_cpu_policy.hpp` owns the common default policy. Each previous
feature environment variable accepts `0` to retain its old implementation.
`SARECOMP_NATIVE_CPU_PATHS=0` disables this entire new group for internal
diagnosis. Neither changes authored timing or previously shipped math paths.
The product observer binding uses the same policy as its owners, so a missing
environment variable cannot silently deactivate direct RAM publication. The
Windows build capability now defaults ON. Linux also defaults its compiled and
normal-start RAM/transfer paths ON, retaining those delivered on September 16
without depending on a developer's previous CMake cache. An internal experiment
that needs either old path must disable its build switch and performance defaults
explicitly; incompatible experimental combinations still fail configuration.

Model packets, sound metadata caching and deferred MIDI commands remain OFF:
their measured results did not justify enabling them. Additional invariant
audits remain controlled by the existing internal diagnostics policy; memory
bounds, executable identity, module lifetime and callback behavior remain active.

## Matched Windows group comparison

Executable `986583e713324466da1a13a5d72aedb1ebb771fb84d6a0c8c8bdb4ed4e6d3bff`,
Gamma Emerald Coast, Original timing, frames 5..305, 800x500 / 50% render scale,
hidden Vulkan offscreen. The entire new group ON versus OFF:

| Metric | OFF | ON |
|---|---:|---:|
| Execution CPU ms/new image | 27.447917 | 25.625000 |
| Execution cycles/new image | 120,623,857 | 111,494,636 |
| Game updates | 720 | 720 |
| New images/s | 25.002 | 25.002 |

This is 6.64% less execution CPU and 7.57% fewer cycles for exactly matched
work. Both start at tick 136 and end at 856; HUD 10,783 -> 10,063 and starting /
final XYZ bits match exactly. PAL 50 Hz, release 2 and logical delta 2 remain
unchanged. Original's target is already attained on this desktop, so no extra
images are produced. Evidence: `runs/native-cpu-all-win-{on,off}-original/result.json`.

These figures describe the qualified group, not a guaranteed Deck speedup.
The requested 20–25 ms target remains open until measured on the target hardware.
Linux normal-start, actual patch installation and final binary identities are
recorded with the delivery once those checks complete.
