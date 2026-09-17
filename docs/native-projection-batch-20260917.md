# Whole-model native projection batch

This is a new, internal, default-OFF experiment. The delivered September 17
patch is unchanged. The user reports that its Deck performance is already
better and requests further native replacements; no new Deck frame-time
measurement was supplied with that confirmation.

`SARECOMP_NATIVE_PROJECTION_BATCH=1` replaces the point loop inside the
existing authenticated PAL model-transform owner. It captures the matrix
once, transforms the model as one host operation, projects four vertices
together and publishes final CPU state once. The retained loop used repeated
SH4 arithmetic helpers and per-word RAM reads for every point. This differs
from the rejected September 15 experiment, which retained that scalar helper
sequence and only specialized each operation.

Guest projection RAM is still committed by the existing transaction, including
the original fourth word of every output record. The original even padding
point, final read-ahead point, clip mask/count, FR registers, FPSCR, T and
integer epilogue remain observable and exact. No geometry, simulation update,
collision, callback or title buffer is skipped. The ordinary scalar path
handles unsupported input without a partially mutated guest state.

## Admission and arithmetic

The caller supplies a synchronous, unobserved direct RAM span and its live
FPU epoch. CPUID/OSXSAVE checks gate the separate AVX2/FMA object. Admission
requires scalar DN mode, supported rounding, no traps/enabled FP exceptions,
and an already-sticky Inexact flag. Matrix and point inputs must be finite.
Projected-coordinate inputs and scales are zero or have magnitudes from
2^-20 through 2^20; depth must be nonzero in that interval. Other inputs use
the retained path. Under these bounds projection arithmetic can only set the
already-sticky Inexact flag. The final FADD uses the retained helper so its
Cause bits remain exact, rather than being approximated from a batch status.

Transformation finishes for the entire model before hardware projection. In
the admitted domain, retained projection arithmetic uses integer helpers that
leave host FP flags unchanged between FTRV operations. A first interleaved
prototype passed Windows but exposed the previously documented TCG FMA/status
dependence in Linux case 32. Preserving this transform order passes the same
unmodified oracle on both systems. No case was skipped, tolerance introduced,
VM detection added, or runtime SIMD disabled. Incoming host state is restored
on both success and rejection.

## Correctness evidence

- Windows and Linux each pass 4,096 differential cases / 133,356 vertices,
  plus 14 mutation-free rejections. The independent reference retains the
  original scalar helper order. It compares GPR/FR/XF banks, PC/PR/GBR,
  FPSCR/SR/T/FPUL, exception generation, trap/sleep flags, instruction/cycle
  counts, output/clip bytes, padding and restored host state.
- Windows Gamma Original 5..65: 35,829 complete model comparisons and
  1,233,143 vertices match; two models use the retained fallback.
- Linux Chaos-4 entry Original 5..15: 2,847 model comparisons and 78,228
  vertices match; one model uses fallback. The probe stops normally.
- Both real-game checks retain PAL50, release 2, logical delta 2.

`SARECOMP_NATIVE_PROJECTION_BATCH_VERIFY=1` computes both forms within the
same game call and aborts on any mismatch before the guest RAM transaction.
It is a private correctness mode, not a performance configuration.

## Windows comparison

Same executable, hidden/muted Vulkan offscreen, Original timing, 800x500 at
50%, diagnostics/provider telemetry off, isolated input and copied saves.
Each window contains 300 new images (5..305). Gamma ran OFF then ON; Chaos 4
ran ON then OFF. Boundary XYZ/HUD/clock/native-owner counts match exactly.
Gamma advances 720 game ticks. Chaos-4 entry resets its clock during the
window, so its endpoint difference must not be called a logic-update count.

| Scene | OFF CPU ms/image | ON CPU ms/image | OFF cycles/image | ON cycles/image |
|---|---:|---:|---:|---:|
| Gamma Emerald Coast | 22.864583 | 22.291667 | 99,616,612 | 97,104,996 |
| Chaos-4 entry | 20.625000 | 20.885417 | 89,965,229 | 90,905,445 |

Gamma saves 2.51% CPU and 2.52% cycles. Chaos 4 costs 1.26% more CPU and
1.05% more cycles. Both remain at the authored 25 new images/s. This mixed
result alone is not a global performance qualification or a Deck guarantee.

## Linux comparison

Same executable and settings as above, four-vCPU TCG VM, llvmpipe with two
raster workers, Original timing, windows 5..45. Gamma ran OFF then ON; Chaos 4
ran ON then OFF. Each completed 40 new images and 136 game ticks. Boundary
XYZ bits, HUD/clock, PAL cadence and sampled native-owner counts match in
both pairs. Both stop normally. Provider timers and verification are OFF.

| Scene | OFF execution CPU ms/update | ON CPU ms/update | OFF images/s | ON images/s |
|---|---:|---:|---:|---:|
| Gamma Emerald Coast | 214.633099 | 174.187420 | 0.916905 | 1.237432 |
| Chaos-4 entry | 223.563110 | 226.787680 | 0.898029 | 0.889719 |

Gamma saves 18.84% execution CPU/update and produces 34.96% more new images/s.
Chaos 4 costs 1.44% more execution CPU and produces 0.93% fewer images/s.
The VM's translation and software rasterizer make these relative measurements
useful for its workload only; they are not Deck frame-time or FPS predictions.
These are one matched pair per scene, not confidence intervals. The mixed
Windows and Linux results leave the new path internally OFF pending broader
native CPU work and a representative hardware measurement. No delivered
installer, patch, saves, or original cadence was changed.

## Reproduction and artifacts

The source is in `src/sonic_projection_batch*`, integrated through the
identity-bound title adapter on Windows and Linux. `sonic-projection-batch-tests`
is excluded from the normal game build. The normal-start switch remains OFF.
Both benchmark scripts accept `--native-projection-batch off|on|verify`.

Windows game SHA-256:
`6ba9a62f4a08a65a2975c1dd1589d1ed501a40d412fc5955297acd83caeeecf4`.
Linux game SHA-256:
`137d36ad0fcbfd76e86a68c7297fb11bfec17e98d50b848c804f8210d8ce49d4`.
These development builds retain the separately documented EXTENDED RAM
experiment in their CMake caches. They are not the delivered patch binaries.

Evidence: `runs/projection-batch-component-{win,linux}-20260917.log`,
`runs/projection-batch-win-verify-20260917/`,
`runs/projection-batch-linux-verify-20260917.log`,
`runs/projection-batch-{windows,linux}-comparison-20260917.json`, the four
Windows `runs/projection-batch-win-{gamma,chaos4}-{off,on}-20260917/result.json`
files and Linux `runs/projection-batch-linux-{gamma,chaos4}-{off,on}-20260917.json`.
