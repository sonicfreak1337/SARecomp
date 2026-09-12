# Options finalization and VSync

This batch retains the immutable r354 baseline and its compiled AOT pack.
The build is experimental; no baseline promotion is implied.

## Final scope

Render interpolation is withdrawn. The setting is absent from all five
localized menus, and legacy INI entries are ignored and omitted on save.
The prototype remains as commented/compile-time-disabled reference code.
Native hook dispatch remains bound to the current Options, language and
camera hooks, with the interpolation call-scope instrumentation removed.
Replay development was withdrawn; accepted subtitle behavior stays.

VSync On makes the output-FPS row gray, noninteractive and labeled
“Display controlled” in the selected text language. Controller, keyboard,
mouse and the old window-menu shortcuts cannot change that value while On.
The English configuration executable applies the same restriction. The
stored manual limit survives saving and is usable again with Off/Automatic.
VSync itself remains a restart setting and is marked accordingly.

For visible windows On uses D3D11 Present synchronization or Vulkan FIFO
acquisition/presentation, without a second software FPS cap. When a window
is hidden, occluded or presentation is deferred, a bounded retry cadence
uses the current monitor frequency; this avoids a background retry spin.
Display refresh is queried at creation and window/display changes, not per
draw. The title/simulation clock is unchanged. Automatic retains the earlier
renderer defaults; its manual output limit is still applicable.

Native Options images now bypass the internally downscaled scene surface.
Both renderers retain the owned UI texture for repeated presentations and
captures; a following movie/game frame clears that override. Movies preserve
their original fit policy. Gameplay rendering, geometry and timing are not
changed by this UI path.

On display changes borderless bounds follow a reachable monitor, saved
window bounds are repaired, and Vulkan surface/presentation data is refreshed.
Windowed recovery retains reachable windows and relocates stranded title
bars without bringing them to the foreground. Hidden input cleanup no longer
calls the desktop-wide ClipCursor release.

## Verification

All checks used isolated settings/saves and hidden, muted windows.

- `.local/menu-preview/vsync-settings-01.log`: five-language disabled
  control, keyboard/controller/mouse rejection, re-enable and preserved
  120 FPS value; existing settings, profiles, audio/input and privacy tests.
- `.local/enhancement-tests/config-vsync-{1,2}-final`: real hidden config
  controls and serializer preserve 120 FPS with VSync On and Off.
- `.local/menu-preview/vsync-{d3d11,vulkan}-final.log`: the actual consumer
  reports `saved_cap=30` and `saved_cap=144` with `display_hz=144` and
  `cap_applied=0`. Both use the display clock, not either stored cap.
- `.local/menu-preview/pacing-d3d11-off-final.log`: 60.40, 120.38 and
  144.37 output FPS; synthetic simulation 29.79–29.95 FPS.
- `.local/menu-preview/pacing-vulkan-off-final.log`: 60.00, 120.40 and
  144.36 output FPS; synthetic simulation 29.89–30.64 FPS.
- `.local/menu-preview/host-ui-{d3d11,vulkan}-final.log`: exact RGBA pixels
  at 960×540 output with 480×270 scene, padded/bottom-up sources, retained
  image lifetime/replacement, movie fit and return to normal scene rendering.
- `.local/menu-preview/renderer-{d3d11,vulkan}-final.log`: off-desktop
  window recovery through WM_DISPLAYCHANGE, unchanged focus/visibility and
  continued rendering. Vulkan Alt+Enter enters, ignores key repeat and
  restores bounds/swapchain.
- `runs/options-final-ui-01/result.json`: actual Options/Sound-Test
  transitions pass, authenticated OPTION.ADX remains, original Options
  stays suppressed and modal guest frame/instruction deltas are zero.
  Scene rendering is 50%; captured native Options is 1280×720. The exit is
  the planned diagnostic deadline, not a crash.

Initial On-mode throughput comparisons between hidden windows failed: output
fell from roughly 140 to 65 FPS despite an unchanged 144 Hz consumer clock.
This is not evidence of visible scanout speed or a dependence on the manual
cap. Those runs are retained in the local logs. On-mode verification therefore
checks actual clock selection and unchanged simulation; the hidden FPS
counters remain observational. No physical monitor frequency was changed.
Off-mode rate checks still require measured target throughput.

## Build and limits

`.local/menu-preview/build-options-final-03.log` passes the native link
audit, binary size 1,909,876,224 bytes, with no AOT translation-unit rebuild.
`build-vsync-config-01.log` rebuilds only the separate configuration tool.
The quick r354 integrity audit and git whitespace check pass.

An earlier link attempt failed because the frozen dispatcher did not contain
the new native hooks. The final build restores the authenticated current
dispatcher bridge; it does not restore interpolation instrumentation or
change the frozen AOT functions.

Physical audio-device unplugging, actual sleep/resume and visible scanout
across different monitor refresh rates remain untested. The existing audio
and standby failure-injection checks are documented in audio-device-recovery.md.
Separately textured ending effects and mixed voice/effect banks still need
representative gameplay acceptance; pre-rendered mixed movie audio follows
Master rather than pretending to separate voices and music. No new replay
system, full stage matrix or personal-save mutation was performed.
