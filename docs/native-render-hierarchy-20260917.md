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
Its gameplay measurements are still being collected. The group remains
internal-OFF (`SARECOMP_NATIVE_RENDER_HIERARCHY=1` opts in); published installers
and the September 17 v2 patch are unchanged.

## Next connected scope

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
This scope currently provides no new implementation or performance claim.
