# Native shared land display and model composition

This private experiment connects the common land task to its complete static
and animated display paths and the existing native model/motion hierarchy.
It is selected from the execution profile in
`native-model-roots-profile-20260918.md`, rather than from a Gamma-only path.
The delivered September 18 Deck patch, installers, baseline and saves are
unchanged. `SARECOMP_NATIVE_LAND_RENDER=1` opts in; the default remains OFF.

## Connected scope and boundaries

`tools/land-render-owners.json` adds twenty authenticated original owners,
883 reachable instruction PCs and 49 call sites. Together with the existing
render/motion hierarchy, the shared operation covers 74 owners.

The public entry is the land task `0519C0`, including its later continuation.
Its children cover `051E56` (live static list display), `051F64` (animated land),
`052048` (animation time) and `0520C8` (visible-list preparation). The group
also owns their material/context setup, point/matrix transforms and motion
wrappers. It reaches the existing model submission operation without another
independent RAM/source admission at each model. Enabling land rendering also
enables that shared model operation; it is not an isolated list-loop toggle.

The live reverse list at `75A24C`, record order, camera transforms and animation
state remain observable. Constructor/initialization and foreign callbacks
remain real calls. Neither visibility distances, task cadence nor original
animation timing are changed. Dynamic callbacks invalidate borrowed memory
and source proofs. Source-write fences and retained continuations remain.

The matrix restore `02E7E4` temporarily switches the SH-4 transfer width.
Its odd floating-point register encodings select the opposite bank's pairs;
the native operation preserves that behavior. Computed tails carry their
actual transfer site. The final revision also preserves the retained return
or tail source after a fallback completes, rather than borrowing a previous
child's return site. It never invents a global continuation entry.

## Correctness and builds

The component suite compares every CPU register, all RAM and callback order
against original PAL instructions with the original lexical FPU scopes.
It includes static/animated lists, empty lists, differing flags, live list and
transform changes at callbacks, both supported rounding modes, GBR aliases,
unaligned faults and observer replacement. Model/GPU callbacks are explicit
fixture boundaries in this suite; actual game rendering is checked separately.

Five cases also execute the retained original owner bodies through their real
sparse dispatch entries. Four exercise actual retained fallbacks, including
the observer-change continuation and its final tail source. These join the
64 instruction comparisons for 73 final cases, passing on Windows and Linux.
The previously affected
33 hierarchy AOT cases passed before that final metadata correction.

The hidden Windows Gamma pair matches all sixteen selected state fields at
both endpoints. Its captured frame 270 is byte-identical with the group OFF
and ON: `f351cb258cecce1622bc275cc6a06b47b9aeeef6eac57f85f2df24616c846498`.
The capture was visually inspected and is excluded from performance evidence.
The measured interval invokes the land root 48 times and keeps 8,091 model
calls inside the shared operation, with no hierarchy/contact resumes or model
revocations.

Both actual game targets build incrementally. There are two exact artifact
bindings: `runs/land-render-executables-20260918.json` records the initial
Gamma measurement revision; `runs/land-render-final-executables-20260918.json`
records the final retained-exit correction. Both verify all 27 allocated ELF
sections against the stripped Linux copy. The private Windows filename is
reused, so the final manifest is authoritative for the current executable.

## Bounded Linux performance comparison

The Gamma pair uses the same initial executable on both sides, Original timing,
Deck aspect, two software-raster workers and diagnostics/telemetry OFF. All
three related groups (movement/contact, model submission and land rendering)
are OFF in the reference and ON in the candidate. It is a combined result,
not an incremental gain over the previous model/contact percentages.

All sixteen selected endpoint fields and 68 game updates match. The native
land root executes 68 times; 10,643 model calls remain within the shared
operation. There are no hierarchy/contact resumes or model revocations.

| Scene | Execution CPU/update | Process CPU/update | New game images/s |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | -9.48% | -2.96% | +5.56% |
| Knuckles Sky Deck | -8.16% | -1.02% | -0.45% |

These are short QEMU/TCG measurements with software Vulkan, not Steam Deck FPS.
The final change affects retained-exit metadata only; that fallback is not
entered in the Gamma comparison. The unchanged Gamma pair is not repeated.
Sky Deck uses the final executable, matches all sixteen endpoint fields and
68 updates, and has no hierarchy/contact resumes or model revocations.

The combined group reduces execution CPU in both scenes, but whole-frame
throughput improves only in Gamma. Keep all three private groups OFF pending
a useful broader result. Do not repeat these completed pairs or the component
suite unchanged, claim Deck FPS, add older percentages, or package a new update
on the strength of this bounded VM result alone.

Evidence includes the two executable manifests, Windows capture comparison,
component logs, `runs/land-render-gamma-comparison-linux-20260918.json` and
`runs/land-render-knuckles-sky-deck-comparison-linux-20260918.json`.
The comparison JSON retains raw times, counters, endpoint values, exact hashes
and group configuration. Existing completed pairs are not added to these
percentages or rerun unchanged.
