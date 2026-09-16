# Integer register retention at FPU boundaries

This is an isolated, disabled-by-default experiment. It changes no floating
point arithmetic, guest instruction, clock, exception policy, or save data.
No new installer or performance patch is qualified by the component test.

The reviewed generated code releases its entire selected integer/scalar
register cache before individual FPU helpers, then reloads it. The earlier
hardware-FPU-region experiment replaced arithmetic inside existing epochs;
it did not remove these register transfers. This experiment addresses those
transfers at each eligible instruction, including isolated instructions mixed
with integer and memory work. It does not borrow a proof across instructions.

## Admission and source contract

Only `fpu_binary`, `fpu_square_root`, and `fpu_reciprocal_square_root` envelopes
are transformed. Admission requires an already owned register cache, FD=0,
PR=0, DN=1, all guest FPU exception enables clear, and rounding mode 0 or 1.
These helpers and their effective Linux fast/fallback implementations write
only FR/FPSCR under that admission. They neither access integer registers nor
invoke callbacks. DN=1 is necessary: denormals with DN=0 can raise unmaskable
Cause.E even when all ordinary exception enables are zero.

On admission failure the original release/helper/reload path remains. The
original opcode legality checks, delay-slot owner, precise arithmetic and
FPSCR effects, instruction/cycle accounting, host rounding behavior, and
exception paths remain byte-for-byte. Compare/FPUL helpers are excluded.
If a preceding boundary released the cache, its original reload still occurs.

The preparation authenticates the complete retained source generation, each
unit's size/hash, the effective FPU implementation and the register-class
header. It checks complete envelope shapes and reverses its two substitutions
back to the original body. Selected source inputs are never modified.
The pilot contains 2,818 sites in 41 common profile-selected units: 2,790 binary,
9 square-root, and 19 reciprocal-square-root sites. These are instruction
sites, not newly recovered functions. The CMake option and runtime switch are
both off by default; `SARECOMP_FPU_REGISTER_CACHE=1` enables the prepared path.

## Component evidence

The native Linux test passed in the existing VM:

```
SONIC_FPU_REGISTER_CACHE_OK cases=14608 retained=11888 fallback=2736 state=exact exceptions=ordered callbacks=published
```

The reference and candidate use the same unchanged FPU implementation. Cases
cover both legal rounding modes, zeros, denormals, infinities, NaNs, extreme
and random values, delay slots, disabled FPU, invalid precision/rounding,
enabled exceptions, unmaskable exceptions, already released caches and dirty
GPR/scalar values. Callback cases publish the cache, change R15/PR/DN, then
execute a denormal operation; the next instruction must recheck admission.
Full CPU state, exception state, instruction/cycle counts, RAM, memory-access
counters and host FPU state are compared at the observable boundary.

The fixture uses a full register mask and a separately expressed original
envelope. It is not an execution test of every generated function or of the
runtime environment switch. Gameplay comparisons provide that integration
check. All 41 prepared outputs matched their reports; three added malformed
envelopes (extra callback, wrong exception helper, changed completion) failed
the preparation grammar. Read-only independent review found no correctness
defect, with those component-test limits explicitly retained.

Local evidence: `runs/fpu-register-cache-component-20260916.log` and
`build-linux/generated/fpu-register-cache/preparation.json`.

## Alternative considered

Localizing AOT exit metadata cannot safely synchronize only at integer
register release/reload. Preflight error reporting precedes some releases;
runtime tail returns can omit reload; write observers can run without either;
immutable-write exits replace the final metadata. That design is not applied.
The narrower FPU boundary has a reviewed callback-free contract instead.

## Gameplay qualification

The hidden Linux Gamma Emerald Coast comparison did not establish a gain.
Both runs completed the 5..25 image window with the expected host-deadline
stop, isolated saves, Original PAL timing, native gameplay math, and diagnostics
off. No forced termination or crash occurred. QEMU TCG/software-rendered VM
rates are not Steam Deck rates.

| Variant | New images | Game updates | Execution CPU ms/update | New images/s |
| --- | ---: | ---: | ---: | ---: |
| Pre-22:00 D2 control | 20 | 67 | 279.25472 | 0.38100 |
| Register-retention candidate | 20 | 68 | 282.31870 | 0.37754 |

The candidate costs 1.10% more execution CPU per update and produces 0.91%
fewer images/s in this pair. Its extra update means the complete workloads
are not identical. This is not a reproducible regression percentage for Deck,
but it provides no basis to retain or expand the experiment. Keep it OFF.
The earlier lack of a reproducible positive net gain since 15 September
22:00 remains the reporting baseline. No installer or patch is produced.

Evidence: `runs/fpu-register-cache-gamma-pre22-20260916.json` and
`runs/fpu-register-cache-gamma-retained-20260916.json`, with matching full logs.
The candidate SHA-256 is
`f991a9860a9ee2c0cca0a94d8b777d294cfa64cbf52f8291c018cc21796bf4f3`;
the D2 control is
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.

The retained Linux CMake cache now has `SARECOMP_LINUX_FPU_REGISTER_CACHE=OFF`.
Restoration reused the original objects: only the archive and executable were
relinked, with no AOT compilation. Host `out/fpu-register-cache-experiment/game`
and VM `/home/sonic/preloaded-v1/game` both returned byte-for-byte to the prior
prepared-transfer control `6344e07c5c9090c15f163f9a4ba6b303d91499d4baedc627460936d524bcd8bb`.
The control's prepared-transfer experiment remains separate from this rejected
register change. Installed products, installer archives, personal saves and
the baseline were not replaced. See `runs/sadx-reference-restore-build-20260916.log`.
