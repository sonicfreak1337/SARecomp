# Guest render completion: isolated native experiment

The original render notification now has an executable, **default-off** native
candidate. It follows successfully consumed guest Present commands, not a
one-second timer or the independent output rate. This is a correctness step;
the first live run does **not** establish a performance gain or 60 new images/s.

## Original contract

The [original update audit](original-update-live-20260913.md) binds PAL boot
SHA256 `b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af`
and proves that registered callback8C06C118 originates from TSP render done.
Its counter sample goes to8C78C544 and R4; service8C604486 then publishes
8C88F710=0,8C88F718=1 before optionally calling the pointer at8C6733B8.
The native counter scale and epoch-reset owner are unchanged.

An additional bounded Astra/high read-only audit found:

- Setter8C604FD4..FD8 stores arbitrary R4 into8C6733B8. Neither the two retained
  Primary disassemblies nor a literal scan establishes a gameplay caller or a
  nonnull callback target. Both retained post-PAL RAM fixtures have a null slot.
- 8C88F710 is a pending/busy flag:8C605044..504A sets it before8C642380;
  8C60502A..5036 waits for zero with a bounded timeout. It is not a tick count.
- 8C88F718 is an enable/completed latch: initialization clears it, completion
  sets it, and8C605008..5010 gates the wait on it. The separate post-wait
  callback at8C6733B4 must not be confused with B8.

Unknown later callback registrations remain an acceptance limit. The candidate
uses the existing authenticated AOT callback dispatcher; it does not accept an
arbitrary host pointer, replace unknown callbacks or add an address bypass.

## Submission and delivery

`SARECOMP_RENDER_COMPLETION_EXPERIMENT=1` additionally requires
`KATANA_PORT_BACKGROUND_TEST=1`. Without both, the existing product callback
behavior remains unchanged. No Options setting exposes this private candidate.

Only frame-begin, frame-turnover, kamui-drain and the already-open guest frame
handed over to a movie acquire submission scopes. Repeated title boundaries,
scenario modal frames, Options, decoded video images and autonomous output
repeats cannot reserve a guest completion. Initial guest clear frames count as
submitted work even without a draw; the generic drawn-frame counter is not used.

The producer attaches a ticket and command ordinal to the queue sequence. Its
sidecar is published before the existing queue release; the consumer reads it
after acquiring that exact lease. Retiring the reply before slot reuse retains
the depth-two ownership contract, including synchronous resource prefixes.
Only successful execution of the tagged Present publishes completion. A
duplicate or out-of-order ticket fails rather than implicitly filling a gap.

The execution thread delivers each completed ticket once at its existing safe
service boundaries. It samples the current timer at delivery, writes the original
sample word and invokes8C604486 with that sample in R4. The existing interrupt
service guard preserves the interrupted registers, including the original R4;
guest RAM effects survive. A later timer setup owns the epoch even if an older
render is still outstanding. No extra timer reset, update, frame wait, device
interrupt controller or CPU wait for a physical GPU fence is introduced.

Each guest lifetime has its own reference-counted completion generation. Queued
old tickets may finish safely after reset but cannot notify restored state. Before
quicksave capture, the existing graphics drain is followed by notification
delivery, so the RAM snapshot includes completed work. Host tickets are not
serialized; existing save formats and personal saves are untouched. Full live
quicksave/movie/menu transitions under this candidate remain untested.

Completion here means the native renderer has processed the render command and
submitted its ordered GPU work successfully. It is neither measured physical
scanout nor proof that the GPU has retired every instruction.

## Verification

Incremental retained build:
`.local/menu-preview/build-render-completion-game-01.log`. Only four remaining
game build steps after the renderer/component build; no full AOT compile.
Ten-entry selected-AOT ownership and native link closure passed. The INVERSE
arithmetic candidate remains selected; its CMake default is still OFF.

Executable: `out/experimental/game.exe`,1,910,361,600 bytes, SHA256
`a5ca5a4c7571986fcdaf7bb67806b4ee8e8a3af44a921e60a4bd118bbf934f96`.

`sonic_render_completion_tests` passed with both D3D11 and Vulkan, hidden:
48 submitted synthetic frames yielded exactly16 guest notifications; host
frames,48 explicit repeats and48 image presentations were excluded. Resource
prefixes, queue-slot reuse, rejected Present without a frame, nested scopes,
duplicate/out-of-order rejection, replacement generations and4,096 concurrent
ticket publications passed. Logs:`render-completion-d3d.log` and
`render-completion-vulkan.log` under`.local/menu-preview`.

`sonic_legacy_video_tests` executes the SHA-bound original604486 with null and
fixture callbacks, verifying the two flags before the call, R4, callback return,
stack and R14. Both original update wrappers and existing video-option cases
also pass. Log:`render-completion-original-contract-01.log`. This is a bounded
original-code component, not a claim that every possible callback was exercised.

Live evidence:`runs/render-completion-ec-01/{result,update-timing,update-contract-analysis}.json`.
One hidden/muted60-second Emerald Coast forward fixture, copied saves, isolated
physical inputs with normal remapping,1280x720 D3D11,100%,VSync off,144 output.
First10 seconds excluded. No capture, execution sampler, fault or forced stop.

| Steady observation | Result |
| --- | ---: |
| Guest renders submitted / notifications delivered | 895 / 895 |
| Render notifications and new images per second | 18.0039 |
| Original task traversals per second | 60.8109 |
| Task traversals per new image | 3.3777 |
| Output presentations per second | 143.4879 |
| Execution CPU ms per new image | 54.2947 |
| Original timer setups | 895 |
| Former one-second callbacks | 0 |
| Observed nonnull additional render callbacks | 0 |

The final sample retains one asynchronous guest render outstanding, as expected
from the pipeline. The first/last steady counters are404/403/403 and
1299/1298/1298 for submitted/completed/delivered. The original trace analyzer
validates371 complete wrappers, their PAL phase, threshold, wait arguments and
post services. Five partial final events and8,137 events beyond the bounded
prefix are explicitly excluded; full-window counters continue. All witnesses
retain PAL50,TVword1,release2,delta2 and the checkpoint clock owner.

The preceding diagnostic measured54.84 CPU ms/image; this run measured54.29.
The PC was in use and routes are time-bounded, so that difference is not a
demonstrated optimization. Original task traversals are already near60/s while
new images remain near18/s. Making suitably timed states render at60/s and
reducing the work needed for them remain open. Interpolation stays withdrawn.
