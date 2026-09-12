# Immutable dispatch-source memo

Integrated in the Sonic port on 2026-09-13 after Grounder's bounded source
review. The original r354 AOT and the old Katana repository are unchanged.

The generated table constructs and validates 983,835 unique, sorted entries
before publishing a `static const` vector. Its element addresses live for the
process lifetime. The new 64-slot TLS memo stores only an exact source address
and a pointer into that table, approximately 1 KiB plus diagnostic counters.

Only `find_exact_entry(admission.dispatch_source)` is replaced. Coverage
preflight, target validation, owner class, null-function checks, generation
checks and error reporting execute as before. No runtime address, dynamic
owner, admission result or negative lookup is cached. Collisions compare the
full source key; P1/P2 aliases are not folded. Existing dispatch-scope reset
clears the memo. An overlay's retirement cannot extend its execution rights
through a cached pointer, because admission remains outside the memo.

`prepare-motion-dispatch.py` verifies the SDK source-generation manifest and
three exact integration boundaries. Comparing the entire coverage-preflight
and owner-check body before/after shows only the lookup name changes.
`sonic_dispatch_memo_tests` passes exact-key collisions, aliases, repeated
missing lookups, null-function preservation and scope reset. It does not claim
to test the SDK's complete ownership implementation.

Private diagnostic environment variables:

- `SARECOMP_DISPATCH_MEMO_DISABLE=1`: same executable, original lookup path.
- `SARECOMP_DISPATCH_MEMO_STATS=1`: count hits/misses/conflicts and report once
  when the dispatch scope ends. The normal path does not increment counters.

## Native measurements

Three isolated, hidden/muted Emerald Coast runs, D3D11, 1280x720, 144 target
presentation FPS, existing forward input profile 3. Each gameplay window is
60 seconds; the first ten seconds are excluded from steady metrics. The first
two use the same executable with diagnostic counters and detailed timing off.

| Run | New draws/s | Presentation/s | Process CPU ms/title boundary |
| --- | ---: | ---: | ---: |
| dispatch-memo-off-01 | 13.396 | 143.906 | 84.126 |
| dispatch-memo-on-01 | 13.794 | 143.404 | 82.420 |
| dispatch-memo-stats-01, counters enabled | 14.056 | 143.830 | 85.837 |

All complete with their planned deadline and no recorded runtime failure.
The first pair is +3.0% new draws/s and -2.0% CPU/boundary, a preliminary
observation rather than an established whole-game speedup. The moving fixture
is frame-indexed, so scene progression can differ, and the user's host remains
active. The counter run is not a clean timing comparison.

The counter run reports 21,257,139 hits, 5,304,754 misses, 5,304,690 occupied
slot conflicts, zero missing entries. Thus about 80.0% of these immutable
table lookups were avoided over this process, including startup and gameplay.
This is not an 80% CPU reduction. The 64-slot size is bounded, not a claimed
global optimum; the already larger SDK caches were not enlarged.

Every cadence sample still reports active video 50 Hz, release slots 2,
logical delta 2. These are not measurements of 60 simulation updates/s.
No game timing or withdrawn render-interpolation behavior was changed.

Final executable SHA-256:
`6b317d31304251a58bdc2cf4b34cf4ab10a513397b62ae4b47f95add5c3be0ab`.
Incremental `build-dispatch-memo-01.log` passes the link audit, 1,909,946,880
bytes. It rebuilds the port-local dispatch and adapter, not any frozen AOT TU.
The r354 quick integrity check passes.
