# Twinkle Circuit results and Big's widescreen HUD

The user identified the Gamma crash as Trial → Minigame → Twinkle Circuit,
after finishing the race. Capsule
`katana-crash-session-1789321496268-14096.log` reports
`loaded-aot-entry-identity-missing` at runtime `8C90A81E`, source
`8298A81E`, dispatched by the resident task loop.

The creator at `8298A800` publishes physical pointer `0C90A81E`. The retained
MINICART module lacked that complete results owner and its directly called
drawing owner `8298AA60`. Both original functions are now compiled with the
pinned native backend, without rebuilding the frozen archive. This shared
six-state results/selection/exit flow serves every character.

## Binding and protection

- MINICART.PRS: 331,314 bytes, SHA256
  `44d5b16521acee36a392b2ca0e57d9ba2826c83b14983e25f550e01d975747e0`.
- Decoded: 1,434,052 bytes, source base `82980000`, SHA256
  `2d3ec72d9f62a0ec626155f822a77bac7209999aa89f825b3082a69cf85db2a1`.
- Generation requires exactly those two function roots. Original instruction
  windows and actual emitted resume cases produce 388 block identities and
  776 P1/P2 dispatch entries. The original 9,286 blocks remain.
- Original tables are validated before extension. Duplicate addresses/offsets
  fail. The old complete module universe must reproduce the retained digest
  from authenticated shards. Its extended canonical identity and derived
  supplemental pack identities bind the new inventory; checks stay enabled.
- The product links only native emitted code. Authoring libraries remain
  outside its audited closure. Generation leaves unchanged output files alone
  so unrelated adapter builds do not recompile these functions.

Normal VMU records are unaffected. Development checkpoints remain bound to
their exact module universe rather than silently accepting incompatible code.

## Validation and limits

`sonic_minicart_tests` compares all six states, eight timer/input variants and
both P1/P2 placements: 96 differential cases. Registers, RAM and external-call
state are compared. External services use identical deterministic fixtures;
this is not an end-to-end race completion test.

The archived component interpreter has the previously documented FTRC
operand defect. The fixture corrects only six exact F23D/F33D instructions
using the decoded source register. Expected output is never copied from the
candidate. Neither the product nor baseline interpreter is changed.

The inventory passed hidden game startup and stage loading, plus a complete
Gamma Emerald Coast benchmark without a crash. The post-race flow has not
yet been driven through the actual minigame.

## Big HUD

Big's main fishing HUD owners classify WEIGHT, rings and life artwork as
left-anchored. The shared numeric formatter is classified only when its exact
saved caller identifies the weight, ring or life counter. This read uses the
existing direct-RAM guard, without an extra observer-visible scalar access.

Centered fishing/catch overlays share these assets, so texture-list matching
alone is insufficient. Unproven callers, 3D sprites and original 4:3 retain
their existing presentation behavior.

Presentation tests cover the HUD and negative shared-asset cases, alongside
16:9, 21:9 and original projection. The hidden Vulkan capture at 3440×1440,
`runs/big-hud-ultrawide-01/frames/frame-1050.bmp`, confirms weight/rings/lives
at the left edge and the mission card in the center.

## Built artifact

`out/experimental/game.exe`, SHA256
`787cc083a5ece9daba4812ec37ae7b675189ab2270ff7e8bff96244395469317`.
The final incremental build took 78.529 seconds; the retained AOT archive was
not recompiled and unchanged supplemental C++ was reused. Native closure and
FPU link audits passed. The final differential test also compares every GPR,
FR/XF register, PR and FPSCR at each recorded external service boundary.
