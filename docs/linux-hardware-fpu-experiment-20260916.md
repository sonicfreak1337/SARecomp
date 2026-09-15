# Scalar hardware-FPU experiment

**Rejected for delivery; OFF restored.** Direct scalar SSE arithmetic is
correct for the admitted subset, but preserving host FP state on every
operation costs more than the current integer arithmetic. The native kernel
comparison is substantially slower; one Linux Gamma run provides no game gain.
No patch, installer, accepted Windows binary or personal save is changed.

## Motivation and exact scope

The [read-group experiment](linux-read-group-experiment-20260915.md) did not
reduce global execution cost usefully. The current runtime computes ordinary
single-precision sums, products and quotients with integer bit arithmetic to
retain exact results and exception flags without repeated MXCSR changes.
This distinct experiment checks whether hardware arithmetic is cheaper.

`prepare-hardware-fpu.py` derives its input from the same SHA-authenticated
SDK recipe as the accepted runtime. The prepared control source SHA-256 is
`1b5ced39e89c7a2ca2b223218538a8dcde933d2374234e2dca4ce6f212ef31eb`, matching
the current Linux source. Two reversible substitutions add a helper and
attempt it before the unchanged existing integer fast path. The complete
original exceptional paths, ABI, instruction admission and epoch TLS remain.
Only the one runtime object is replaced at its existing archive position.
Existing inlined inverse/title arithmetic is not rewritten.

`SARECOMP_LINUX_HARDWARE_FPU` defaults OFF. Its helper is reached only under
the original nontrapping guard: single precision, no enables, FD clear,
valid register indices and no pending trap. NaN, infinity, denormal inputs,
division by zero and exceptional/boundary results retain the previous path.
Accepted nonzero results have exponent 2 through 252; zero is admitted only
without exceptional host flags. This deliberately stays inside the original
integer helpers' domain, including their host-state behavior inside epochs.

Each trial saves MXCSR, applies the guest rounding mode with exceptions masked
and DAZ/FTZ disabled, executes one scalar add/subtract/multiply/divide, reads
status and restores the exact previous MXCSR before publishing any result.
Ordered volatile assembly makes those boundaries explicit. Only normal
inexact status is translated into the existing FPSCR update. There is no
FMA, reassociation, fast-math flag, altered timestep or silent loss of flags.

## Linux differential verification

The existing retained-runtime oracle runs against the new implementation:

```text
SONIC_FPU_RUNTIME_TEST_PASS cases=131896 traps=28662 op0_fast=6229 op0_fallback=26745 op1_fast=2098 op1_fallback=30876 op2_fast=5374 op2_fallback=27600 op3_fast=2332 op3_fallback=30642 reference=retained-renamed both_overloads=1 host_state=exact
```

This checks both actual ABI overloads, guest register/state snapshots, exception
provenance, callbacks, counters, RAM and host FP state. It covers special and
boundary bit patterns, rounding/DN/PR/enables, all register pairs, out-of-range
encodings, dependent arithmetic and nested/mismatched rounding epochs.
These fast counts cover the combined hardware/integer fast path, not solely
the new hardware branch. Actual Linux disassembly contains the scalar SSE
instructions and explicit status save/read/restore, followed by the old fallback.

## Native CPU kernel comparison

TCG emulates the CPU instructions too, so a second standalone component runs
directly on the Windows host CPU without launching the Windows game. It uses
the authenticated `sonic_fpu_body.hpp` integer helpers and the same new SSE
helper, verifies that the hardware branch itself accepts **16,384** ordinary
input/rounding combinations, and compares exact output bits, causes and MXCSR.
All cases pass. Every timed pair also has an equal result fingerprint.

The corpus has 1,024 deterministic, finite ordinary operand pairs. Each timed
run performs 262,144 calls through a volatile function pointer. Four pairs
per operation alternate order and rounding. This isolates arithmetic plus
admission/status overhead; it is neither a whole-runtime nor a game benchmark.

| Operation | Integer milliseconds, observed range | Hardware milliseconds, observed range |
| --- | ---: | ---: |
| Add | 1.198–1.698 | 3.942–3.978 |
| Subtract | 1.197–1.335 | 3.938–3.958 |
| Multiply | 0.921–1.016 | 3.934–3.945 |
| Divide | 1.026–1.155 | 3.940–3.971 |

Hardware is slower in every pair. Replacing the integer arithmetic one
operation at a time is the wrong direction on this CPU. These are not Deck
measurements, and their ratios must not be reported as game regressions.

The standalone build uses the retained clang-cl compiler with `/O2`,
`/std:c++20`, `/EHsc`, `/fp:strict`, `/MD`, the generated FPU-body include,
the baseline SDK include/generated-include directories and `src`. It has no
game or installer dependency and does not relink the accepted Windows product.

## Bounded Linux gameplay and restoration

Gamma Emerald Coast ran hidden/muted in the existing Ubuntu / QEMU TCG VM
under Xvfb, with Original PAL50 / release2 / delta2, native gameplay math,
diagnostics OFF, 800x500 / 16:10 / 50% render scale and isolated user data.
It reached the expected HostDeadline normally, without a forced stop or fault.
Frames 5 through 25 contain 20 new images and 68 actual game updates.

| Metric | Prior matching D2 control | Hardware candidate |
| --- | ---: | ---: |
| Execution CPU ms/image | 838.583 | 868.480 |
| Execution CPU ms/game update | 246.642 | 255.435 |
| Process CPU ms/image | 3330.890 | 3711.941 |
| New images/s | 0.5563 | 0.3706 |

The host was in use, and the small native kernel was compiled during this
game run. Late wall-frame times rise sharply. This is not a clean causal
throughput comparison, and the old control is not a contemporaneous return
run. It establishes successful execution and no evidence of a speedup; the
native kernel result already rejects the proposed per-operation mechanism.
No further Windy run was used to chase a gain from the slower mechanism.

The runtime switch was disabled and the original object reused. The final
link/strip restores the exact full D2 executable hash:

| Binary | SHA-256 |
| --- | --- |
| Candidate | `d6422b6a9f8c7821db2ed794aafe424d357be950601e37aa85a1d54aa0d30725` |
| Control / restored | `d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b` |

All hardware-FPU, grouped-read and preloaded-read switches are OFF. Build
metadata was snapshotted before every retained Ninja build. Local evidence:

- `runs/test-linux-hardware-fpu.log`: real Linux differential result.
- `runs/hardware-fpu-kernel-native.txt`: hardware-branch checks and timings.
- `runs/linux-hardware-fpu/gamma.json`: complete gameplay samples and identity.
- `build-linux/generated/hardware-fpu/provenance.json`: source/helper identities.
- `runs/build-linux-hardware-fpu-*.log`: component, incremental game and return builds.
- `out/hardware-fpu-experiment/`: isolated candidate and byte-identical return.

The 20–25 ms/frame target remains unmet. A future arithmetic-region proposal
would need to amortize status work across sufficiently large, proven regions,
not repeat this per-operation approach. A read-only inventory in the 41 common
profile units finds only 148 immediately consecutive arithmetic groups / 435
operations, mostly pairs; its existence alone is not a useful global cost case.
No region rewrite or new performance promise follows from this experiment.
