# Prepared RAM read experiment

This development experiment is **off by default**. It reuses a translated
address across the preflight and load of an ordinary MOV.L instruction.
It does not replace the retained r354 AOT archive or alter Katana itself.
The matched comparison below does **not** establish a useful performance
improvement. The normal build was restored to OFF; this is retained as a
bounded development fixture, not a delivered performance enhancement.

## Scope and original semantics

The two source units from `docs/performance-ram-read-followup.md` have the
same SHA-256 in the working product and the protected r354 source snapshot.
Their complete generated-artifact manifest and individual payload hashes
are checked before preparing separate build copies. The transformation
admits 154 and 115 ordinary GPR reads respectively (269 source envelopes,
not 269 newly discovered game functions). There are 24 exported runtime
entries in the two complete compilation units.

Admission requires the exact original instruction envelope, matching SH-4
MOV.L opcode/operands, two-cycle attempt, origin, catch and following PC.
Delay-slot fault ownership, FPU/paired loads, postincrements and extra
intervening work do not match. Nine mutated-envelope checks reject these
changes. A changed source hash aborts preparation before writing output.

Preparation translates and validates the address without reading memory.
The instruction-local token retains the guest and translated addresses.
At the original load point, consumption checks the current address and
calls the pinned SDK guarded reader, retaining live generation, alignment,
bounds, actual late load and both access-counter increments. A miss invokes
the **unchanged original generated reader**, with its original allow flag.
The miss/flush/reload path can therefore change the source address before
the real read. Exception state, retirement, MMIO boundaries and safepoints
remain in their original positions. There is no cross-instruction token,
private SDK-field access, early speculative load or cached RAM value.

This removes a second architectural translation on admitted successful
reads. It deliberately retains the SDK's second guard/range validation.

## Verification and fixture findings

`sonic_prepared_read_tests` passes 41 differential cases against the copied
original translator/read lambda and ordinary instruction envelope. Checks
cover RAM aliases, mode/MMU, bounds/alignment, late read, changed address
after a missed preflight, observer order, exact access accounting, stale
guards, memory-owner moves, MMIO success/rejection, register state, fault
provenance, attempted/retired instructions and guest cycles. These are
component tests, not a full-game equivalence proof.

Two fixture assumptions were corrected without changing the product helper:
MMUCR alone does not install/activate a RuntimeAddressSpace, and the pinned
MMIO boundary epoch begins at 1. The harness explicitly installs and selects
the MMU mode, compares address-space snapshots and also retains the bare-CPU
fallback case. One successful MMIO read advances the boundary epoch to 2.

`tools/test-prepare-ram-read.py` checks the real source qualification and
rejects missing, duplicate and retained-original link owners. The actual
map audit demands exactly one public definition of each selected runtime
entry in `sonic_ram_reads`; nested lambda symbols containing an entry name
are not counted as extra definitions. No selected original archive member
may also appear. The ordinary native closure audit additionally admits the
named experiment archive only when this build mode is active.

## Reversible build comparison

`SARECOMP_RAM_READ_EXPERIMENT` has three CMake values:

- `OFF`: original selected archive members; normal default.
- `CONTROL`: byte-identical source copies, compiled with the experiment's
  exact Release flags and toolchain.
- `PREPARED`: the same two copies with the qualified substitutions.

Both comparison variants use `/O2 /Ob2 /fp:strict /clang:-fno-lto` and
retained Ninja `-j2`. This controls for recompilation/compiler-flag effects;
comparing only against an older frozen binary would not isolate the change.
Other AOT members, installed assets and game timing are retained. A build
preparation report records source/output/helper hashes and every edited
instruction. Switching back to OFF requires only the existing link and
closure checks, not AOT regeneration.

The gameplay comparison used the existing hidden, muted 60-second
Emerald Coast scenario with copied saves, forward input profile 3, D3D11,
1280x720, 100% rendering, VSync off and 144 output target. Execution-thread
CPU per observed title boundary is the primary metric; the first ten
gameplay seconds are excluded. No physical input, visible window or full
level matrix is part of this experiment.

## Measured result and decision

Both runs completed with the intended stop, no forced termination and no
recorded crash or contract failure. Both link maps contain the 24 required
public entry definitions in the selected experiment archive and no selected
original member. The native closure audit also passes in both modes.

| Metric | CONTROL | PREPARED |
| --- | ---: | ---: |
| Execution-thread CPU ms / title boundary | 73.238 | 72.846 |
| Raw thread cycles / title boundary | 322,221,218 | 321,774,319 |
| New draws / second | 13.408 | 13.420 |
| Presentations / second | 143.920 | 143.832 |
| Aligned process CPU ms / title boundary | 79.535 | 85.093 |

Sources: `runs/ram-read-control-ec-01/result.json` and
`runs/ram-read-prepared-ec-01/result.json`. CONTROL executable SHA-256:
`a0c6e5175a5114988ca53926231a5d9125bc2174484e0ad841061427e29b4b4c`.
PREPARED executable SHA-256:
`d68a21e1f1a55cb81e2117f6a689a2f76738fd947da30ce8a602a88e62fa0185`.
Each run used active video 50 Hz, release 2, logical delta 2 throughout.

The roughly 0.53% thread-CPU difference and 0.14% raw-cycle difference are
too small for a benefit claim from one pair. Whole-process CPU moved in the
opposite direction. Scene progress and ambient work are not instruction-
identical controls. No broad speedup, low-end target or 60-SIM result follows
from this experiment. The historical native duplication evidence in the
follow-up audit is not represented as a new disassembly of these binaries.

**Decision:** do not expand this optimization to more AOT units or enable it
in the normal product on this evidence. No extra level matrix or repeated
gameplay probes were added to pursue a marginal result. The next useful
performance step is profiling larger execution-thread costs. The semantic
oracle and fail-closed preparation remain available for a later justified
experiment.

Restoration log `.local/menu-preview/build-ram-read-off.log` passes the
normal native closure audit. `out/experimental/game.exe` is back to OFF,
1,910,005,760 bytes, SHA-256
`6371fdaa033e0561f13e51c7683d196b057c4b10888e48b010b1757265ea6210`.
Only the link/audit was rebuilt for restoration; no frozen AOT recompilation.
All game/compile processes from this comparison have exited.
