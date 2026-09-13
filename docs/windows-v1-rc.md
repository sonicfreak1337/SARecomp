# Windows v1.0 release candidate

Release approval withdrawn on 2026-09-13: the user reports sub-60 simulation
throughput and slow-motion gameplay in Gamma's Emerald Coast. Linux work is
paused. The current priority is stable 60 real simulation updates with CPU
headroom and original wall-time speed. The candidate below is a preserved
development checkpoint, not an approved release.

The user designated the current Windows port as the v1.0 candidate on
2026-09-13. This does not replace the immutable r354 reference. Linux / Steam
Deck development must start from a separately preserved copy of this candidate.

Release polish:

- Enabling widescreen shows "Some cutscenes may be glitchy." in the selected
  in-game text language. The separate English configuration dialog warns too.
- Vibration is no longer listed in Options. Existing configuration files remain
  readable.
- Subtitle size and background are no longer listed in any menu language.
  Legacy INI values are accepted but ignored, preserving original subtitle
  presentation. Text language, voice language and subtitle on/off remain.
- Numeric output FPS and anisotropy controls are retired. Game timing selects
  Original/Recompiled, with a required restart. Recompiled outputs 60 FPS without
  VSync; VSync follows the monitor. Original retains original simulation and
  output rates per scene. Both settings UIs expose timing and VSync.
- Share / Create / View no longer opens the private scenario menu. Ctrl+F10
  remains its opening shortcut; normal controller navigation inside it remains.
- The game window has no attached native Options / Developer tools menu bar,
  including after fullscreen transitions. Settings remain in the in-game menu.
- Ctrl+F1 toggles the existing output and simulation FPS overlay in D3D11 and
  Vulkan. A held key toggles once, not on every key-repeat message.
- The retained developer command menu is private and explicitly destroyed by
  the renderer. The FPS sampler remains active independently of menu visibility.

The preceding Amy hammer callback fix is recorded in
`amy-hedgehog-hammer-crash.md`. The additional Super Sonic story cadence fix is
recorded in `super-sonic-cadence-crash.md`. Native 60 Hz gameplay remains the default.
Personal saves and the frozen r354 AOT archive are not modified by this polish.

The earlier polish-only instruction excluded additional game runs. The later
performance task authorizes focused hidden/muted measurements again; it does
not authorize release promotion. Current measurement evidence is tracked
separately from this earlier candidate checkpoint.

Initial candidate incremental build: 75.426 seconds, zero AOT recompiles. The native port
link audit and both FPU link checks pass. Windows artifacts are preserved in
`out/windows-v1.0-rc1`; the ordinary test build remains `out/experimental`.
The candidate contains no personal INI or save files and still requires the
separately installed original content.

- game.exe SHA256:
  `093516ce53d45d94b3472e248105952dc2e54d476d175baa60534f2050e20231`
- sonic-config.exe SHA256:
  `46d59113986886b7f7bc8790b32fdfe45e9475b567fb4511e0216338f058d339`
