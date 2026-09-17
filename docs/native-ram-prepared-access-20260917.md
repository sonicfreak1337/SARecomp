# Function-local prepared RAM capabilities

Experimental, internal OFF by default. The compile-time pilot options are
`SARECOMP_LINUX_RAM_PREPARED_ACCESS` and `SARECOMP_WINDOWS_RAM_PREPARED_ACCESS`.
Both default OFF, so ordinary generation omits the owner cache and extra argument.
Within a pilot build, `SARECOMP_RAM_PREPARED_ACCESS=1` selects the candidate
without rebuilding. This does not change Original or
Recompiled cadence, the delivered September 17 patch, or installation data.

The existing 41-unit RAM pilot has 1,490 closed prefixes in 510 owners. Each
prefix formerly reconstructed a writable View: registration-slot lookup,
observer admission, scoped writable-guard capture and backing validation.
The new owner-local optional retains that capability as a hint. Each prefix
still validates the original read guard, current Memory generation, registered
owner/immutable-guard pair and current observers. A miss captures a fresh View
or uses the untouched original instruction. No pointer outlives its Memory.

Read/write page proofs remain local to each closed prefix. Changing protected
code ranges between prefixes is observed even when the Memory mapping generation
does not change. Privilege/MMU state is captured anew. Access counts are committed
before callbacks/fallback, not at the outer function's exit. No RAM contents,
guest registers or original instruction results are cached across a callback.
This differs from the rejected observer-permission boolean cache, which left
the remaining writable-capability setup and Memory operations intact.

The source transformer authenticates the existing helpers and verifies that
removing every insertion recovers the retained source exactly. The original
fallback is unchanged. Disabled mode retains fresh capability capture.

Both Windows and native Linux pass 5,377 comparisons. This includes the 5,040
retained instruction/page/FPU cases plus 336 warm-cache/callback/mutation
combinations and one multi-owner/nested-context check. New coverage includes
stale versus fresh read guards, watches, trace/access sinks, observer replacement,
unbinding/rebinding, lookup/alias changes, privilege/MMU changes, new/removed
executable ranges, changed RAM values and equal-generation distinct owners.
CPU state, exceptions, callback ordering, counters and memory agree with the
independent original path. Both platforms also pass with the environment switch
disabled: the 5,040 existing cases then use fresh capture, while the 337 new
targeted cases explicitly force a warm cache to exercise its boundary contract.

Evidence: `runs/ram-prepared-component-{win,linux}-20260917.log` and
`runs/ram-prepared-component-{win,linux}-off-20260917.log`.

The Windows same-executable OFF/ON pair uses Original timing, Vulkan offscreen,
800x500 at 50%, no input or audio, and images 5..305. Both boundaries agree on
scene, game ticks, player position, HUD clock, cadence and native call counts.

| Scene | CPU OFF, ms/image | CPU ON, ms/image | CPU change | CPU-cycle change |
| --- | ---: | ---: | ---: | ---: |
| Gamma Emerald Coast | 22.239583 | 22.291667 | +0.23% | +0.21% |
| Chaos 4 entry | 20.989583 | 20.625000 | -1.74% | -1.59% |

Both remain capped near 25 new images/s by Original cadence. This does not
establish a useful global gain. Evidence:
`runs/ram-prepared-windows-comparison-20260917.json` and the corresponding
`runs/ram-prepared-win-*-20260917/result.json` files. Windows binary SHA-256:
`73725efa6aec448d91c44aa3e3aae50ac3fe9f97157d8e2d23cbc27c6872f160`.

The native Linux comparison uses the same executable for both settings, four
TCG vCPUs, two software-raster threads, 800x500 / 50%, Original, and images 5..45.
Every run completes normally with 40 new images and 136 game updates. Both
sample boundaries agree on the same state/cadence/native-call fields as Windows.

| Scene | CPU OFF, ms/update | CPU ON, ms/update | CPU change | New-image throughput change |
| --- | ---: | ---: | ---: | ---: |
| Gamma Emerald Coast | 225.493460 | 222.617193 | -1.28% | -0.45% |
| Chaos 4 entry | 229.326473 | 233.750571 | +1.93% | +0.04% |

There is no useful global or frame-rate gain. Do not enable, expand, ship or
repeat this unchanged experiment. These VM figures do not predict Deck FPS.
Evidence: `runs/ram-prepared-linux-comparison-20260917.json` and the four
`runs/ram-prepared-linux-*-20260917.json` / `.log` pairs. Measured Linux SHA-256:
`d48fece01206a783e581a5acf3adec6d1d3b42eb89733292eb915b6b8d4368c9`.

## Final integration and artifact identity

After the measurements, generation gained explicit compile-time pilot switches.
An independent comparison of all 41 files proves that OFF reproduces the
previous committed generator output exactly, and ON reproduces the measured
prepared files exactly. See `runs/ram-prepared-generation-gate-20260917.json`.
Only development caches retain the pilot option ON; its runtime switch stays
OFF by default. The source defaults are OFF. Final incremental builds pass,
including Windows map ownership for all 894 entries.

The final rebuild identities differ from the measured executables above:

- Linux `build-linux/game`:
  `57ddf8d0a5e14db8b8785c3592187c0ebf4ee96a3b08e4b9eb39cd589cb0d3da`.
- Windows `out/ram-prepared-windows-20260917/game.exe`:
  `14781ceed53d4da77832506ebba87f13b9042ae0293ddb7a268860bd47c66786`.

A final hidden Windows Recompiled Gamma probe with prepared access ON completes
60 new images and exactly 60 game ticks (126..186); clock=60, release=1, delta=1.
This checks integration/timing, not a new performance comparison. The final
Linux relink was built but not subjected to another unchanged gameplay suite;
the Linux performance evidence belongs to the explicitly identified earlier
executable. All 41 pilot source files are byte-identical across this packaging
gate change. No result is silently relabeled with the final artifact hash.

The owned VM upload was removed after collecting evidence, reclaiming
1,744,491,120 bytes. The delivered September 17 binaries and patch are unchanged.

## Next source findings

Several already-native families still compare entire expected retail byte spans
on each invocation: collision math, triangle contacts, animation hierarchy and
pose blending are confirmed examples. Presence of these comparisons is not a
measurement of their cost. Investigate a common identity-proof lifetime before
implementing a cache. Two important constraints from the pinned SDK:

- `tracks_address(a,n)` proves **any overlap**, not full byte coverage.
- Immutable-guard `generation()` counts observed writes; adding/removing runtime
  executable ranges does **not** advance it. A cache using only that generation
  would be unsound across module retirement/reload. Literal/data spans may also
  remain writable. Mapping, observer and Memory/guard lifetime must be bound.

An additional concrete native-data opportunity is visible in
`sonic_collision_math.cpp` and `sonic_triangle_contacts.cpp`: their already
admitted stores still call `Memory::try_write_direct_linear_u32` for every word.
The shared `sonic_native_model_memory.hpp` capability used successfully by model
owners could cover these closed ranges too. Triangle contacts must commit its
counters/drop its capability before each retained bridge call and recapture only
after its existing revalidation. Arbitrary stable observers must keep the
ordered original stores. This is a next implementation candidate, not a gain
established by this report. The triangle-contact body is statically expanded C++;
its word arrays are identity witnesses, not a runtime bytecode interpreter.
