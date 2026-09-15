# Linux execution layout and buffer comparisons

Internal follow-up to [the Vulkan/media batch](performance-media-20260915.md).
No installer has been produced. The accepted Windows executable, frozen r354
baseline, game assets and personal saves remain untouched. New Deck installs
still default to Original timing while the Recompiled performance issue is open.

## Layout experiment: left disabled

Two fresh Linux profiles (Windy Valley and Gamma Emerald Coast) selected 229
function symbols in 138 input code sections. The linker moved 72,343,404 bytes
into a code-only section; it did not remove functions or alter guest sources.
`prepare-linux-code-layout.py` accepts only completed, distinct-scenario profiles
whose executable SHA matches the reference file. It records input-object hashes.
The optional `SARECOMP_LINUX_CODE_LAYOUT` path is empty by default and in the
current retained Linux build.

The script uses the ordinary GNU/LLD
[input-section selection](https://sourceware.org/binutils/docs/ld/Input-Section-Basics.html)
and [INSERT policy](https://lld.llvm.org/ELF/linker_script.html).
Shared `.text` sections move whole; named COMDAT sections retain normal selection.
There are no KEEP rules, fixed addresses, data reordering or AOT compiler changes.

Configuration picked up the current Windows-staged dispatch source. Both A/B
executables were therefore relinked from the exact same current object/archive
hashes. The training ELF is a separate unchanged file. Both comparison ELFs have
265,413 function symbols with identical name/size/binding inventories, the same
`.eh_frame` size and no writable/executable load segment.

| Windy Valley, 16:10, 60 seconds of actual gameplay | Control | Reordered |
| --- | ---: | ---: |
| New game frames/s, after the first sample | 0.506928 | 0.491692 |
| Execution CPU ms/new frame | 402.170 | 395.802 |
| Process CPU ms/new frame | 3772.211 | 3887.326 |

Both finish at the expected HostDeadline, without a game failure. The 1.6%
execution-time difference is insufficient to claim a gain; throughput is 3.0%
lower. The option remains off. No extra stage matrix was run to chase it.
Artifacts: `runs/linux-code-layout-{control,native}-windy/summary.json`.

## Buffer-comparison change

The actual game ELF contains Zig compiler-runtime `bcmp`/`memcmp` loops which
compare one byte per iteration. These calls are used throughout mesh validation,
native source-identity checks, material comparisons and render-state reuse.
The new `sonic_memory_compare.c` keeps every comparison and its complete range.
It does not memoize source identities or omit memory/observer protection checks.

- Short ranges use unaligned, alias-safe 64-bit loads only when eight bytes
  remain, followed by a byte tail. Ordered comparisons select the first
  differing byte in x86-64 little-endian order.
- At 128 bytes and above, comparisons call the existing glibc implementation
  through `memcmp@GLIBC_2.2.5`. This is below the existing glibc 2.31 floor.
  glibc supplies CPU-selected implementations; no AVX requirement is added.
- Definitions are hidden. They replace the compiler archive's weak definitions
  inside this executable without interposing on SDL, codecs or the Vulkan driver.
  The ELF has a versioned undefined import and no exported unversioned replacement.
- Zero-length calls retain the old behavior; `bcmp` retains its precise 0/1
  result. `memcmp` retains unsigned-byte ordering. Copy, fill and move routines
  stay unchanged.

The source uses the documented
[versioned-reference binding](https://sourceware.org/binutils/docs/ld/VERSION.html).
The CPU-selection implementation is visible in glibc's
[x86-64 multiarch sources](https://github.com/bminor/glibc/tree/master/sysdeps/x86_64/multiarch).

The initial all-memory bridge was rejected: glibc copying and filling were
slower on TCG, while short comparisons paid too much call overhead. The retained
experiment covers comparisons only, with a local short-range path.

## Component verification

Both original and candidate pass 69,442 cases covering protected-page ends,
unaligned inputs, zero lengths, every unsigned byte pair, first-difference
ordering, overlapping moves and write boundaries. Benchmark checksums match.
The control explicitly links the 128-bit compiler archive used by the full game;
without that dependency, a tiny C executable would already use libc instead of
the game's portable routines. Disassembly verifies the actual control loop.
Volatile function pointers prevent pure-call or builtin hoisting in the benchmark.

| Equal distinct buffers, VM thread-CPU speedup | bcmp | memcmp |
| --- | ---: | ---: |
| 16 bytes | 1.85x | 1.93x |
| 64 bytes | 3.60x | 4.07x |
| 256 bytes | 3.10x | 3.75x |
| 4096 bytes | 5.21x | 5.91x |
| 65536 bytes | 5.37x | 6.59x |

These are component results, not game FPS or Deck predictions. Artifacts:
`runs/linux-memory-primitives/compare-{control,hybrid}.txt` and
`.local/menu-preview/linux-memory-compare-run.log`.
## Matched gameplay measurements

The build option `SARECOMP_LINUX_MEMORY_COMPARE` now defaults on, with an off
control retained for hardware comparisons. This changes host buffer comparisons
only. The game required a relink: all prior input-object/archive hashes match,
with one additional C object and 472 additional bytes in the unstripped ELF.

| 16:10, 60 seconds of gameplay | Control | Comparison optimization | Change |
| --- | ---: | ---: | ---: |
| Windy Valley, new frames/s | 0.506928 | 0.515066 | +1.6% |
| Windy Valley, execution CPU ms/frame | 402.170 | 395.741 | -1.6% |
| Windy Valley, process CPU ms/frame | 3772.211 | 3681.754 | -2.4% |
| Gamma Emerald Coast, new frames/s | 0.412322 | 0.420479 | +2.0% |
| Gamma Emerald Coast, execution CPU ms/frame | 414.586 | 419.730 | +1.2% |
| Gamma Emerald Coast, process CPU ms/frame | 4705.357 | 4546.051 | -3.4% |

Gamma used reverse order (candidate, then control). All four runs finish at
their expected deadline without a game failure. These are single pairs: the
small whole-game differences can include run noise, and Gamma does not establish
an execution-thread CPU reduction. The much larger component speedups must not
be reported as a game gain or extrapolated to Deck. This does not achieve +50%.

Artifacts: `runs/linux-code-layout-control-windy/summary.json`,
`runs/linux-memory-compare-windy/summary.json` and
`runs/linux-memory-{control,compare}-gamma/summary.json`.

## Inclusive profile and next scope

The new optional `--profile --callgraph` mode records frame-pointer call chains
on the owned execution thread at 99 Hz and exports inclusive `perf-families.txt`
alongside the flat report. The first Emerald Coast recording has 523 samples,
no lost samples, and valid chains through the guest task traversal back to main.
It completes normally and retains the exact profiled binary hash. The recording
is diagnostic, not an unprofiled throughput result.

The model draw owner accounts for about 10.1% inclusive execution samples, model
transform 5.5%, palette lighting 4.0% and collision candidates 3.6%. Parents and
children overlap; these percentages cannot simply be added. At only 523 samples
and a short simulated opening sequence, they identify investigation targets,
not precise hardware costs or later-stage coverage. Artifact:
`runs/linux-family-profile-emerald/`.

The task traversal's 86.8% includes its entire callback tree. Replacing that
traversal would not remove the cost of its children. Further work should target
the shared model/geometry owners without bypassing mutation or memory checks.
The previously rejected NEAR collision owner is not silently enabled.

## Measurement provenance

The VM uses QEMU TCG, two vCPUs and llvmpipe. The new `--aspect deck` probe uses
16:10 culling at 800x500 / 50% render scale, VSync off, Recompiled cadence,
isolated input and fresh private user data. This is not native-resolution Deck
performance. Earlier 4:3 results remain separate rather than being mixed in.

`benchmark-linux-stage.py` now records the executable hash, checks it again after
the run, exports raw symbol names with profiling, and requires both gameplay
completion and the expected stop frontier. Linux ELFs currently have no build ID:
old perf files must not be resolved against a subsequently replaced executable.
The fresh training profiles use an unchanged, separately named control binary.
The probe checks identity both before resolving perf data and after reporting,
and its compact measurement summary uses newly drawn frames after the first
sample, with separate execution-thread and process CPU time.
