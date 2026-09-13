# Earlier native 60-frame development checkpoint

Historical measurement checkpoint, superseded by
[standard60 integration and stability](standard60-stability.md). The user
subsequently accepted the measured performance and requested default activation.
The numbers below describe the earlier diagnostic builds, not the final default.

At this checkpoint the private Emerald Coast fixture
executes the original 60-Hz video constructor and the original single-step
gameplay scheduler. It produces distinct simulated/drawn frames and keeps the
144-Hz presentation thread separate. Interpolation remains disabled.

This is **not yet a finished 60-FPS enhancement**. CPU capacity remains below
60 steps/s, so this private single-step fixture currently advances gameplay
too slowly in wall time. Normal user runs retain the original cadence; the
fixture requires explicit hidden-test flags and is not exposed in Options.

## Measured work

Same private EC route, held-forward isolated input, copied saves, hidden/muted,
1280x720 D3D11, 100% render scale, VSync off and 144 output. Each run records
actual task traversals, guest timer ticks, distinct draws and presentations.
The IP-sampled rows are diagnostic measurements, not isolated speedup claims.

| Run under `runs/` | New draws/s | Execution CPU ms/frame | Notes |
| --- | ---: | ---: | --- |
| `sixty-frame-profile-ec-02` | 38.96 | 24.52 | Before new native leaves; IP sampled |
| `sixty-frame-compact-ec-01` | 38.43 | 25.15 | Compact AOT experiment rejected, off |
| `sixty-frame-runtime-ec-01` | 39.95 | 24.05 | Exact FPU runtime fast path |
| `sixty-frame-palette-p0-ec-01` | 40.21 | 23.83 | Palette admits P0 RAM; IP sampled |
| `sixty-frame-native-leaves-ec-01` | 41.61 | 23.04 | Direct stores; normals still fell back |
| `sixty-frame-native-leaves-ec-02` | 41.66 | 22.81 | Palette and normals both execute natively |
| `sixty-frame-stack-draw-ec-01` | 42.38 | 22.48 | Native matrix push/pop and direct vertex construction; IP sampled |
| `sixty-frame-collision-index-ec-01` | 42.93 | 22.30 | Native vector math and indexed descriptor candidates |
| `sixty-frame-matrix-batch-ec-01` | 41.32 | 23.29 | Same EXE, SDK matrix store batching; rejected and off |
| `sixty-frame-native-inverse-ec-01` | 43.98 | 21.67 | Entire inverse plus determinant; IP sampled |
| `sixty-frame-fpu-body-ec-01` | 44.16 | 21.74 | Specialized FPU body; lookup counters enabled |

The last row records 170,282 complete native inversions with no original
fallback. Its actual task/timer rate is 44.02/s, output 143.98/s, frame p95
25.13 ms and p99 27.27 ms. The scenario
completed successfully; `sixty_frame_target_passed` is false. It does not
support a claim of stable 60 FPS or correct wall-time speed for the fixture.

## Implemented CPU changes

- The source-bound FPU runtime accepts the common nontrapping scalar case
  before general overload dispatch. Integer rounding and FPSCR results remain
  exact; the original implementation handles rejected cases. 131,896
  differential cases passed, including 28,662 actual traps. This replaces one
  SDK archive member locally; no baseline SDK source is modified.
- The existing 64-slot exact-source lookup memo now also serves the later
  immutable-table identity lookup. Runtime admission, current generations and
  executable binding checks still run. Negative lookups are not cached.
- Palette lighting `8C037350` is a complete native leaf. 404 original-byte
  differential cases passed. The archived component interpreter's FTRC source
  decoding defect is corrected only in the reference fixture at its three
  authenticated FTRC instructions; unchanged generated AOT confirms those
  operands. No product interpreter or baseline change is involved.
- Vertex-normal generation `8C0563AC` is a complete native leaf. 260 cases
  passed. Shared read arrays and strip look-ahead are admitted; output and
  stack remain disjoint from all inputs. Per-vertex contribution and store
  ordering remain original, including Cpu versus Fpu write sources.
- Matrix-stack push `8C639BB0` and pop `8C639AD8` implement complete original
  leaves. 1,040 cases passed: 382 scalar and 658 batch-contract cases.
  The scalar cases include 168 push and 214 pop. Signed push capacity,
  unsigned wrapped pop subtraction, FPSCR/register changes and each
  StoreQueue/Fpu/Cpu write are compared. Unique exported wrappers satisfy the
  existing native-hook contract; both fall back before any guest mutation.
- Optional SDK matrix store batching preserves all event fields, including
  declined-batch scalar replay, but lost its same-EXE measurement. It remains
  private and off. Its staged-group counter witnesses staging and flush, not
  successful fast batch admission. No performance win is claimed.
- Cross product `8C027360`, length `8C63A69C` and normalize `8C63A88C`
  execute entire native leaves. 659 original-byte cases passed, including
  aliases, exceptional floating-point values and mutation-free declines.
  All three executed in the live comparison without original fallback.
- Descriptor lookup indexes candidate positions by physical backing address;
  it still performs every original live validity/ownership check. All 14
  descriptor-list mutations invalidate the index. Candidate order, duplicate
  claims, resets, allocation failure and address reuse passed the differential
  check and explicit source-mutation audit.
- Inverse `8C638FF0` includes its fixed determinant callee `8C64F32C` in one
  native body. 225 byte-reference cases passed, including both RAM and XMTRX,
  singular matrices, fixed internal PR, stack residues and ordered writes.
  The reference really executes the original callee; it does not fake it.
- A source-authenticated FPU body context removes repeated mode/operation
  dispatch from these 559 arithmetic sites. Retained integer arithmetic and
  every intermediate FPSCR/FR result remain exact. An original epoch is
  created lazily only if a special value needs fallback. 140 mixed-operation
  cases plus the 225 complete matrix cases passed. The subsequent whole-game
  run is essentially unchanged within measurement variation; no significant
  speedup is claimed for this refinement alone.
- The model renderer can construct ordinary vertices directly in its already
  reserved output buffer. ENV, scalar and diagnostic paths retain staged
  construction. Failure removes the provisional corner and keeps the
  existing triangle rollback. Its gain is not separately established.

At this checkpoint native leaf flags required the private 60-frame fixture. Builds
retain the compiled r354 AOT archive and use two compiler jobs; successful
game-build logs report zero AOT recompiles.

## Timing still to close

The original game distinguishes gameplay 2/2, cutscenes 2/1 and some menu/
minigame 1/1 scheduling. A blanket 1/1 override is therefore incorrect. The
fixture also intercepts the authenticated gameplay cadence reset after death;
it is not a proof for every story, script or display callback.

The original wrapper tests cover single-step dispatch and both post-services.
Local code review identifies once-per-image ring blinking and outer fades
alongside genuinely tick-based consumers. These must be kept distinct.

PAL ring blinking advances `8C7996EC` by `0x400` at `8C08A356..35C` only
after its zero-ring and mode-16 gates. An authenticated 2/2-to-1/1 gameplay
conversion needs `0x200` on this reached branch, not a wall-FPS correction.
Hint-monitor update `8C061300` and child `8C061560` gate animation and state
on the wrapper index at `8C754E08`. Their angular springs, state transitions
and material ramp cannot be fixed by halving only the bobbing constant.

Alternating that index globally or even just inside task traversal `8C0987E4`
is not a solution: monitor display `8C0613BC`, model-draw gates `8C03700C` /
`8C037098`, sphere query `8C038D00` and route sprites `8C09E6B8` suppress
rendering at index 1. Some coroutine and post-service consumers lie outside
the traversal. The original wrapper itself uses this index for its loop and
wait decisions. These are investigated constraints, not implemented timing
fixes or evidence that the complete game now runs at correct 60-Hz speed.

The [original 60-FPS patch author's explanation](https://dcmods.unreliable.network/index.php/2021/05/29/new-code-patches-for-sonic-adventure-better-60-fps-drift-fix/)
independently confirms the scheduling distinction and warns that CPU overload
still slows the game. The public patch also corrects selected visual/movement
consumers (including animals, Leon, hint monitors, tails and sky animation).
Its US image offsets are not our PAL addresses and must not be copied blindly.

The next acceptance point requires 60 actual task/timer steps and distinct
draws each second, 144 presentations, satisfactory frame intervals, and the
remaining original-tempo consumer contracts. A passing boot or repeated
presentation frames cannot satisfy that condition.
