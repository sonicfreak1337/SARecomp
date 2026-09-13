# Transient authored-corner reuse

The Recompiled gameplay renderer now prepares each authored corner of an
eligible NINJA Basic polygon once and submits its triangles through the
existing D3D11/Vulkan index path. It does not cache a mesh across frames.
The Original timing path retains the expanded vertex stream.

Admission requires a live direct-memory read guard, TitleBasic ownership,
smooth shading and an object transform. ENV mapping, exceptional SDK color
writers, normal-draw observations and mesh diagnostics keep their original
path. A private `SARECOMP_INDEXED_CORNERS=0` override permits a matched control.

The key is the authored corner within one polygon, not the model point index:
UV seams and polygon-specific values remain distinct. Triangle order and
winding are unchanged. Near-clipped triangles retain the existing clipper and
append its expanded output to the same ordered index stream. The old 65,536
expanded-corner budget still applies, including temporary clipping input.
Pending draw queues already copy index and vertex spans into owned storage.

## Verification

- `sonic_corner_indices_tests`: 41 cases compare the fully expanded indexed
  stream with an independent reference, across both strip windings, polygon
  sizes, distinct UV/color seams, mixed 0/3/6-corner clipping output, resets,
  invalid corner indices and logical output limits.
- `corner-indices-verify-gamma-01`: a hidden, muted 60-second Gamma Emerald
  Coast walk. The private `SARECOMP_INDEXED_CORNERS_VERIFY=1` oracle rebuilt
  every reused corner through the unchanged vertex builder and compared all
  76 bytes. The last periodic record reports 27,769,286 exact comparisons and
  no mismatch. The run completed normally. Oracle overhead makes this run
  unsuitable for estimating speedup.
- D3D11 captures `corner-indices-d3d11-on-01` and
  `corner-indices-d3d11-off-01`: visually compared Emerald Coast at 1280x720.
  Geometry, material boundaries and texture placement agree; animation/timer
  sampling differs slightly. The enabled run additionally checked 674,916
  reused corners by its last periodic record and stopped at the normal
  diagnostic deadline. This is not a pixel-identical animation replay.

## Matched performance evidence

Both runs below used executable SHA-256
`3c04df551467469413a2d9b47182754cd8cb28587558e70bb7e385177d6bfb56`,
Gamma Emerald Coast, Recompiled timing, Vulkan at 3440x1440, VSync off,
60 output FPS and the same isolated forward input. The first ten seconds
are excluded. Neither run enabled the expensive per-corner oracle.

| Run | Actual game draws/s | Execution CPU ms/draw | Execution cycles/draw |
| --- | ---: | ---: | ---: |
| `corner-indices-off-gamma-01` | 49.764 | 18.434 | 80,813,802 |
| `corner-indices-on-gamma-01` | 50.505 | 18.106 | 79,192,257 |

This pair shows 1.8% less execution CPU per draw and 1.5% more actual draws.
The enabled run stored 41,296,408 vertices for 69,279,306 logical corners in
its last periodic record: 40.4% fewer full vertices on the eligible path.
This is a small shared geometry improvement, not evidence of stable 60 FPS
or of the same gain in every stage. Index uploads also have a cost.

The subsequent `corner-indices-profile-gamma-01` contains 1,937 execution-IP
samples over 30 seconds. It is diagnostic evidence for remaining shared
adapter, FPU and memory work, not another clean performance comparison.

The shipping-default update changes the enable policy and formatting only;
the same narrow admission and vertex construction remain. The normal
incremental build took 74.2 seconds with zero AOT recompiles. All test saves
are isolated copies; the r354 baseline and personal saves are unchanged.

## Later Vulkan startup limitation

The final executable is
`e5eb0821bba2c13e016323f16705265ee56803894ac46c95c95d5785e92f6a4c`.
The later Vulkan captures `corner-indices-vulkan-default-01` through `03`
and `corner-indices-final-gamma-01` failed before their first presentation:
`vkGetPhysicalDeviceSurfaceCapabilitiesKHR` returned `VK_ERROR_UNKNOWN`
(-13) for a valid hidden HWND. No indexed gameplay had executed. Changing
resolution and removing the capture helper's obsolete 144-FPS override did
not resolve that surface error.

The independently compiled, unchanged `sonic_renderer_tests.exe` from
2026-09-13 17:56 also failed at the same surface call, without loading game
code; see `.local/menu-preview/vulkan-surface-independent-01.log`. This
separates the failure from the corner change, but does not identify its driver
or window-system cause. The desktop was accessible and not a Remote Desktop
session. No visible-window test was run because the PC is in use.

Therefore the later Vulkan capture/final benchmark is **not** a pass and
provides no new speed measurement. The earlier matched Vulkan runs and their
live corner oracle remain the performance/data evidence. Resolving the hidden
Vulkan surface failure remains open. D3D11 captures of the final binary passed.
