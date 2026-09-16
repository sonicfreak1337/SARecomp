# Shared registers across private AOT procedures

This is an executable architecture prototype, not a game performance patch.
No game target includes the new carrier. The experimental game remains B04B;
installed builds, saves, timing and r354 are unchanged.

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

## Remaining integration work

Before any gameplay measurement, authenticate a connected set of actual
procedures, classify all direct and indirect CPU/register consumers, preserve
public wrappers, and select private calls only between admitted bodies. Calls
to original bodies and native hooks must retain explicit publication. Verify
the transformed bodies against their originals, especially differing masks,
early exits and exceptions. Only a successful game comparison can justify
expansion. The large measured gain and 20–25 ms/frame goal remain open.
