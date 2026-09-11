# CPU performance investigation — 2026-09-11

## Outcome

No substantial CPU reduction was established. The final direct comparison
favored the accepted widescreen build, so **all game/adapter/runtime
experiments were reverted**. The original adapter and manifest match Git
commit `22c157c` byte for byte. No SDK archive, guest memory check, FPU behavior,
simulation cadence, baseline asset or user save was changed.

`out/experimental/game.exe` is the restored, tested reference executable:

```
c15acd3199022ac41cac3721e2cfb7175e0c8fa5415524c3a2de67cdb343c9f4
```

Its matching map was restored from `.local/performance-reference` as well.
This is a restoration of the accepted binary, not a new optimized executable.
The latest build log describes the rejected candidate; it must not be used
as that reference binary's provenance. Existing compiler objects/logs were
not rewritten or timestamp-adjusted. The next build recompiles the changed
adapter normally and refreshes its source-bound provider metadata.

## Measurements

Hidden, muted D3D11 runs at **3182 × 1332**, render scale 100%, presentation
target 144 Hz. Each probe copies saves into its own directory, enters a debug
scenario and applies forward input for 60 seconds of gameplay. Steady rates
exclude the first ten seconds. Input is frame-indexed: faster runs can travel
farther, so these are representative path comparisons, not identical trace
replays. Concurrent desktop work and host clock changes add uncertainty.

CPU time is the sum of Windows process kernel/user time across **all game
threads**, divided by completed simulation frames. It is not GPU time, CPU
temperature, a percentage reduction in power, or an exclusive main-thread
duration. Instrumentation is enabled equally for the comparisons.

The final consecutive Emerald Coast comparison was:

| Variant | Sim FPS | Output FPS | CPU ms / sim frame |
| --- | ---: | ---: | ---: |
| Adapter experiment, original SDK/AOT | 16.47 | 143.99 | 70.43 |
| Accepted reference, restored for delivery | **18.67** | **143.98** | **61.49** |

Both completed with the intended deadline, no forced termination and no
reported crash/dispatch/contract fault. The restored reference was about
13% faster and used about 13% less CPU time per frame than the experiment.
**This is a rejected regression, not an improvement over the previous build.**
Stable 30 simulation FPS remains unmet. Output cadence alone does not measure
game speed or input responsiveness.

Earlier observations, including unsuccessful experiments:

| Local run tag | Sim FPS | CPU ms / frame | Change |
| --- | ---: | ---: | --- |
| perf-before-ec | 17.74 | 66.76 | Accepted reference |
| perf-reference-ec-recheck | 18.24 | 64.77 | Accepted reference, repeat |
| perf-before-windy | 19.60 | 61.67 | Reference, Windy Valley |
| perf-after-ec | 16.27 | 71.14 | Adapter + FPU fast paths |
| perf-after-windy | 21.29 | 55.45 | Same experiment, Windy Valley |
| perf-second-ec | 17.69 | 68.60 | Reduced FPU experiment |
| perf-hot-ec | 19.41 | 59.33 | Two O3 game partitions + FPU experiment |
| perf-hot12-ec | 17.16 | 68.44 | Twelve O3 partitions |
| perf-lto-ec | 17.04 | 69.56 | Twelve partitions + FPU ThinLTO |
| perf-pinned-ec | 18.23 | 63.67 | Two O3 partitions, original FPU |
| perf-inline-ec | 16.46 | 69.88 | Increased inlining threshold |
| perf-sdk-final-ec | 16.60 | 73.40 | Three exact SDK sources as ThinLTO |

All these runs maintained approximately 144 output FPS. The isolated favorable
O3/Windy samples did not establish a consistent whole-game gain and were not
selected for delivery. Full logs and JSON results remain locally under
`runs/<tag>/`; no personal saves are committed.

## What the investigation established

The CPU profile is dominated by translated execution and its SDK calls, not
GPU submission or resource-fence waits. In the initial Emerald Coast run,
render submission totaled about 2.24 seconds and resource-fence waits about
0.017 seconds across the run. The broad AOT interval was about 64.94 seconds;
provider/render timing intervals can overlap and must not be summed as
exclusive costs. An independent instruction-pointer sample in Windy Valley
also found substantial time in generated code and memory/FPU/runtime helpers.

The pinned FPU already has gated SIMD implementations. Native replacements
already cover important transform/draw paths; their existence cannot be
inferred from generated source alone. Small repeated memory/guard calls and
collision/object traversal remain useful areas for future CPU work, but no
checks were removed to make a benchmark faster.

The byte-identical SDK ThinLTO experiment successfully replaced its three
intended archive members (622 map entries, all from LTO, none from the original
COFF members), passed the native link audit, and still lost the comparison.
It and every AOT/FPU override were removed from CMake.

## Retained development improvements and verification

- `tools/benchmark-stage.py`: bounded hidden/muted probes, isolated saves,
  reference-EXE selection, simulation/output/CPU metrics, strict deadline and
  crash-frontier checks, cleanup of its own process.
- Persistent linker ThinLTO cache; linker limited to three worker threads.
  This is a development-build change, not a runtime speed claim.
- Build logs count actually recompiled AOT objects. The last candidate build
  took 79.5 seconds with **zero** AOT recompilations and passed the original
  native link audit. The SDK experiment took 122.7 seconds, also with zero AOT
  recompilations. Cache/backend rebuilds still vary with link inputs.
- r354 executable and AOT metadata hashes reverified; accepted reference EXE
  hash verified before and after restoration; Python benchmark syntax checked.
- No full level matrix, story replay, visible window or audible test. The
  delivered executable is byte-identical to the accepted widescreen version,
  so this batch introduces no new graphics implementation to qualify.

## Next scope

Optional Vulkan alongside D3D11, with Linux support as the direction. Keep
backend selection separate from game timing, content and saves. Vulkan is
**not implemented in this batch**. The measured CPU execution bottleneck
must not be assumed to disappear with a renderer change.
