# Connected player state, movement and display operation

This extension was qualified behind `SARECOMP_NATIVE_PLAYER_OPERATION`.
The user subsequently requested a new Deck patch after this optimization.
It now joins the default native gameplay group, with the seven related
per-feature switches and the global `SARECOMP_NATIVE_GAMEPLAY_GROUP=0`
override retained. Original and Recompiled share this CPU policy.
The September 18 morning update and existing installers stay unchanged;
the follow-up patch is a separately named v2.

The operation adds 121 authenticated retained owners (15,034 reachable
instructions) around `8C0CBD40`, `8C0CCFE8` and `8C0CFA0E`. The first two
roots contain the 56-state and 55-state dispatches; the third is their display
operation. Their movement, animation-state and model helpers run within the
existing native operation. Real foreign calls still use the title adapter,
including its existing collision/model hooks. This is not a claim that every
character, enemy or scene has been converted.

The inventory retains 117 original lexical FPU scopes. Source hashes are bound
to the unchanged PAL generated-artifact manifest. The 106 earlier native
render/land/actor bodies are byte-identical in the author comparison.

## Control flow

- Computed branches inside an owner remain local labels, without new PR or
  host call frames. All 171 reviewed local jump sites retain target capture
  before their delay slot. An unknown target resumes the original branch
  before executing that slot.
- The two additional 50-entry tables branch to existing retained state
  owners. Together with shared tails there are 23 reviewed state-transfer
  sites, including one conditional transfer.
- The original fallthrough at `8C0CED2C` enters the genuine `8C0CED2E` owner.
  Its branch to `8C0CF9A6` selects the existing `8C0CCFE8` local epilogue.
  It does not create a global function at that local address, repeat a prologue
  or acquire a new return frame.
- Original local continuation scopes remain available for access faults,
  observer changes and code invalidation. Functional memory/module guards,
  real callbacks and original timing are retained.
- A source-proof failure after a transfer resumes the actual entry PC. The
  regression case starts at `8C0CED2E`, changes an unused byte in the larger
  owner, and reaches its local `8C0CF9A6` epilogue. The old owner-address restart
  replayed the larger prologue; the corrected path matches the original CPU
  and RAM without doing that work again.

## Qualification

Windows and Linux each pass 140 full CPU/RAM comparisons. These comprise 116
state-table inputs, six additional alternate-rounding/register-bank cases,
sixteen retained-AOT/continuation cases and two local-transfer source cases.
The state-zero constructor case deliberately returns a failed model-buffer
allocation and checks its exact original fault; it is not a successful
constructor fixture. Real allocation/initialization runs in the game check.
Foreign calls have explicit fixtures; these cases are not exhaustive gameplay
coverage. The 33 existing Windows render continuation cases also pass.

The hidden and muted Windows Gamma Emerald Coast run completes. Its captured
frame is byte-identical to the preceding six-group candidate (SHA-256
`f351cb258cecce1622bc275cc6a06b47b9aeeef6eac57f85f2df24616c846498`), and all
sixteen selected fields match at both measurement endpoints. Capture/readback
was enabled, so this is functional evidence, not a performance comparison.

Both private game builds complete incrementally. The Linux staged executable
retains all allocated ELF sections exactly. Current hashes are bound by
`runs/player-operation-executables-20260918.json`; the private Windows filename
is reused. The staged Linux program is `out/player-operation-linux-20260918/game`.
An obsolete VM-only actor executable was removed after its SHA-256 matched the
preserved local copy; this does not remove its measurements or local artifact.

The Linux Gamma pair uses one identical executable, Original timing, four
emulated vCPUs and two software-raster threads. The six preceding connected
groups are ON on both sides; only the new player group changes. All sixteen
selected fields match at both endpoints, including position bits and palette
work, and both sides perform 68 updates and produce 20 new images.

| Metric | New player group OFF | ON | Change |
| --- | ---: | ---: | ---: |
| Execution CPU ms/update | 168.87135 | 124.92333 | -26.02% |
| Process CPU ms/update | 1077.24635 | 741.05653 | -31.21% |
| New images/second | 0.82433 | 1.22714 | +48.86% |

The ON window has 68 new-player admissions, 340 actor admissions and no
render/contact resumes or model revocations. This is an incremental VM
comparison in the tested composition, not a sum of older percentages, a
comparison against the delivered morning patch, or measured Steam Deck FPS.

Short hidden Windows Knuckles Sky Deck and Lost World checks also pass with
all seven groups. Both record zero new-player calls and zero render/contact
resumes in these entry windows. No additional zero-call OFF comparisons are
needed. The earlier six-group Linux checks remain the relevant functional
evidence for these unchanged paths.

## September 18 v2 delivery

The user requested a new Deck patch after this optimization. The seven connected
groups now default ON together, while the internal group and individual overrides
remain available. Release builds completed incrementally for both platforms.
All 27 allocated ELF sections match before and after stripping; the final Linux
runtime is 1,720,072,232 bytes, SHA-256
`41766cc1a9dd44962ffbeedd5d25b97877c13451999f66adae493ae19be0b394`.

The published `out/patches/SonicAdventureRecompiled-CPU-Update-2026-09-18-v2.run`
is 309,893,784 bytes, SHA-256
`f02096aa3cfff012b14be5135922f087c86e3cfe1bf5315b7d009807d3dddf85`.
It is byte-identical to the package applied as an ordinary user in the Linux VM.
The actual installer test covers the September 17 test installer, September 17
v2 update and September 18 morning update. It verifies all three launch paths,
program/manifest backups, idempotent application, retained settings and synthetic
Story/Chao records, preserved diagnostic policy and both matching diagnostic
switches. The original VM installations remain unchanged. No GDI, sudo or new
game installation is required. Matching v2 diagnostic switches are 21,144 bytes
each and contain no replacement program.

The installed Linux program and final Windows build each pass a short Gamma
Emerald Coast check in Original and Recompiled. All seven feature selectors are
`installed`, so no positive feature override enables the new defaults. The
original cadence remains 50 Hz / release 2 / delta 2; Recompiled remains
60 Hz / release 1 / delta 1. Native player, actor, land, object-contact, camera
and shared-model counters advance, with zero recorded contact/world/render
resumes or model revocations. Nested model work borrows the render operation;
the standalone model-root count is consequently zero in these windows.
These four short checks verify normal-start behavior, not new performance
percentages or complete story coverage.

`runs/native-gameplay-release-verification-20260918.json` binds both programs,
all adapter source components and the four checks. Installation and publication
reports are `runs/native-gameplay-installation-20260918.json` and
`runs/native-gameplay-release-publication-20260918.json`. The morning patch and
existing installers are preserved. This requested optimization batch is complete.
