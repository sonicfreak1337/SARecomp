# Complete collision-world production group, 17 September 2026

The complete collision-world producer is implemented as one native group.
After the three matched comparisons below, it joins the **default-ON native
CPU group** for the next Windows and Linux development builds. Internal
`SARECOMP_NATIVE_COLLISION_WORLD=0` or `SARECOMP_NATIVE_CPU_PATHS=0` restores the
original owner; diagnostics also retain the original route. Existing installers
and the delivered v2 patch are unchanged. Admission preserves mutable callbacks
and original memory/FPU behavior. This is not a new baseline/release promotion.

## Scope

Eleven whole owners cover candidate selection, active-list reconciliation,
object allocation/recycling, hierarchy transforms, vertex conversion, polygon
production/allocation, depth buckets and final list assembly:

| Owner | Original entry |
| --- | --- |
| Collision-world transaction | `8C028EC2` |
| Eligibility selection | `8C052518` |
| Object hierarchy | `8C028B00` |
| Polygon conversion | `8C0287A0` |
| Vertex conversion | `8C028666` |
| Object allocation | `8C02CEF4` |
| Object release, including backwards branch to `02CF20` | `8C02CF48` |
| Polygon dot product | `8C02CF68` |
| Bucket clear | `8C02CFA4` |
| Polygon allocation | `8C02CFC0` |
| Bucket joining | `8C02D00E` |

Finite build-time authoring emits C++ for 1,326 normal instructions and 166
delay-slot instructions. The game contains no new instruction decoder. Source
admission covers 3,796 original instruction/literal bytes. Only the root entry
is hooked; the remaining owners share its native transaction privately.

Two local native indexes replace nested list searches. They retain **first**
eligible occurrence, raw 32-bit key equality (including zero and P1/P2 aliases),
duplicate new candidates, active-list order and live special-key clearing.
They are used only after validating the original pools and disjoint live data;
malformed, aliased or oversized input retains the original uncapped search loop.
Polygon order, bucket order, pool exhaustion and partial production remain
original. This is not a distance/culling or activation-range change.

One admitted RAM capability covers each callback-free stretch. Guest-visible
writes remain immediate, including stack arrays, output pools and counters.
External calls flush accounting, release arithmetic state and revalidate the
context on return. Each internal owner also closes/reopens its FPU epoch;
arithmetic epochs are not broadened across matrix/SDK calls.

Unsafe accesses resume before the exact original instruction. A delayed-call
failure restores PR and resumes the original branch once. Private local AOT
continuations cover nested geometry, pool and eligibility owners without adding
public global dispatcher entries. An original stop/trap is classified as an
interruption, not successful native completion.

## Evidence and identities

- Original PAL RAM SHA-256:
  `b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c`.
- Initial Windows qualification game SHA-256:
  `c845791b11d73d7b7a131b99075375a6cd0601de19d001e5cca1cbb45a66e097`.
- Initial Linux qualification / Gamma comparison game SHA-256:
  `b971c496164cac2966b61de2469ab6a25e4e2a62a5e8807a129758ed7b3a286f`.
- Initial shared provider identity:
  `8fdefcf206df8093f5714a92f798cfe3fd24c24066b5cc05312cbad961b28e50`.

The bridge authenticates three original retained AOT units and any existing
RAM-region preparation before editing derived units. It does not regenerate
the whole AOT pack or modify the baseline. Build metadata lives in
`build-*/generated/world-bridge/` and `collision-world-resolver/`.
There are 207 new private local continuation routes (150 geometry, nine pool,
48 eligibility); the original public entry table remains unchanged.

Windows and Linux each pass 78 original-byte reference cases (26 fixtures, three FPU/bank
configurations). Full architecture and 16 MiB RAM match at each real external
callback entry/return and at completion. This corpus visits 1,178 of 1,492
instruction PCs; it is not an exhaustive branch claim. Original SDK matrix/math
bodies execute in the oracle. The alternative eligibility producer and debug
outputs use matching test fixtures.

Eight additional actual-retained-AOT cases pass on each platform, including nested
misaligned-access traps, exhausted polygon allocation and an observer installed
by a callback. The fixture uses the actual 735 original chainable entries of
these owners; private `0287C8` remains absent from the global entry table.
Already executed callbacks are not repeated during fallback.

The first Linux component invocation was stopped after profiling showed 95%
of its sampled time in the compiler runtime's bytewise `bcmp`. Both component
executables now link the game's already qualified bounded/glibc comparison
object. The complete suite then passes with the full comparisons intact. This
is a test-harness correction, not a new game optimization. Final Linux evidence:
`runs/collision-world-linux-tests-20260917-b.log`.

A short hidden/muted Windows Gamma Emerald Coast Recompiled run reaches its
requested frame window with the new group active, zero declines/restarts and
unchanged one-update/one-image 60-Hz timing. It is a function check, not a
Windows performance comparison.

The fresh pre-change Linux Gamma CPU profile assigns 3.06% inclusive sampled
execution time to world production (the 1.68% eligibility figure is included,
not additive). Model and movement work still dominate that entry window.
Profiles identify scope; they do not establish the new implementation's gain.

## Short Linux gameplay comparisons

The comparison uses four TCG vCPUs, two llvmpipe threads, 800x500 at 50% render
scale, 16:10 culling and Original timing. Diagnostics and provider timers are
OFF. The shipped object/model group and BASE RAM/transfer paths remain ON;
movement, collision closure and other unqualified experiments remain OFF.
Only the collision-world switch changes in each same-executable pair.

Gamma Emerald Coast matches all 19 endpoint fields at frames 5 and 25,
including bit-exact position, HUD, animation counters and 50/2/2 PAL cadence.
Both sides execute 68 updates for 20 new images. Native root calls advance
190 to 258, with no declines/restarts. Thus the subsequently corrected fallback
return marker is not exercised in this measurement.

| Gamma metric | OFF | ON | Change |
| --- | ---: | ---: | ---: |
| Execution CPU ms / update | 192.342 | 185.339 | -3.64% |
| Process CPU ms / update | 818.919 | 810.139 | -1.07% |
| New images / second | 1.0465 | 1.0530 | +0.62% |

This is a small execution-time reduction, not a substantial frame-rate gain.
Evidence: `runs/collision-world-gamma-comparison-linux-20260917.json` and the
same-named OFF/ON summaries. These are VM measurements, not Steam Deck FPS.
Knuckles Sky Deck also matches all 19 fields at both boundaries, including the
same 68 updates. The final binary is used for both sides. Native calls advance
190 to 258 with zero declines/restarts; 6,616 membership searches and 6,614
eligibility searches take the local indexed path in the measured window.

| Sky Deck metric | OFF | ON | Change |
| --- | ---: | ---: | ---: |
| Execution CPU ms / update | 228.986 | 214.226 | -6.45% |
| Process CPU ms / update | 1402.162 | 1354.404 | -3.41% |
| New images / second | 0.58365 | 0.60485 | +3.63% |

Evidence: `runs/collision-world-knuckles-sky-deck-comparison-linux-20260917.json`.
Knuckles Lost World matches the same 19 fields at both boundaries and performs
68 updates. Root calls advance 190 to 258 with zero declines/restarts, and both
lookup indexes handle 1,768 searches each in the measured window.

| Lost World metric | OFF | ON | Change |
| --- | ---: | ---: | ---: |
| Execution CPU ms / update | 279.077 | 256.459 | -8.10% |
| Process CPU ms / update | 798.274 | 762.283 | -4.51% |
| New images / second | 1.03322 | 1.13180 | +9.54% |

Evidence: `runs/collision-world-knuckles-lost-world-comparison-linux-20260917.json`.
All six stage probes finish at the expected host deadline without a forced stop
or game failure. These are single short entry-window pairs, not full-level,
story or hardware qualification. No extra unchanged pairs are run to chase
better percentages. The 20-25 ms Steam Deck objective remains open.

The coherent group improves all three measured execution costs and image rates,
so it is retained in the next development default alongside the shipped object
and model groups. Movement, closed collision math and other unqualified switches
stay OFF. No patch/installer is produced from this round alone.

The final source audit additionally corrected the dispatch return-site marker
after a complete retained fallback. The authenticated root has one RTS at
`8C029398`; its fallback must not publish a stale child return or zero. The eight
actual-AOT cases now reset and assert that marker, and pass again on Windows
and Linux (`runs/collision-world-{windows,linux}-aot-final-20260917-c.log`).
No ordinary arithmetic, indexed lookup or callback path changed. The full
78-case suite is not repeated unchanged for this metadata-only correction.

Return-boundary revision Windows game SHA-256:
`65bc00d0b6efa3f4dfe1d0ce7af58f7ac4e51d477b370f6991d18623d68ea093`.
Return-boundary revision Linux game SHA-256 (both Knuckles pairs):
`066f110b7f5068b9fc7c1d05680955af8a8ba4da04322f4323b1ef4fb44afe06`.
Return-boundary revision shared provider:
`6b32a7edc1a66ce0a45d8caf786af6df11a6ea7a18b76ac26d921ed31ebe3b2e`.
The Gamma performance hash above intentionally identifies the pre-correction
executable. Do not attribute those measurements to a different binary silently.

Default-enabled development builds, with the compared arithmetic/indexes and
corrected return boundary unchanged:

- Windows game: `e665447cd4a011f3453eb467db92df54b62f2bdce20694366d3132e0ed9d0d87`.
- Linux game: `0cf1db2fcec9fdc2d5c8beecfdafd709152a4097bccfdc4de17e0bc4d88c98bb`.
- Provider: `dbda2e2963d98629858e96896422b6b17a1e7221a6242bb3722c2cd7908763ae`.

Both builds complete incrementally. The eight actual-AOT cases pass on each
platform with feature/group/diagnostic environment overrides absent, exercising
the new default gate (`runs/collision-world-{windows,linux}-default-test-20260917-d.log`).
No extra stage pairs are run for the startup-policy-only change. The benchmark
tools now default to `installed`, keeping explicit ON/OFF comparisons available.
The VM's staged game remains the recorded Knuckles comparison binary; the newest
default-enabled Linux executable is `build-linux/game` on the Windows host.

The initial Linux control launch stopped before gameplay because the isolated
executable directory lacked `assets/ui/options-background.png`. The test now
uses the existing authenticated read-only asset directory through a relative
link. The failed launch is excluded from performance results; neither code nor
installed assets were changed to bypass that identity check.

## Next related boundary

The measured native world still crosses 11,968 external boundaries in Gamma
and 20,366 in Sky Deck over 68 updates. The next related scope is the complete
SDK matrix/transform group used by selection and geometry production: point
transform `638E0C`, push/pop `639BB0/639AD8`, rotation `639E08/639E9C/63A10C`,
transform helpers `63A744/63A52C`, identity load `63A820` and signed square root
`63A904`. Addresses above are prefixed `8C`.

Audit their actual register, matrix, stack and memory effects as a whole before
sharing admission with the world owner. Existing matrix kernels and original
bytes are references; the retired NEAR-POLY experiment is not an implementation
to revive. Preserve each arithmetic epoch and retain real callback handling
for alternative selection, debugging and unsupported modes. The external-call
counts identify repeated work, not measured CPU time or a promised speedup.
No further optimization or extra gameplay tests of this next scope were started
in this bounded three-stage round.
