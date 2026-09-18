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
Final package hashes, real installation and installed-game evidence are added
after the delivery artifact is built. The reported Deck crash without a
capsule or reliable reproduction is still unresolved and is not claimed fixed.
