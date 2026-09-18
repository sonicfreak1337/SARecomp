# Complete morph hierarchy and shape sampling

This private group adds 14 complete PAL owners to the shared render closure:
`040880`, `040720`, `04073C`, `040760`, `040498`, `0402D0`, `04031E`,
`03FEF2`, `040200`, `0403C0`, `040404`, `040448`, `0402A4`, `040240`
(all prefixed `8C`). They cover recursive hierarchy traversal, channel setup,
key selection, interpolation, normalization, position/normal arrays and model
draw/restore. Existing matrix, SRT and model drawing owners are reused rather
than counted as new replacements. The combined inventory contains 54 owners.

The real draw at `037098` remains mutable. Original model position/normal
pointers are saved before drawing and restored afterward even when the draw
changes them. The channel cursor advances from its original saved value.
Child/sibling pointers and callbacks remain live at their original read sites.
Scratch/source overlap keeps the original scalar ordering and do-while loops;
invalid counts, alignment and pointers are not silently repaired. Normal
interpolation retains original FIPR, FSRRA and degenerate-vector handling.

Seven exact original arithmetic scopes are added. Their instruction ranges
and precision guards, together with all 51 existing hierarchy scopes, are
verified against the authenticated retained AOT during source preparation.
The multiplication in the normal interpolator's return delay remains outside
its preceding scope. No host rounding scope crosses a callback or memory fault.

Two public roots now share the retained `0400A0` compilation unit; only their
admission hooks change. The original morph capture/commit wrapper `0409EE`
is retained. Private original restarts retain sparse dispatch entries and the
root's actual `04093E` return marker. No AOT-pack regeneration is required.

Revision C passes 502 exact whole-CPU/RAM cases and 33 actual retained-AOT
cases on both Windows and Linux, covering 3,123 visited instruction/delay PCs.
The new morph coverage includes all four channel combinations, missing and
single keys, wraparound and equal key positions, degenerate normals, live
draw mutations, overlapping arrays, observer/FPSCR changes, aliases and
partial faults after the first output vertex. No tolerance was relaxed.
Evidence: `runs/morph-c-{windows,linux}-{components,aot}.log`.

Short hidden Windows counter-discovery runs in Gamma Emerald Coast and Amy
Hot Shelter use no morph-root calls in their measured sections. They prove
neither a speedup nor full-level coverage; an additional Linux zero-call
comparison would not answer the performance question and was not run.

`SARECOMP_NATIVE_MORPH_HIERARCHY=1` remains an internal opt-in requiring the
existing render group. Normal defaults remain OFF pending a representative
gameplay performance comparison, despite functional qualification. Published
patches, baseline and personal saves are unchanged.
