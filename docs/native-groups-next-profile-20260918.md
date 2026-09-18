# Next connected native boundary after the September 18 candidate

The actual installed stripped executable is
`55889c92ee5c5ab6a34f1d8733a8698bab6da3b20eaa416fa48b593c0b335f71`.
A fresh Linux execution-thread profile covers the opening gameplay portion
of Gamma Emerald Coast, Original timing, with the qualified groups active.
It has 1,932 CPU-clock samples: 1,913 leaves in the game, one unresolved game
leaf. Profiling is diagnostic and is excluded from the throughput comparison.

The raw call stacks are resolved against the matching unstripped ELF, after
verifying the stripped program's executable segments. Every resolved raw leaf
and its sample count reproduces the separate exclusive `perf report` result.
Frames are counted once per sample even when recursion or several helpers
from the same family appear. The resulting sets are **not additive**:

| Connected scope | Samples | Share of sampled execution thread |
| --- | ---: | ---: |
| Render/model, projection, palette and matrix families together | 438 | 22.67% |
| Render hierarchy, including nested model work | 240 | 12.42% |
| Model pipeline/draw, including calls outside that hierarchy | 202 | 10.46% |
| Movement root and contact/math descendants together | 217 | 11.23% |
| Complete movement root `073018`, including its descendants | 194 | 10.04% |
| Native contact candidates, including their descendants | 123 | 6.37% |
| NEAR/eligibility query, including its descendants | 38 | 1.97% |

These shares include useful work and are not removable-cost estimates.
Movement and contact numbers overlap substantially. Re-enabling a retired
small child on the strength of an inclusive percentage would not be justified.
The native collision-world producer is only 31 samples (1.60%) in this portion
after the completed optimization; more isolated work there is low priority.

## Follow-up scope

The largest coherent remaining native area is the complete model submission:
hierarchy/matrix state, captured model attributes, projection/palette output,
corner expansion and ordered delivery to the renderer. The new hierarchy and
model capture must be considered together. Any alternative must keep the
guest-visible projected/palette RAM, exact material/vertex values, mutable
callbacks, queue lifetime and fallback behavior. This is a structural producer
and consumer boundary, not permission to disable culling or skip work.

The existing compact-model submission was previously measured before this
qualified complete hierarchy/model combination. One bounded follow-up checks
that combination: current Windows verification compares all 50,345 submitted
packets (1,221,104 vertices) byte-for-byte and passes. The matched Linux Gamma
window preserves all 16 endpoint fields and 68 updates. Execution CPU/update
falls from 159.4282 to 154.9457 ms (-2.81%), total process CPU/update falls
0.63%, and new images/s changes -0.006%. This is no useful whole-frame gain;
compact-model submission remains OFF. Do not repeat the unchanged pair.
Evidence: `runs/linked-packets-windows-verify-20260918/result.json` and
`runs/linked-packets-gamma-comparison-linux-20260918.json`.

The existing capture copies points/normals out of guest RAM; packet submission
then copies them again into separately owned storage for the asynchronous
consumer. A future complete producer/consumer change must qualify immutable
snapshot ownership and queue lifetime, rather than just turn this old switch
back on. The current measurement does not establish how much time those
copies individually cost and is not evidence that removing them alone helps.

The distinct next movement scope must cover `073018/074214`, NEAR eligibility,
TOUCH candidates and the contact-classification consumers together, preserving
contact order, mutable calls and partial-fault continuations. The previous
isolated movement/NEAR experiments remain rejected; this profile does not
supersede their measurements or qualify enabling them.

## Address provenance

For this stripped ET_EXEC, unsymbolized exclusive-report entries are file
offsets, whereas raw `perf script` stack IPs are ELF virtual addresses. Do not
run the exclusive-offset conversion on parent IPs: doing so invents plausible
but wrong retained owners. The initial scratch attempt to do that was discarded.
The accepted analysis uses bounded ELF function extents, raw IPs and exact
exclusive-leaf/count cross-checks, not nearest-symbol guesses.

Evidence is under `runs/native-groups-profile-gamma-20260918/`:
`summary.json`, `perf-symbols.txt`, `perf-stacks.txt`, `resolved.json` and
`stacks-resolved.json`. The read-only raw-stack resolver is
`runs/resolve-groups-stacks-20260918.py`.
