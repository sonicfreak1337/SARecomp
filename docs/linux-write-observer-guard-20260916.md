# Generation-bound write observer permission

This is a separate, default-OFF Linux experiment. It does not change the
delivered internal diagnostics patches or the accepted Windows/Linux product.

## Finding

The original generated code first asks whether the current write observer
allows prevalidated linear writes, then resolves the address through its
captured `DirectLinearMemoryGuard`. Its scalar write helper repeats this
observer query before another guarded resolution. A fresh Linux execution
profile attributes 1.68% self samples to the observer permission getter; that
sample share is not a prediction of a large end-to-end gain.

The selected 41 common AOT units contain 735 functions with 1,782 such query
sites. The experiment captures the permission once with each function's
existing read guard. All original address resolution, privilege checks,
writeability checks, write observers, code-generation checks, pending cycles,
instruction/fault state, counters and fallbacks remain. No stage module is
recompiled and no instruction/update is skipped.

## Invariant

The new boolean is only a hint. A store can be admitted only when the exact
guard captured with it also accepts its address and width. Every observer,
mapping and lookup change advances Memory's direct generation. Thus a stale
true permission cannot admit a store; a stale false permission cannot suppress
an otherwise valid fast access because its guard is already invalid. Observer
callbacks still execute through the original Memory write implementation.

The preparation script authenticates the retained source manifest and each
member, checks every replaced query's immediate resolution boundary, and
verifies that reversing the two substitutions recovers the entire source.
It preserves original archive member positions. No pinned SDK, baseline,
installed executable, original generated source or personal save is edited.

## Component verification

The real Linux runtime passed 1,584 paired cases in the existing Ubuntu VM.
These cover no observer, a stable observer and a general observer, followed
by replacement/removal, trace, watchpoint, access sink, lookup mode, alias
window changes and batch-observer removal. Each runs with stale and refreshed
guards, all scalar widths, mirrors, misalignment and invalid addresses.

Admission, complete CPU/cycle/fault state, fault provenance, memory counters,
observer order/values and final backing bytes match the original path. Successful
accesses are repeated with the same value to verify unchanged-byte observer events.

```text
SONIC_WRITE_OBSERVER_GUARD_OK cases=1584 admission=exact writes=exact observers=exact faults=exact
```

## Gameplay measurement

Both candidate stages completed in the existing Ubuntu QEMU/TCG VM without a
crash, forced stop or invalid measurement window. All four control/candidate
runs used Original timing, native gameplay math, diagnostics OFF, the same
Vulkan/software graphics configuration (800x500, 16:10, 50% render scale),
isolated input/saves and new-image window 5..25. No compilation or sampling
profiler ran during their timed gameplay windows. They exited through the
expected host deadline (game exit 1, stop reason 2).

| Stage / build | Updates | Execution CPU ms/update | Process CPU ms/update | New images/s |
| --- | ---: | ---: | ---: | ---: |
| Gamma Emerald Coast / control | 68 | 238.993 | 972.800 | 0.5735 |
| Gamma Emerald Coast / candidate | 67 | 234.184 | 1179.785 | 0.4524 |
| Sonic Windy Valley / control | 67 | 257.527 | 985.391 | 0.5897 |
| Sonic Windy Valley / candidate | 68 | 247.506 | 1070.845 | 0.5145 |

Execution CPU per update fell 2.01% in Gamma and 3.89% in Windy, but whole-process
CPU per update and image throughput both worsened. One short window per stage
cannot establish why the other CPU work changed. Normalizing by game updates
also avoids attributing a different number of original PAL updates to faster
code. The component proof establishes behavioral equivalence within its tested
cases; it does not establish a useful end-to-end speedup.

**Rejected for release; default remains OFF.** VM throughput is not a Steam
Deck frame-rate estimate. No performance patch or installer is produced, and
the overall 20–25 ms/frame objective remains open. The diagnostics ON/OFF
patches are unchanged.

Candidate SHA-256:
`f2037483d04ca130ababaf33a479de498474c1d2747ad17e2997180d23c7e97d`
(1,678,775,904 bytes). The original control is
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.
The selected objects' combined `.text` decreased from 51,100,269 to 50,931,509
bytes (0.33%); this is not a substantial code-size reduction.

Evidence: `runs/linux-write-observer-matched-stages.json`,
`runs/linux-write-observer-component.txt`, source-authentication manifest under
`build-linux/generated/write-observer-guard/preparation.json`, and the matching
control/candidate session and build logs. After disabling the experiment, the
retained original objects were re-archived and linked without recompiling the
pack. The stripped local experiment executable and VM test executable were
both restored to the exact control SHA and 1,678,965,680-byte size. Verification
is in `runs/linux-write-observer-restore-identity.json` and
`runs/linux-write-observer-vm-restored.json`. No gameplay process was running
when the VM test executable was replaced. All other rejected AOT experiments
and PGO remain OFF; accepted installers and the Windows build were untouched.

## Separate code-size lead

A read-only inventory of these same 41 original units found 6,881 switches
on the unrelocated PC with 125,516 labels. Every switch's labels occur in
adjacent P1/P2 pairs selecting the same statement, differing only in bit
`0x20000000`. Canonicalizing that bit could halve source labels without
changing the switch's accepted addresses. No switch has been changed in this
experiment, and the compiler may already fold some of these cases. Machine
code size and gameplay measurements are required before claiming a benefit.
