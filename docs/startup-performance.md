# Startup progress and cold-start preparation

The game now shows an English loading window while preparing its native
runtime. It displays actual per-phase counts for program-page prefetch and
shader/pipeline preparation. Phases without a known total use an animated
bar; there is no invented overall percentage. It closes after the original
game loop starts. First-start configuration still happens before preparation.

The UI has its own message thread so compilation and disk work cannot freeze
the bar. An unavailable window/thread/timer does not prevent boot. Hidden,
muted automated runs suppress the window; the test fixture captures an owned,
hidden copy through WM_PRINTCLIENT. It never takes desktop focus.

## What previously happened

D3D11 compiled its main, composite and Type-2 shaders on every launch. Its
overlay shader and minimum dynamic geometry buffers were created lazily.
Vulkan compiled pipeline variants when first used and discarded its driver
pipeline-cache data when the process ended. Texture archive loading, PRS/PVR
decoding and uploads also occur as original content is requested; those are
separate from shader preparation.

The native executable is unusually large: approximately 1.6 GiB of executable
code plus about 0.24 GiB of read-only data. Cold mapped pages are a plausible
additional source of first-run stalls. This is an inference from the binary
layout and first-access behavior, not a measured Windows-reboot diagnosis.

## Changes and fallback behavior

- Before gameplay, request read-only mapped program pages through
  `PrefetchVirtualMemory`, executable sections first. The budget is the
  smaller of 2 GiB and one third of currently available physical RAM, in
  batches of at most 8 MiB. No page is pinned, no writable data is touched,
  and this does not load every stage's assets. Windows treats this API as
  a memory-constrained hint, not a residency guarantee. See
  [Microsoft's contract](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-prefetchvirtualmemory).
- Persist D3D11 DXBC by full source, entry point, profile, flags and the actual
  compiler DLL's SHA-256. Integrity-check and load it before device shader
  creation. A missing, unreadable or corrupt entry compiles through the
  original path. Prepare the overlay shader and minimum dynamic vertex/index
  buffers during the loading phase as well.
- Persist Vulkan driver cache data and actual pipeline recipes on normal
  shutdown, bound to device, driver, cache UUID, embedded shaders and Type-2
  capacity. On subsequent starts, recreate previously used states before
  play; new states still use normal lazy creation. Never-bound warm states
  are evictable before they consume live pipeline capacity. Warmup resource
  failures discard unused warm states and continue lazy; device loss remains
  an error. [Khronos describes the cross-run cache mechanism](https://docs.vulkan.org/guide/latest/pipeline_cache.html).

Cache files have a versioned header, payload length and SHA-256. Writes use
unique temporary files and atomic replacement. Payload limits are 4 MiB per
D3D shader, 64 MiB for a Vulkan driver cache, and at most 4,096 recipes bounded
by the configured pipeline-state capacity. Cache IO failure is optional.
GPU work, blend/depth rules, game timing and personal saves are unchanged.

The normal cache is under the selected user-data root's `cache/startup-v1`,
or `%LOCALAPPDATA%/SARecomp/experimental/cache/startup-v1` when that root is
unspecified. `SARECOMP_CACHE_ROOT` isolates test caches. Developer controls:
`SARECOMP_DISABLE_STARTUP_CACHE=1` and `SARECOMP_DISABLE_CODE_PREFETCH=1`.

## Verification on 2026-09-12

Tests were hidden and muted. These figures are **application-cache cold/warm**
on the same running Windows session, not before/after a reboot. The graphics
driver's own cache and OS file cache were not cleared.

| Measurement | Empty application cache | Reused application cache |
| --- | ---: | ---: |
| D3D11 component: total seven shader preparations | 280.695 ms | 9.411 ms |
| D3D11 actual game: total seven shader preparations | 275.028 ms | 8.321 ms |
| D3D11 actual game: program-page preparation | 1,408.256 ms | 135.079 ms |
| D3D11 actual game: startup session through frame 2 | 3,857.152 ms | 2,390.053 ms |
| D3D11 shader cache hits | 0 / 7 | 7 / 7 |
| Vulkan component: prior pipeline recipes warmed | 0 | 11 |

The startup-session timer begins inside the executable, after first-start
configuration; it excludes Windows process-loader time. Both D3D11 game runs
reached Emerald Coast and their expected diagnostic deadlines. Live neutral
DualSense input stayed neutral and produced zero manual camera overrides.
The native captures include Sonic, Tails, scenery and the HUD.

All thirteen component images are byte-identical between cold and warm
launches on each renderer, and also between D3D11 and Vulkan. Vulkan reports a
valid driver-cache hit on the warm launch. Component tests cover textures,
geometry, blend/depth/transparency, clipping, movie images, fog and primitives.
Cache integrity, invalidation, corrupt/truncated data, unavailable storage,
the real D3D cache domain and hidden loading-window rendering passed.

Local evidence:

- `.local/startup-d3d-{cold,warm}-03.log` and corresponding `runs/` captures.
- `.local/startup-vulkan-{cold,warm}-03.log` and corresponding captures.
- `runs/startup-game-d3d-cold-04`, `runs/startup-game-d3d-warm-04`.
- `runs/startup-camera-vulkan-04`: normal game deadline and captured orbit.
- `.local/startup-fixture-final/startup-progress.bmp` and cache fixture.
- `.local/startup-build-04.log`: 66.688-second incremental build, zero AOT
  recompiles, native link audit passed. Protected r354 executable/AOT metadata
  hashes passed the quick baseline check.

Current executable SHA-256:
`197dec5558809b3cf2b63185c22d75afbd355988f938fa89619df3e5d63ede14`.

## What these measurements do not establish

They do not prove that every first-run hitch after reboot is eliminated, nor
that all future Vulkan pipeline variants are known. Original stage asset
loads and texture uploads remain. No Windows reboot, file-cache purge or
full level matrix was performed while the user was using the PC.

`SARECOMP_STARTUP_TRACE=1` logs the first 600 original frame intervals, total
process page-fault deltas, read-byte deltas and working set. Page-fault counts
include soft faults; they are not a hard-fault/disk-IO counter. Native BMP
capture itself adds periodic stalls, so these capture runs must not be used
as steady-state FPS benchmarks. Future reboot reports can separate shader
misses from program/content loading without assuming that every pause is a
shader compile.
