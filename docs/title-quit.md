# Quit from the title screen

B / Circle or Escape opens an in-game confirmation by default on Press Start
and the root Adventure / Trial / Options menu. Menu Confirm/Cancel mappings
also apply here, using the physical input snapshot before gameplay remapping.
A / Cross or Enter quits by default; B / Circle or Escape cancels.
A neutral release is required after opening, reconnecting
a controller, returning focus and closing the dialog. Cancel wins simultaneous
confirm/cancel input. Alt+Enter remains the fullscreen command.

The popup uses the configured text language, or the original game's current
language when the preference is **Use game setting**. Japanese, English,
French, Spanish and German are provided. The displayed hints use the menu
bindings and Xbox/PlayStation/keyboard style of the opening device. Keyboard
confirmation does not require optional keyboard gameplay controls; Enter and
Escape remain accessible fallbacks. Left clicks commit only after release
inside the same displayed button; dragging out or losing focus cancels a
pending click. Other bound mouse buttons use release edges.

## Ownership and safety

The adapter checks the active, generation-bound ADVERTISE module identity
`6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5`,
entry `8C9001A0`, source offset `1A0`, runtime base `0C900000`, size `E4648`.
Main state `8C7608B0 == 11` alone is insufficient. The screen controller at
`8C960AE8` must have matching current/ready screens and no pending transition.
Screen 6 additionally requires task `8C9645D8` / callback `8C909380` in its
ready Press Start state. Screen 7 requires task `8C9645DC` / callback
`8C909A50`, direct root-selection state `+28 == 1`. VMU, file, character,
options and nested dialogs are excluded. These are reads, never state patches.

The Press Start task's `+36` counter is deliberately not a visibility guard:
it delays the original Start button for 180 updates, after which the BGM
sequence can immediately request the attract demo. Task state `+0 == 2`
already proves the entrance fade finished. The host quit action is available
then, including while the original Start delay is still counting.

The opening edge is consumed before returning to the title. One final
original title frame supplies the background. At its completed-frame boundary,
the adapter revalidates ownership, flushes original draws and appends a dim
quad and the popup using the native Overlay class. One small RGBA texture is
rasterized with offscreen GDI, uploaded once and released after closing.
Both renderers consume the same native packets; no renderer code is changed.

While open, the host repeats that completed image and owns input exclusively.
It dispatches no original game instructions and pauses original audio. No
demo, save operation or guest frame advances behind the dialog. Lifecycle,
focus and diagnostic deadlines remain serviced. Confirmation requests the
normal HostRequested stop, allowing existing graphics/audio/input-journal
cleanup to run. Closing before the diagnostic first-game-frame milestone now
returns successful process status without claiming that milestone passed.
Cancellation consumes the closing key until release and resumes the title.

## Verification tools

Build `sonic_quit_prompt_tests` with `tools/build.ps1`. Its checks cover held
buttons, simultaneous actions, neutral rearming, focus/reconnect, excluded
screens, transitions, invalid pointers, and all five language selections.
It writes five offscreen popup BMPs to the supplied isolated directory.

`tools/test-title-quit.ps1` launches a hidden, muted copy of the normal title
path using separate saves and settings. `-InputMode controller` exercises
the native P1 face-button decoder; `keyboard` supplies logical Escape/Enter
edges without injecting Windows input. Each opens twice, cancels once, then
confirms. It requires exit code 0, HostRequested and zero guest instruction
and frame deltas during both modal intervals. Diagnostic input is inert
unless both `KATANA_PORT_BACKGROUND_TEST=1` and the explicit quit-test selector
are set. Native frame captures provide visual evidence.

This is a bounded title/UI test, not a full level matrix or a physical
controller/TeamViewer test. The original r354 snapshot, AOT partitions and
personal saves remain separate from the development output.

## Verified build, 2026-09-12

- `runs/quit-d3d11-verified/result.json`: German, native controller-button
  input, two popups, one cancellation, one confirmation, exit code 0.
- `runs/quit-vulkan-verified/result.json`: French, logical Escape/Enter input,
  the same completed sequence and exit code 0. Inspected native capture:
  `runs/quit-vulkan-verified/frames/frame-871.bmp`.
- Both modal intervals in both runs report zero guest instructions and zero
  guest frame advancement. Opening frames were 353 and 373; normal user exit
  was at frame 374. No diagnostic deadline was used to finish these runs.
- All five translated rasters and the input/title-state checks passed.
  The final incremental build took 64.972 seconds, recompiled zero AOT
  partitions and passed the native link audit. Baseline executable/metadata
  hashes passed their quick check. The personal display INI hash is unchanged.

The earlier Vulkan attempts exposed the incorrect original-Start timer guard
and are retained as failed diagnostic evidence, not counted as passing tests.
No hardware keyboard/controller or visible foreground test was performed.

Executable SHA-256 for those earlier checks:
`f7ce944522c7a5c50ee0a6621086cc77a45f436bae01849203ecba01c8ca5acc`.

## Menu remapping integration, 2026-09-12

`runs/quit-remapped-movie-01/result.json` passes on Vulkan with Confirm mapped
to physical Y and Cancel to X (keyboard Z/C). The hidden test sends those
explicit button bits, opens twice, cancels once, confirms once and exits with
code 0. Both modal intervals retain zero guest frames/instructions. The same
run also decodes the real intro at 50% Master volume before the diagnostic
Start edge, covering the movie provider's live gain path.

The component results at `.local/enhancement-tests/quit-remapped-final-01`
also cover remapped keyboard input, Alt+Enter exclusion, Escape fallback,
release-inside, drag-out cancellation and lost-focus/reconnect disarming.
The actual remapped PlayStation prompt raster is in that directory.
