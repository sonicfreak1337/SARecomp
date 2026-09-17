# Whole-owner native memory access

The September 17 native CPU candidate enables closed model memory for
five already-native families: palette lighting, matrix stack, matrix vectors,
matrix inverse and motion sampling. Complete preflight still admits every
read/write/code range. Within the callback-free owner, the registered product
observer permits one writable RAM capability instead of repeating scalar
observer/mapping work for every ordered write. Aliasing and read-after-write
behavior remain intact. Unknown observers retain the public write path.

`SARECOMP_NATIVE_CLOSED_MEMORY=0` retains the previous scalar publication.
See `native-cpu-defaults-20260917.md` for combined measurements and policy.

Animation hierarchy and pose blending use the same capability independently
of the leaf switch. Their admission no longer depends on enabling the separate
AOT scalar/RAM-region experiments. No guest timing or callback is skipped.

Windows `SARECOMP_NATIVE_MEMORY_CAPABILITY` builds the same prepared Memory
and service binding into the existing admitted runtime archive. No pinned SDK
files, public class layouts or AOT generation are changed. The complete Memory
member replaces its original archive member; duplicate-symbol suppression is
not used. Original public writable-guard requests with observers remain denied
outside the private capture. Functional code invalidation remains active.

## Qualification

- Windows and Linux: 588 differential leaf cases compare CPU state and all
  16 MiB of guest RAM, including product-observer direct writes and revoked
  permission fallback. Both witness 2,396 direct words with scalar/RAM-region
  flags explicitly OFF.
- Both platforms: 108 hierarchy and 76 pose cases, plus 18 mutation-free
  rejection cases, pass with the product observer and direct-write witnesses.
- Windows Gamma, same executable, 5..605: both runs pass and produce the same
  endpoint XYZ bits and HUD timer. Native animation/pose and asynchronous audio
  are ON in both; only the closed-leaf switch differs. Producer CPU is 15.3125
  versus 15.4948 ms/image (1.18% less); cycles 66.6339 versus 68.1504 million
  (2.23% less). Both remain capped at 60 new images/s. This is a small gain.
  The ON run witnesses 1,382,962 calls / 29,468,373 words over its full lifetime;
  OFF reports zero. These totals are admission evidence, not steady-window cost.

Evidence: `runs/native-memory-{windows,linux}-*-test*.log`,
`runs/closed-memory-windows-{on,off}/result.json` and their game logs.
Linux Original Gamma and the Chaos-4 stage-entry/cutscene probes now complete
with this capability enabled. Windows Original Gamma and Chaos-4 entry also
pass; the latter witnesses 7,075,330 direct words. Both retain 50 Hz video,
release slots 2 and logical delta 2. Evidence:
`runs/native-memory-linux-original-gamma-summary.json`,
`runs/native-memory-linux-chaos4-intro-command.log` and
`runs/native-owner-windows-{original,chaos4-intro-b}/result.json`.
These short probes do not claim a full boss playthrough. No installed patch
has been changed and no Steam Deck speedup is claimed from these tests.
