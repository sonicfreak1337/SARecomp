# Recompiled features since r354

Reference: accepted r354 / 0.49.9. This inventory distinguishes delivered
enhancements from the current experimental in-game Options package. It does
not count the seven completed stories, existing quicksave, crash capsules,
Chao persistence or the original 144 Hz presentation thread as new features.

## Delivered enhancements

- Separate Sonic repository, independent protected r354 snapshot, isolated
  experimental output and copied test saves; incremental retained-AOT builds.
- Real Hor+ 16:9, 21:9 and actual monitor aspect; Original 4:3 remains available.
- Edge-anchored supported HUD elements, Adventure Field ring icon correction,
  full-width fades and expanded source-reviewed render-culling paths. Movies
  retain their original aspect. This is not a claim that every unreviewed
  object/cutscene/UI owner has been independently verified.
- Optional native Vulkan renderer alongside D3D11: textures/mips, geometry,
  blend/depth/fog, clipping, Type-2 transparency, films and native frame capture.
- Resolution, render scale, output FPS up to 144, windowed/borderless/exclusive
  modes where supported. Vulkan Alt+Enter returns to the saved window bounds.
- English first-start `sonic-config.exe`, also usable separately later.
- Text/voice/subtitle preferences, including Use game setting; explicit choices
  survive loads and are persisted by the original normal save serializer.
- Original/Recompiled camera styles. Right-stick yaw and pitch inherit the
  original view, retain the look while standing, and return to the original
  camera while walking after three idle seconds. Wall collision, scripted
  camera guards, scene/teleport/restore resets and corrected DualSense axes.
- Visible startup preparation: bounded program-page prefetch, persistent
  D3D11 shader cache, Vulkan driver/pipeline recipes and known-state warmup.
  Invalid or inaccessible caches fall back to normal creation.
- Localized title-screen quit confirmation via B/Circle/Escape and
  A/Cross/Enter. Neutral-release protection and no guest ticks in the dialog.
- Legacy 50/60 Hz Apply/Test no longer tears down the PC renderer.
- Native Options entry and exit no longer show the original Options screen;
  its separate display callback is suppressed throughout both fades. Original
  main menu, task updates and Sound Test remain active.
- Additional CPU/FPS and startup measurements, hidden capture/probe tooling,
  retained build-cache support and actual AOT-recompile reporting.
- Source-corrected exceptional SDK colors for the reported Speed Highway 2
  crash. The earlier station-hall crash was not reproducible and does not
  have a proven common cause.

No general CPU reduction or Vulkan simulation-FPS advantage was established.
The 144 Hz output still repeats completed guest pictures. No Linux host or
ray tracing has been implemented.

## Current Options package: implemented and tested within the stated limits

- Sonic-style host Options using the supplied original SA sky background.
  Original OPTION.ADX music and original Sound Test remain. Text follows the
  game's Japanese/English/French/Spanish/German text setting.
- About page with the requested SEGA, SoNiCFReaK and KatanaRecomp credits.
  The three credit lines keep their exact supplied wording; navigation is
  localized with the rest of Options.
- Controller, mouse and keyboard navigation; release-inside mouse activation,
  cancellation, remapped prompts and neutral controller reconnection.
- The title quit dialog also uses the menu bindings, with physical-input
  decoding, mouse release-inside and Alt+Enter exclusion. Intro movie skipping
  now follows the normal gameplay Start mapping.
- Action remapping; mouse-camera input; independent X/Y sensitivities and
  inversion; movement/camera deadzones, vibration, glyph selection, stick swap
  and configurable original-camera return delay, including Never.
- Live volume/camera settings; restart-only display fields remain staged.
- Master/music/voice/effects volume, background mute, subtitle scale/backplate,
  and existing language preferences. HUD uses the accepted fixed aspect layouts
  for 4:3, 16:9 and 21:9; no customizable HUD size or margins.
- Master/background mute also cover pre-rendered movies, retaining their
  original timing and video data. Their mixed soundtrack follows Master only.
- Audio-device recovery retains unplayed PCM and uses a silent media clock
  while no endpoint is available. Standby rebases title timers and camera
  history. Error-injection tests pass; physical hotplug/suspend acceptance
  remains pending. See audio-device-recovery.md.
- Profiles and exact whole-VMU Story+Chao snapshots; versioned automatic/manual
  backups, import file browser, export/restore preview and explicit confirmation.
  Digest binding prevents changed files from being restored after preview.
- Out-of-process display confirmation and rollback for rejection, crash,
  timeout, failed child start and an orphaned pending configuration.
- VSync, render scale capped at 100% and anisotropic filtering settings.
  VSync On visibly disables the FPS limit in Options and the configuration
  tool, preserving it for Off/Automatic. Visible output uses driver pacing;
  the manual cap no longer adds a second throttle. Changes require restart.
  The user withdrew the experimental 200% scale after reporting graphics bugs;
  existing settings above 100% migrate to native resolution on load.
- Optional pause on focus/controller loss. Diagnostic report export excludes
  saves, memory dumps, personal paths and machine identifiers. Translated native
  failure dialogs and the shared window-menu entry are integrated.
- Options UI uses actual output resolution even when the scene is rendered
  below 100%. Movies and gameplay return to their own established render path.
- Display-change recovery follows the surviving monitor in borderless mode,
  makes stranded windowed/title-bar bounds reachable and rebuilds Vulkan
  presentation after a display change. Hidden-window recovery tests pass.

Verified so far: five-language model/input/save tests; real hidden/muted
Options-to-Sound-Test-and-back runs, unchanged guest frame/instruction count
inside Options, persisted music volume; six real-process display rollback
cases. Native UI pixel/lifetime checks pass in D3D11 and Vulkan; a real hidden
Options/Sound-Test run also passes at 50% scene resolution. These checks do
not make the whole package release-ready. See options-finalization.md.

## Remaining work

- Replay improvements were withdrawn by the user; existing behavior is retained.
  Subtitle appearance was accepted and needs no further redesign.
- Verify full-width cutscene/ending color overlays in-game and inspect
  any separately textured effects. The common color-plane rule passes the
  actual Vulkan and D3D11 edge-coverage tests, and the user confirmed the
  emblem dimming in-game. The corrected title-model UV
  factor passes its retail-instruction oracle and hidden Emerald Coast visual
  check; the hard sky/horizon seam is absent in the new captures.
- Complete physical audio-device/standby and monitor-frequency acceptance;
  review mixed voice/effect banks in gameplay. The common audio
  endpoint now passes removal/reopen/error and standby-clock tests.
  Localized Audio/Profile status rows now show recovery and backup failures;
  cleanup failure is distinguished from a safely published backup.
- Render interpolation was withdrawn after whole-scene motion glitches. The
  prototype is compiled out, removed from Options and cannot be re-enabled by
  old settings. Independent presentation up to 144 FPS is preserved.
- The new output-resolution UI, monitor recovery and bounded pacing checks
  are implemented and integrated. VSync/FIFO frame counters from a hidden
  window are not physical scanout measurements.
- Existing r354 is not automatically promoted or replaced by successful tests.
