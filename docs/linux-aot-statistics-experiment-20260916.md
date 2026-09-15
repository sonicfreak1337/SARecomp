# Optional statistics inside hot AOT code

This is an isolated performance experiment, not a new patch or installer.
`SARECOMP_LINUX_AOT_STATISTICS` defaults OFF. The accepted Windows executable,
installed Linux product, saves and r354 snapshot are untouched.

## Evidence and scope

The fresh Windows Original/Gamma profile is
`runs/native-original-gamma-profile-20260916/execution-ip-resolved.json`.
Executable SHA-256:
`5743f31f874ac01139a9d9f6bbbee7adfca4281cd970614b08218363d7681f62`.
It collected 1,935 instruction-pointer samples over 30,013.4 ms, including
1,348 in the game module. Suspension totaled 118.116 ms. This diagnostic run
finished normally, with isolated input, copied saves and hidden/muted Vulkan
offscreen rendering. It is not a performance A/B comparison or Deck result.

Costs remain distributed across generated code, FP helpers, memory and
dispatch. Nearest-symbol attributions do not justify eliminating those
functional helpers. The new experiment instead makes two kinds of purely
observational totals optional: attempted/retired instruction counts and
successful direct-read access counts. They still exist in the native game
after the earlier runtime diagnostics switch.

Forty-one profile-selected common AOT units contain 53,769 instruction-attempt
sites and 1,383 scalar direct-read helper references (204 byte, 349 word, 830
long). The latter include reusable per-function helpers; they are not dynamic
access counts. Stage modules and the prior inverse specialization are excluded.

The generated-artifact manifest authenticates every source. Preparation
replaces only qualified helper names; reversing those substitutions recovers
the complete original unit exactly. Outputs replace members at their original
archive positions. The original sources and original compiled members remain.

## Policy and invariants

`SARECOMP_LINUX_AOT_STATISTICS_MODE=RUNTIME` uses the existing internal policy
at process startup. With diagnostics ON, both counters and the original access
accounting remain. With diagnostics OFF, selected helpers omit statistics only.
The second mode, `OMIT`, removes those counters at compile time, without a
per-instruction policy branch. In that binary an ON marker cannot restore
the omitted counters: a full-counter binary would have to be restored as well.
Neither experiment changes the already delivered internal ON/OFF patches.
Consequently,
raw instruction/access totals in this experiment are **partial and unsuitable
for measurement when OFF**. The experiment has not disabled bookkeeping in
the whole game, nor made a claim of comprehensive coverage.

A separate source inventory also finds 16,939 inline attempt/retire pairs
accounting for 25,946 non-faulting instructions in these same 41 units. These
pairs do not use the replaced class and remain unchanged in both experiments.
Thus this is partial even inside the selected units, not a complete removal
of their statistics. `runs/linux-aot-statistics/inline-counter-inventory.json`
records this gap; those static counts do not establish the dynamic cost.

Active instruction PCs, physical fault origins, pending guest cycles,
exception-generation checks, memory mapping/lifetime guards, callback order,
write observers, executable invalidation and MMIO are retained. No task update,
audio work or geometry is skipped. FPS and gameplay-progress measurements use
actual image counts and the original game-update counter, not these statistics.

The attempt helper is copied from authenticated runtime.hpp, SHA-256
`74661d49e5556055b6fd7d05e42d89c7fc4f8aed9167b21012f204de752c52a3`.
Only its two observational totals are optional. Fault PC/cycle handling is
the original body. A distinct class avoids changing the definition of the
SDK's existing inline class in only some translation units.

## Checks

The real Linux runtime component passes 280 ON/OFF cases: normal instructions,
changed exception generations, FPU-disabled exceptions, memory faults and host
unwind; source/relocated/outside-block PCs; byte/word/long RAM reads, mirrors,
alignment and bounds, stale guards after tracing/watchpoint/lookup/sink changes.
ON compares complete CPU/counter state. OFF compares complete state except the
explicitly disabled statistics. Read values, admission and callback logs agree.

```text
SONIC_AOT_STATISTICS_OK cases=280 on=exact off=statistics-only cycles=exact faults=exact guards=exact
SONIC_AOT_STATISTICS_OK cases=280 mode=omit counters=disabled cycles=exact faults=exact guards=exact
```

All 41 prepared source/output hashes and exact reverse transformations were
verified. A changed runtime header is rejected. The separate grouped-read API
is unchanged. Logs and preparation evidence live under `runs/` and
`build-linux/generated/aot-statistics/`, outside distribution.

## Gameplay comparison

The existing Ubuntu/QEMU TCG VM runs native Linux Vulkan/llvmpipe at 800x500,
50% render scale, 16:10 culling. Each run uses isolated input and save roots,
dummy audio, Original PAL timing (50 Hz, release 2, logical delta 2), enabled
native gameplay math, diagnostics OFF, and exactly new-image boundaries 5..25.
There is no sampler and no concurrent build during the timed windows. All four
initial runs finish at their intended deadline without a fault or forced stop.
VM throughput is not a Steam Deck frame-rate estimate.

Both controls are fresh D2 runs from this session, SHA-256
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.
Runtime candidate SHA-256:
`c9844d381c761a2219810f82b4fe954c9ec3613c5d099daddc8c2c338c09bb1a`.
Compile-time OMIT candidate SHA-256:
`c75a6421ecdac79d33c9ad3423bea3aa9187f9bf85b6171720339e5c4456a626`
(1,677,574,144 bytes).

| Stage | Variant | New images | Game updates | Execution CPU ms/update | Process CPU ms/update |
| --- | --- | ---: | ---: | ---: | ---: |
| Gamma Emerald Coast | Control | 20 | 68 | 234.445 | 967.444 |
| Gamma Emerald Coast | Runtime OFF | 20 | 68 | 283.661 | 1055.500 |
| Gamma Emerald Coast | Compile-time OMIT | 20 | 68 | 226.296 | 1011.892 |
| Sonic Windy Valley | Runtime OFF | 20 | 67 | 253.375 | 977.940 |
| Sonic Windy Valley | Control | 20 | 66 | 255.836 | 993.693 |
| Sonic Windy Valley | Compile-time OMIT | 20 | 68 | 242.181 | 981.788 |

The runtime switch costs about 21.0% more execution CPU per Gamma update;
Windy's 0.96% reduction is too small to establish a useful gain. The 41 selected
object files grew from 93,639,424 to 95,502,384 bytes (+1.99%). The branch/code
growth is a reason to test compile-time removal, not a demonstrated causal
explanation for every timing difference. **RUNTIME is rejected for delivery.**
Do not compare with yesterday's Gamma control (246.642 ms/update) to inflate a
gain: this session's control is already substantially faster.

OMIT reduces selected object bytes to 92,264,192 (-1.47% against control).
Its Gamma execution CPU/update is 3.48% lower, but total process CPU/update
is 4.59% higher and new-image throughput falls from 0.574 to 0.545/s. This
single VM comparison does not establish an overall performance improvement.

Windy's OMIT execution CPU/update is 5.34% lower, while new-image throughput
falls from 0.594 to 0.585/s (-1.63%). Its window contains 68 game updates,
versus 66 in control: normalization exposes the work difference but does not
make these short moving windows identical. Neither stage demonstrates the
large end-to-end improvement required for a new performance patch.

**Keep both experiments disabled.** All six stage runs completed their exact
image windows without a crash or forced stop; all samples retained PAL 50/2/2
and native gameplay math. This completes the bounded Linux check, not the
overall 20–25 ms performance objective. Do not broaden either transformation
or rebuild every game module based on these results alone.

Evidence lives under `runs/linux-aot-statistics/`, including the six summaries,
source manifests, object sizes, and `comparison.json`. No new performance
patch or installer has been made. The existing diagnostic ON/OFF patches
are unaffected by this experiment.

## Restoration

The retained Linux cache returns to `SARECOMP_LINUX_AOT_STATISTICS=OFF` and
mode `RUNTIME` (inert while OFF). The original cached guest objects are
re-archived and linked; no game analysis or full AOT compilation is needed.
The stripped result is byte-identical to D2: 1,678,965,680 bytes, SHA-256
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.
The isolated host output `out/aot-statistics-experiment/game` and VM slot
`/home/sonic/preloaded-v1/game` were restored to that control as well, after
confirming the test process had exited. Candidate source/object caches and
recorded hashes remain available for reproduction. The installed product,
existing patches, personal saves and r354 were not replaced by either candidate.
