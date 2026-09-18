# Composed native model submission

Private switch: `SARECOMP_NATIVE_MODEL_SUBMISSION=1`. Default OFF. Global native
CPU/model zero overrides and diagnostics retain the previous implementation.
The September 18 delivered patch is unchanged.

The operation joins the existing complete live render hierarchy to all three
PAL model submissions: `037098`, `037108` and the blended model's `03700C`.
The latter includes its renderer-context capture, ordered drawing, `036FFC`
material-state publication and renderer-context commit. Transform, projection,
palette lighting and draw remain the already-qualified native implementations.

A synchronous hierarchy operation supplies its admitted RAM capability and
source proof lifetime to its model children. A completed closed child does
not return through the generic AOT callback dispatcher or invalidate every
parent proof. Model sources are authenticated at most once until a real guest
call. Every real callback, original continuation or declined native child
revokes that shared state before guest execution; interrupted calls preserve
their actual frontier.

Both sides exclude writes to the other's authenticated sources, even with a
narrow installed immutable range set. Each model still admits its complete
write footprint. For `03700C`, that includes the packet snapshot, renderer
header and list cursors, and excludes aliases with model input, wrapper frame,
control pointers, sources and the hierarchy. Mesh data, scene state and
callback targets remain live. No proof or mutable model snapshot survives a
root operation, arbitrary guest callback or queued-renderer ownership boundary.

The existing independent model path retains its former source set and access
policy. New model admission is explicitly selected by the composed hierarchy;
it is not enabled merely by entering a graphics scene.

## Qualification and disposition

Windows passes 218 complete model CPU/RAM cases (145 existing plus 73 new)
and nineteen composed-hierarchy cases. These compare all registers and all
16 MiB of RAM, including child entry state, both context branches, saved
registers, source protection under a narrow guard, rejected aliases and real
fallback/interruption boundaries. No comparison tolerance was relaxed.

These are wrapper/composition oracles: cull, transform and draw are mocked
at their call boundaries; palette and render-context children use their
native implementations. The subsequent real-cull qualification and corrected
six-word material admission are documented in
`native-model-visibility-20260918.md`. The earlier suite alone did not prove
the cull child's complete write footprint.

Evidence:

- `runs/model-submission-components-windows-final-20260918.log`
- `runs/model-submission-hierarchy-windows-final-20260918.log`
- `runs/model-submission-executables-20260918.json`

Both actual game binaries compile incrementally. Linux's staged stripped copy
has exactly the same 27 allocated ELF sections as its unstripped source.
Linux also passes all 73 new model cases and nineteen hierarchy cases; see
`runs/model-submission-linux-components-launch-b-20260918.log`.

The Windows Gamma gameplay comparison enables both the complete model group
and the 61-owner movement/contact group together. All sixteen endpoint fields
and 48 game updates match. Its exact frame-270 capture is byte-for-byte equal
(SHA-256 `96077af800ac42a61315d75fda50804d2c601d411701ab32a1a4dd0034abb0de`).
It closes 5,811 model submissions, reducing foreign hierarchy calls from
9,507 to 3,696, without an unsupported-access resume or model revocation.
Evidence: `runs/model-submission-gameplay-comparison-windows-20260918.json`.
Capture/readback was enabled, so this pair is excluded from performance claims.
The initial Windows pair ended in the authored stage-entry camera; its capture
frame was never reached. It is not used as gameplay/image evidence.

The matched Linux Gamma pair retains all sixteen endpoint fields and 68
updates. The combined groups reduce execution CPU/update 6.51% and process
CPU/update 1.40%; new-image throughput changes only +0.05%. It closes 8,348
model calls, leaving 5,236 genuine foreign hierarchy calls, without a resume
or revocation. This is not a demonstrated whole-frame improvement.
Evidence: `runs/model-submission-gamma-comparison-linux-20260918.json`.

The Lost World pair completes without a resume or revocation and closes all
3,100 hierarchy model calls in the measured window. However, its start and
end differ by one game tick and the corresponding cumulative animation and
palette calls. Its timing percentages are excluded: equal update deltas do
not erase the different animation start state. The exact earlier contact-only
Lost World comparison remains the valid evidence for that group. Evidence:
`runs/model-submission-lost-comparison-linux-20260918.json`.

A separate Windows Gamma pair, with capture disabled and the longer 5-125
window, matches all sixteen endpoints. Combined execution CPU/boundary falls
5.88%, process CPU/boundary 8.25% and execution cycles/boundary 6.04%.
Evidence: `runs/model-submission-timing-comparison-windows-20260918.json`.
These are combined-group comparisons, not evidence that the new model group
adds those percentages on top of the contact group. There is no new Deck
measurement. Keep both private switches OFF while pursuing the next larger
composition; the current data does not justify a separate released update.
Do not repeat the unchanged Gamma pairs or full component suites.

The compiled revision is identified by:

- Windows: `out/model-submission-windows-20260918/game.exe`, SHA-256
  `058c090ed4347dbc32d479bcdd7d9422e94f24d1ff041af747bb040247cef76f`.
- Linux unstripped: `build-linux/game`, SHA-256
  `b8a5a4b577f77a70048dcdaead4fce6c6ba57ac1814e15108d4b1eb9f70fc818`.
- Linux staged: `out/model-submission-linux-20260918/game`, SHA-256
  `f5305bd8a54f2415b36d4ab07baf2c7c6e7fc1b169917de8e2e401e241e88305`.
