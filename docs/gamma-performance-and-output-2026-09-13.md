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
The later contact/atan measurements and default promotion are recorded below.

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

## Native math follow-up and SADX comparison

The complete contact and atan/quotient/polynomial/scale families are now on by
default in Recompiled gameplay. Original remains excluded by the existing
gameplay/cadence guard. Source bytes, supported FPU modes, memory ownership and
preflight admission still gate each replacement; unsupported cases retain the
original function. Exceptions after mutation remain fatal, never silently
replayed. `SARECOMP_NATIVE_TRIANGLE_CONTACTS=0` and
`SARECOMP_NATIVE_ATAN_MATH=0` opt out for comparison. The benchmark's
`--original-math-families` sets both to zero; conflicting overrides are rejected.

Revalidated differential suites: contact 88 cases (704 retained angle calls)
and atan 512 cases, including CPU/FPSCR/RAM/ordered stores, decline behavior and
host FP restoration. Logs are
`.local/menu-preview/native-contacts-standard-preflight-01.log` and
`.local/menu-preview/native-atan-standard-preflight-01.log`.

| Gamma EC / D3D11 / 3440x1440 / 60 output | New draws/s | Execution CPU ms/boundary |
| --- | ---: | ---: |
| Previous default, retained contact/atan | 46.697 | 19.684 |
| Native families explicitly enabled | 48.750 | 18.695 |
| Final clean build, native defaults, no override | 47.510 | 19.279 |

The last row is `runs/gamma-ec-default-native-final-d3d-01`, executable SHA-256
`5115b8de556b0f9c2d3972dbb91c3364e495f1d47999b2f024d01d4819c9e630`.
It outputs 60.004 FPS but advances the game timer at 54.148 ticks/s; therefore
output rate must not be mistaken for full-speed simulation. The 73.804-second
incremental build recompiled zero retained AOT units. This row precedes the
separate VSync/presentation correction.

The native-family run executes 418,147 atan calls, 181,415 scale calls and
5,309 contact owners with no fallback or crash. Evidence:
`runs/gamma-ec-native-families-d3d-01`. It uses standard Recompiled timing,
isolated forward input, copied saves and a hidden/muted 60-second route, with
no execution profiler. The roughly 4.4% throughput / 5.0% execution-CPU gain
does **not** establish stable 60 FPS or the requested headroom.

Two additional renderer hypotheses were investigated and withdrawn entirely
from product source, rather than promoted based on successful compilation:

- Bounded scalar arithmetic matched 20,480 differential cases and 1,536
  retained retail color cases, but the same-binary Gamma pair was slower:
  48.416 -> 47.158 draws/s; 18.742 -> 19.380 ms execution CPU/boundary.
  `runs/gamma-ec-scalar-{control,candidate}-d3d-01`. This is not a speedup.
- A per-mesh SDK color cache produced zero hits and misses on the Gamma route.
  Its dedicated coverage gate correctly failed despite a crash-free completed
  run: `runs/gamma-ec-color-cache-validate-d3d-01`. No bitwise-validation or
  performance benefit is claimed for an unexecuted path.

Rejected source and its component test remain only as private investigation
artifacts under `.local/research/renderer-scalar-rejected-20260913.patch` and
`test_render_scalar-rejected-20260913.cpp`; neither is built or shipped.

The installed Steam SADX executable and published timing code were inspected
read-only. [The separate report](sadx-timing-reference.md) binds the installed
EXE and distinguishes its 60-Hz/multiplier evidence from other SADX versions.
It supports keeping authored scene timing separate from presentation; it
does not supply a limiter-only solution to the measured execution bottleneck.
