# Page-proven virtual code-address translation

This is a port-local, default-OFF experiment, not a released performance fix.
The installed SADX comparison is in `sadx-execution-reference-20260916.md`.
It identifies substantially less per-instruction administration in the native
PC game, without establishing that any one replacement will solve our CPU cost.

## What changes

The original inline code-address lookup reads a thread-local interval and tests
its start/extent before adding its relocation delta. Alternating between owners
and loaded images can replace that interval on successive lookups. The new path
uses one 64-KiB-page entry holding a proven affine delta, or a fallback marker.
It caches address translation only; it never caches game memory or callbacks.

There are separate forward/reverse arrays, preserving virtual P1/P2 aliases and
the original single-step translation semantics. Identity pages initialize to
zero. After every successful scope insertion or retirement, the original caches
are invalidated and the affected source/runtime pages are refreshed using the
original resolver's exact maximal interval. A direct entry is accepted only
when that interval covers the entire page. Partial pages and delta -1 keep the
old ABI lookup. Removing a mapping beneath newer live mappings is supported.

The pinned SDK and retained guest source are unchanged. Authored preparation
copies `block_abi.cpp`, checking its full SHA, and changes only the lookup names
in the selected effective guest units. Reversing those substitutions restores
every input byte. Distinct helper names avoid differing inline definitions in
modified/unmodified translation units. Both paths observe the same live mapping
list and original lifetime rules. No CPU mode, diagnostic, exception, scheduler,
executable invalidation or guest timing contract is removed.

The cost is 512 KiB TLS per thread plus resolver work when mappings change.
Temporary template mappings also pay this update cost; it is not assumed to be
limited to rare module loading. The initial gameplay scope is the existing 41
profile-selected units, with 255,453 static lookup sites. This count is not a
dynamic hit count or a performance prediction.

## Component evidence

`runs/code-address-component-20260916.log` records 253,912 address comparisons
against an independent reverse-priority mapping oracle and the unchanged inline
helpers. Cases cover nested/partial overlaps, arbitrary non-LIFO retirement,
forward/reverse directions, 32-bit end boundaries, sentinel delta, invalid
mapping rejection, and separate threads.

The Linux VM literal-PC microbenchmark (four lookups per iteration, one million
iterations, matching checksums) measured these thread CPU times:

| Mapping state | Original interval | Page entry |
| --- | ---: | ---: |
| Identity | 11.897 ms | 10.276 ms |
| Module | 94.673 ms | 10.592 ms |
| Nested module | 175.687 ms | 10.986 ms |

This deliberately small lookup workload does not measure game performance or
amortize mapping mutations. Gameplay measurements must decide retention.

## Fresh profile and separate RAM diagnosis

The exact B04B candidate was sampled in the hidden Linux Gamma scene. The
offline resolver authenticates the stripped SHA and executable segment bytes
against the local unstripped ELF, then accepts only contained function symbols.
It resolves 1,086 of 1,087 game samples, including 594 in generated owners.
It avoids uploading another 1.7-GB ELF to the space-constrained VM.
Evidence: `runs/ram-page-profile-gamma-20260916/resolved.json`.

Two apparent ordinary-RAM fallback locations in owner 8C057B00 were then
instrumented in a separate diagnostic build. The Gamma probe completed normally
and recorded zero misses at PCs 8C057FEA/8C057FEC. This does not establish that the
sites were frequently executed or rule out misses elsewhere. It does not justify
refreshing a stale guard or bypassing memory proof. That bounded diagnostic is
disabled in the code-address candidate and is not throughput evidence.

Diagnostic SHA:
`9c694cff505c162a74b443b6cf447f1de2d948e63f3d0497c8a4981d93833cfe`.
Summary: `runs/ram-guard-probe-gamma-20260916.json`.

No installer or patch is authorized by these component results. The required
large measured gameplay gain and 20–25 ms target remain open.

## Gameplay result: not retained in the active candidate

The 41-unit candidate has SHA-256
`60ea91d0177f00764f51984ee762f1542808ecfb0a3203543c2b25d9f80cb43a`,
1,679,350,256 bytes. It contains the same B04B RAM regions and prepared transfers;
the temporary RAM-miss probe is disabled. Both hidden Linux probes use Original
PAL 50/2/2, native math, 800x500 / 16:10, 50% scale, diagnostics and telemetry OFF.
Both complete the requested 5..25 window and normal HostDeadline shutdown.

| Stage | Updates | Execution CPU ms/update, B04B → pages | New images/s, B04B → pages |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | 65 → 65 | 257.737 → 250.281 | 0.42418 → 0.40812 |
| Sonic Windy Valley | 68 → 68 | 271.563 → 274.527 | 0.46235 → 0.46002 |

Gamma's CPU cost falls 2.89%, but its initial/final game ticks differ by one,
with differences in coordinates, HUD and native-call work. Image throughput
falls 3.79%. Windy retains matching coordinates and HUD, with a one-tick offset
and 22 additional collision-length calls; CPU cost rises 1.09% and image rate
falls 0.50%. These single VM pairs do not establish a useful global improvement.

The 40 uninstrumented comparable object files shrink executable text by 2.44%;
the 41st old object still contained the temporary probe and was excluded from
that calculation. The selected 894 owners account for 438 of 594 generated-owner
samples (74%) in the preceding exact profile. Neither code size nor owner
coverage turns the synthetic lookup gain into a gameplay speedup.

`SARECOMP_LINUX_CODE_ADDRESS_PAGES` returns to OFF. The original compiled
members are reused, with only the previously instrumented guest unit rebuilt.
The restored local and VM executables both match B04B byte-for-byte, verified
in `runs/code-address-restore-local-20260916.json`. No installed product, Windows
executable, installer, patch or save is changed.

Evidence: `runs/code-address-{gamma,windy}-20260916.json`,
`runs/code-address-comparison-20260916.json`,
`runs/code-address-object-sizes-20260916.json`, and the incremental build logs.
Keep the authored experiment for reference, not as a reason to compile ALL units
or to claim progress toward the requested FPS target.
