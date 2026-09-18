# Native CPU groups, September 18, 2026

The user requested another Steam Deck update around 05:20 Europe/Berlin.
This work replaces connected original owners while keeping the authored game
timing, mutable draw/callback boundaries, exact arithmetic and guest-visible
state. Original and Recompiled share the same native execution policy.

## Connected implementation

- Collision-world construction: 11 owners, active-list membership, hierarchy
  traversal and polygon construction; see `native-collision-world-20260917.md`.
- Render/motion: the complete live one-pose and two-pose trees, sampling,
  transforms and SDK descendants, extended by the complete rigid object tree.
  Source proofs are shared only until a real foreign callback, then revoked.
- Inverse trigonometry: seven complete asin/acos/atan2 parents reuse the four
  existing native children. The exact original arithmetic scopes are preserved.
- Morph hierarchy: 14 additional complete owners are implemented and qualified
  functionally, but remain OFF because the short reviewed gameplay sections
  do not call this root. No performance benefit is attributed to them.
- The related inverse-trig memory transaction remains OFF after matched Gamma
  and Lost World pairs show no useful whole-frame gain. This does not disable
  the qualified inverse-trigonometry group itself.

The render closure's source dependencies are coalesced from 211 spans to 118
without changing their byte coverage. The generator verifies equality of the
original and merged byte sets. All generated function bodies, arithmetic
scopes and non-identity metadata remain byte-identical. This combines adjacent
literal checks and removes repeated checks of literals already inside a body;
it does not persist permissions between calls or suppress invalidation.
Evidence: `runs/render-source-coalescing-proof-20260918.json`.

## Qualification

Windows and Linux each pass 1,319 exact inverse-trig cases plus 199 memory
transaction cases and 158 actual-AOT cases, and 502 exact shared-hierarchy
cases plus 33 actual-AOT cases. The latter
cover 3,123 instruction/delay PCs. The original unchanged exact assertions
exposed and then verified corrections to host-FPU scope boundaries. Real
retained-AOT continuations remain independently checked, including observer
replacement, changed draw state and partially committed memory faults.

The isolated rigid revision D regresses Lost World and is not the delivery
policy. The corrected complete-family revision C authenticates only entered
owners at each root while preserving all mutable boundaries. Its matched
three-scene measurements qualify the rigid/render group together: execution
CPU changes -10.62% / -3.63% / -1.32% for Gamma / Sky / Lost, and images/s
change -0.76% / +0.57% / +2.04%. See the rigid-hierarchy report for its exact
binary identity and scope. Individual
optimization percentages must not be added; the net comparison uses the
previous delivered September 17 v2 executable as its control.

These are short Linux VM windows with four emulated vCPUs and two software
raster threads. They are neither Steam Deck FPS measurements nor a full story
or complete-level regression run. The 20–25 ms Deck target remains open.

The preliminary direct comparison to the delivered September 17 v2 patch
matches all 16 endpoint fields and 68 updates in Gamma. Revision D, with the
memory extension OFF, reduces execution CPU/update by 13.37% and process
CPU/update by 4.15%; new images/s rises 1.77%. This includes all active groups
and dependency coalescing, not an addition of individual percentages.
`runs/net-gamma-candidate-memory-off-linux-20260918.json` records this
development-program comparison. The final installed-program check is separate.

## Update mechanism

The package supports both the September 17 v2 runtime and the stripped runtime
in the September 17 Steam Deck/Linux test installers. It authenticates either
known installed SHA-256 and unpacks one complete compressed program. This
avoids requiring a stripped installation to have the other delta's reference
program. Original disc/content files are not shipped or reinstalled.

The same atomic publication, reversible program/manifest backups, running-game
rejection and integrity checks remain. All supported installed launch paths
are updated; Steam shortcuts continue to work. Story/Chao data, configuration
and diagnostic policy are preserved. Small matching diagnostics ON/OFF
packages change policy only and cannot replace or downgrade the program.

The updated installer engine passes 18 Linux fixtures, including both existing
delta behavior and full-runtime success, corruption rejection and rollback.
The actual self-extracting package also passes against both real prior
executables in isolated Linux installations. It verifies original-program and
manifest backups, updates both launch directories, preserves Story/Chao/settings
fixtures and existing diagnostic policy, and tolerates a second application.
Both directories share the verified replacement inode. ON/OFF and repeated OFF
change policy only; program bytes, inode and mode stay unchanged. Both source
installations remain unchanged. Evidence:
`runs/native-groups-actual-install-linux-20260918.log`.

The reported Deck crash without a capsule or reliable reproduction is still
unresolved and is not claimed fixed by this update.

## Final installed-program comparison

The final stripped program has identical loaded ELF sections to the qualified
development executable. The two incremental release builds require no work;
no whole-AOT regeneration is performed. Runtime source is commit `2e745a5`.

The installed Original Gamma window matches all 16 selected endpoint fields,
68 updates and 20 new images against the September 17 v2 control. This is the
release result, superseding the preliminary development-program percentage:

| Metric | September 17 v2 | Installed September 18 | Change |
| --- | ---: | ---: | ---: |
| Execution CPU ms / update | 180.2463 | 159.4282 | -11.55% |
| Process CPU ms / update | 790.5531 | 770.1599 | -2.58% |
| New images / second | 1.22617 | 1.23024 | +0.33% |

New-image throughput is effectively unchanged in this software-rendered VM.
Do not describe the execution CPU reduction as a measured Deck FPS gain.
No compiler or encoder overlaps either measured window. The installed run
positively exercises 2,608 render roots, 147,224 internal render calls,
2,176 rigid roots and 12,343 inverse-trig parents, with zero declines or
resumptions. Morph and inverse-memory extensions remain OFF.
Evidence: `runs/native-groups-installed-net-linux-20260918.json`.

The final installed program also completes one direct v2 comparison in each
requested Knuckles stage. Both match all 16 endpoint fields, configuration,
68 updates and 20 new images. No compiler, encoder or bulk transfer overlaps
the measured windows. These are net whole-update comparisons, not sums of
earlier individual switch gains:

| Scene | Execution CPU/update | Process CPU/update | New images/s |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | -11.55% | -2.58% | +0.33% |
| Knuckles Sky Deck | -5.73% | -0.66% | -0.71% |
| Knuckles Lost World | -8.05% | -4.50% | +8.94% |

Sky Deck execution CPU/update falls from 216.7417 to 204.3202 ms; Lost World
falls from 277.1916 to 254.8738 ms. Their new-image rates are respectively
0.70082 to 0.69585 and 1.04037 to 1.13340. Gamma and Sky Deck image throughput
are effectively unchanged; Lost World improves in this short VM window.
None of these measurements establishes Steam Deck FPS or attainment of the
20-25 ms Deck target. Reports:
`runs/native-groups-net-knuckles-sky-deck-comparison-linux-20260918.json` and
`runs/native-groups-net-knuckles-lost-world-comparison-linux-20260918.json`.
Both exercise render, rigid and inverse-trig groups with zero declines or
resumptions; the two excluded extensions remain unused. No further unchanged
comparisons or full-matrix runs are needed for this delivery.

The same installed Linux executable also passes the Recompiled 5..25 window,
at video/release/delta 60/1/1. Original remains 50/2/2. Both have positive
collision-world, render-hierarchy and inverse-trig admission, no render or
inverse declines, and zero calls to the two excluded extensions. Reports:
`runs/native-groups-installed-{original,recompiled}-linux-20260918.json`.

Hidden Windows Vulkan checks also pass in Original and Recompiled with normal
native-group defaults. They preserve respectively PAL video/release/delta
50/2/2 and Recompiled 60/1/1. They are functional checks, not performance
comparisons, because compression was active on the host.

| Artifact | SHA-256 |
| --- | --- |
| Linux unstripped program | `d90b6b3474a897413f042fb0afdc7b58374831ebf9d8090d18d3df275a0c84af` |
| Linux/Deck stripped program | `55889c92ee5c5ab6a34f1d8733a8698bab6da3b20eaa416fa48b593c0b335f71` |
| Windows program | `70f63cbf5444b98a319d5e9d84575d70962ea6988d8d5537b481311ad642f1b4` |

The program has 1,684,458,312 bytes after stripping. The update container has
262,349,464 bytes; each policy-only diagnostics switch has 21,144 bytes.

## Published update

Published September 18 at 04:42 Europe/Berlin, ahead of the requested 05:20
deadline. The delivery copies the exact self-extracting packages already
executed in the Linux installation check; no payload or wrapper is rebuilt.
Publication verifies all three source and destination SHA-256 values:

| File under `out/patches/` | SHA-256 |
| --- | --- |
| `SonicAdventureRecompiled-CPU-Update-2026-09-18.run` | `173c0754bad9b429ef5e31260d17f34a07bce1dc69b09f119bb1ed24d3c27003` |
| `SARecomp-Diagnostics-ON-2026-09-18.run` | `a87a12849e432c906038ba34efd685cd66493c971cfb411e41c39b77f05e2b1e` |
| `SARecomp-Diagnostics-OFF-2026-09-18.run` | `25501115ba71cd8b5134cc6f9bedd1fe948e3cb84bdf4532c881a405ae238615` |

The adjacent dated README and JSON metadata give installation instructions and
the tested program/source identities. Evidence:
`runs/native-groups-publication-20260918.json`. The previous v2 update and
existing full installers remain unchanged; this delivery is a runtime patch.
The next connected-work profile is in `native-groups-next-profile-20260918.md`.
