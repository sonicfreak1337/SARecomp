# Amy: Hedgehog Hammer callback closure

2026-09-13, on `enhancements/ingame-settings` after `360745d`.

The reported crash is captured in
`out/experimental/user-data/katana-crash-session-1789300605618-23640.log`:
`MissingStaticEntry`, target `8C0DF84C`, task dispatcher callsite `8C0986F6`,
PR `8C0986FA`, frame 22169. It is an executable-coverage gap, not evidence of
an invalid task pointer or a native60 timing failure.

The historical function-entry catalog already names this entry, but the
retained AOT unit `unit-v8C0DE71C-8C0DF938-6fb770a02af53098.cpp` has no body
for it. The immutable baseline dispatcher has no entry either. The original
Amy owner registers it through literal `8C0D8A80`; the callback is not tied
exclusively to the minigame. Within the catalog's `8C0DC000..8C0E1000`
neighborhood it was the only entry absent from the compiled dispatch.

## Change

`sonic_amy_hammer_effect.cpp` restores the complete original callback and
is bound as a required, fully replacing native entry. The provider refresh
permits this exact source/size/symbol tuple, not arbitrary missing entries.
The source span is `8C0DF84C`, size `EC`, SHA-256
`6848e8ae03d8ca34e4d1a6b3cb274912ba92e25b4bc3d6245f6dd557f5f97315`.
PC-relative literals are read from the installed original memory, including
the external `8C0DFAA0..8C0DFAB8` island. The authenticated boot SHA-256 is
`b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af`.

All original continuations are preserved:

| Entry | Responsibility | Implementation |
| --- | --- | --- |
| `8C0DDD5C` | Amy animation/state selection | Retained AOT |
| `8C07CF1E` | Animation-work lookup inside that selector | Retained AOT |
| `8C63A8F8` | Original sine-table lookup | Retained AOT |
| `8C0DF804` | Effect fade, then display or deferred deletion | Retained AOT |
| `8C0DF6A0` | Effect drawing | Retained AOT |
| `8C0986C6` | Install the ordinary deferred-delete callback | Retained AOT |

The restored body installs the original display callback, applies the
original integer angle step and FPU FMAC, or executes the fade once before
handing over its update callback. Missing character work takes the original
deferred-deletion tail. Guest PR, stack contents, register results and store
ordering remain original. Interrupted retained calls fail explicitly and
cannot fall through into the absent body. This fix applies to both native60
and original-cadence mode; it does not retime the effect.

## Verification

The focused original-byte differential test passes 33 cases: animations
87/88, every other mode returned by the original Amy selector, angle wrap,
missing character, both tested FPSCR settings including the capsule's
`4006D`, fade through deferred deletion, and invalid/interrupted contracts.
It compares architectural registers/FPSCR, all 16 MiB RAM including stack,
ordered writes and original call targets/arguments/PR. Selection, sine,
fade and deletion execute original SH-4 bytes in the reference; only the
drawing boundary is isolated. This test is not a visual comparison.

The generated dispatch contains the new replacing hook and compiled entries
for all six existing continuations above. The incremental product and link
audits pass: 76,807 ms, **zero retained AOT recompiles**.

`runs/amy-hammer-build-01` boots Amy's Twinkle Park through the real Vulkan
product in a hidden, muted window, using copied saves. It reaches gameplay
and ends at its intentional scenario deadline, frame 517, with no contract
failure. This verifies product startup/integration; Hedgehog Hammer was not
replayed, and that short launch did not exercise the hammer-effect callback.

The same batch removes the vibration row and its help/label from all five
Options languages. Existing device backend/INI compatibility stays internal.
The existing enhancement component suite passes after the removal.

Build: `out/experimental/game.exe`, 2026-09-13 14:28:27 local.
EXE SHA-256:
`14da06279b18e9cb79506a165698d954dcc2ff01dac06c1e1cca0b959590655c`.
Provider SHA-256:
`8e2b771f236ef4d6a00bb99af568538c8e9486f7d43d7cbe00adca3430a535f3`.
Build log: `.local/menu-preview/build-amy-hammer-01.log`.

## Required-hook tail correction, later 2026-09-13

Capsule `katana-crash-session-1789329199590-6200.log` reports
`InvalidHookResult / direct-hook-result`, PC `8C0DF6A0`, PR `8C0986FA`,
frame 17343. The restored callback returned `Jump` to the retained drawing
routine. The pinned SDK deliberately rejects that result for a Required,
FunctionEntry, ReplacesOriginal hook: such a replacement must finish its
declared closure and return, or report a typed Abort.

Both original tails now execute through the existing retained-call bridge,
with the restored caller PR unchanged: `8C0DF6A0` draws, `8C0986C6` installs
deferred deletion. The wrapper returns only after the original tail returns.
Deletion remains deferred; it does not free the task synchronously. Interrupted
tails remain fatal and cannot become ContinueOriginal after partial mutation.

The component test now applies the production `valid_native_port_hook_result`
validator to the exact binding and verifies that the former Jump is rejected.
It also checks both interrupted-tail branches. All 35 original-byte cases
pass, including register/FPSCR, full RAM, ordered stores, targets and PR. The
graphics boundary remains isolated, so these are callback/contract tests,
not a full visual Hedgehog Hammer playthrough.

The other eight remaining Jump return sites were inspected. Their exported
bindings (20 sites in scanout, video timing, bus revision, IML4, interrupt
acknowledgement and soft reset) are Instruction hooks with declared immediate
continuations. None has the same whole-function contract violation.

Corrected game SHA256:
`b266dfb6ea4ccf2211a4c33ef77cc383d1040e55713089f238107247b52da17b`.
Build log `.local/menu-preview/amy-hammer-tail-game-build.log`: 76.547 seconds,
zero retained AOT recompiles; native closure and FPU link audits pass.
The unrelated transient-corner performance experiment was stashed before this
fix and is not part of this executable. Personal saves/configuration are untouched.
Hidden/muted startup check `runs/amy-hammer-tail-boot-01` loaded Amy in Twinkle
Park and stopped at its intentional deadline, frame 446, without a contract
failure. It verifies product integration, not the minigame's effect activation.
