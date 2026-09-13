# Shared CPU work after the Vulkan VSync repair

The target remains 60 actual gameplay updates per second with the original
game tempo and CPU headroom. Lower CPU cost per update matters more than the
independent presentation counter, especially on slower processors. No
interpolation, frame skipping, collision approximation or activation-radius
change is part of this work.

## Four complete matrix/vector leaves

The Windy Valley profile on executable `662bc66a...` contains samples in the
still-translated point and direction matrix helpers. They are SDK routines
used by object and collision code, rather than a Windy/Gamma exception.
The port now implements the complete point transform, direction transform,
XMTRX store and translation-extraction family in `sonic_matrix_vectors.cpp`.

| Entry | Bytes | Original operation |
| --- | ---: | --- |
| 8C638E0C | 88 | Point transformed by XMTRX or a RAM matrix |
| 8C638E68 | 104 | Direction transformed by XMTRX or a RAM matrix; retain the normalization switch at 8C88FFB0 |
| 8C638ED4 | 44 | Store the full XMTRX register bank to RAM |
| 8C638F00 | 32 | Extract the translation from XMTRX or a RAM matrix |

Every code/literal span is authenticated against installed PAL v1.003 bytes;
identities are in `sonic_matrix_vectors.hpp` and the provider manifest.
The original FTRV, FIPR, FSRRA and multiply helpers retain their operation
order and FPSCR semantics. Matrix arithmetic is not reassociated or replaced
with a different host precision. Register-bank switches and all scalar writes
retain their original order. The point RAM branch deliberately reads later
rows after earlier output stores, preserving overlapping operands.

All required RAM ranges are admitted before any mutation. Unsupported FPU
modes, protected writes, active observers that cannot support prevalidated
access, diagnostic memory tracing and unsafe mappings fall back to the
original AOT entry. There is no persistent matrix/source-data cache. Original
timing and non-gameplay scenes retain the original path. The Recompiled path
can be compared using `benchmark-stage.py --matrix-vectors retained|native`.

The component test executes the untouched installed SH4 instructions through
the retained reference executor. **750 cases pass**, comparing architectural
state, full RAM, ordered store values/sources/changed bits, callback-visible
pointers and register banks, and host MXCSR. Cases include both matrix sources,
normalization on/off, signed zero, finite and exceptional floats, FR banks,
rounding modes, input/output/matrix overlaps, P0/P1/P2 aliases and mutation-free
declines. Log: `.local/menu-preview/matrix-vectors-test-02.log`.

Integration rebuilt no retained AOT units: `.local/menu-preview/matrix-vectors-game-build-01.log`,
76.283 seconds including the manifest/dispatch rebuild and link audits.
Two matched hidden/muted pairs used Recompiled timing, Vulkan, 3440x1440,
VSync on, isolated forward input and copied test saves. Each uses the same
executable (`006d7184...`), runs 60 gameplay seconds and excludes the first
ten seconds. No sampler or screenshot capture was active. All runs complete
the planned stop without a runtime error.

| Scenario | Family | New draws/s | Execution CPU ms/draw | Raw cycles/draw | Output/s |
| --- | --- | ---: | ---: | ---: | ---: |
| Sonic Windy Valley | Retained | 57.466 | 16.524 | 72,553,251 | 143.986 |
| Sonic Windy Valley | Native | 58.068 | 16.347 | 71,859,325 | 143.988 |
| Gamma Emerald Coast | Retained | 48.149 | 19.190 | 83,970,801 | 143.987 |
| Gamma Emerald Coast | Native | 49.162 | 18.606 | 82,047,156 | 143.999 |

These are the `runs/matrix-vectors-{retained,native}-{windy,gamma}-01` runs.
They show about 1.1% and 3.0% less execution CPU per new draw in these pairs.
The control retains the new hook registrations and selects ContinueOriginal;
this compares the two implementations in the same executable, not a binary
reconstruction of the previous commit. Host activity and moving-fixture
differences limit attribution, especially for the small Windy difference.

The three used native leaves record 2,719,068 calls in Windy and 816,679 in
Gamma, with zero original fallbacks. Translation extraction is covered by
the differential component; neither route calls it. Windy's task and timer
rate equals its new-draw rate. Gamma includes non-drawing update boundaries:
54.684 retained / 55.535 native task traversals per second. Neither route
establishes stable 60 updates or enough reserve on a lower-end processor.

## Static dispatch exclusions

The runtime tests its fixed native-hook exclusion chain before every positive
static-chain lookup. The disabled `prepare_static_chain.py` experiment preserves
both original bitmap-construction validation passes, then clears the exact
37 excluded source addresses and their P1/P2 counterpart bits. A positive
lookup can return immediately. Misses still check the identical exclusion
predicate and use the unchanged hook/binder path. No dynamic dispatch result,
owner admission, generation or negative lookup is cached.

The preparer accepts only the existing OR-of-exact-address-comparisons shape
and reverses its edits back to the exact authenticated source. The native
component compiles both actual index class bodies and compares all 983,835
current dispatch entries, adjacent/odd/P1/P2 variants and random addresses:
5,019,249 comparisons pass. Empty tables, duplicate excluded entries and
alignment failures retain their original validation behavior. These are
dispatch entry sites, not newly discovered game functions.

The game run `static-chain-native-windy-01` completed without failure but
measured 57.714 draws/s, 16.412 ms execution CPU/draw and 72,154,843 cycles/draw.
The immediately preceding matrix-native control was 58.068, 16.347 and
71,859,325 respectively. This does not establish a gain. The private CMake
option `SARECOMP_FILTERED_STATIC_CHAIN` defaults to OFF and was restored to OFF
before the final build. Do not expand the experiment on the strength of its
source-level reduction alone.

## Code layout experiment

`tools/prepare-hot-code-order.py` selects exact function-owner symbols from
both resolved Windy Valley and Gamma profiles, checks their presence in the
link map and writes an optional order. It changes placement, not instructions,
arithmetic or cadence. The selected list is in `cmake/hot-code-order.txt`.
Configure `SARECOMP_HOT_CODE_ORDER` with its absolute path to use it, or empty
to retain original order. This experimental build uses it; the CMake default
remains empty pending broader confirmation.

The mechanism uses the documented
[/ORDER function placement contract](https://learn.microsoft.com/en-us/cpp/build/reference/order-put-functions-in-order?view=msvc-170).
Only reorderable COMDATs move. Of the selected owners, 68 (45.9% of owned
profile samples) ended up in the first 10 MiB. Not every selected SDK routine
was moved.

Consecutive Vulkan/VSync Windy runs at 3440×1440:

| Run | Actual draws/s | Execution CPU ms/draw | Execution cycles/draw |
| --- | ---: | ---: | ---: |
| `hot-layout-control-windy-01` | 55.628 | 17.003 | 75,008,773 |
| `hot-layout-native-windy-01` | 59.872 | 14.678 | 64,927,476 |

This pair shows 13.7% less execution CPU per draw, at about 144 presentations/s
in both runs. The candidate needed a 37-second relink, without AOT recompilation.

After the MINICART supplement, `hot-layout-native-gamma-04` completed Gamma
Emerald Coast at 49.556 actual draws/s and 18.373 execution CPU ms/draw,
VSync off, 60 presentations/s. Earlier matrix-native Gamma was 49.162 and
18.606, but at a different output/VSync setting: this is not a matched estimate
of the layout gain. Two intervening hidden VSync attempts were unusable: the
first exposed the intermediate inventory identity mismatch (fixed), the next
stalled during startup. The final stage run completed normally. These results
do not establish comparable Gamma gains, global stable 60 FPS or sufficient
headroom on weaker processors.

## Other findings and rejected work

- A conditional MXCSR restore was tested in the global FPU epoch destructor.
  All 131,896 differential cases passed, but the extra read/branch made the
  epoch microbenchmark slower: outer 10.287 to 14.801 ns, nested 8.459 to
  11.587 ns. It was reverted before any game build. The current game retains
  unconditional restoration. Private evidence:
  `.local/research/fpu-epoch-conditional-rejected-20260913.patch` and
  `.local/menu-preview/fpu-epoch-cost-01.log`.
- The existing prepared-RAM-read experiment remains rejected. Its prior
  roughly 0.5% CPU difference did not justify expanding it. SDK matrix store
  batching also remains off after its earlier slower game measurement.
- Replacing 28 adapter calls to `canonical_physical_address` with its exact
  inline implementation also failed to establish a game gain. The SDK ABI
  function delegates directly to that helper, so this changes call placement,
  not address semantics. `inline-address-native-windy-01` measured 57.009
  draws/s and 16.673 ms CPU/draw (73,147,181 cycles/draw), worse than the
  matrix-native control. Those call-site changes were reverted. No additional
  tests or wider expansion were used to chase this small difference.
- The memory guard already has a 256-byte page classification index. A
  persistent non-executable token keyed only by its write generation would be
  unsafe: adding executable ranges invalidates classification without raising
  that generation. Protection checks were not removed.
- A read-only audit identified NEAR-poly 8C028BFE and TOUCH-poly 8C029B00 as
  further shared collision owners. They are not pure math leaves: they update
  candidate/contact lists, retain 96/16-element caps and diagnostic branches,
  and call other original functions. They remain unchanged pending a complete
  admission/continuation contract. No speedup is claimed for this finding.
