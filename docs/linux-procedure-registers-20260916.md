# Shared registers across private AOT procedures

This started as an executable architecture prototype. A default-OFF Linux game
integration now prepares six audited procedures and eleven connected call sites.
Installed builds, saves, timing and r354 are unchanged. The integration still
requires measured gameplay evidence; the component microbenchmark is not a
game performance claim.

The installed SADX investigation motivates reducing administration across
complete native procedures. The current AOT ABI copies selected registers from
caller to CpuState, into the callee's private registerfile, back to CpuState,
and back into the caller. Existing nesting/depth guards do not share registers.

`src/sonic_procedure_registers.hpp` implements one explicit register carrier
per public CPU invocation. Prepared procedures can pass it by reference. It
holds all GPRs, T, PR, GBR, MACH, MACL and FPUL; FR/XF, SR, PC and instruction/
cycle state retain their existing CpuState ownership. The carrier has no global
or thread-local selection mechanism and no default-enabled runtime switch.

At a public boundary it publishes all held fields and releases ownership.
Normal continuation imports the resulting state explicitly. A newly entered
guest exception prevents reacquisition; no destructor writes stale registers
over the handler. A reentrant public call creates a separate carrier from the
published state. Host exceptions before a boundary publish the owned private
state during root unwinding; host exceptions after release leave CpuState alone.

This is not a drop-in replacement for NativeAotRegisterFile. Preparation must
rewrite every direct GPR/scalar access as well as local-register expressions,
and cannot let references survive public boundaries. Original memory/observer
preflights, PC/exception/depth/owner checks, pending dispatch selection, guest
stack writes and scheduler safepoints remain necessary. The prototype does not
change or qualify those contracts.

## Linux qualification

`sonic-linux-procedure-register-tests` is EXCLUDE_FROM_ALL. It passed **70,657**
cases in the hidden Linux VM:

- 5,120 comparisons against the original full-mask NativeAotRegisterFile ABI:
  recursion, normal callbacks, SR bank changes, FPU bank changes, an actual
  `raise_illegal_instruction`, host exceptions before/after publication,
  public reentrancy, unchanged AOT callees and an independent CPU context.
- One transfer proof: 32 nested private procedures perform one root import
  and no intermediate publication; explicit final release publishes once.
- 65,536 comparisons using the actual retained inner body of `8C055C8E`.
  Its R3/R4 cache mask is `0x18`; it also directly reads R5 and PR and writes
  R0. The fixture redirects those three raw accesses, keeps the real branch,
  return delay slot and accounting, and calls it three times from a differently
  masked caller with unpublished arguments. Both P1/P2 entry aliases are tested.
  Final registers, PC/provenance, exception state and counters match.

The real-body fixture is bound to the complete retained unit's manifest/SHA
and generated only in build-linux. It does not replace any game function.
It excludes the public BlockExit wrapper; this test does not establish that an
automatic whole-unit transformer or real dispatch integration is correct.

The synthetic five-procedure benchmark completed 200,000 roots with matching
state. Initial/final component binaries measured 97.429/48.597 ms and
107.820/78.141 ms for original/private calls respectively. Those timings vary
and use full-mask synthetic bodies, not gameplay. They establish neither a
global gain nor a Steam Deck percentage. No release decision follows from them.

Final component SHA-256:
`86da9a90b98280360fd374c35892db7bbb1a9441f0d86dd83f865acdd193de66`.
Evidence: `runs/procedure-register-witness-20260916.log` and
`runs/procedure-register-witness-build-final-20260916.log`. The first witness
compile exposed a missing test include; that include is corrected in the final
successful build. The original simpler component is recorded separately in
`runs/procedure-register-component-20260916.log`.

## Bounded game integration

`SARECOMP_LINUX_PROCEDURE_REGISTERS=ON` selects four incremental translation
units. Preparation authenticates the retained manifest, the exact original
body hashes and any preceding RAM-region preparation. It admits only these
reviewed complete procedures:

| Procedure | Role | Private call sites |
| --- | --- | ---: |
| 8C057B00 | Recursive hierarchy to matrix-buffer construction | 7 |
| 8C040784 | Object/motion hierarchy traversal | 1 |
| 8C040942 | Public entry into the traversal | 1 |
| 8C041A2E | Recursive animation hierarchy | 2 |
| 8C0417C8 | SRT mixer with live rotation callback | 0 |
| 8C055C8E | Partial-mask angle difference leaf | 0 |

The original public bodies of the two leaves remain intact, avoiding a larger
root register import for unrelated callers. Other public roots create an
explicit Bank. Internal calls receive the same Bank. Original depth, pending
selection, continuation PC, exception, memory-generation and scheduler checks
remain in place. Unknown callees and native hooks retain publication.

`Frame` separates local ownership from Bank validity. A private call suspends
local ownership without publishing. A failed admission followed by a public
fallback still publishes the Bank, even though local ownership was suspended.
There is no caller-local snapshot to flush over newer callee results. A root
publishes before its public BlockExit epilogue and during host unwinding.

All 121 raw GPR/scalar access expressions in the admitted bodies follow actual
Bank validity. Released FLOAT/FTRC windows and exception PR rollbacks therefore
continue to use CpuState directly. Two FPU comparison result imports deliberately
retain `cpu.t`: those helpers write T without first releasing the original local
registerfile. Rewriting these result reads to the Bank would lose the comparison.

The existing stable prevalidated-write observer contract explicitly prohibits
CPU/scheduler inspection or mutation. NativePort binds the immutable-write
tracker under that contract. General observers, MMIO, watchpoints, exceptions
and runtime callbacks keep their original publication/fallback paths. This is
not permission to remove observer behavior or generalize the six-body audit.

The updated Linux component passed **71,681** comparisons. Its real-body
fixture now uses the actual game preparer, rather than an independently authored
register rewrite. The additional 1,024 cases exercise released FPUL/GPR windows,
FPU Compare T imports, failed-private-admission publication, and exception PR
rollback. Evidence: `runs/private-procedures-component-20260916.log`.

The first integrated stripped executable is 1,680,630,752 bytes, SHA-256
`30746fa754b1fb5758b72573f5d840060374ae774234105bb65e0012a620abce`.
Incremental build evidence: `runs/private-procedures-build-20260916.log`.

## Scope boundary

The hidden Gamma pilot completed normally. Comparing against an older B04B run
initially suggested lower CPU cost (220.103 versus 257.737 ms/update). A fresh
identical B04B control instead measured **208.813 ms/update**. The private-ABI
candidate therefore costs **5.41% more CPU/update** in this pair, with 66 rather
than 67 updates. Image rate differs by only +1.02% in the software-rendered VM.
The much larger change between the two identical B04B binaries demonstrates
why the old observation cannot be attributed to the new code.

Both programs reach the requested 5..25 image window and expected host stop.
Workload differences prevent an isolated small-percentage claim. There is no
useful gain supporting expansion, so the private procedure switch is **OFF**.
It is excluded from the subsequently requested CPU update. Evidence:
`runs/private-procedures-comparison-20260916.json` and the two complete summaries.
No further Windy run of this rejected candidate is needed; the final patch's
qualified paths receive the installed Windy check instead.

The six-body integration is not a qualified global compiler transformation.
Additional owners would require their own access/helper audit or an independently
qualified generic boundary classifier. Only a useful game comparison can
justify expansion. The large measured gain and 20–25 ms/frame goal remain open.
