# Native model projection layout follow-up

The September 17 Deck update is unchanged. This is a follow-up to the
default-OFF whole-model projection experiment, not another replacement of
game timing. The user's latest report confirms improvement on Deck from the
delivered native CPU approach, without supplying new numeric measurements.

The shared Windows/Linux projection kernel now transposes four complete
transformed vertices in SIMD registers. It eliminates the per-quad scalar
scatter/gather through temporary XYZ arrays and writes four-word private
records directly. Tail handling explicitly separates actual vertices, the
authored even-padding vertex and the read-ahead vertex. Projection RAM still
receives exactly three words per record; its fourth word remains untouched.
Clip counts include the even-padding point, whereas the title clip array
does not. Final FR/FPSCR/T/R13 publication retains the original contract.

When the native batch is enabled and the existing reader admits an unobserved
RAM span, output padding is captured by one complete buffer copy. The retained
loop still overwrites every XYZ word if batch arithmetic declines. Observed
reads and unsupported address ranges retain the per-word reader. The ordinary
guest write transaction and its observer/code checks are unchanged.

All matrix transforms still precede hardware projection, preserving the
previously qualified FPU operation order in TCG as well as on hardware.
There is no per-stage threshold, VM detection, skipped geometry or game update.

## Verification

Both platforms pass 4,096 differential cases / 109,936 vertices and 14
mutation-free rejections against the retained scalar owner. The size matrix
now includes six- and eight-point models and both tail shapes. Checks cover
exact registers, output bytes, preserved padding, clip state and host FPU
restoration. Windows Gamma Original additionally verifies 35,829 models /
1,233,143 vertices with zero differences, with two ordinary fallbacks.
Linux Chaos-4 entry verifies 2,847 models / 78,228 vertices with one fallback.
Windows Recompiled Gamma verifies 35,670 models / 1,228,665 vertices and
retains 60-Hz/release1/delta1 timing: 60 images advance ticks 126..186. This
last run is a functional check, not a performance comparison.

Both game builds are incremental. Windows retains all 894 selected entries
from the same 41 RAM-region units, with zero selected original members linked.
No pinned SDK, original AOT pack, baseline or personal save was modified.

## Windows performance

Same executable, Original timing, hidden/muted Vulkan offscreen, isolated
input and copied saves, 800x500 / 50%, diagnostics/provider timers OFF.
Windows order: Gamma OFF, Chaos 4 ON, Chaos 4 OFF, Gamma ON. Each window is
300 new images (5..305); Gamma explicitly waits for gameplay. Both pairs have
exact matching boundary game ticks, player XYZ bits, HUD and native-owner
counts. PAL50/release2/delta2 is unchanged; output remains capped at 25 images/s.

| Scene | OFF CPU ms/image | ON CPU ms/image | CPU change | Cycle change |
|---|---:|---:|---:|---:|
| Gamma Emerald Coast | 21.927083 | 21.822917 | -0.48% | -1.01% |
| Chaos-4 entry | 20.468750 | 20.312500 | -0.76% | -0.75% |

These are small single-pair observations, not a demonstrated large gain.
The comparison is the whole current projection path ON versus retained OFF;
it does not isolate the new layout from the earlier batch implementation.
Earlier percentages from different executable controls must not be added.

## Linux performance

Four-vCPU QEMU/TCG VM, two llvmpipe raster workers, the same hidden/muted
800x500 / 50% settings, Original cadence and internal diagnostic timers OFF.
Order: Gamma OFF, Chaos 4 ON, Chaos 4 OFF, Gamma ON. Each window contains
40 new images (5..45) and exactly 136 updates. Both pairs have identical
boundary XYZ bits, HUD, game ticks, cadence and sampled native-owner counts.
All four probes complete through the expected diagnostic stop, not a crash.

| Scene | OFF CPU ms/update | ON CPU ms/update | CPU change | New images/s OFF / ON |
|---|---:|---:|---:|---:|
| Gamma Emerald Coast | 229.254300 | 223.305917 | -2.59% | 0.854628 / 0.861735 |
| Chaos-4 entry | 224.099240 | 218.515832 | -2.49% | 0.882807 / 0.880069 |

Image throughput changes +0.83% and -0.31%. This is not a large global gain
or a Deck frame-time result. The earlier batch's 18.84% Gamma observation
used another executable and is not reproduced by this comparison; it must
not be presented as current performance. Neither experiment was promoted.
This pair measures the whole current batch against scalar projection, not
the layout change against the old batch within one executable. There is no
measured claim that the new layout itself is faster than that old layout.

The source switch remains OFF, the delivered patch remains byte-for-byte
unchanged, and the 20--25 ms Deck target remains open. Further work should
target repeated native RAM-capability setup/CPU state publication across
the common generated paths. A retained capability must not survive callbacks,
mapping/observer/code-generation changes without exact revalidation; the
previous rejected observer-permission cache is not new work.

## Artifacts

Windows: `out/projection-layout-windows-20260917/game.exe`, SHA-256
`c261427db04ff4b0845dc3e36d460607cb44df9de03c7f43fa0b2a22b82e0168`.
Linux: `build-linux/game`, SHA-256
`e8bd1157b5016f003e625ce81366f3b21ec0b4a2e000491aa17365b125814429`.
Both development caches retain the separate EXTENDED RAM/FPU candidate;
they are not release builds. The projection switch remains internally OFF.

Evidence: `runs/projection-layout-component-{win,linux}-20260917.log`,
`runs/projection-layout-win-verify-20260917/`,
`runs/projection-layout-windows-comparison-20260917.json`, and the four
`runs/projection-layout-win-{gamma,chaos4}-{off,on}-20260917/result.json` files.
Linux summaries/logs are `runs/projection-layout-linux-{verify,gamma-off,
gamma-on,chaos4-off,chaos4-on}-20260917.{json,log}`; the paired report is
`runs/projection-layout-linux-comparison-20260917.json`. Recompiled evidence
is `runs/projection-layout-win-recompiled-20260917/result.json`.

The preliminary read-only register/code-shape audit confirmed `-O2` follows
`-O3` in the Linux guest compile command; this is an optimized Release build.
The sampled `8C0912C0` body still contains many translated memory/call
boundaries, but it has only one FPU binary helper occurrence. Its size is not
evidence that fusing more FPU instructions is the main opportunity there.
No new register ABI, FPU-envelope transformation or ALL-source expansion was
made from that observation.

The current Linux ELF contains 1,404,903,939 bytes of executable `.text`,
versus 44,589,688 bytes of exception tables and 88,584,400 bytes of unwind
frames. Ordinary debug sections therefore do not explain its overall size.
Removing unwind data would break the retained exception contract and is not
an optimization made here. Section size alone is not a CPU-cost profile.
