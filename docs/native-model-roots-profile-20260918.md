# Remaining execution work after the combined model/contact experiment

This diagnostic run uses the exact Linux program from
`native-model-roots-20260918.md`, Original timing, Deck aspect and two software
raster workers. Both private model-submission and movement/contact groups are
ON. It is separate from all throughput comparisons and establishes no FPS gain.

The recorder exits 1 after producing 1,457 CPU-clock stack samples; its log
contains the successful capture count but no explanation for that exit status.
The game reaches the requested stop without a forced termination. The existing
record was exported with `perf script` and `perf report`, both successfully,
without another gameplay run. Treat this as a bounded sample record, not a
clean full-duration recording.

The resolver verifies the matching unstripped ELF and stripped executable.
There are 1,332 game-leaf samples, seven unresolved. Resolved raw leaf symbols
and their counts reproduce the independently resolved exclusive report exactly.
The remaining owner counts below are inclusive, overlapping and not additive.
They include necessary game work, not just overhead that can be removed.

| Retained owner | Samples | Share of execution-thread samples |
| --- | ---: | ---: |
| Main task traversal `0986CC` | 1,011 | 69.39% |
| `0CBD40` and descendants | 202 | 13.86% |
| Model/motion wrapper `040942` | 100 | 6.86% |
| `0FDC20` and descendants | 90 | 6.18% |
| Land task `0519C0` and descendants | 88 | 6.04% |
| Original camera `019F4A` and descendants | 77 | 5.28% |
| Land display `051E56` and descendants | 76 | 5.22% |
| `0342E0` and descendants | 48 | 3.29% |

The next selected scope is the common land display family, not another isolated
model-copy change. The original `051E56` body traverses the live list at
`75A24C`, applies each record's translation/rotation and submits its model via
`037108`. `0519C0` also selects the animated land list and preparation routes
`051F64`, `052048` and `0520C8`. These are shared scene routines rather than a
Gamma-specific replacement. Native list traversal, transforms, model submission
and animated hierarchy must be considered together. Initialization, mutable
external callbacks and list ordering remain observable boundaries.

This is distinct from the already-native collision-world producer `028EC2`
and its eligibility owner `052518`. Original-camera work is also present in
this profile; its cost must not be attributed solely to the Recompiled camera.
The large outer task percentage includes all descendants and is not evidence
that replacing its short loop alone would remove that cost.

Evidence: `runs/model-roots-profile-gamma-20260918/summary.json`,
`perf-command.log`, `perf-stacks.txt`, `perf-symbols.txt`, `resolved.json` and
`stacks-resolved.json`. The exact raw-stack resolver is
`runs/resolve-model-roots-stacks-20260918.py`.
