# Original 50/60 Hz options

The original ADVERTISE screen-frequency menu targets Dreamcast TV hardware.
In this PC port, selecting 50/60 Hz, applying it, or using Test leaves the
video mode unchanged. The original menu navigation and Test countdown remain
available. PC resolution, presentation FPS, renderer and window mode belong
to `sonic-config.exe` and the host configuration.

## Failure and shared boundary

The user's capsule `katana-crash-session-1789211814938-6604.log` failed at
frame 1906: guest PC `8C653388`, PR `8C6577CC`, error `53415816`, destination
`A05F8124`, value `00518C80`. The existing provider permits a bounded
SOFTRESET write at `A05F8008`; the TV-mode reset instead reached that leaf
with a different PVR configuration register. ADVERTISE resources had already
been retired when the contract failed. Ignoring the final MMIO operation
would leave that teardown in place.

One source-bound entry, `8089928E` (runtime `8C90E28E`), owns all three paths:

| Original call | Return | Action |
| --- | --- | --- |
| `8C90E7FA` | `8C90E7FE` | Apply selected 50/60 Hz; persist flag 1 |
| `8C90E46E` | `8C90E472` | Test 60 Hz; persist flag 0 |
| `8C90E4AC` | `8C90E4B0` | Restore previous mode after Test; persist flag 0 |

The adapter returns immediately at this entry, before its first texture
release, SDK shutdown/reinitialization, or persisted TV-mode write at
`8C754B44`. All three callers ignore the return value. The hook preserves
the complete CPU and guest RAM state; normal UI control resumes at PR.
It does not alter PVR providers, renderer state or game cadence.

The manifest binds the `D8`-byte function with SHA-256
`2eea7fcacf69722f68fb85461b4a455b2ae301aa89b6785df124ad3a95a32bb8`
inside ADVERTISE module
`6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5`.
Provider refresh admits only this reviewed additional entry and verifies its
existing frozen shard `98441`. Direct AOT chaining must pass back through the
new hook. No original AOT partition or accepted r354 file is rebuilt.

## Checks

Build `sonic_legacy_video_tests` alongside `game` with `tools/build.ps1`.
Run the test executable with the installed content root. It executes the
original BSR/delay instructions for Apply 50, Apply 60, Test and restoration
to either mode, then checks the real post-call menu state writes. Every
intercept checks full RAM and register preservation. Device accesses fail
the test. The reference interpreter is a test-only dependency.

The user's original input trace is also replayed against the actual game,
hidden and muted, with copied saves and a separate display configuration.
This menu replay reproduced the exact pre-fix failure, providing a matching
runtime regression case. Local evidence is under `runs/legacy-frequency-*`.

## Verified build, 2026-09-12

- All five retail caller cases pass with preserved RAM/registers and the
  expected post-call UI state. `sonic_legacy_video_tests` supplies the check.
- `runs/legacy-frequency-before` reproduces the user's PVR failure at frame
  1906. `runs/legacy-frequency-after` reaches the new Test interception at
  frame 1905; the original trace then ends, so the strict replay reader stops
  at frame 1907 with `input-replay-exhausted`. This is not a completed test.
- The continued trace preserves all 1,909 original samples byte-for-byte
  and appends 6,000 neutral polls. Only its header counts are updated.
  `runs/legacy-frequency-continued/result.json` records the trace identities.
  The Test call returns at frame 1905; restoration to mode 1 returns at
  frame 2214. The game reaches frame 3841 without a contract failure, then
  stops at the planned 85-second HostDeadline (reason 2, process exit 1).
- Inspected captures `frame-5900.bmp` and `frame-8900.bmp` show the Test
  countdown and the restored frequency menu. This runtime replay used
  Vulkan; Apply 50/60 is covered by the original-caller component checks.
  No level matrix or physical controller test was needed for this boundary.
- The final incremental build took 30.949 seconds with zero AOT recompiles
  and passed the native link audit. Baseline executable/metadata checks pass;
  the user's display INI hash remains unchanged.

Final `out/experimental/game.exe` SHA-256:
`570eb31fc2f0e92988bfc3d3575897aac21252d4014059a05d4c6259b9ba6a1a`.

## Original clock mapping, September 13

The component test additionally executes the original TV persistence branch
and both SDK packet constructors in isolated RAM, stopping before hardware
apply. It confirms menu 1 -> stored word 1 -> mode 58 -> PAL50 constructor,
and menu 2 -> stored word 0 -> mode 56 -> 60-Hz constructor. Apply/Test/restore
interception still passes all five existing cases; four persistence cases
and two original constructor packets pass. See `docs/sixty-hz-feasibility.md`
for the exact tuples, component boundaries and setup corrections.
No game, personal save, display setting or product clock was changed by this
check. Only `sonic_legacy_video_tests` was incrementally compiled.
