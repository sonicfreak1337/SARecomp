# VSync and renderer-change startup failure

The reported VSync slowdown was on Vulkan. The two subsequent startup crash
capsules (`1789314030915-17876` and `1789314035163-7080`) instead identify
D3D11 Present: operation digest `5063765699783595554`, HRESULT `887A0001`
(`DXGI_ERROR_INVALID_CALL`), before any presented image at guest frame 1.
The saved configuration selects D3D11, exclusive fullscreen and 3440x1440.

## Startup/fullscreen correction

The flip swapchain entered exclusive fullscreen through `SetFullscreenState`
but the resize path returned early when the client dimensions already matched.
[Microsoft's contract](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-setfullscreenstate)
requires `ResizeBuffers` after this transition regardless of size equality.

Fullscreen entry/exit and same-size display/focus transitions now invalidate
the swapchain buffers. The normal consumer-owned resize boundary releases the
backbuffer view, recreates buffers/views and restores any open working frame.
Alt+Enter is owned consistently by the host on both renderers; DXGI's implicit
Alt+Enter is disabled so it cannot bypass this invalidation.

## Separate presentation from simulation progress

VSync previously disabled nonblocking presentation. FIFO image acquisition
could wait indefinitely, and the shared render consumer repeated old images
after resource/geometry prefixes and within long draw lists. The simulation
could then wait for replies from a render owner sleeping for the display.

Independent presentation now keeps VSync enabled while deferring unavailable
output: Vulkan uses zero-timeout image acquisition and checks the next fenced
submission slot without waiting; D3D11 uses SyncInterval 1 with DO_NOT_WAIT.
[The DXGI flag contract](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-present)
explicitly provides a busy result instead of a thread wait. Serial paths keep
their existing behavior. Vulkan FIFO is retained; this does not disable VSync.

Queued commands take priority over idle image repeats. A completed Recompiled
frame no longer waits for a future output deadline before publishing its guest
render-completion ticket. The retained completed image is retried at bounded
display-rate deadlines. Original keeps its authored scene cadence; repeats do
not acknowledge guest render work or advance simulation.

## Verification and limits

- Hidden renderer fixtures pass on D3D11 and Vulkan. All 13 captured BMPs match
  byte-for-byte between backends. Same-size display recovery, Alt+Enter entry,
  key-repeat suppression and restoration pass without changing desktop focus.
- The resource-prefix pacing fixture reaches 59.960 simulated updates/s on
  D3D11 and 59.722 on Vulkan with VSync enabled. Vulkan retains FIFO.
- An injected temporarily busy presentation preserves forward progress.
  D3D11 Original transitions through 25/60/30/50 Hz without extra repeats.
- The render-completion contract passes ordered guest/host image ownership,
  resource prefixes, reset boundaries and 4,096 concurrent completion tickets.
- These are hidden-window tests, not measurements of physical visible scanout
  or a real exclusive desktop mode change. The fullscreen root cause is backed
  by the exact crash HRESULT/operation and the violated API contract.

The user confirmed Vulkan VSync is working on 2026-09-13. Read-only process
and session-log observations also show manual starts of both D3D11 and Vulkan
on the new build closing normally; this is separate from the hidden fixtures.

The final incremental build took 37.422 seconds with zero retained AOT units
recompiled and passed both FPU ownership audits and the native link audit.
Executable SHA-256:
`662bc66a636658c75ee7e4042d7e98763a2977c1ddbe055633959ac8278df27b`.

| Windy Valley, Vulkan, 3440x1440, VSync on | New draws/s | Execution CPU ms/update | Output/s |
| --- | ---: | ---: | ---: |
| Before, `windy-vulkan-vsync-before-01` | 55.500 | 17.153 | 143.887 |
| After, `windy-vulkan-vsync-after-01` | 54.942 | 17.300 | 143.955 |

Both standard Recompiled runs completed the same hidden/muted 60-second
forward route with copied saves and no crash. Task and timer rates match new
draws. This pair does **not** show lower execution CPU cost; hidden-window
presentation did not reproduce the user's visible VSync stall. It establishes
that the remaining action-stage CPU bottleneck needs separate optimization.

`windy-global-cpu-profile-20260913-01` additionally sampled the real execution
thread for 15 seconds (973 samples) with a matching executable/map. Its timing
is instrumented and not part of the comparison. Shared FPU, RAM/immutable
guards, dispatch and model drawing remain the next CPU investigation targets.
