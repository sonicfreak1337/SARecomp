# Matrix-unit arithmetic expansion

2026-09-13. Private CMake experiment `SARECOMP_FPU_CALL_EXPERIMENT=UNIT`.
This extends the [221-site INVERSE candidate](inverse-arithmetic-experiment.md)
to 423 arithmetic call sites in the same retained AOT unit. It does not
replace a matrix algorithm or alter the original update/render cadence.
The CMake default stays OFF; the r354 baseline and pinned SDK are immutable.

## Why expand this unit

A fresh hidden/muted, hardware-isolated Emerald Coast sample was captured
before modifying the executable or map:
`runs/execution-inverse-isolated-01/execution-ip-resolved.json`.
It contains 1,291 instruction-pointer samples over 20,006.1 ms, including
1,238 in game.exe. Sampling suspended the game thread for 56.5285 ms total;
its CPU timing is diagnostic, not a clean performance control.

Unambiguous object ownership attributes 134 samples to the retained FPU
runtime and 65 to the selected AOT unit. Another 132 game samples have folded
multi-object ownership and cannot be assigned uniquely. Nearest EH labels
inside the selected unit are not reliable function names. Disassembly from
a verified native symbol boundary maps sampled instructions to original
guest markers in both the XMTRX and RAM inverse branches, including
`8C6394D4`, `8C6395BC`, `8C639600`, `8C63962E`, `8C6396A4` and `8C639770`.
This establishes that the still-unmodified RAM branch runs in this fixture;
it does not establish an exact percentage of time exclusively in that branch.
Native inspection: `.local/menu-preview/profile-inverse-native-region.txt`.

The additional 202 sites were independently reviewed read-only. Integration,
source substitutions, compilation and measurements remain root-owned.

| Original owner | Added sites | Behavior retained |
| --- | ---: | --- |
| `8C638FF0`, RAM branch `8C6393FE..8C63978E` | 197 | In-place 16-float matrix inverse; 129 multiply, 35 subtract, 33 add |
| `8C6397F8`, `8C639810/812/814` | 3 | Two input squares and their sum before retained reciprocal square root |
| `8C639B1C`, `8C639B34/B46` | 2 | Multiply transformed X/Y by reciprocal Z before retained scale/offset/stores |

The full selection contains **274 multiply, 82 subtract and 67 add** sites.
The original four-argument divides at `8C639054` and `8C639B2E` remain.
All 197 new RAM-inverse operation/register combinations already occur in
INVERSE. Four combinations are new to the selected sequence: multiply
`<1,1>`, `<2,2>`, `<7,2>` and `<7,3>`.

## Semantic and native checks

The source is still SHA-bound
`unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp`, SHA256
`79ecc1ebbdf3e06c517d5edcc42850c08eb50c7dfe6fe4359c59b0e626472558`.
The arithmetic bodies are the same verbatim retained integer product/sum
implementations used by INVERSE. Only constant operation/register parameters
and forceinline decoration change their placement. Exceptional or unsupported
inputs still call the real retained helper.

All 202 added sites reside in 33 retained host-FPU epoch scopes. Initial
checked instructions, all five-argument calls, guards, bank switches,
accounting, memory operations, singular handling and instruction order remain
unchanged. Reversing only the 423 substituted statements recovers the entire
original source exactly, apart from removing the one new include. No
fast-math, reassociation, fused operation or new host-rounding policy is used.

The real-runtime differential component passes **344,768 cases**: 17,590
accepted inline cases, 327,178 fallbacks and 193,101 reference traps. This
includes the complete 423-operation dependent skeleton, checking state after
each operation, plus 96 targeted cases for the four new combinations across
actual `write_fpscr` FR-bank swaps. Normal/boundary/NaN inputs, PR and enabled
exception fallbacks, both register banks, CPU state, exception callbacks,
sentinel RAM and MXCSR agree. The skeleton intentionally omits intervening
non-arithmetic work and is not presented as a matrix/game benchmark.
Log: `.local/menu-preview/test-unit-arithmetic-01.log`.

Native object relocations match the source change exactly:

| Variant | Four-argument FPU relocations | Five-argument relocations | Object bytes |
| --- | ---: | ---: | ---: |
| INVERSE | 204 | 737 | 3,020,130 |
| UNIT | 2 | 939 | 3,111,890 |

The extra 202 five-argument references are the retained fallbacks; the
516 originally checked calls remain. Evidence and hashes:
`.local/menu-preview/unit-arithmetic-native-evidence.json`.
The initial game build compiled just this one replacement unit, archived it
and relinked. Ten-entry exclusive ownership and native closure audits pass.
The control return reused the cached INVERSE object without recompilation.

## Bounded game comparison

Same hidden/muted hardware-isolated Emerald Coast forward fixture, copied
saves, normal remapping, 1280x720 D3D11, 100%, VSync off, 144 output target.
Each run lasts 60 gameplay seconds and excludes the first ten. The sampler,
update trace and render-completion experiment are off. Results are recorded
in the three `runs/unit-arithmetic-*-ec-01/result.json` files.

All three runs completed the planned stop without a fault or forced exit.
Clock ownership remains the original PAL checkpoint with stored TV word 1.

| Run / variant | New images/s | Output/s | Execution CPU ms/image | Raw cycles/image |
| --- | ---: | ---: | ---: | ---: |
| control-ec-01 / INVERSE | 15.536 | 144.000 | 63.106 | 277,106,144 |
| candidate-ec-01 / UNIT | 17.331 | 143.628 | 56.217 | 247,205,446 |
| control-return-ec-01 / INVERSE | 18.280 | 143.766 | 53.005 | 232,973,215 |

UNIT uses 10.92% less execution CPU than the first control, but **6.06% more**
than the return control. Raw-cycle changes likewise range from -10.79% to
+6.11%. Aligned process CPU/image is 62.717 ms for UNIT versus 74.168 and
62.105 ms for the controls. The user's PC remains in use, and time-bounded,
frame-indexed forward movement does not guarantee identical world intervals.
The spread does not support claiming a repeatable performance improvement
or a proven inherent regression. No additional matrix was run to chase it.

## Integration decision

**Keep UNIT disabled and retain INVERSE in the experimental game build.**
The source-bound expansion and differential test remain available as a
private comparison; they are not promoted as an optimization. No new user
setting is exposed. The original CMake default is OFF. The render-completion
candidate and withdrawn interpolation remain off for normal play.

Final retained control: `.local/menu-preview/build-unit-control-return-01.log`.
Native closure and ten-entry ownership pass; the selected INVERSE object hash
is unchanged. `out/experimental/game.exe` is 1,910,361,600 bytes, SHA256
`a24665d09d890336ab0cc3b4a8b7f9f8c21c06594f9e123e0cbf541389c08c5c`.
It is the executable used in the successful return-control run. Only comments
and documentation changed afterward. No test or compiler process remains.

These tests do not establish 60 new rendered images per second or complete
gameplay/animation updates at 60 Hz.
