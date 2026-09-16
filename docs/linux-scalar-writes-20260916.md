# Qualified scalar RAM stores

This optional experiment combines the successful scalar AOT RAM-store path.
The retained path repeats writable-observer admission, address translation,
RAM resolution and immutable-range checks, reads the old value, writes the
new value and invokes the write observer. For an exactly identified native
observer and an untracked RAM range, that observer has no side effect.

The new path captures a generation-checked writable view at function entry,
checks the current immutable-range map once per store, then stores directly.
Writes remain immediate and preserve the original memory accounting. It does
not batch writes or change game instructions, clocks, register ownership,
fault provenance, post-instruction safepoints or executable-write exits.

## Admission and lifetime

Only the authenticated native-services constructor registers an observer pair.
Both original observers call the same immutable-write guard. An arbitrary
observer marked Stable is insufficient. A private, synchronous capture token
allows this pair to obtain a writable view after the ordinary Memory mapping,
access-sink, watchpoint and write-permission checks. Outside that scope, the
public writable-view API retains its previous rejection of observed memory.

Any mapping or observer change invalidates captured views. Code and read-only
image ranges, including RAM mirrors and newly added dynamic code, still use
the original store and invalidation path. Unaligned, translated, MMIO, watched
and batched accesses retain their original fallbacks. Diagnostic mode and the
private environment switch disable capture. Constructor/destructor binding
is bounded, thread-local, and fails closed when a slot is unavailable.

The transformation verifies retained source identities and is mechanically
reversible. It changes only functions with scalar write helpers. Preparation
for the 41 common code units found 1,047 helpers in 735 functions; 159 other
functions incur no new view construction. The original SDK, guest sources
and baseline are not modified.

## Build and private controls

`SARECOMP_LINUX_SCALAR_WRITES` defaults OFF. `SARECOMP_LINUX_SCALAR_WRITE_SCOPE`
defaults PROFILE (41 existing common units); ALL can cover the remaining
retained guest units without changing archive member order. The existing
inverse-arithmetic replacement is excluded. ALL has not been qualified as a
game build and must not be enabled merely because source preparation succeeds.

`SARECOMP_SCALAR_WRITES=0` selects the original helpers. The developer probe
exposes `--scalar-writes original|fused`, defaulting to original. This permits
a same-executable comparison without changing other performance controls.

## Verification so far

The Linux component executable passes 792 paired cases. CPU and cycle state,
RAM contents, memory counters, observer events, fault provenance and immutable
write results match. Cases cover unchanged writes, boundary/unaligned accesses,
observer replacement/removal, read/write watchpoints, access tracing, lookup
mode changes and runtime executable-range addition/removal. There were 116
actual fast stores in the paired cases. Additional checks exercise all eight
native segment/backing mirrors over 16 MiB RAM, private admission, diagnostic
capture rejection, and retirement of an old writable capability.

The 41-unit transformation also passed complete source restoration and
per-function-scope checks. The incremental game build compiled 41 guest units
and the two runtime adaptations; it did not rebuild the complete AOT pack.

## Gamma gameplay comparison and disposition

The stripped candidate is SHA-256
`77f6c5ea60d005a6e7118b7901cb48ddff59966d01716fdc488738846adc6882`,
1,677,835,072 bytes. It ran hidden/muted in the Linux VM, 800x500 / 16:10,
50% render scale, Original PAL 50/2/2, native math, diagnostics and provider
telemetry off. Prepared transfers were also disabled in both runs. Fused
ran first, then Original inside the same executable. Both completed the
5..25 image window and their intended HostDeadline shutdown.

| Path | Game updates | Execution CPU ms/update | Process CPU ms/update | New images/s |
| --- | ---: | ---: | ---: | ---: |
| Original | 67 | 260.393 | 1323.110 | 0.424917 |
| Fused | 68 | 250.489 | 1277.596 | 0.432666 |

Normalized execution CPU fell 3.80%, but image throughput rose only 1.82%.
The measured start state matches. The end differs by one logical update and
approximately 1% of several native-call totals, so this is not an identical
work trace. The fused run's start/end state and native-call counters match
the earlier pre-22:00 Gamma control exactly; its execution CPU cost does not
improve on that older control. See `linux-net-progress-20260916.md`.

This result does not justify expanding the experiment across all guest units
or publishing a patch. Keep it OFF. Windy was not run for this experiment
after Gamma failed to establish the required gain. No Deck FPS improvement
is claimed. Evidence is in `runs/scalar-writes-comparison-20260916.json`,
its two full Gamma summaries and the incremental build/component logs.

The build cache was returned to SCALAR_WRITES=OFF using the retained original
objects and four link/archive steps. The VM test executable returns to the
previous prepared-transfer candidate (SHA-256 6344e07c5c9090c15f163f9a4ba6b303d91499d4baedc627460936d524bcd8bb).
No release patch, installer, accepted Windows executable, personal save or
r354 baseline has been replaced.
