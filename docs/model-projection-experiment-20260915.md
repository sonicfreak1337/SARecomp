# Model projection experiment and fixed-work probes

The projection specialization is **removed from the game**. It remains an
excluded differential-test target, without a runtime setting or installer
dependency. It did not produce a useful FPS gain on either tested system.
The accepted executable, r354, user saves and existing installers were not replaced.

## Scope and verification

The trial specialized the already admitted NINJA model-transform owner. It
preserved the retail pair loop, padded point, final read-ahead, FTRV/FDIV/
FCMP/FMUL/FADD sequence, clip counts, final registers and FPSCR, and the
fourth word in each 16-byte output record. Guest writes stayed in the existing
transaction. Whole-span direct RAM access could only replace unobserved reads.

Both Windows and the Linux VM passed 4,101 differential cases: 4,096 seeded
cases plus five mutation-free mode rejections. Coverage includes signed zero,
subnormals, boundary magnitudes, infinities, SH-4 NaNs, both supported rounding
modes and incoming status. Output bytes, clip bytes, registers/status and host
FP restoration were compared. The existing body suite also passed 140 cases
on each platform. These counts are not a complete input-domain proof.

The final Windows game oracle additionally logged 700,000 models and
24,072,181 points with exact results during Gamma Emerald Coast. It finished
at the expected deadline. This diagnostic run is not a performance sample.

The borrowed-epoch body overload keeps the caller's existing matching CPU
epoch alive across this synchronous operation. Its caller must preserve
rounding/mode admission and cannot dispatch callbacks. Existing constructors
retain their prior lazy-epoch behavior.

## Measured outcome

All Windows rows used the same executable:
`96a7535aa6f6682dfdb8dc8221e45dd831a10cb675b63aa08207a8eac3045e59`.
Runs were hidden/muted, isolated forward input, Vulkan offscreen, 1280x800,
100% scale, Recompiled timing and VSync off. Each measured 60 wall-clock
seconds of gameplay, excluding the first ten seconds. They all stopped normally.

| Scenario / metric | Retained loop | Specialized loop |
| --- | ---: | ---: |
| Sonic Emerald Coast, new draws/s | 53.239753 | 52.848331 |
| Sonic, execution CPU ms/boundary | 17.826036 | 17.965491 |
| Gamma Emerald Coast, new draws/s | 52.095504 | 51.997514 |
| Gamma, execution CPU ms/boundary | 17.375144 | 17.371146 |

Sonic ran control then candidate; Gamma reversed that order. The difference
is small and does not establish an improvement. See `runs/projection-*`.

The Linux comparison used one immutable executable:
`6743d9f7f84360fb274f3e058af24bd1c88d09ddd7babc4a7137c4a271eaadca`.
The QEMU TCG/llvmpipe VM used the reduced 800x500 Deck-aspect probe. Both runs
completed normally and produced 34 measured new frames. New draws/s were
0.570164 retained versus 0.570269 specialized; execution CPU ms/frame were
374.132 versus 363.438, but whole-process CPU was 3347.548 versus 3357.456.
There is no throughput gain and no Steam Deck performance measurement here.

Local reproduction material is under
`.local/performance-experiments/model-projection-20260915/`, including the
removed integration patch and the exact older Linux benchmark script used.

## VM arithmetic observation

The initial Linux test failure was traced to the unchanged SDK's FMA vector
helper producing a different low bit with identical guest inputs but different
incoming host exception flags. In the failing fixture, MXCSR 0x1f80/0x1f81
produced FR0 0xc440da40; 0x1fa0/0x1fb3 produced 0xc440da41. The executable
contained native vector FMA instructions. This VM reports QEMU
11.1.0-12130-ge470268ff4 with TCG; it is not a native Deck CPU.

This is an observed VM-dependent arithmetic difference, not a diagnosed game
bug or a proven QEMU root cause. Borrowing the owner's existing epoch removed
the unnecessary inner host-status reset, after which the full unmodified
4,101-case comparison passed on Linux. No case was skipped and no result was
normalized. QEMU and the SDK vector implementation were not patched.

## Fixed-work measurement improvement

The hidden probes now accept optional `--begin-frame` and `--end-frame`.
The game accepts them only with hidden mode, isolated input and a private
save root. It emits both exact boundary samples and stops through the normal
HostDeadline frontier. Invalid/missing boundaries cannot make the benchmark
pass. Without these arguments, the previous 60-second probe is unchanged.

This avoids comparing different lengths of a forward walk just because one
version runs faster. Exact position bits, game ticks and HUD timer ticks are
recorded at the boundaries. Equal frame counts still do not prove bit-identical
world state: loader timing, asynchronous services and scene evolution can
remain confounders. These witnesses must be inspected with performance results.
The Linux helper also reparses its final log after process exit, so a final
sample cannot be missed by its polling interval.

## Live RAM-copy follow-up: also rejected

A separate trial grouped live primary/secondary color reads and copied the
current transformed-record padding from one admitted RAM span. All arithmetic,
the guest commit, scalar/observed fallback and Original timing stayed intact.
It was also removed: the measurements do not establish useful execution-thread
CPU savings. Its patch and exact benchmark scripts remain in the local
`model-ram-copy-20260915` experiment directory.

The Windows oracle logged 38,092,672 exact color words and 18,911,434 exact
padding words over at least 600,000 models and stopped normally. The measured
build SHA was `2cc408ef6a13e84ab0149d6898eb7e402246a7005130c5f4c6507e1e7ccd54b7`.
Its first time-based Sonic pair was 52.673414 versus 51.914232 new draws/s
(control/candidate). The two runs finished at different world positions,
which was a reason to add the fixed-work mode instead of treating this as
a precise regression percentage.

The following comparisons use exact title-boundary windows:

| System / metric | Retained reads | Grouped reads |
| --- | ---: | ---: |
| Windows, frames 300..1800, new draws/s | 52.482623 | 52.811491 |
| Windows, execution CPU ms/boundary | 18.125000 | 18.041667 |
| Windows, execution cycles/boundary | 79,784,156 | 79,299,894 |
| Linux VM, frames 5..35, new draws/s | 0.581035 | 0.602458 |
| Linux VM, execution CPU ms/new frame | 354.639357 | 355.930593 |
| Linux VM, process CPU ms/new frame | 3285.024837 | 3166.698970 |

Both pairs ran candidate then control, all passed their exact window and
normal stop checks. Windows covered 1,500 measured boundaries. Position bits
matched at the starting boundary; ending position differed slightly despite
equal HUD timer values. Linux covered only 30 measured new frames, with
identical position bits and both timer witnesses at both ends. It therefore
tests a very short opening interval, not a complete stage or Deck frame rate.

The Windows execution difference is below half a percent; Linux execution
CPU is slightly higher. The VM's 3.7% throughput difference alone is not
evidence of less execution work or a reliable hardware gain. No extra stage
matrix was run to chase these small differences. Artifacts:
`runs/fixed-ram-copy-{native,control}-emerald/` and
`runs/linux-fixed-ram-copy-{native,control}/`.

The fixed-window Linux binary is
`9706d2d854faeaca3205430b70f07d905e66f63bce1944bcdeaba7f3bf237d44`.
The fixed-window Windows pair used
`3966b6175bd853b362cfdfcc0ef9ed8beb11ea893f3545e39bcfdfadefd4cbff`.
Both experimental runtime switches have been removed from the current source.
The real model-transform and color-read bodies match their pre-experiment
implementations again. The better probe boundaries and standalone arithmetic
oracles remain available without linking the trial projection kernel into game.

## Final integration checks and next scope

After removing both trial integrations, incremental Windows and Linux game
builds completed without an AOT export. Windows FPU/provider link audits passed.
A final hidden Windows probe passed the exact short 5..35 frame window and
normal stop frontier; its executable is
`0e6fdeca6de6eee7cbfff73b7792515776860a188dab7e5cdab0b43f89a4ab8f`.
This is a smoke check, not a throughput measurement. The corresponding final
Linux build is
`0b3afc852b2126f0a3a893e2c8f245455187c49e882e2ac175f5982a673ece95`;
it was built, not given another full stage run after restoring the old bodies.
The accepted `out/experimental/game.exe` remains
`825c3f84ef0f5b0c874da945d1c4d7f800a12222e19761b44c927220e9af03e3`.

A bounded read-only countercheck by Sage also rejected instruction-metadata
batching as a new global lever. The actual retained inverse unit already groups
21 instructions in guest region 8C6394EE..8C639518 into one final PC/accounting
publication (attempted/retired +21, cycles +42). Its checked and faulting paths
must retain their observable boundaries. This was confirmed in the source.

Local accumulation of FPSCR Cause/Sticky across only that region's 16 binary
operations remains a possible narrow research question, requiring publication
before every fallback or observable exit. No measured store-cost attribution
supports it as a large optimization, so no global rewrite or new runtime path
was introduced from that suggestion. The +50% Deck goal remains unachieved.
