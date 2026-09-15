# Linux RAM-read groups

The experiment remains **off**. Grouping ordinary reads did not establish a
useful global improvement: execution CPU per game update fell 2.32% in Gamma
Emerald Coast and rose 0.98% in Windy Valley. No patch or installer is produced
from this result. The user's 20–25 ms/frame goal is still open.

## Scope and contract

This is distinct from the preceding per-instruction
[preloaded-read experiment](linux-preloaded-read-experiment-20260915.md).
It shares admission and accounting across **1,215 groups / 2,644 instructions**
in the same 41 common AOT units. Group lengths are 2 (1,034 groups), 3 (155),
4 (19) and 5 (7). Stage modules and the existing inverse replacement are
excluded. `SARECOMP_LINUX_READ_GROUPS` defaults OFF and cannot be combined with
the per-load experiment. The original archive member order is retained.

The preparer verifies the generated manifest, source sizes/hashes and exact
ordinary MOV.L/scalar-FMOV envelopes using the preceding qualifier. Admission
is tied to the actual source occurrence, not merely its guest PC. Adjacent
instructions must be consecutive, share indentation and have only whitespace
or the exact next resume label between them. Calls, writes and other work
break the group. Every original instruction and resume label remains present.

The candidate stages reads and dependent register values in locals. It checks
privilege, translation, bounds, alignment and guard lifetime; scalar FMOV also
requires FD and SZ clear. No actual registers or counters change before the
whole group succeeds. The public unobserved-access accounting gate must admit
the group, including absence of access observers and MMIO tracking. Otherwise
the complete original sequence runs, preserving faults after partial progress,
callbacks and individual instruction accounting. Successful groups publish
their final provenance and exact original instruction/cycle counts.

Source authentication and these bounded checks do not prove whole-game
equivalence. This is an opt-in research fixture, not a replacement memory model.

## Verification

Native Linux component results:

```text
SONIC_READ_GROUP_OK cases=52 partial_faults=exact observers=ordered counters=exact resume=original
SONIC_PREPARED_READ_OK cases=93 oracle=original-envelope aliases=checked observers=ordered counters=exact faults=provenance
SONIC_READ_GROUP_QUALIFICATION_OK groups=43 instructions=101 original_fallback_exact=1 rejected_gaps=3 duplicate_pc_isolated=1 dependent_reads=1
```

The new differential executable compares the original sequence with grouped
execution, including dependent pointers, aliases, address wrap, partial faults,
privilege/MMU/FP modes, tracing, read and write-only watchpoints, MMIO tracking,
MMIO success/failure, flush-time state changes and resume inside a group.
Architectural state, provenance, exact counters, callback order and RAM are
compared. Extracting the shared fixture leaves the previous 93 cases passing.
The qualifier reverses its substitutions to recover the exact source and
rejects changed gaps and a duplicate-PC occurrence with a mutated envelope.

The 41 original/output payloads and helper hash match the preparation report.
Selected object size rises from 93,639,424 to 94,283,304 bytes (+0.69%). Native
symbols still show out-of-line group helpers; source grouping alone does not
mean all overhead has disappeared. No independent peer review is claimed.

## Hidden Linux gameplay comparison

Ubuntu 24.04 / QEMU TCG, two virtual CPUs, Xvfb and llvmpipe. Original PAL50,
release 2, logical delta 2, native gameplay math, diagnostics OFF and caches ON.
Both stages use 800x500 / 16:10 / 50% rendering, isolated saves, muted audio
and the same frame-indexed input fixture. The window is frames 5 through 25,
twenty newly rendered images. The controls are the matching D2 runs from the
preceding experiment. Each row contains 68 actual game updates.

| Stage / variant | Execution CPU ms/image | Process CPU ms/image | New images/s | Execution CPU ms/update |
| --- | ---: | ---: | ---: | ---: |
| Gamma, control | 838.583 | 3330.890 | 0.5563 | 246.642 |
| Gamma, grouped reads | 819.100 | 3347.395 | 0.5631 | 240.912 |
| Windy, control | 832.961 | 3227.088 | 0.6054 | 244.989 |
| Windy, grouped reads | 841.137 | 3278.559 | 0.5966 | 247.393 |

Both valid candidate runs reached the planned HostDeadline without forced
termination or a game fault. Harness exit 1 / stop reason 2 denotes that
expected stop. One earlier Gamma attempt omitted Xvfb and failed window
creation before gameplay; it is excluded, not counted as a gameplay sample.

These are single pairs without confidence intervals; equal update counts do
not establish identical trajectories. They provide no useful improvement
across the two stages. VM throughput is not Steam Deck FPS, and none of these
absolute times demonstrates the requested hardware frame budget.

## Restoration and evidence

Only the selected 41 units were compiled. Disabling the experiment reused the
original objects and required an archive operation and link. Ninja metadata
snapshots precede all retained builds. All read-experiment switches are OFF.
After stripping with `--strip-all --keep-section=.comment`, the restored
executable is byte-identical to the control, including metadata:

| Binary | SHA-256 |
| --- | --- |
| Control / restored | `d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b` |
| Grouped candidate | `9fb36852a03881833ec95bf70422ee0b6e31c1a48a675a84c3dc552279ed8a2c` |

The accepted Windows executable, r354 snapshot, installed VM control and
personal saves are unchanged. Local evidence, excluded from delivery:

- `runs/linux-read-groups/comparisons.json`: control/candidate summaries and samples.
- `build-linux/generated/read-groups/preparation.json`: authenticated scope.
- `runs/build-linux-read-groups*.log`: incremental build/restoration logs.
- `out/read-group-experiment/`: isolated candidate and restored binary.

## Follow-up

The comparison with other recompilation projects motivates reducing repeated
operation overhead, not changing guest timing. XenonRecomp's
[PPC memory helpers](https://github.com/hedge-dev/XenonRecomp/blob/ddd128bcca99fe8bfbb99bea583c972351fa6ace/XenonUtils/ppc_context.h)
use direct base-plus-address accesses; Unleashed's
[recompiler configuration](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecompLib/config/SWA.toml)
keeps selected CPU state local. These architectural choices do not by
themselves establish a safe equivalent transformation or speedup for this port.

A read-only inventory finds another 1,202 qualified MOV.L postincrement sites,
1,019 in multi-instruction groups. This is not implemented or evidence that
those sites are hot. The current result does not justify broadening the same
read-group approach without a stronger cost argument. Another distinct avenue
is arithmetic-only FPU regions that reduce repeated software arithmetic while
retaining exact rounding, exception flags and original fallback behavior.
