# Gamma performance and simplified timing options — 2026-09-13

Windows performance remains below the stable-60 target; Linux work and release
approval remain paused. The changes below preserve the r354 AOT archive and
personal save namespace.

## Product controls

- Options and the English `sonic-config.exe` expose Original/Recompiled timing
  and VSync On/Off. Timing and VSync changes require a restart. A timing-only
  restart does not start the display-confirmation rollback countdown.
- Recompiled is the default: 60 gameplay FPS target, 60 output FPS without
  VSync, display-paced output with VSync. Numeric output FPS and anisotropic
  filtering controls are removed. Old INI values for both are ignored; numeric
  FPS is no longer serialized. Legacy VSync Automatic migrates to On.
- Original preserves the original per-scene update and output cadence. It
  disables autonomous render-thread repetitions and the additional fixed-rate
  quantization of title-requested presents. Deferred first presentations still
  retry, and the render thread's title clock/event service remains active.
- The retained artifact's `{30,144,144}` timing metadata is deliberately
  unchanged: the launcher supplies the current 60 output default explicitly.
  This avoids a full AOT export for a host setting. Private CLI overrides remain
  diagnostic tools, not user-selectable settings.
- The stage benchmark no longer forces a private 144-FPS override. The normal
  development launcher preserves timing and other existing settings when an
  explicit resolution/camera choice rewrites its INI.

## Removed allocation cost

The renderer's environment-map and SDK float-color scratch CPUs constructed
`Memory{0}` for individual draws. This still allocates a 65,536-entry address
lookup table (256 KiB). A thread-local leased scratch slot now retains only
that empty memory object's capacity. Every other CPU field is reset as for a
fresh aggregate, and FPSCR is copied for each draw. Nested leases use separate
state. No guest mappings, source bytes or computed normals are cached here.

The differential test passes 455 fresh/reused cases, including denormals,
NaNs, infinities, rounding/exception modes, reset behavior and nested leases.
After warming the slot it observes zero additional page-table allocations.
Sampled stacks containing the scratch Memory constructor disappear (5 to 0).

This is a verified reduction in allocation work, not a material FPS gain:

| Matched Vulkan / 3440x1440 / 144 output | New draws/s | Execution CPU ms/boundary |
| --- | ---: | ---: |
| Before scratch reuse | 45.962 | 20.027 |
| After scratch reuse | 46.035 | 19.987 |

The roughly 0.16% throughput difference is noise-sized. Both runs use Gamma
Emerald Coast, isolated native forward input, copied saves, hidden/muted output,
and the same 20-second execution sampling options. Evidence:
`runs/gamma-ec-profile-20260913-01` and
`runs/gamma-ec-scratch-20260913-01`. Resolved IP reports bind their own EXE and
link map; do not resolve an older profile against a newly rebuilt map.

The largest remaining work is distributed across model expansion, guest
memory/immutable-range checks, FPU operations/epochs and collision traversal.
The existing private contact/atan experiments can now be measured in standard
Recompiled gameplay as well as the former fixture. Both remain off by default;
there is no claim of a measured gain or product promotion for those switches.

## Verification of the current output policy

Incremental build: 75.873 seconds, **zero AOT recompiles**. Native link audit
and both FPU link audits pass. Current game SHA256:
`7946ed280c8f6fe7db8c98d32d3689973de477df9f486f3f7c1bfe5b5780f5a4`.

- Settings/menu component suite passes in all five languages, including
  migration, timing persistence, restart isolation and removal of both rows.
  The hidden native configuration self-test saves Original and VSync On and
  reads them back correctly. Test roots are under `.local/menu-preview/`.
- D3D11 synthetic original cadence changes 25 → 60 → 30 → 50 Hz within one
  render owner. All phases track their source rate with **zero repetitions**.
  Recompiled's 30-Hz fixture outputs 60.004 FPS while source cadence is retained.
- VSync selects the current 144-Hz display clock and ignores the fixed 60 cap.
  This is a hidden-window check of clock selection, not a physical scanout or
  multi-monitor certification (hidden DWM throughput measured about 132 FPS).
- Original Gamma run: 60 seconds of the diagnostic route complete without a
  crash; 24.092 new draws/s and exactly 24.092 output frames/s. The retained
  PAL video owner reports 50 Hz with original 2/2 gameplay scheduling.
  `runs/gamma-ec-original-output-d3d-01`.
- Recompiled Gamma run: same D3D11 route completes without a crash; 46.697 new
  draws/s, **60.005 output frames/s**, 19.684 ms execution CPU per boundary.
  This verifies the output default but **does not meet the 60-sim target**.
  `runs/gamma-ec-recompiled-output-d3d-01`. Its backend/output policy differs
  from the earlier 144-FPS Vulkan pair; do not report that delta as a speedup.

Gamma's HUD timer counts down and can gain time, so it is not a reliable
wall-time speed meter. Loading/respawn transitions can also change the logical
delta; keep per-boundary cadence witnesses separate from aggregate draw FPS.

Vulkan validation is currently limited by this host returning
`VK_ERROR_UNKNOWN (-13)` from `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` for
valid hidden windows, before any gameplay. This reproduces in the unchanged
renderer test executable dated 2026-09-12 22:04 as well as the new pacing/game
tests. D3D11 passes. The cause of that host/driver failure is not established,
and no new Vulkan runtime pass is claimed. No driver or display settings were
changed to bypass it.

Failed diagnostic starts now still produce benchmark result JSON instead of
raising an unrelated empty-timing-sample IndexError. Tests touch neither
personal saves nor the baseline, and no full level matrix was run.
