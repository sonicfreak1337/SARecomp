# Net progress since 15 September, 22:00 Europe/Berlin

No reproducible positive total performance gain has been established against
the executable available before that time. The earlier +3.4% Gamma figure was
a dispatcher-toggle comparison inside one newer executable, with provider
telemetry enabled. It is not the net improvement since 22:00.

The fresh direct comparison used the pre-22:00 D2 executable (commit d338acb,
SHA-256 d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b)
and the prepared-transfer candidate (commit 84d6a97, SHA-256
6344e07c5c9090c15f163f9a4ba6b303d91499d4baedc627460936d524bcd8bb).
Both ran hidden in the same Linux QEMU/TCG VM with Vulkan/llvmpipe, 800x500,
50% rendering scale, Original PAL timing, native gameplay math, isolated
saves, and diagnostics/provider telemetry disabled. The frame window was
5..25, containing 20 new images and 68 game updates. No compilation or other
game probe ran during the measured windows.

| Stage | Execution CPU ms/update, old -> new | New images/s, old -> new | Qualification |
| --- | ---: | ---: | --- |
| Gamma Emerald Coast | 246.848 -> 258.041 (+4.53% cost) | 0.56555 -> 0.40410 (-28.55%) | Both completed; boundary state and native-call counters match |
| Sonic Windy Valley | 287.803 -> 280.011 (-2.71% cost) | 0.45479 -> 0.46489 (+2.22%) | Excluded: one game-tick offset and old control aborted during shutdown |

The Windy control completed the requested gameplay window but then reported
`malloc_consolidate(): unaligned fastbin chunk detected` and SIGABRT. Its
measurement is retained as evidence, not a successful performance result.
The abort occurred in the unchanged old control, before the scalar-store
experiment was built or installed. Its cause remains unassigned.

These single VM pairs do not establish a stable regression percentage on
Steam Deck either. Software rendering and TCG dominate much of the total
frame cost. They do establish that the earlier small component result is
insufficient for a positive overall progress claim or a release patch.

Prepared transfers remain an optional experiment; no installer or user build
has been replaced. The 20-25 ms target remains open. The following scalar
write experiment is a separate candidate and is not included in this table.

Evidence: `runs/net-since2200-comparison-20260916.json` and its four referenced
full summaries. All boundary differences and binary identities are recorded.

## Later mixed RAM execution candidate

The subsequent candidate C772 combines prepared transfers with 967 mixed
RAM/ALU/FP prefixes in 41 common units. Fresh D2 controls and candidate runs
all completed; this does not resolve or erase the older shutdown abort above.

| Stage | Execution CPU ms/update, D2 -> C772 | CPU cost reduction | New images/s, D2 -> C772 |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | 271.871 -> 256.172 | 5.77% | 0.39841 -> 0.41968 |
| Sonic Windy Valley | 303.250 -> 275.487 | 9.16% | 0.45021 -> 0.45811 |

Gamma differs by one update and by native-call/position boundaries, so its raw
5.34% image-rate increase is not a qualified net gain. Windy retains 68 updates,
matching coordinates and HUD timer; its absolute game tick is offset by one and
the candidate makes two extra collision-length calls. Its image throughput
improves only 1.75%. These are single VM pairs, not a reproducible global or
Deck FPS improvement. Do not present the CPU percentages as net FPS gained.

See `linux-ram-regions-experiment-20260916.md` and
`runs/ram-vector-comparison-20260916.json`. No release patch or installer exists
for this candidate; the large-gain requirement and 20–25 ms goal remain open.

## Page-proof follow-up, B04B

The subsequent closed-region page-proof helper passes 1,440 component cases and
both hidden gameplay probes. On the same scope it saves only 1.42% execution CPU
versus C772 in Windy and costs 0.61% more in Gamma. Neither is a qualified global
increment; Gamma also performs 65 rather than 67 updates in its image window.

Against the fresh pre-22:00 D2 controls, the latest measured CPU/update costs are
257.737 ms in Gamma (-5.20%) and 271.563 ms in Windy (-10.45%). Raw image rates are
0.42418 and 0.46235 respectively. Gamma's workload differs; Windy retains the
update count, coordinates and HUD timer but has 20 fewer collision-length calls
and a one-tick absolute offset. Its +2.70% image rate is one VM pair, not a
repeatable global/Deck gain. No honest single positive net FPS percentage has
yet been established for the requested period.

SHA-256: `b04b0d28594452918151e2780e25eee04328e2fc006e0edb81ab1cddde359d75`.
Evidence: `runs/ram-page-comparison-20260916.json`. The experiment remains
default OFF. No new patch or installer; the 20–25 ms target remains open.
