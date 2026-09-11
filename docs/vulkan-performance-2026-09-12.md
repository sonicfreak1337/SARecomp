# Vulkan / D3D11 comparison

The native Vulkan backend does not yet provide a consistent reduction in CPU
work or an increase in simulation FPS. Both renderers maintain 144 output FPS.

| Scene | Renderer | Sim FPS | Output FPS | Process CPU ms / sim frame |
| --- | --- | ---: | ---: | ---: |
| Emerald Coast | D3D11 | 16.049 | 143.922 | 73.465 |
| Emerald Coast | Vulkan | 15.909 | 144.010 | 72.278 |
| Windy Valley | D3D11 | 18.223 | 144.003 | 63.961 |
| Windy Valley | Vulkan | 17.603 | 144.005 | 66.147 |

Vulkan uses 1.6% less CPU time per simulation frame in Emerald Coast, but 3.4%
more in Windy Valley. Simulation is respectively 0.9% and 3.4% slower. These
small mixed changes do not justify claiming Vulkan is faster. Stable 30 Sim
FPS remains unmet in these measured paths.

All four probes use the same executable, 3182 x 1332 at 100% render scale and
144 target output FPS on the Radeon RX 7900 XTX. Each runs hidden and muted,
with copied saves, forward input for 60 seconds, and no frame capture or
validation layer. The first ten seconds are excluded from steady metrics.
Runs are sequential, with no concurrent game/compiler owned by this task.
Order: D3D11 EC, Vulkan EC, Vulkan Windy, D3D11 Windy.

Measured executable SHA256 (build 9, before the configuration/language UI):
`18ab07aeff19775c6b00d1b0db4987008c91cc87273a72377c9d0c13fd2b325f`.
Local evidence: `runs/vulkan-control-ec`, `runs/vulkan-candidate-ec`,
`runs/vulkan-control-windy`, `runs/vulkan-candidate-windy`. All completed the
gameplay probe with the expected deadline stop, no forced kill, and no crash,
dispatch or contract failure. This probe's expected deadline uses exit code 1.

The previous accepted D3D11 measurements were 18.67 Sim FPS / 61.49 CPU ms per
frame in Emerald Coast and 19.60 / 61.67 in Windy Valley. They were recorded
earlier, on a different executable and host-load interval. Fresh D3D11 controls
are also slower now, so the historical difference cannot be assigned to
Vulkan. See `performance-2026-09-11.md` for those experiments and provenance.

CPU time sums all process threads; it is not GPU duration, power draw or CPU
temperature. Vulkan GPU timestamps are not implemented. Frame-indexed input
means faster runs can travel farther. Desktop activity and CPU clock changes
add uncertainty. These are two representative stage paths, not a full matrix
or identical replay, and not a claim of whole-game renderer parity.
