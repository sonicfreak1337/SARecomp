# Mixed RAM execution regions

This is a port-local execution experiment following the installed SADX
comparison. It is not enabled in a release or installer. The retained r354
source pack, SDK and user saves remain unchanged.

## Change

The common profiled units contain long straight-line sequences interleaving
integer arithmetic, pointer-dependent RAM loads/stores and scalar FMOV.
The new preparer emits a native prefix for these sequences. It keeps working
GPR values local and publishes instruction/cycle accounting once per completed
prefix. FR values are local in move-only regions; regions with arithmetic use
the original FR/FPSCR state and original runtime arithmetic helpers. It does
not change simulation cadence or skip game work.

Unlike the earlier adjacent-read or stack-sequence pilots, this handles reads
after writes and dependent pointer loads in the same region. Stores happen in
original order and reads see actual RAM, including aliases and partial writes.
There is no speculative write journal or rollback.

Every access must satisfy the original function-entry read guard and address
translation. Stores additionally require the exact current NativePort observer
pair, write permission, and a current check excluding immutable/executable
ranges. Successful memory operations retain their original memory counters.
No diagnostic observer, memory-mapped I/O, fault or executable write is elided.

If access N cannot use the prefix, completed instructions 0..N-1 are published
and execution jumps before the entire original instruction N, including its
preflight scheduler flush. It never repeats previously committed stores.
Original interior-resume labels bypass the new prefix. ALU accounting groups
remain whole; instruction provenance follows the last actual Attempt, which
may precede trailing ALU instructions. FD/SZ modes requiring original FMOV
semantics, open write batches and pending write exits retain the original path.

## Scope and qualification

- Build option: `SARECOMP_LINUX_RAM_REGIONS=ON`, default OFF.
- Private runtime switch: `SARECOMP_RAM_REGIONS=1`, default OFF.
- Benchmark switch: `--ram-regions fused`; default `original`.
- Same 41 profiled common units, retaining archive member order.
- Current expanded scope: 967 regions, 15,103 instructions, 9,278 RAM accesses,
  3,546 stores, and 685 original floating-point operations.
- The initially measured narrower candidate had 768 regions, 10,774
  instructions, 7,115 accesses, and 2,968 stores. Its results do not measure
  the expanded implementation.
- Largest admitted region: `8C0CC090..8C0CC134`, 83 instructions / 60 accesses.
- Exact source-generation and per-unit SHA binding; unknown shapes excluded.
- Branches, integer arithmetic flags, delay slots, FP mode changes, integer
  increment/decrement addressing, and existing grouped FP epochs are excluded.
- Scalar FMOV postincrement, cached GBR-relative accesses, and fully shape-
  authenticated existing FMOV read groups are included. A read group is an
  indivisible atom with its own instruction weight and original all-or-miss
  behavior, not N independently reordered reads.
- Original scalar Add/Subtract/Multiply/Divide and square-root helpers are
  admitted only with FD/PR/SZ clear, DN set, exception enables clear, and legal
  RM. This excludes unmaskable denormal traps as well as enabled exceptions.
  A host FP epoch spans the prefix and ends before every original fallback.

These are static coverage counts, not execution frequency or speedup claims.

The Linux component executable passes 1,022 comparisons: full architectural and
fault state, memory counters, alias/partial RAM, immutable state and observer /
scheduler logs. This includes actual original and transformed 83-instruction
game source, with late faults after stores, scheduler mutation and interior
resume entries. Cases also cover stale guards, user/MMU modes, sign extension,
counter wrap, dynamic executable ranges, tracing and foreign observers. Four
real retained/transformed witnesses cover 143 instructions: the initial long
RAM region, a 35-instruction load/multiply/store region, a 16-instruction
GBR/vector-read region, and a nine-instruction postincrement/Add/read-group
region. FP modes, signed zeros, denormals, infinities and NaNs, host MXCSR
restoration, and late faults after floating-point operations are covered.

The two latter owners were selected from exact PE-runtime-function AOT self
samples, not inclusive parent samples. The current 41 source units contain
435 of 657 uniquely attributed AOT samples in that Windows Gamma profile.
This is source-unit coverage, not prefix execution frequency or a speedup.

## Gameplay measurement

Initial candidate SHA-256:
`e09ab3358ac60e9d71d109d5618ead1c3e933be6fdcf3f97309e83c69a3ffc1e`.
Prepared transfers are enabled too. The pre-22:00 control is D2, SHA-256
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.
Runs used the same QEMU/TCG Linux VM, Vulkan/llvmpipe, Original PAL 50/2/2,
native gameplay math, diagnostics/telemetry OFF, 800x500 at 50% render scale,
isolated saves, and new-image window 5..25. All five runs completed at the
expected host deadline without forced stop. No builds overlapped measurements.

| Run | Updates / 20 images | Execution CPU ms/update | New images/s |
| --- | ---: | ---: | ---: |
| Gamma pre-22:00 | 66 | 271.87075 | 0.398414 |
| Gamma initial prefix ON | 64 | 261.33824 | 0.428000 |
| Gamma same binary prefix OFF | 68 | 264.34735 | 0.370975 |
| Windy pre-22:00 | 68 | 303.25024 | 0.450207 |
| Windy initial prefix ON | 68 | 283.36602 | 0.450674 |

Gamma shows 3.87% less execution CPU/update versus the pre-22:00 control,
but only 1.14% versus the same binary with this prefix disabled. Its differing
update counts, ending coordinates, and native-call deltas prevent attributing
the raw image-rate difference to equivalent work. Windy shows 6.56% less
execution CPU/update with matching coordinate bits, HUD timer and native-call
boundaries; the absolute game-tick counter differs by one in both boundaries.
Whole-frame throughput changes only +0.10%. These are single pairs, not a
demonstrated large global or Steam Deck FPS gain.

Evidence: `runs/ram-regions-{gamma-on,gamma-off,gamma-pre22,windy-on,windy-pre22}-20260916.json`.

Expanded candidate SHA-256:
`c772df4f67d0aaec840c82bdddc7046a48c0cd262886d9bdc88968dd7ecbad99`.
Both additional runs completed at the expected deadline under the same settings.

| Run | Updates / 20 images | Execution CPU ms/update | New images/s |
| --- | ---: | ---: | ---: |
| Gamma expanded prefix ON | 67 | 256.17241 | 0.419683 |
| Windy expanded prefix ON | 68 | 275.48682 | 0.458106 |

Versus the fresh pre-22:00 controls above, execution CPU/update is 5.77% lower
in Gamma and 9.16% lower in Windy. Gamma performs one extra update and differs
in native-call counts and ending coordinates, so its raw +5.34% image rate is
not an equivalent-work gain. Windy has matching update count, coordinate bits,
and HUD timer. It retains the one-tick absolute offset and performs two more
native collision-length calls by the ending boundary. Its image rate improves
only 1.75%. These single pairs do not establish the requested large net gain.

Actual prefix-owner coverage in the Windows Gamma self profile is 282 of 657
uniquely attributed AOT samples (365 owners). This is an upper bound on prefix
hotness: samples elsewhere in the same owner do not prove a prefix was hit.

Evidence: `runs/ram-vector-{gamma-on,windy-on}-20260916.json`,
`runs/ram-vector-comparison-20260916.json`, and
`runs/ram-region-profile-coverage-20260916.json`.
The large gain and 20–25 ms target remain open. No installer or patch was produced.
