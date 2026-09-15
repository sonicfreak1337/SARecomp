# Linux preloaded RAM-read experiment

The experiment remains **off**. It does not establish a large, global game
performance improvement and is not included in a new patch or installer.
The retained Linux build was restored to the exact diagnostics-build executable.
The accepted Windows executable, personal saves and r354 snapshot are untouched.
The user's 20–25 ms/frame target remains open.

## What was measured

The earlier prepared-address experiment retained a second guarded read at the
instruction's original load point. This experiment instead performs the one
admitted RAM read at preflight and consumes its value in the same instruction.
Only the original noexcept CPU bookkeeping may intervene. There is no retained
value across a callback, write, instruction boundary or scheduler flush.

The final scope is 41 common 8C AOT units selected from the Linux execution
profile: **5,838 ordinary MOV.L and 3,875 scalar FMOV envelopes**, 9,713 total.
These are instruction sites, not newly discovered functions. Stage modules and
the existing inverse-arithmetic replacement are excluded. Sources are copied
into a separate generated directory and substituted at their original archive
positions; the original generated files are not edited.

Admission checks the complete generated-artifact manifest, selected source size
and SHA-256, exact instruction/opcode/operands, fault origin, cycle accounting,
catch, following PC and the successful read/translation helper bodies. The
original direct-read allowance must be true. A source or helper mismatch fails
preparation. Both supported original fallback shapes remain untouched.

Scalar FMOV is admitted only with FD clear and SZ clear. Paired loads, disabled
FPU and illegal-instruction checks retain their original paths and precedence.
An unsuccessful preflight retains the original register flush, pending-cycle
flush, address reload and memory read. A flush that changes FP mode therefore
continues through the original current-state checks. Delay slots, incrementing
loads, stores and envelopes containing extra work are excluded.

Both options default off and are off in the retained build:

- `SARECOMP_LINUX_PRELOADED_READS`
- `SARECOMP_LINUX_PRELOADED_FPU_READS` (requires the first option)

## Verification

The native Linux differential executable reports:

```text
SONIC_PREPARED_READ_OK cases=93 oracle=original-envelope aliases=checked observers=ordered counters=exact faults=provenance
```

It compares original and candidate GPR/scalar-FMOV envelopes over RAM aliases,
MMU/privilege modes, alignment/bounds, fault provenance, observer order, MMIO,
register state, exact access/instruction/cycle accounting, guard lifetime,
paired reads and changes to the address or FP mode during a missed-preflight
flush. The candidate's successful read counters move to preflight exactly once;
that path cannot invoke an observer. The older prepared-address test retains
its artificial intervening-write/callback checks; such actions are outside the
preloaded envelope and are not claimed as supported.

`tools/test-prepare-ram-read.py` passes the existing nine envelope mutations,
changed-source rejection and link-owner checks. Added checks reject two changed
helper/allowance forms and three invalid FMOV envelopes (postincrement opcode,
wrong destination and delay-slot fault ownership). The pinned witness unit
admits 154 GPR and 209 scalar FMOV sites. All 41 original and prepared payload
hashes were checked against the final preparation report.

These component checks are not a proof of full-game equivalence. No independent
FPU review is claimed.

## Matched Linux gameplay

Tests were hidden and muted in the existing Ubuntu 24.04 VM, using QEMU TCG,
two virtual CPUs and llvmpipe. Each variant used isolated user data, the same
installed content, Original PAL timing, release slots 2, logical delta 2,
native gameplay math enabled and diagnostics off. Rendering was 800x500,
16:10, 50% render scale. The measured window spans relative frames 5 through
25: twenty newly drawn game images, excluding initial loading.

All seven valid runs reached their intended host deadline without forced
termination or a game failure. The harness represents this expected stop as
exit code 1 with HostDeadline; it is not a crash. An earlier run with the invalid
ID `windy-valley` stayed outside the intended stage, was explicitly stopped and
is excluded. The harness now accepts only reviewed scenario IDs, including
`sonic-windy-valley`.

| Scenario / variant | Execution CPU ms/image | Process CPU ms/image | New images/s | Game updates | Execution CPU ms/update |
| --- | ---: | ---: | ---: | ---: | ---: |
| Gamma Emerald Coast, control | 838.583 | 3330.890 | 0.5563 | 68 | 246.642 |
| Gamma, 10 GPR units | 812.109 | 3278.449 | 0.5740 | 68 | 238.856 |
| Gamma, 41 GPR units | 800.559 | 3287.916 | 0.5761 | 68 | 235.459 |
| Gamma, 41 GPR + FMOV units | 786.231 | 3217.892 | 0.5764 | 68 | 231.244 |
| Windy Valley, control | 832.961 | 3227.088 | 0.6054 | 68 | 244.989 |
| Windy, 41 GPR units | 821.691 | 3218.238 | 0.6078 | 67 | 245.281 |
| Windy, 41 GPR + FMOV units | 776.695 | 3164.960 | 0.6151 | 64 | 242.717 |

The final candidate reduces execution CPU per game update by **6.24% in Gamma**
and **0.93% in Windy**. Windy's raw 6.76% CPU/image reduction includes fewer
game updates in the same image window and must not be called a 6.76% execution
efficiency gain. Original's authored overload recovery intentionally changes
the number of updates per image; see `original-update-live-20260913.md`.
The benchmark now reports the game-tick delta and CPU cost per update alongside
image cost. This is a coarse work normalization, not identical instruction
traces: scene progress and ambient work can still differ.

The first 10-unit Gamma pair has identical sampled positions and cadence. The
expanded Gamma pair has small later position differences; Windy has differing
update counts. Thus the gameplay samples do not demonstrate bit-identical
trajectories. These are single comparisons without confidence intervals, and
the software-emulated VM's absolute throughput is **not Steam Deck FPS**.
Neither large hardware gains nor the target frame budget follow from them.

## Reproduction and restoration

Incremental builds retained the same strict-FP, O2 settings and original
archive order. Only selected experiment units were rebuilt. Disabling the
options required the existing archive operation and final link, without AOT
regeneration or a full compile. Ninja recovery snapshots precede every build.

| Executable | SHA-256 |
| --- | --- |
| Control / restored | `d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b` |
| 10 GPR units | `b84e75e9869005e7667a28d12e6670a4491a4bc081a8299df3ab9e2001889373` |
| 41 GPR units | `e1fde24666377c1349f74aa639b8e1b0c8de24aadc049ff962b640d762ff811a` |
| 41 GPR + FMOV units | `a1ce2bf7e653993e29f1c6ee07035e254628c82cb1116b7ce70ffb0bbdd22916` |

The restored file was stripped with `--strip-all --keep-section=.comment`,
matching the delivery's metadata policy. Its complete SHA-256 equals the
reference, not just its loaded code/data sections. Both build switches are
confirmed OFF; no gameplay process remains running.

Local evidence, excluded from distribution:

- `runs/linux-preloaded-reads/all-comparisons-full.json`: all seven summaries,
  samples, binary identities and normalized measurements.
- `build-linux/generated/preloaded-reads/preparation.json`: source/output/helper
  identities and all admitted sites.
- `runs/preloaded-read-selected-units.json`: selected profile units.
- `runs/build-linux-preloaded*.log`: incremental build and restoration logs.
- `out/preloaded-read-experiment/`: isolated experiment binaries and restored
  comparison file; none replaces an installer or accepted Windows build.

The experiment is retained as an opt-in research fixture. Further work should
target larger repeated execution costs rather than broaden these per-load
substitutions on this evidence. A read-only scan found 1,215 consecutive
side-effect-free read groups covering 2,644 sites in the same units; sharing
admission/accounting across a group is a separate, unimplemented hypothesis.
It requires its own semantics and performance evidence before use.
