# Optional native Vulkan renderer

Select Vulkan in the English **sonic-config.exe**, or use the existing
**Optionen > Renderer (Neustart) > Vulkan (experimentell)** menu, then restart.
Direct3D 11 remains the default and can be selected in the same menu. The
choice is persisted as `renderer=d3d11` or `renderer=vulkan` in
`sonic-display.ini`; display dimensions and render scale are preserved.
`tools/start.ps1 -Renderer vulkan` selects it for that separate run profile.

**Alt+Enter** toggles Vulkan between the saved window rectangle and borderless
fullscreen on that window's monitor. The native menu returns in windowed mode.
The chosen render resolution and aspect are retained; the completed picture
is fitted to the fullscreen output without stretching. Configured exclusive
fullscreen uses DXGI on D3D11 and, when supported, `VK_EXT_full_screen_exclusive`
on Vulkan. It falls back to borderless when exclusive acquisition is unavailable.
The Vulkan path checks surface support and releases ownership on focus loss.
See the [Khronos acquisition contract](https://docs.vulkan.org/refpages/latest/refpages/source/vkAcquireFullScreenExclusiveModeEXT.html).

This adds a native Vulkan GPU backend, not a D3D-to-Vulkan translation layer.
The current host still requires Windows and the pinned Windows SDK/AOT. A
Linux host, window/input integration, and Linux-compatible runtime/AOT are
separate work. The replacement ingame Options screen is also a later feature.

## Shared behavior and ownership

Only the pinned graphics implementation is compiled as a port-local override.
The public graphics ABI, command validation, generation-tagged resources,
producer/consumer queue, scene ordering, and host window stay shared with
D3D11. This GPU backend does not change guest code, simulation cadence, saves
or content. All 1,157 compiled AOT partitions and the immutable r354 snapshot
remain unchanged. The separate, opt-in language setting adds source-bound
load/save hooks described in `configuration-language.md`.

The native link audit still checks imports, required symbols, forbidden
owners, and closure. An explicit authoring copy admits exactly the additional
`sonic_vulkan` library and `native_port_graphics.cpp.obj` owner. This does not
disable the audit or alter the archived manifest.

Vulkan implements textures and mip uploads, persistent and transient meshes,
point/line/triangle primitives, culling, flat and smooth shading, logical
clipping, depth, alpha/blend rules, fog, movie images, overlays, frame capture
and Type-2 transparency with secondary accumulation. The Type-2 node format,
capacity, ordering and resolve shaders follow the pinned renderer. Three
fenced submissions own upload memory and deferred resource destruction.

Working and completed scene images are separate, allowing the host's 144 Hz
presentation thread to repeat a completed frame without advancing gameplay.
Hidden tests use the same Win32 Vulkan swapchain path as visible runs.

Shared HLSL is compiled to embedded SPIR-V using pinned DXC. Explicit Vulkan
adaptations cover resource bindings, Y orientation, one-pixel point size,
the attachment-free transparency gather, and UNORM24 depth quantization.
Depth quantization is also applied to transparency's manual depth comparison;
this preserves coplanar fragments on hardware that offers D32 instead of D24.
Shader extraction and adaptation fail if the expected source contract changes.

## Requirements and limits

The backend requires Vulkan 1.3, a graphics/present queue, dynamic rendering,
synchronization2, shader demote, fragment stores/atomics, geometry capability,
depth clamp, wireframe, anisotropic sampling, and a storage-buffer range large
enough for the configured Type-2 arena (about 512 MiB at the current default).
The tested GPU is an AMD Radeon RX 7900 XTX. Other vendors have not yet been
verified. An unsupported device reports a Vulkan initialization error; set
`renderer=d3d11` in the INI if the window cannot start.

GPU timestamp telemetry is not yet implemented for Vulkan. Shared process
CPU, simulation, submission and presentation measurements remain available;
a zero GPU-duration field is not evidence that rendering costs zero time.

## Verification

- Thirteen offscreen scene captures rendered through a real hidden swapchain
  match D3D11 pixel for pixel: texture sampling/update, retained geometry,
  clipping, Type-2 transparency and secondary accumulation, movie image,
  both windings, flat shading, lookup fog, lines, strips and points.
- Khronos validation, including synchronization validation, reports no Vulkan
  API errors in that final contract test. Stale external overlay manifests
  and disabled implicit layers produce unrelated loader warnings locally.
- Hidden, muted Emerald Coast runs through actual gameplay. Its captured
  world, characters, rings, water and HUD were inspected.
- In-process Options integration exercises all four aspect choices and both
  renderer choices, restoring the exact test configuration afterward.
- Projection/HUD tests still pass for 16:9, 64:27, 43:18 and original mode.
- Alt+Enter entry, held-key suppression, restoration and both swapchain resizes
  pass in a hidden owned test window. Real exclusive acquisition requires a
  foreground window and has not been exercised during silent desktop testing.

Local evidence is under `runs/renderer-contract-d3d11-full`,
`runs/renderer-contract-vulkan-final`, `runs/vulkan-ec-validation` and
`.local/vulkan-validation-final.log`. Validation is opt-in with
`SARECOMP_VULKAN_VALIDATION=1` and an installed/local Khronos layer;
`VK_LAYER_VALIDATE_SYNC=1` enables its synchronization checks. Validation
is disabled for performance measurements.

The configuration follow-up retains pixel equality across all thirteen
captures (`runs/renderer-config-vulkan`) and has no Vulkan API or synchronization
errors in `.local/renderer-config-validation.log`. The startup fullscreen/menu
regression is covered by the game-level check documented in
`configuration-language.md`.

These are bounded renderer checks, not a new full-story or full-matrix pass.
Performance comparisons are in `vulkan-performance-2026-09-12.md`; they do not
establish a simulation or CPU advantage over D3D11.
