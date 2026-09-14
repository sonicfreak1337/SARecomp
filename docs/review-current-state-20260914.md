# Review against the current Sonic port

The supplied review inspected `41e4e19` on the former camera branch, not the
current enhancement implementation. This pass started at `5a7eaef`. That entire
43-commit development history was first fast-forwarded and pushed to `main` at
the user's request. Integrated enhancement branches were deleted. r354 remains
immutable; merging development does not promote it to an accepted release.

| Review item | Current finding and action |
| --- | --- |
| Reduced-ratio render scaling | Confirmed. Round each raster dimension to the nearest requested pixel; keep output-based camera/HUD aspect. 1366x768 at 25% becomes 342x192; 1919x1079 becomes 480x270. |
| Old `sonic_options.hpp` whole-state writes | That module is gone, but the replacement menu and config program also wrote stale whole snapshots. Both now merge fields changed by that editor into fresh disk settings under one mutex. Restart uses the same merge. |
| Optional Vulkan repeat waiting for a busy slot | The successor-slot probe was already present. Retained and tested; `start(true)` now also uses an explicit zero fence timeout. Busy admission preserves the live command prefix. |
| Swapchain recreation inside optional repeat | Confirmed. A dirty swapchain defers optional output; real game-frame/host-image lifecycle work rebuilds it. Required presentation can still rebuild. |
| `OUT_OF_DATE` counted as presented | Confirmed. It requests recreation and reports Deferred; SUBOPTIMAL remains a successful present with recreation requested. Semaphore submission/waits are unchanged. |
| Writes next to the executable | Confirmed for INI, crash logs and automatic input recordings. They now share the existing writable save root. Portable mode is explicit. Existing saves stay in place; settings migrate only if the destination does not exist. Crash-log creation failure falls back to stderr. |
| Camera geometry parsing / per-point trig | Still a possible optimization, not a newly demonstrated functional fault or current CPU bottleneck. No unsafe address-only cache or collision rewrite was introduced. |
| Camera replay timing | Not changed; replay feature expansion was excluded by the user. |
| Old 16–18 FPS reports | Historical only. No new performance claim or whole-game diagnosis is based on them. The accepted native-owner/cache improvements remain active. |
| Descriptor batching, numeric pipeline keys, allocation suballocation and low-end capacities | Performance/portability proposals, not demonstrated failures of this build. Left out of this correctness batch after the user's request to end optimization. No geometry or transparency limit was lowered. |

The Vulkan result distinction and zero-time fence behavior follow the
[Khronos present contract](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueuePresentKHR.html)
and [fence contract](https://docs.vulkan.org/refpages/latest/refpages/source/vkWaitForFences.html).
Driver calls and resource creation are not claimed to have a hard real-time
latency bound. The change specifically removes our indefinite slot wait and
queue-idle swapchain rebuild from optional repeats.

## Validation

- `sonic_presentation_tests` passes: existing HUD/aspect and original-mode
  checks, 16 awkward-resolution/scale combinations, field/channel merges,
  explicit paths, portable precedence and one-time non-destructive migration.
- `sonic-config.exe --self-test` passes with hidden native controls, persistence
  and invalid-input rejection in its isolated configuration directory.
- `sonic_vulkan_present_tests` passes 22 cases, including every successor-slot
  index with/without a live prefix, busy deferral without submission, required
  work, late zero-time timeout, dirty-swapchain deferral and present outcomes.
- Real hidden `sonic_renderer_tests` passes on both D3D11 and Vulkan: 13 images,
  monitor rehoming and Alt+Enter/restore with no focus change. All 13 paired
  RGB captures are pixel-identical. Logs/captures are under
  `.local/menu-preview/review-20260914-{d3d11,vulkan}`.
- Real restart-watchdog component tests pass accept/reject/crash/start-failure,
  retaining a concurrent music-volume edit. Final default-user-root accept and
  reject cases also run the child's settings initialization while its parent
  holds the transaction lock. Existing-config reads stay lock-free to avoid a
  parent/child deadlock. These use a pre-game test child, not personal saves.
- `runs/review-20260914-sonic-scaled` boots Sonic into Emerald Coast on Vulkan,
  hidden/muted with copied saves and isolated forward input. The forward fixture
  owns a fixed 60-second interval (it supersedes the capture helper's requested
  10-second scenario deadline); it completes at 60,002 ms, frame 3316, then exits
  through the intentional deadline with zero runtime contract failures. The
  viewed `frame-750.bmp` shows intact scene/HUD and a real 342x192 raster for
  1366x768 output at 25%. Its crash-session file is under the isolated user
  root's `logs`, containing only the normal arming record. This product run
  preceded the final configuration-read lock correction; that correction is
  covered by the final default-root restart tests above.

Final build: `.local/menu-preview/review-20260914-final-build.log`, 75.180 s,
zero AOT recompiles, native-port and both FPU link audits pass. The earlier
combined component/product build took 103.984 s, also with zero AOT recompiles.
`out/experimental/game.exe` SHA-256:
`825c3f84ef0f5b0c874da945d1c4d7f800a12222e19761b44c927220e9af03e3`.
Reviewed provider identity:
`d0937199555c030982f71ff1f3f5d6ed5ed790dcf00cf222c53868f2168acb8a`.

No complete story matrix, personal save modification or new performance
comparison is part of this batch. The migration/component cases verify path
behavior; no claim is made of a new full installer/ACL deployment test.
