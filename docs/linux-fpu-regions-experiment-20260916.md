# Closed FPU-region experiment, 2026-09-16

This is a port-local experiment, not a new performance patch or installer.
The installed product, personal saves and r354 are not replaced. The overall
Deck target of at most 20–25 ms/frame remains open.

## Difference from the rejected scalar hardware experiment

The earlier hardware helper saved/restored MXCSR for every guest operation.
That made the native arithmetic kernel slower despite using SSE arithmetic.
The retained AOT already groups pure FPU instructions inside SDK rounding
epochs. This experiment borrows those existing epochs: it does not create
larger epochs, remove their boundaries or repeat their setup per operation.

`sonic_fpu_region.hpp` admits only single precision, DN set, all guest exception
enables clear and rounding mode nearest-even or toward-zero. Normal/zero
operands use SSE. Non-finite and subnormal operands, division by zero and
boundary/underflow results use the retained helper. Precision-polymorphic AOT
epochs stay unchanged. Arithmetic is not reassociated and contraction stays off.

The guest Inexact flag is calculated independently of host exception status:

- Binary32 products fit exactly in binary64; compare against the rounded result.
- For division, compare result × divisor against dividend in binary64.
- For addition/subtraction, exponent gaps up to 24 have an exact binary64 sum;
  larger nonzero gaps necessarily lose binary32 precision.

FPSCR Cause and Sticky flags are committed after every accelerated operation.
Intermediate host flags may differ only inside the closed callback-free region;
the original SDK epoch restores the exact incoming MXCSR at its boundary.
This is deliberately not a replacement for the general-purpose runtime API.
The retained helpers used in these regions either ignore host status or clear
it before reading arithmetic flags. Memory, services and guest dispatch are
excluded. Instruction admission, fault origins, cycles, counters, register
flushes/reloads, first checked instruction and the slow branch stay intact.

## Bounded binding

`prepare-fpu-regions.py` verifies the retained source manifest and each complete
source hash. It requires the exact existing single-precision epoch predicate,
the known set of pure helper calls and original final accounting. Reversing
the substitutions and removing the borrowed token reconstructs every original
source byte. An unknown guard or helper is rejected. The two existing epochs
without a PR=0 predicate are explicitly left unchanged.

The selected 41 common units contain 571 transformed regions and 971 replaced
four-argument binary calls. The first checked five-argument calls stay original.
No stage module, runtime ABI or title dispatch table is regenerated. Only the
selected objects and guest archive are rebuilt before relinking.

`SARECOMP_LINUX_FPU_REGIONS` defaults OFF and cannot be combined with the older
read/statistics/hardware experiments. The component test target is excluded
from normal builds and never enters a distribution package.

## Qualification

Component comparisons cover normal values, signed zeros, exponent boundaries,
underflow/overflow, infinities, both NaN conventions, random raw bit patterns,
both rounding modes, dirty ambient MXCSR, varied sticky flags and dependent
chains mixed with retained sqrt, comparison, MAC and fallback operations.
Every guest CPU-state field in the existing fixture is compared after each
step; exact host state is checked at the closed-region boundary.

Both final Windows and Linux components pass 303,616 cases: 233,238 binary
cases take the hardware path, 53,994 use the retained fallback, and 16,384
dependent-chain steps are checked separately. All 41 generated source hashes
and reverse transformations match. Injected unknown helpers and a changed
admission predicate are rejected before generation.

The initial native Windows kernel compared 262,144 operations in groups of
2, 8 and 32, with alternating order and median-of-five results. It showed
about 1.23–2.31× throughput for this arithmetic work, including epoch costs.
That is neither whole-game improvement nor a Steam Deck measurement. The
Linux TCG VM is used for compatibility and matched gameplay comparisons,
not as a hardware-equivalent Deck throughput model.

Evidence: `runs/fpu-region-*.log` and
`build-linux/generated/fpu-regions/preparation.json`.

The stripped candidate is 1,679,214,672 bytes, SHA-256
`fb5650d748d8d4847eebaac72770ffe0d88798f7e2a1f9097df97a5f666d919c`.
The 41 object files grow from 93,639,424 to 93,917,920 bytes (+0.30%).
The matched control is the unchanged D2 internal-diagnostics build,
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.

## Gameplay result: do not enable

Four fresh hidden/muted Linux Vulkan runs use separate save roots and isolated
input, 800x500 / 16:10, 50% render scale, diagnostics OFF, native gameplay math
and image boundaries 5..25. No build or profiler runs during the measured
windows. Order: Gamma control, Gamma candidate, Windy candidate, Windy control.
Every sample retains PAL video 50 Hz, release 2 and logical delta 2. All runs
complete at the requested deadline (HostDeadline, exit 1), without a fault or
forced termination. These are short gameplay probes, not full-stage playtests.

| Stage | Variant | New images | Game updates | Execution CPU ms/update | Process CPU ms/update | New images/s |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Gamma Emerald Coast | Control | 20 | 68 | 242.932 | 989.459 | 0.5589 |
| Gamma Emerald Coast | Candidate | 20 | 68 | 241.877 | 980.060 | 0.5664 |
| Sonic Windy Valley | Candidate | 20 | 67 | 253.729 | 986.507 | 0.5834 |
| Sonic Windy Valley | Control | 20 | 66 | 249.443 | 976.758 | 0.6070 |

Gamma saves only 0.43% execution CPU/update; final player position bits and
game tick 258 match exactly. Windy costs 1.72% more execution CPU/update and
loses 3.88% image throughput. Its authored overload handling performs 67
updates versus 66; final game ticks and positions therefore do not match.
Normalization makes that workload difference visible, but these are not
identical instruction traces. No useful overall improvement is established.

The native kernel result does not justify expanding this transformation to
the whole AOT pack. Keep the option OFF; do not publish a new performance
patch/installer or infer Deck gains from the kernel. The existing internal
diagnostic ON/OFF patches remain unchanged. Measurements and comparison JSON
are under `runs/linux-fpu-regions/`.

## Restoration

The Linux cache returns to `SARECOMP_LINUX_FPU_REGIONS=OFF`. Reuse the retained
control objects and relink without a full AOT compile. The isolated host
`out/fpu-region-experiment/game` and VM `/home/sonic/preloaded-v1/game` return
to D2 after all game processes have exited. Candidate generated sources,
objects, hashes and measurements remain available for reproduction. No
installed-user binary, save, diagnostic patch or r354 file is changed.

Both restored outputs are verified byte-for-byte by D2 SHA-256 and size
1,678,965,680. The first host strip removed the non-loadable `.comment` section;
all loadable sections already matched. Keeping `.comment`, as the existing
packager's objcopy does, reproduces the exact control. Restoration records and
the section comparison are saved with the measurements.

## Follow-up boundary

Further individual floating-operation substitutions are not justified by this
result. A distinct, untested global candidate is profile-guided compilation:
both current product definitions still report PGO off, and no Linux profile
generation/use configuration exists. Clang supports separate instrumented
training and profile-use builds; the training binary is not a performance
candidate. See the [Clang manual](https://clang.llvm.org/docs/UsersManual.html#profile-guided-optimization).
Before a costly game build, qualify that path with the pinned Zig/Clang 21.1
cross-toolchain and a small Linux component, including a matching profile
runtime/tool. No PGO gain is measured or promised here, and the existing perf
samples cannot be passed directly as instrumentation profiles.
