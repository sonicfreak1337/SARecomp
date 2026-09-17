# Native object activation and lifetime

The common PAL owners `8C0912C0` (SET activation), `8C091928`
(out-of-range lifetime) and `8C09105A` (squared-distance predicate) now have
structured C++ implementations shared by Windows and Linux. The private
`SARECOMP_NATIVE_OBJECT_ACTIVATION` switch controls them. Following the user's
explicit request for a new Deck patch, revision F joins the normal native CPU
group. Internal override `0`, group override `0` and diagnostics retain the
original implementations. The previously delivered September 17 patch stays intact.

## Boundary and semantics

The activation loop shares one admitted RAM capability and folds the distance
predicate into the loop. It preserves reference-position selection, signed
counts, per-record/type flags, authored radii, read-ahead, saved object state,
allocation failure, work-pointer absence and task retirement. No activation
distance, world state, simulation cadence or callback ordering is changed.

Task construction and saved-state release publish the original CPU state and
continuation at two real retained call boundaries. They release the
memory capability; the native owner reacquires it and authenticates its source
after return. Mutable tables and globals are reread at their original points.
No task callback is cached, flattened or run on another thread. The complete
read-only leaves `04F7E0` and `04F698` are now included in the native owner;
their 34 code/literal bytes are authenticated at entry and after callbacks.
The first composes two signed halfwords at `7492FA/7492FC`, preserving scratch
R1/R2/R3; the second returns the unsigned halfword at `749308` and preserves
scratch R2. Both preserve T/FPU state and the original call continuation. No
functional port hooks for these two source addresses were found in the audit.

Admission checks source bytes, CPU/FPU mode, RAM mappings, the authenticated
product observer and safe stack writes. Other memory accesses retain original
memory helpers if their direct RAM proof fails. Ordinary unsupported entries
decline without mutation; an interrupted callback cannot restart an already
partially executed owner. The finite distance kernel preserves FMAC and FPU
flags, including early X/XY distance exits and the Z delay-slot read-ahead.
Rejected arithmetic inputs retain the original helpers before guest mutation.

Only two retained containing units are recompiled. Three authenticated public
entry shims select native execution; resume entries and the original bodies
remain available. Each shim checks the full retained-unit manifest identity
and its specific original function hash. No AOT regeneration or SDK-tree edit
is required. Windows link auditing still proves 894 prepared entries, 41 units
and zero duplicate selected archive members.

## Original-byte oracle

The finite-kernel revision E passes 1,250 cases on both Windows and Linux.
The query-fused revision F passes 1,262 Windows cases and all 242 affected Linux
activation/rejection/interruption cases; its distance and lifetime arithmetic
is unchanged. The reference interpreter runs
the actual original owner, query and distance bytes; only the two external callback
contracts use deterministic fixtures. Full architectural state, callback entry
state and all 16 MiB of RAM are compared. Cases cover both admitted rounding
modes and register banks, exceptional floating-point values, signed/empty
counts, reference selection, record/type flags, state copies, callback mutations,
three lifetime references, mutation-free rejection and interruption frontiers.

This caught and corrected the bit-64 reference-selection branch before the
first game build, and raw reserved FPSCR preservation before final game tests.
Evidence: `runs/object-activation-{windows,linux}-components-{e,f}.log`. The full
F suite executes 1,512 distance predicates: 1,154 direct and 358 fallback cases.
The two fixture corrections for F were sorted immutable ranges and mapping
new query-boundary cases to the default activation setup. They did not change
the game implementation or weaken comparisons.

## Direct distance arithmetic

Normal finite inputs and signed zeros use one host rounding scope. Binary32
products are exact in binary64; subtraction requires an exponent gap at most
28, so its result is exact before the single binary32 rounding. Subnormal,
overflow and unsupported inputs decline to the original arithmetic.

For square-adds the sum is nonnegative. With round-toward-zero, nested
binary64/binary32 truncation is equivalent to one rounding. With nearest-even,
double rounding can differ only at a binary32 midpoint. The kernel rejects
that boundary using `(double_bits & 0x1FFFFFFF) == 0x10000000` before narrowing.
Multiplication and addition remain separate; FP contraction is disabled. This
avoids host FMA while retaining its guest result. Only original SUB/MUL operations
accumulate arithmetic sticky flags; original FMAC and comparisons clear Cause.
The final raw FPSCR update preserves reserved bits rather than normalizing them
through LDFPSCR semantics. All live scratch registers and early exits are kept.

The initial broad-epoch helper variant failed Linux at the 0.56 distance
threshold by one ULP. Removing that epoch passed but saved no Windows CPU.
The final direct kernel passes that case, exact midpoint and adjacent-value
cases, wide exponent differences and both rounding modes/register banks. The
incorrect initial Linux game binary was replaced only in the owned VM test path.

## Revision E Windows game measurements

Final same-executable Gamma Emerald Coast, Original, 1280x800 / 50%, 5..185
new-image window, hidden and muted, no capture or provider timers:

| Object family | Execution CPU ms/image | Execution cycles/image | New images/s |
| --- | ---: | ---: | ---: |
| OFF | 22.048611 | 96,122,089.56 | 24.997120 |
| ON | 21.701389 | 96,106,076.00 | 25.002373 |

All 19 checked endpoint state/cadence fields match, including the same 432 game
updates. CPU time falls 1.57%, but cycles fall only 0.017%: this is not a robust
global improvement. The ON endpoint records 354,088/354,088 direct distance
predicates, 567 activation owners, 43,694 lifetime owners, 341,901 records,
94 objects created and 12 retired; no declined owners or slow memory accesses.

The final Recompiled 4:3 Gamma check passes at 59.966 new images/s with one
update per image. Evidence: `runs/object-activation-final-win-*/result.json`.

## Initial image comparison

The hidden, muted Windows Gamma Emerald Coast Original ON/OFF pair completes
the same 5..65 image window with matching checked game/cadence/native-owner
fields at both endpoints. Frame 270 is byte-identical:
`34bf8550573336b337d7df7926ec10cf4cdf783fe28afe28239a2f00fe4ddb61`.
The ON run executes 279 activation owners, 17,875 lifetime owners and 171,929
distance predicates, inspecting 168,237 records and creating/retiring 68/10
objects. It has zero declined owners and zero slow memory accesses.

These initial capture runs establish an image witness for the structured owner
and are excluded from performance qualification. They predate the final finite
arithmetic kernel; the final original-byte and state comparisons are separate.
No Deck FPS improvement is claimed from this family. The overall 20–25 ms
Deck target remains open.

## Revision E Linux measurements

Four-vCPU / 6-GiB TCG VM, llvmpipe with two raster threads, Original timing,
800x500 / 50%, serial hidden runs without capture/provider timers. All 19
checked endpoint fields match in both same-executable pairs. Gamma has 40
images / 136 updates; Chaos-4 entry has 20 images / 68 updates.

| Scene | Family | Execution CPU ms/update | New images/s |
| --- | --- | ---: | ---: |
| Gamma Emerald Coast | OFF | 224.429545 | 0.830513 |
| Gamma Emerald Coast | ON | 206.210382 | 0.878812 |
| Chaos-4 entry | OFF | 217.675499 | 0.790077 |
| Chaos-4 entry | ON | 228.880731 | 0.773246 |

Gamma saves 8.12% execution CPU and produces 5.82% more images; Chaos-4 costs
5.15% more CPU and loses 2.13% image throughput. This is not a global win.
Gamma executes 83,808 direct distance predicates in the measured window versus
1,836 in Chaos-4. Both have zero fallback/declined/slow accesses. The fixed
two-query retained-bridge overhead per activation motivated the separately
verified revision F; it is a hypothesis, not an established cause of E's cost.

Evidence: `runs/object-activation-comparison-20260917.json` and
`runs/object-activation-final-linux-*-20260917.json`. The E Windows binary was
`88507576dc6f0a2156cf731373a4b7ae9cd4ce78930c97870ba1ca81196613ac`,
1,913,818,112 bytes; E Linux was
`713b7f5ec7a122222d7004ff64ef5cb44790e5c26ca58a04287306873c0cd9f5`,
1,744,530,736 bytes. These are historical E identities, not revision F.

## Revision F Windows measurements

After folding the two read-only query bodies, the new same-executable pair uses
the same Gamma setup and 5..185 window. All 19 checked endpoint fields match,
with 432 updates in both runs. The ON window records 270,953 direct distance
checks, 432 activation owners, 35,222 lifetime owners, and 38/12 created/retired
objects. These native counters are not an equality claim against the OFF path.
No native owner declines or slow accesses occur.

| Family | Execution CPU ms/image | Execution cycles/image | New images/s |
| --- | ---: | ---: | ---: |
| OFF | 22.048611 | 96,020,234.44 | 24.997440 |
| ON | 20.659722 | 92,221,093.56 | 24.998651 |

This is 6.30% less CPU time and 3.96% fewer cycles. Original's 25-image PAL
limit is unchanged. Recompiled 4:3 also passes at 59.970 new images/s with one
update per image. These measurements are F versus its own OFF control, not a
claim about an installed Deck.

Evidence: `runs/object-activation-f-win-*/result.json`. Windows F SHA-256 is
`7bb25ef0d75177c8827c7cd086adc89810b249f6d7df92b31ddd85a4aa4e5b74`,
1,913,818,112 bytes.

## Revision F Linux measurements

Same four-vCPU TCG setup and windows as E; these are fresh F ON/OFF pairs.
Gamma's 19 endpoint fields match exactly. Chaos 4 begins one game update apart
(21 versus 22), including different player Y and animation counters, and ends
89 versus 90. Both still execute 68 updates. That boss pair supplies a successful
function check, but not qualified exact-work performance evidence.

| Scene | Family | Execution CPU ms/update | New images/s |
| --- | --- | ---: | ---: |
| Gamma Emerald Coast | OFF | 173.200071 | 1.082251 |
| Gamma Emerald Coast | ON | 170.242059 | 1.065161 |
| Chaos-4 entry | OFF | 211.325576 | 0.784868 |
| Chaos-4 entry | ON | 163.794322 | 1.024275 |

Gamma saves 1.71% execution CPU but loses 1.58% image throughput. The raw boss
differences (-22.49% CPU / +30.50% images) are not promoted as a verified gain
because of the endpoint mismatch. Windows' exact-state CPU/cycle improvement
and the component tests remain valid; a global or Deck gain is not established.

Evidence: `runs/object-activation-f-comparison-20260917.json` and its referenced
summaries. Linux F SHA-256 is
`a1ebdd64618b1fd95b8b743ecd6b65c272e7e912ec05ddcc98cb47c29f667b99`,
1,744,531,480 bytes. This measured development executable includes the separate
unqualified EXTENDED RAM/FPU candidate. It is not the requested patch payload.

The user explicitly requested a patch for another Deck test. Its release build
returns Linux RAM regions to the previously shipped BASE scope and disables
prepared RAM access; Windows returns to its previous retained RAM policy.
The new object family is enabled in both timing modes, with no change to
simulation cadence. At the user's subsequent request, complete model ownership
and SIMD projection were measured together on this release configuration and
are now enabled as one model group. Matrix bulk, collision closure and the
extended RAM/FPU experiments remain OFF. Actual combined measurements, package
identity and installation checks are recorded separately in
`cpu-update-native-objects-20260917.md`.
