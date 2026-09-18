# Complete rigid object hierarchy

The original PAL owner `8C036BC0..8C036F12` is a complete static object tree:
translation, rotation, scaling, model drawing, child recursion and sibling
iteration. It adds 313 instructions and 64 delay slots to the previously
qualified 39-owner render/motion closure. Its six matrix/SRT children are
already in that closure and are not counted again as new replacements.

All eight combinations of the low three transform flags are retained, as
are draw suppression, child suppression and YXZ versus ZYX rotation. The
all-transforms-disabled case does not push or pop the matrix stack. Guest
stack use remains 36 bytes per recursive node, with the original live register,
matrix, RAM, partial-fault and return behavior.

Model drawing at `8C037098` remains a real mutable boundary. Flags and child
pointers are read after that call, and sibling pointers after matrix pop,
at the original points. No hierarchy snapshot, null-node repair or persistent
permission cache is introduced. A foreign call revokes source/RAM proofs.

One additional retained compilation unit receives the root admission hook.
The private original continuation preserves its sparse global entry table
and exact `8C036F0E` return marker. The source preparation composes with the
active Linux BASE RAM preparation; the retained AOT pack is not regenerated.
The separate capture/commit wrapper at `8C036F12` remains original.

Revision D passes 406 exact whole-CPU/RAM cases and 25 actual retained-AOT
cases on both Windows and Linux. New coverage includes all 64 combinations
of rigid flags, mutable draws, child/sibling changes, stack/matrix faults,
alias addresses, observer replacement, mode changes and interrupted draws.
The original observer-contract change may yield at `036CA8`; the test drives
the existing sparse global table afterward and checks the complete state
and callback sequence again instead of fabricating a root return.

That Linux continuation exposed a two-ULP SDK matrix mismatch. The shared
render closure now retains 51 exact original lexical host-FPU scopes,
including the original FSCA/FTRV combinations. Instruction-local operations
outside those scopes remain local. Adjacent scopes are kept separate.
The unchanged exact assertions pass after the correction; no tolerance or
case was removed. The emitted scope inventory is additionally checked against
the authenticated original compilation units. Evidence is in
`runs/hierarchy-epochs-d-{windows,linux}-{components,aot}.log`.

The isolated D gameplay comparison does not qualify this root for release.
Gamma / Knuckles Sky Deck / Knuckles Lost World match all 16 selected endpoint
fields and 68 updates, but execution CPU per update changes -1.67% / -0.14% /
+15.05%; new-image throughput changes +0.63% / -0.65% / -16.85%. These are
short Linux VM entry windows, not Steam Deck FPS. The original D root must not
be enabled merely because its functional checks pass.

The follow-up changes source admission for the complete render family: a root
authenticates its own body and literals, and each entered child authenticates
its own sources. Proofs last only within the connected call and are revoked
at every foreign callback, with the continuing parent checked again. Source
overlap still prohibits native writes even when an unusual observer does not
mark those bytes immutable. The optional stack window remains bounded to 4 KiB.
This removes the eager whole-inventory scan from every small object root;
it does not remove functional invalidation or cache permissions across frames.
`SARECOMP_NATIVE_RENDER_ROOT_PROOFS=0` retains the eager control for comparison.

The complete corrected family is now qualified in three same-binary Linux
comparisons (revision C), with inverse-trig and collision-world active in both
sides. All 16 selected endpoint fields and 68 updates match in each window.

| Scene | Execution CPU/update | Process CPU/update | New images/s |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | -10.62% | -2.04% | -0.76% |
| Knuckles Sky Deck | -3.63% | -0.69% | +0.57% |
| Knuckles Lost World | -1.32% | -1.33% | +2.04% |

These compare the complete corrected family against its retained counterpart,
not the isolated rigid root against revision H. They do not isolate each
subchange, prove an equivalent Deck FPS gain or add to earlier percentages.
SHA-256: `169aa76057329e9aec5861eced10a6d47a61aed673a545c751b90b30f03826f7`.
Evidence: `runs/render-family-*-comparison-linux-20260918-c.json`. No native
declines or original resumes occur in the measured sections. The source proof
coalescing described in the September 18 update report follows this measurement;
its net comparison must use the final delivery candidate, not this hash.

The rigid root now joins the default-ON render family for both timing modes
and platforms. `SARECOMP_NATIVE_RIGID_HIERARCHY=0`, the render-group/global
native OFF controls and diagnostics retain the original route. Published
updates and installers are unchanged until the scheduled package is delivered.
