# Native live render/motion hierarchy

The first implementation replaces a complete live model tree at `8C040784`,
its five SRT dispatchers, static SRT tails, motion sampling and six SDK matrix
owners. The group contains 23 owners, 1,167 ordinary instruction PCs and 81
delay slots; 17 exact source spans authenticate 2,644 original bytes. It shares
one admitted RAM view between known children. There is no runtime decoder.

Mutable SRT targets, object callbacks and model draws remain real boundaries.
Node flags retain their original register snapshot, while model, child and
sibling pointers are read live after calls. Sampling cursors and count tables
are published in their original order. The direct-rotation branch deliberately
uses the original stack values, including the saved PR/register words. Full
SDK non-null matrix paths, ordered alias effects and partial faults remain.

Arithmetic retains helper-local FPU scopes. Neither a broad FPU epoch nor a
captured immutable tree is introduced. An unsafe access resumes the actual
retained owner immediately before that instruction; delayed calls restore PR
before restart. Private continuations add no global dispatcher entries.

A completed tail callback needs special handling: if it invalidates the memory
or FPU admission after returning to its caller, the finished owner must not be
reentered at that caller address. The native runner propagates this state to
the parent. Ordinary access faults remain distinct even if PR aliases their PC.

## Qualification of revision A

Both Windows and Linux pass 198 exact original-byte comparisons, covering all
1,248 instruction/delay PCs. The oracle compares the complete architectural
CPU state, all 16 MiB of RAM and callback order at each foreign boundary and
at completion. Cases include five dispatchers, nested live trees, all sampled
channels, key counts, flags, mutation, address aliases, SDK matrix modes,
pair-register operations, stack overflow/underflow, interruptions and faults.

Both platforms also pass ten tests using the actual retained AOT continuation
bodies and 619 authenticated existing entry-table rows. These include ordinary
and observer-changing callbacks, misaligned/partial accesses, and both observer
revocation and FPSCR.SZ changes in the final SRT tail callback. The first eight
cases alone did not cover that tail failure; the final ten do.

Evidence: `runs/render-hierarchy-{windows,linux}-test-20260917-*.log`,
`runs/render-hierarchy-{windows,linux}-aot-test-20260917-*.log`.

The same-executable Windows Original Gamma pair (images 5..25) matches all 18
selected endpoint fields. Execution CPU/image falls 3.85%, execution cycles
fall 1.85%, and output remains capped at 25 images/s. The short CPU-time sample
is quantized; it is not a large throughput or Deck claim. The Recompiled check
passes at 60.024 new images/s, exactly 20 updates across 20 images, release=1,
logical delta=1 and video=60 Hz. Both use the same executable:
`372482a91c1da324b2418684ee167b32189e0cd561e33ecaa025257233c32e90`.

The Linux component-qualified revision-A executable is
`6bcb75449dad05f23eeb6f23d168cd7d33b0dbdd7edfc0648d8b567c8a8db860`.
Short same-binary Original Linux comparisons use four VM vCPUs and two
llvmpipe threads, images 5..25, 68 game updates, with matching configuration
and all 18 selected endpoint fields in every scene:

| Scene | Execution CPU/update | Process CPU/update | New images/s |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | -6.78% | -1.86% | +1.09% |
| Knuckles Sky Deck | +9.66% | +5.99% | -3.67% |
| Knuckles Lost World | +3.34% | +1.91% | -4.20% |

This mixed result does not qualify the first group for release. These are
VM measurements, not Steam Deck rates. Reports are
`runs/render-hierarchy-*-comparison-linux-20260917-a.json`. No native calls
declined or resumed in these gameplay windows. The group remains internal-OFF
(`SARECOMP_NATIVE_RENDER_HIERARCHY=1` opts in); published installers and the
September 17 v2 patch are unchanged.

## Complete two-pose group, revision D

The distinct two-pose tree `041A2E..041B0E` collects two channels, blends them
with `0417C8`, then performs callbacks, drawing, child recursion and siblings.
Its five record dispatchers, three static channels, three sampled channels and
three sampling children form a coherent additional closure. `057B00` is a
different matrix-output owner, not a replacement for this tree.

Preserve the signed-byte context offset versus unsigned channel indexes,
per-channel output and flag publication, live dispatcher targets, full SDK
matrix branches and the mutable rotation callback. `03700C` is an external
draw boundary. Additional morph/double-callback entry `041B0E` and its table
alternatives are outside the proposed normal root and must remain original.
The second group is now implemented in the same admitted closure: 39 owners,
2,034 ordinary instruction PCs and 146 delay slots, with both public tree
roots hooked. The existing pose bridge and active retained units compose
without duplicating ownership. Generation and original/output hashes are
checked before accepting either prepared input.

Both platforms pass 306 complete CPU/RAM comparisons covering all 2,180 PCs,
and 19 actual retained-AOT continuation tests using 1,030 authenticated entry
rows. Additional cases cover both live channels, all five dispatchers,
sampled keys, mutable callbacks, SDK non-null matrices, aliasing, faults and
observer/FPSCR changes. Callback boundaries compare CPU and all RAM exactly.

The tail correction now carries the actual transfer kind: an ordinary call
must not be mistaken for a completed owner when its incoming PR aliases an
internal continuation. The actual retained AOT can also return a pending
dynamic tail for its outer dispatcher. The native bridge finishes that tail
through the original callback service before returning to the parent. The
new PR-alias case checks all three callbacks and the exact final continuation.

Evidence: `runs/blended-hierarchy-windows-test-20260917-c.log`,
`runs/blended-hierarchy-windows-aot-test-20260917-d.log`, and
`runs/blended-hierarchy-linux-validation-20260918-d.log`. The updated adapter
declaration is included in the successful incremental revision-E game builds.
The Linux executable is
`597e6c542c3bab5d2b13f962167b565ff294852cb3014438a9a13ed64da315eb`.
These revision-E measurements are superseded by the scoped-proof revision H below.

## Shared admission across the complete group, revision H

The 39 owners now reuse source proofs only inside one admitted operation.
Every foreign callback revokes all proofs, including when it edits source RAM
without increasing a memory-generation counter. The returning parent is checked
again before its continuation. Native writes also reject code aliases even when
the caller's immutable guard omits that source. The bounded 4-KiB stack proof is
refreshed at those same boundaries; deeper or unusual accesses retain the full
per-access checks. There is no cache surviving the root operation.

Both platforms pass 310 exact CPU/RAM comparisons and 19 actual-AOT cases.
The four new cases cover parent, blend and future SDK source mutation and a
partially completed MOVCA store to an unguarded source. Revision E's Windows
ON/OFF capture at frame 265 is byte-identical. The later combined Windows
Original and Recompiled probes pass; native inverse-trig admission is a separate
experiment and is not implied by promoting this render group.

Short, quiet Linux pairs use exactly the same revision-H executable:
`ebb6e1266dc8aa2895cfe2f2447de8f442ac7efa4f77e5f0cdcebf03a6488407`.
All 16 endpoint fields and 68 updates match in each pair (images 5..25).
Standalone pose-call counters are intentionally replaced by the inlined closure.

| Scene | Execution CPU/update | Process CPU/update | New images/s |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | -8.48% | -2.25% | -0.19% |
| Knuckles Sky Deck | +0.37% | +0.77% | -1.20% |
| Knuckles Lost World | -6.93% | -4.77% | +7.24% |

Sky Deck is approximately neutral, not an improvement. The earlier revision-E
Lost World measurement overlapped a host build and is excluded. Revision H's
measurement window ended before the subsequent build began. These VM results
do not predict Steam Deck FPS and are not additive with earlier percentages.

The complete render group now defaults ON for both timing modes/platforms.
Global native-CPU OFF, per-group OFF and diagnostics retain original owners.
Other rejected experiments remain OFF. The published September 17 programs,
baseline and user saves have not been replaced by this source promotion.

Evidence: `runs/hierarchy-scoped-*-comparison-linux-20260918-h.json`,
`runs/hierarchy-scoped-proofs-*-20260918-h.log`, and
`runs/inverse-trig-gamma-*-windows-20260918-a/result.json`.
