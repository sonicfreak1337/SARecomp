# Active execution chains and larger optimization units

The previous Windows `--profile-stacks` mode intentionally selected instruction
pointers outside `game.exe`. Its bounded external waits cannot identify the
inclusive owners of actual AOT work. The existing mode remains available;
`--profile-active-stacks` selects the main executable instead, collecting at
most 256 chains, up to 64 frames deep, with 90 ms minimum spacing. Sampling is
diagnostic and never a throughput comparison. Suspension ownership, PID/TID
validation and guaranteed resume remain in the same collector.

The sampler self-test passes, including active-executable unwinding, rejection
of foreign process/thread pairs and preservation of an existing suspension.
The hidden/muted Gamma run `runs/active-chain-gamma-20260916` completed:

- Executable SHA-256 `5743f31f874ac01139a9d9f6bbbee7adfca4281cd970614b08218363d7681f62`.
- 1,936 IP samples; 1,316 in the game; 256 active chains.
- 30,010.1 ms sampling; 497.59 ms total suspension; maximum 3.4789 ms; no errors.
- SHA/PE-qualified resolution uses `build-performance/game-native-port.map`.
  Folded aliases and nearest symbols do not establish exclusive ownership.

The common task traversal occurs in most chains because it calls almost all
gameplay work. Its inclusive percentage is not removable overhead. The two
large owners `8C0CBD40` and `8C073018` occur in 25 and 17 chains (38 unique), but
have dozens of callees, indirect callbacks and object-dependent memory. Neither
is a justified closed native kernel based on this profile alone.

## Native math admission: rejected explanation

Generated direct calls already consult `native_chainable_entry`, which excludes
the reviewed native math entries. The suspected direct-call bypass is absent;
no bridge was added. The old original-path counters do omit an early inactive
scene return, so zero original calls alone cannot prove that path never ran.

A temporary bounded gate trace was built for Linux and removed after checking
Gamma Original timing, frame window 5–25. It recorded **zero inactive gameplay
math admissions**. The run completed with the expected deadline and no forced
termination. Evidence: `runs/math-scope-gamma-control-20260916.json`; diagnostic
executable SHA-256 `ef3fb0d82a3d55d8ba48e8017dc34caf2f5f312c81508b8b6744c3943396f533`.
This does not establish behavior outside that measured window. It does not
justify broadening scene eligibility or claim any performance gain.

## Relevant differences from other projects

[N64Recomp's memory contract](https://github.com/N64Recomp/N64Recomp/blob/ffb39cdad1da5de07eaaa48bd1db4a89a7986771/include/recomp.h#L95)
uses direct accesses to its known RAM representation. Its replaceable-function
rules also deliberately restrict interprocedural optimization. Blanket LTO is
therefore not a universal explanation for successful recompiled ports.

[XenonRecomp's generator](https://github.com/hedge-dev/XenonRecomp/blob/ddd128bcca99fe8bfbb99bea583c972351fa6ace/XenonRecomp/recompiler.cpp#L269)
and [Unleashed's configuration](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecompLib/config/SWA.toml#L7)
make a narrower guest ABI practical. Katana already caches registers locally;
the next step must remove more than another isolated helper call. Their guest
exception assumptions cannot silently replace this port's contracts.

The bounded pilot combines complete, authenticated integer/PR stack
sequences while retaining actual stack stores and every interior resume. It
removes repeated instruction wrappers inside one prevalidated operation;
it does not disable functional code protection, callbacks or memory faults.
Its initial game comparison was marginal, and review required an additional
scheduler admission check. The corrected component passes, but the experiment
stays OFF. See the stack-frame report for the exact limits.

Inspection also confirms ordinary nonfaulting integer arithmetic already uses
grouped accounting: for example, five instructions at `8C073026..8C07302E`
share one attempted/retired/cycle update. Simply grouping these again is not
a new optimization. The remaining architectural candidates must cover whole
memory-bearing regions or private call boundaries, with safepoints preserved.
