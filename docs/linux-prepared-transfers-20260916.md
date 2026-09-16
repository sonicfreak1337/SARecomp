# Prepared indirect transfers across the whole game

This experiment moves repeated indirect-call admission work out of the hot
path. It applies to every generated module through their shared dispatcher,
without regenerating or recompiling individual AOT units. It does not skip
game updates, change timing, or change any guest function implementation.

The previous dispatcher repeats address normalization, proof-table lookup,
request construction, coverage admission and exact-entry selection. Even a
hit in its existing coverage cache still performs the surrounding work.
The new layer remembers the complete successful pending-selection result.
It does not enlarge the existing entry or coverage caches.

## Lifetime and behavior

- Keys retain raw source, callsite, target, continuation and call/jump kind.
  Aliases are not widened into newly accepted calls.
- A common epoch binds all owning contexts, table lifetime/generation/readiness,
  both runtime generations, image and binder lifecycle/immutable-code stamps,
  and binder identity storage. Any change invalidates all prepared plans.
- Dispatch-scope entry/exit explicitly invalidate the cache. Reusing the same
  addresses in another game session cannot resurrect a previous selection.
- Only successful resolutions are remembered. The epoch is captured again
  afterward because the resolver may bind an executable owner.
- Pending selections, incomplete contexts, enabled runtime diagnostics or
  coverage recording use the original resolver, including original failures.
- Instruction accounting, memory permissions, code-write notification, module
  retirement, exceptions, host stop/pause polling and saves are unchanged.

`SARECOMP_LINUX_TRANSFER_PLANS` is OFF by default. The copied effective
dispatcher is authenticated by SHA-256 before adaptation. Original retained
sources, the pinned SDK and r354 remain untouched. Enabling the experiment
changes one dispatch translation unit; the complete original preflight body
remains available in the same executable.

## Private comparison controls

`tools/benchmark-linux-stage.py --transfer-plans original|cached|verify`
explicitly selects the path. Original and cached measurements can therefore
use the exact same executable, avoiding a code-layout change between pairs.
Verify executes the original resolver on each would-be cache hit, compares
the selected owner/entry/target, and reports checked hits at scope exit.
It is a correctness run, not a throughput measurement. Product code does not
increment hit/miss counters when verification is disabled.

The Linux component test covers all five key fields, all 20 epoch fields,
scope reset and capacity collisions. It passes in the VM.
No performance patch or installer has been produced from this experiment.

## Real-game verification

Candidate ELF SHA-256:
`6344e07c5c9090c15f163f9a4ba6b303d91499d4baedc627460936d524bcd8bb`
(1,679,198,304 bytes, stripped while retaining `.comment`). The incremental
build compiled the dispatcher and the small component test; no AOT unit
was recompiled. See `runs/linux-transfer-plans-build-20260916.log`.

Both hidden/muted VM verification runs completed image boundaries 5..8 and
their intended HostDeadline shutdown. They used fresh isolated saves/input,
800x500 16:10, Vulkan/llvmpipe, render scale 50%, Original PAL 50/2/2 and
enabled native gameplay math. No physical input or personal save was used.

| Stage | Verified repeated plans | Original resolutions on a miss |
| --- | ---: | ---: |
| Gamma Emerald Coast | 1,020,801 | 21,063 |
| Sonic Windy Valley | 1,115,943 | 29,303 |

All 2,136,744 comparisons matched the original selection. These counts include
loading; they establish real use and matching results, not a frame-rate gain.

## Measurement instrumentation

The six initial runs use the pre-existing benchmark telemetry setting ON on
both sides. This adds timers around native provider calls; it is separate from
the runtime-diagnostics switch. Frame counts, game-update counts and execution
CPU time come from independent per-boundary observations.

The Linux and Windows benchmark tools now expose `--telemetry on|off`, default
OFF, so future throughput runs can omit the provider timers. This is a change
to developer probes, not a claimed optimization of the shipped executable,
whose telemetry was already opt-in. The running series uses the unchanged
uploaded harness consistently; no timed pair mixes instrumentation policies.

## Matched throughput results

The same candidate executable ran Original then Cached in Gamma, and Cached
then Original in Windy. All four runs completed image boundaries 5..25, with
20 new images and 68 game updates, Original PAL 50/2/2 and native gameplay
math active throughout. No sampler or concurrent build ran in these windows.

| Stage | Execution CPU ms/update, original → cached | Process CPU ms/update, original → cached | New images/s, original → cached |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | 242.527 → 231.310 (-4.63%) | 982.747 → 954.874 (-2.84%) | 0.5662 → 0.5853 (+3.37%) |
| Sonic Windy Valley | 257.433 → 246.669 (-4.18%) | 985.326 → 961.343 (-2.43%) | 0.5765 → 0.5986 (+3.83%) |

These are single pairs, not confidence intervals or Deck frame-rate estimates.
Gamma's final native-call counters, game ticks and player coordinates match.
Windy finishes one absolute game tick apart despite 68 updates in both measured
windows; several native-call totals differ by about 1% and player X by 0.2.
Its result therefore includes a small initial-state/workload difference.

The change is retained as a tested, global experiment for further CPU work,
not presented as the requested large improvement. The 20–25 ms target remains
open. The build switch defaults OFF; the current isolated development build
and `/home/sonic/preloaded-v1/game` contain the measured candidate. No installer,
patch, accepted Windows executable, r354 or installed VM control was replaced.
The installed VM control remains SHA-256
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.

Evidence: `runs/linux-prepared-transfers-20260916/` contains the six complete
summaries, both verification logs and `comparison.json`, including exact
workload differences. The change is not rejected merely because it falls short
of the overall target, but these small gains do not authorize a performance
patch under the user's delivery condition.
