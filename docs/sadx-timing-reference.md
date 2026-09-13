# Installed Sonic Adventure DX timing reference

Read-only investigation, 2026-09-13. The Steam installation and its settings
were not modified or launched. No SADX assets or code were imported into the
port. This is static analysis, not a measured SADX performance comparison.

## Installed version

- Steam app 71250, build 411939.
- `C:/Program Files (x86)/Steam/steamapps/common/Sonic Adventure DX/`.
- `Sonic Adventure DX.exe`: 90,922,000 bytes, PE32/x86, image base 0x400000.
- SHA256: `e4330c00d7ee3910ecc1acab789ffd2a2d571c506984b99a8de4d3663e697fd6`.
- Imports include Direct3D9, QueryPerformanceCounter/Frequency and timeGetTime.
  No mod loader was present in the installation root.

## What the installed binary establishes

The timing family can be read directly without unpacking or executing the EXE:

| Installed Steam address | Observed operation |
| --- | --- |
| 0x4226B0 | Initializes the performance-counter frequency and previous counter. |
| 0x4226F0 | Sets a multiplier and derives the deadline interval as frequency times multiplier divided by 60. |
| 0x422730 | Polls QueryPerformanceCounter until that interval has elapsed, then stores the new counter. |
| 0x429D40 | Supplies multiplier 1 during initialization. |
| 0x4245D0 | Passes its scene/configuration argument through to the interval setter. |
| 0x430700 | Tail-jumps to the polling limiter; reached through further tail wrappers at 0x422E70/0x424599. |

This is evidence of a native 60-Hz base with selectable interval multiples,
not a universal fixed-30 engine or an arbitrary-display-rate simulation.
The limiter contains a busy-poll loop. Copying that into this port would not
reduce execution CPU cost. Static inspection does not establish which path
accounts for every presented frame or the runtime cost of the installed game.

## Cross-check against published SADX work

[Michael Fadely's timing implementation](https://github.com/michael-fadely/sadx-frame-limit/blob/66701e7761af12d29025ba6224b4359ee013f589/sadx-frame-limit/mod.cpp)
uses a 60-Hz base and a frame multiplier, explicitly retains 30-FPS cutscenes,
and separates waiting from the number of recovery updates. Its documented
tradeoff is directly relevant: omitting recovery updates makes overload slow
the game down; accumulating timing error can instead create uneven recovery.
This source targets a different SADX executable layout. Its addresses and
60.5-Hz correction must not be applied to the installed Steam binary or the
Dreamcast port. The only directly referenced 60.5 float found in this Steam
EXE's static code scan belonged to scene-coordinate setup, not the limiter.

The [Mod Loader's cutscene code](https://github.com/X-Hax/sadx-mod-loader/blob/eb4cdcca9637d611a98b910fcfed6ce18580451c/data/Codes.lst)
labels its forced-60 cutscene patch as double speed. That independently warns
against treating a cadence change as a speed-preserving enhancement.

## Consequences for Recompiled

- Keep gameplay, authored cutscenes and presentation ownership separate.
  Original already retains scene cadence; Recompiled requests 60 gameplay
  updates, while VSync controls output. Display refresh is not simulation time.
- The installed DX code is native x86. A different frame limiter cannot remove
  our measured retained SH-4 memory/FPU/dispatch work. Our existing profiles
  identify that work as the remaining bottleneck, not the output cap.
- Replace complete, authenticated math families only when state, exception and
  memory behavior compare correctly. The contact/atan families now have both
  differential and actual Gamma measurements; their default promotion is
  documented in `gamma-performance-and-output-2026-09-13.md`.
- Do not transplant DX's rendering/material choices. The accepted Dreamcast
  appearance remains the reference. No claim is made that all DX physics or
  lighting routines have been recovered or matched.

There is no SADX-derived timing patch in this change: the comparison supports
our existing separation of clocks and the need to reduce actual update work.
