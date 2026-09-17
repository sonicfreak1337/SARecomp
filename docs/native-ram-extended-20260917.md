# Extended native RAM/ALU prefixes

This is an internal continuation after the September 17 CPU patch. The delivered
patch, installed user product and r354 are unchanged. The build switch
`SARECOMP_LINUX_RAM_REGIONS_EXTENDED` defaults OFF. The experiment reuses the
same 41 selected source units, rather than expanding blindly to all game code.

The current Linux execution profile attributes 56.25% of samples to generated
function bodies; it does not attribute all that time to removable administration.
An authenticated source inventory identifies recurring gaps in existing regions:
1,737 ordinary word loads with postincrement, 1,581 word stores with predecrement,
1,560 register tests and 1,423 constant left shifts. These are static instruction
shapes, not invocation counts or percentages of frame time.

The extension executes these operations inside the same callback-free native
prefix as surrounding RAM/integer work. PR stack reads/writes, the T condition
flag, fixed shifts and simple non-faulting instruction envelopes are also
recognized. Pointer updates occur only after the successful access. A failed
access publishes the exact completed prefix and resumes that original
instruction, including its original scheduler preflight. Live PR/T state is
published with the GPR/FR state before fallback; stores are never replayed.

Memory permissions, mapping generations, code-write invalidation, observer
ownership and the original exception/FPU contracts remain. Original interior
resume labels bypass the new prefix. Existing whole-operation animation and
render kernels still take precedence over their retained bodies.

In the same 41 units, the preparer finds 1,413 regions / 22,030 instructions,
versus 967 / 15,103 before. The 45.87% larger static instruction coverage is
**not a frame-rate gain**. All 41 outputs with the extension disabled are
byte-identical to the delivered preparation, including their original fallback.
Each prepared input/output remains authenticated by its retained manifest.

Both Linux and Windows runtimes pass 3,004 component comparisons. Four additional
retained game witnesses cover 40 instructions: stack publication plus a condition
test, shifts between dependent reads, PR restoration followed by FP loads, and
interleaved stores/loads followed by register postincrement. Their generated
prefixes actually complete natively, and all four exercise partial completion.
Tests cover P1/P2 and backing aliases, unaligned/end-of-RAM access, immutable
ranges, stale read guards, tracing, watchpoints, foreign observers, scheduler
mutation, counter wrap, privileged/MMU/FPU modes and interior resumes. CPU,
fault provenance, memory counters, full RAM and callback logs match the original.

Evidence: `runs/ram-extended-component-{linux,windows}-20260917.log`,
`runs/native-ram-shapes-20260917.json`,
`runs/native-ram-extended-coverage-20260917.json`, and the generated fixture
identities. Do not present the coverage increase as performance.

## Linux gameplay comparison

The delivered September 17 executable `7dbdf58e16dc910d4f1b39998e7557851106b5c8d18da8bbfb80453f9a137869`
is compared with the extended candidate
`548310b6c2f243d0a22e8fd80003932d836f2371458ed5bb40d2e8c2cb6ab4f7`
(1,744,168,016 bytes). All four runs use the same four-vCPU TCG VM,
`LP_NUM_THREADS=2`, Original timing, 800x500 / 50% software Vulkan rendering,
isolated input, diagnostics/provider telemetry OFF, installed RAM/transfer
defaults and image boundaries 5..45. There is no simultaneous compiler or
profiler. Order: Gamma control/candidate, then Chaos 4 candidate/control.

| Scenario | Variant | Execution CPU ms/update | Process CPU ms/update | New images/s |
|---|---|---:|---:|---:|
| Gamma Emerald Coast gameplay | Control | 234.405 | 1135.549 | 0.834829 |
| Gamma Emerald Coast gameplay | Extended | 222.594 | 1112.879 | 0.860974 |
| Chaos 4 stage entry | Control | 234.182 | 1090.862 | 0.885152 |
| Chaos 4 stage entry | Extended | 228.500 | 1105.699 | 0.871916 |

Each run completes 40 new images and 136 actual game updates, then stops at the
expected deadline without a forced termination or fault. Both paired endpoints
match game ticks, player XYZ bits and HUD timer. PAL 50 Hz, release 2 and logical
delta 2 remain unchanged. Gamma saves 5.04% execution CPU with 3.13% more new
images/s. Chaos 4 saves 2.43% execution CPU, but produces 1.50% fewer images/s and
costs 1.36% more process CPU/update. These short VM comparisons do not qualify a
global frame-rate gain or a Deck claim; the extension remains OFF by default.

Evidence: `runs/ram-extended-{gamma,chaos4}-{control,candidate}-20260917.json`
and `runs/ram-extended-linux-comparison-20260917.json`.

## Windows qualification

`SARECOMP_WINDOWS_RAM_REGIONS=OFF|BASE|EXTENDED` supplies a separate Windows
comparison using the same source transformation and 41 manifest-bound units.
The default is OFF. The existing inverse arithmetic owner is excluded, and
all three whole-operation animation bridges consume the prepared bodies.
The explicit link-map audit requires each selected public entry to originate
from `sonic_ram_regions` and rejects any linked original selected member.
When the Windows build mode is BASE or EXTENDED, ordinary startup enables the
compiled RAM path before any cached environment read. Explicit internal
`SARECOMP_RAM_REGIONS=0` still selects the retained implementation. No benchmark
environment variable is needed to activate the separately built candidate.

The Windows component executable passes the same 3,004 CPU/RAM/fault comparisons,
including identical complete/partial-prefix witnesses. All 894 selected public
entries pass the new map ownership audit, with zero linked original selected
members; the existing closure and FPU audits pass as well.

The initial Windows candidate is `05edaf0127ac8fa1e87f3ccce190e782852f78c70a68d726711d4996640eb76d`.
Its control is the unchanged delivered Windows executable
`0334413ab28a538591b766be2c9bf7582c582e125ae0d15f6b5353e188b0a206`.
Hidden/muted Vulkan offscreen runs use 800x500 / 50%, isolated input,
Original timing, and boundaries 5..305, without another compiler/profiler.
These compare the whole 41-unit RAM path against the retained Windows product;
unlike Linux, the control did not already contain the base RAM prefixes.

| Scenario | Variant | Execution CPU ms/image | Execution cycles/image | New images/s |
|---|---|---:|---:|---:|
| Gamma Emerald Coast gameplay | Control | 25.104167 | 111,593,925 | 25.002323 |
| Gamma Emerald Coast gameplay | Extended | 22.864583 | 102,232,618 | 25.000027 |
| Chaos 4 stage entry | Control | 23.385417 | 104,036,149 | 25.001824 |
| Chaos 4 stage entry | Extended | 20.937500 | 93,593,042 | 25.000868 |

Gamma saves 8.92% execution CPU / 8.39% cycles, with the same 720 game updates,
ticks 136..856 and matching XYZ/HUD/cadence endpoints. Chaos 4 saves 10.47%
CPU / 10.04% cycles. Its endpoints and native-call counters also match; its
game clock resets during the scene, so the net game-tick difference is not
reported as its update count. Both pairs produce 300 new images and retain
PAL video 50 Hz, release 2 and logical delta 2. All stop at the expected probe
deadline. Their image rate is already capped by the authored Original cadence.

After adding ordinary-start activation, only the launcher is rebuilt. The
final isolated Windows executable is
`7ea0dd58b2897d7c1cfa7ba571ca4003165f5e4de4b7ed43dc4dee9e50171a46`
(1,913,464,320 bytes), at `out/ram-extended-windows-20260917/game.exe`.
Both final Gamma probes use `--ram-regions installed` and confirm startup
`ram_regions=1`. Original completes at 25.001 new images/s / 23.28125 execution
CPU ms/image; Recompiled completes at 59.996 new images/s / 12.8125 CPU ms/image.
These short desktop checks are not a complete playthrough or Deck timing proof.

Evidence: `runs/ram-extended-win-{gamma,chaos4}-{control,candidate}-20260917/result.json`,
`runs/ram-extended-win-installed-{original,recompiled}-20260917/result.json`,
and `build-performance/generated/region-extended-writes/link-audit.json`.
Both development caches retain their explicit extended mode; the new source
options still default OFF, and the shipped Windows/Linux files and September 17
patch are unchanged. No new installer or patch was produced. The native CPU
goal and the Deck 20–25 ms target remain open.
