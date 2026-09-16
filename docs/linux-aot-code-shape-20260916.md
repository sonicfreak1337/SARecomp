# Linux AOT code-shape experiments, 2026-09-16

No installer or performance patch is produced. Original timing, memory/exception
semantics, native gameplay math, personal saves and r354 remain protected.
The prior observer-permission experiment is disabled; the retained Linux game
begins this investigation at control SHA
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.

The preceding goal turn made concrete progress: four matched Linux stage runs
rejected that candidate, and local/VM executables were restored bit-for-bit.
This investigation addresses different compiler-generated work.

## Three-unit screening before gameplay

The three retained units are `073018`, `0CBD40..0CCFDC`, and `0400A0..04124E`.
They contain common collision/player/motion code seen in the Linux execution
profile. Trials use the retained Linux compiler/target and O2/strict FP flags.
Each preparation authenticates the retained manifest and entire source bytes,
writes separate files, and leaves original generated inputs unchanged.

1. **Canonical PC switches:** `prepare-pc-switch-canonical.py` requires the
   complete switch grammar. Each retained address has exactly its P1/P2 pair
   with the same action. Reconstructing both preimages proves the accepted and
   default domains for all 32-bit inputs, not just sampled addresses. The three
   units contain 640 qualified switches. Table/relocation storage shrinks, but
   their main `.text` changes only -0.15%, -0.38%, -0.16%. No game build or
   runtime benefit is claimed. This remains an authoring-only size lead.
2. **Cold memory fallbacks:** `prepare-cold-memory.py` moves exact register
   release/read-or-write/reacquire bodies into cold, non-inline helpers.
   Fast access checks and all actual memory APIs remain. Total text changes
   +0.66%, -0.17%, -0.89%; the compiler already avoids much of the suspected
   duplication. This does not justify a gameplay candidate. It is not enabled
   in CMake or any shipped product, and no runtime equivalence claim is made.
3. **Constant dispatch TLS initialization:** five declarations omit `constinit`
   although their definitions are constant-initialized. Original objects carry
   five weak dynamic-initializer dependencies and repeated tests/calls. The
   finished control executable defines none of those initializer functions.
   Adding `constinit` to those declarations removes the five dependencies.
   Total text falls 2.52%, 1.31%, 1.77%. No guest instruction or runtime field
   is removed; the compiler gains initialization knowledge it was missing.

For trial 3, forcing the additional `local-exec` TLS model grows text by 4.52%,
3.58%, 1.59% relative to control. The gameplay candidate therefore keeps the
original TLS model. These code-size figures are not FPS predictions.

Evidence: `runs/linux-pc-switch-trial-object-sizes.json`,
`runs/linux-cold-memory-trial-object-sizes.json`,
`runs/linux-constinit-dispatch-trial-object-sizes.json`,
`runs/linux-constinit-localexec-trial-object-sizes.json`, matching compiler logs
and each isolated preparation manifest under `build-linux/generated/`.

## Why this is valid initialization information

The five variables retain their original type, symbol, thread-local lifetime,
initializer and mutable value. The preparation pins the original defining
translation unit's full SHA and checks the same exact initializers in the
effective Linux dispatch source. A separate compile-only object applies
`constinit` to the real definitions against the current SDK, so a change that
requires dynamic initialization cannot silently pass the premise. That proof
object is never linked into the game. Every transformed unit is recoverable
byte-for-byte by reversing the five declaration substitutions. The actual
dispatch defining translation unit is also copied and given `constinit` on
the same five definitions, as required by the C++ language rule. Its original
logic and include paths stay intact; declarations alone would be insufficient.

This use is explicitly described in the C++ committee's
[P1143R2 proposal](https://www.open-std.org/JTC1/SC22/WG21/docs/papers/2019/p1143r2.html).
It avoids initialization guards for already initialized thread-local variables.
The broader optimization principle matches [XenonRecomp's documented approach](https://github.com/hedge-dev/XenonRecomp/blob/main/README.md#optimizations)
of giving the compiler proven runtime/ABI facts. It does not copy PPC ABI
assumptions, disable SH-4 exception paths, or promise Unleashed's performance.

## Bounded Linux candidate

`SARECOMP_LINUX_CONSTINIT_DISPATCH` defaults OFF and replaces only the 41 common
units already selected by the execution profile, in their original archive
positions. It rejects combination with other unaccepted AOT experiments. The
installed product, Windows build and diagnostics patches remain unchanged.

All four fresh control/candidate runs completed their 5..25 frame windows,
with the expected host-deadline stop and no forced termination. Tests used
the hidden/muted Linux VM, isolated input and save roots, Original timing,
native gameplay math, diagnostics OFF and the same renderer cache policy.
The measured callbacks retain PAL 50 Hz, release slots 2 and logical delta 2;
native call counters advance without original math fallbacks.

| Stage | Build | Game updates | Execution CPU ms/update | Process CPU ms/update | Images/s |
| --- | --- | ---: | ---: | ---: | ---: |
| Gamma Emerald Coast | Control | 68 | 244.133 | 986.694 | 0.5675 |
| Gamma Emerald Coast | Constinit | 68 | 243.401 | 982.606 | 0.5660 |
| Sonic Windy Valley | Control | 66 | 254.297 | 994.234 | 0.5943 |
| Sonic Windy Valley | Constinit | 64 | 247.577 | 987.170 | 0.6194 |

Execution CPU per game update falls only 0.30% in Gamma and 2.64% in Windy.
Whole-process CPU per update falls 0.41% and 0.71%. Windy's apparent 4.23%
image-rate improvement includes fewer game updates in the measurement window;
it must not be reported as an equivalent optimization. Gamma image throughput
is slightly worse (-0.27%). These small single-pair differences do not establish
a useful global gain. The TCG/software-rendered VM is not a Deck FPS predictor.
The 20–25 ms/frame target remains open.

The experiment remains **OFF**, with no broader conversion or release patch.
Across the 41 candidate units, total executable text fell from 51,100,269 to
50,057,325 bytes (2.04%). The actual dispatcher executable/data/TLS sections
were byte-identical; its `constinit` definition changes supply the language
contract without changing that translation unit's machine code. This covers
the five named dispatch variables, not every thread-local in the program.

Candidate SHA-256:
`2cf2e512e3b7b86036a03159e12505c9ad25eb94d73ccdafbf80063e6890adde`
(1,677,624,256 bytes). Evidence is in
`runs/linux-constinit-matched-stages.json`,
`runs/linux-constinit-candidate-code-audit.json`,
`runs/linux-constinit-preparation-check.json` and matching build/session logs.
Every copied unit reverses to the exact original bytes; all ten missing or
duplicate declaration mutations were rejected by preparation.

Disabling the option re-archives and links the retained original objects;
no full AOT compile is necessary. The stripped local test executable has been
restored to the exact control SHA and 1,678,965,680-byte size, verified in
`runs/linux-constinit-restore-local.json`. The VM test executable was restored
from its authenticated installed control after verifying no process used it;
its matching SHA/size is recorded in `runs/linux-constinit-restore-vm.json`. Accepted installers,
diagnostics ON/OFF patches, Windows output and personal saves are unchanged.

## Local artifact cleanup

The redundant `out/write-observer-experiment/game` control copy was removed
after verifying its exact control SHA and output-directory containment.
The authoritative identical copy remains at `out/internal-diagnostics-v1/linux/game`;
the VM control and accepted installers were untouched. This removes only a
reproducible experimental binary, not source/object evidence or save data.
