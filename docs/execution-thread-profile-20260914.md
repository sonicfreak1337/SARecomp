# Execution profile after the authored source-plan cache

Current profile: `runs/mesh-plan-live-profile-gamma-d3d11-02/execution-ip-resolved.json`.
Gamma Emerald Coast, D3D11, 3440x1440, isolated hardware input, Recompiled timing,
native matrix/collision/motion paths and indexed corners. The hidden/muted run
completes and the sampler reports 1,940 samples over 30.004 seconds, with 90.275 ms
total suspension. This is diagnostic data, not a throughput benchmark.

The executable is SHA-256
`c48e046d7f7b50087161f8d5fc896ef16643eaf4bb510ba087e003f573fb5858`, source commit
`b3d3468`. Resolution used the matching executable and map before the next build.
The earlier `-01` run collected only 387 samples over six seconds; the longer
run supersedes it for selecting work. Neither should be resolved against a later
game executable or map.

## Remaining sampled work

| Object attribution | Samples |
| --- | ---: |
| native_title_adapter | 181 |
| folded multi-object aliases | 145 |
| sonic_fpu_runtime | 142 |
| native_port_runtime | 94 |
| generated native dispatch | 87 |
| memory | 69 |
| AOT unit 036BC0..037C3C | 53 |
| native_bringup_coverage | 37 |
| AOT unit 051E56..053338 | 33 |
| AOT unit 056ED4..0585E0 | 31 |
| AOT unit 0400A0..04124E | 31 |
| block_table | 29 |

Notable primary-function attributions include four-argument `fpu_binary` (65),
immutable guard `range_kind_mask` (45), model corner construction (31),
`SonicGuestReader::u32` (28), model owner (26), direct u32 stores (23), native
dispatch (20), direct memory guard creation (19), and FPU epoch construction (17).
The four-argument FPU body includes inlined arithmetic: its samples do not prove
that the forwarding-call wrapper itself costs that amount.

The sampled module totals are game.exe 1,660, ntdll.dll 196, VCRUNTIME140.dll 75,
and nine samples in other modules. These module names alone do not distinguish
copying, allocation, waiting or exception handling. Nearest-symbol attributions
and folded aliases are not exclusive CPU percentages or call counts.

## Rejected premise: reuse the same FPU epoch again

Read-only review of the actual generated FPU runtime confirms that
`ScopedHostRounding` already skips host setup inside an active epoch with matching
guest rounding. The common nontrapping `fpu_binary` path performs integer
product/sum/quotient arithmetic, without host FP or MXCSR setup. Its integer
helpers already handle normal/zero operands; merely adding zero identities or
another same-mode setup gate repeats existing work.

`HostFpuExecutionEpoch` preserves the complete incoming host control/status word,
starts its own defined environment, then restores the original word. A nested
same-mode epoch must still clear its own incoming status and restore it on exit.
The existing runtime test explicitly preseeds inexact before an inner epoch and
observes both its cleared interior and restored exterior. Same CpuState/FPSCR
mode is insufficient to skip this observable boundary. Whole-block/frame reuse
would also cross memory, service and callback boundaries. Per-instruction guest
Causes, sticky flags, exceptions and live FPSCR writes must remain architectural.

Conditional MXCSR restoration and DIRECTFPU were previously measured and rejected;
they are not new candidates. The source-plan admission requiring masked ambient
host exceptions is conservative: the retained UV epoch itself masks host
exceptions. It was not established that an unmasked ambient host trap could be
lost by the UV cache.

The subsequent experiment prepared full indexed-emission order within the
admitted, unclipped source-plan path, reusing the existing live corner builder.
It was verified and then retired for lack of repeatable performance benefit;
see `mesh-emission-plan-experiment.md`. These sampling observations are separate
from its actual throughput measurements.
