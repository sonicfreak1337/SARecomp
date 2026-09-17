# Complete movement/contact owner, 17 September 2026

The complete common movement owner `8C073018`, including body
`0730A0..074214`, is implemented behind the private `SARECOMP_NATIVE_MOVEMENT=1`
switch. Default OFF on both platforms. This is distinct from replacing only its
small stage selector or another single vector operation. Its gameplay speedup
is not presumed from the scope or from component results.

## Scope and preserved boundaries

The authenticated body contains 1,869 normal instructions, 137 delay-slot
instructions and 74 external call sites. The finite authoring generator emits
C++ for the full candidate selection, filtering, sorting, subsequent TOUCH
query, contact response and position/orientation finalization. It does not
decode instructions at runtime. Stage 0902/0903 retain their special movement
owner `077720` through the original tail-call semantics.

One direct RAM capability and host FPU epoch covers each callback-free stretch.
Candidate arrays/counts remain live guest RAM. Real callbacks publish CPU state
and close that capability/epoch; successful returns revalidate memory/source and
reacquire the context. No arbitrary callback flattening, host-only replacement
of guest-visible scratch data, or generic device access is introduced.

Unsupported modes decline before mutation. Unsupported memory accesses resume
before the original instruction. Delay-slot failures resume their parent branch;
JSR/BSR restores the previous PR before that branch runs again. Interrupted
callbacks, changed source, runtime stop/trap and executable invalidation retain
the established dispatcher boundary.

The retained global dispatcher deliberately does not register every inner PC.
The integration therefore provides 354 additional local restart points inside
the same AOT invocation. `ResumeOriginal` falls into those original instruction
scopes directly. It does not attempt an invalid global dispatch to `0730E2` or
`073A40`. The original prologue/block setup precedes local jumps; terminal jumps
enter before target/PR capture and preflight. The original public entry set is
unchanged.

## Source identities

- PAL RAM SHA-256:
  `b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c`.
- Main body SHA-256:
  `a6ea591ff19b73ed4f2124d643c6f94b9651cef5ec9f88cdb7d42cf29eb3e001`.
- Retained AOT unit SHA-256:
  `f18e090b5b8b0f6899c122dc28234f7226464391c7e12339b702d3e9c30caf92`.
- Development provider:
  `aea78670b105bc58f100692730852fc842fe7581d9c9aa9da6ca5b8ad9c687a9`.
- Windows game SHA-256:
  `79fe7ef0b281ccb03964ce8b304fc7fae4dd065bc9e0eb34345d13f734a6c4a6`.
- Linux game SHA-256:
  `bae3f0cf5c0b014a6ffd9356535565c23438f74a92edd80c39d9ef94fff09229`.

Provider refresh changes only the dispatcher identity strings and derived
resident generation; `runs/movement-dispatch-refresh-proof-20260917.json`
records the comparison. The retained archive is not regenerated. The bridge
authenticates both the original unit and any prior RAM-region preparation.

## Functional qualification

Windows and Linux each pass 216 component cases against the original instruction
reference. Tests compare full CPU state and 16 MiB RAM at callback entry/exit and
at completion. Cases cover candidate ordering and mutations, FPU modes/banks,
observer/code invalidation and unsafe-access frontiers. The instrumented corpus
visits 1,527 of 2,006 body/delay instructions (76.12%), not every possible branch.
Nineteen affected boundary cases pass after the final outcome separation.

Ten additional cases per platform compile and execute the actual AOT bridge,
including ordinary complete runs, observer invalidation, source mutation,
unaligned reads, protected writes and JSR delay failure at `073A40`. The fixture
intentionally rejects unregistered inner global entries. CPU, full RAM and
exception boundaries match the retained reference; local continuation is proved.
An existing independent read-only reviewer found no remaining bridge blocker.

Windows Gamma Emerald Coast passes with movement ON in Original timing through
frames 5..25 (183 native calls, no declines/reentries), and with movement OFF in
Recompiled timing. Normal installed Windows gameplay also reaches frames 5..15
at 60/1/1 cadence with zero movement calls. These are functional checks; the ON
run overlapped packaging and is not performance evidence. No full-story or
full-level-matrix claim is made.

The Windows development output under `out/native-objects-windows-20260917` was
relinked with this new default-OFF code; it is no longer the old v2 control.
The newly qualified Windows test installer contains that code OFF. Linux/Deck
test installers retain the earlier v2 executable without the movement code.
The baseline, installed user saves and delivered v2 patch are unchanged.

## Gameplay measurement

The comparison uses the same Linux executable, Gamma Emerald Coast, Original
timing, frames 5..25, four VM vCPUs and two llvmpipe threads. Existing shipped
object/model groups remain enabled. Only the movement switch changes. Results
must distinguish CPU per game update, new images per second, original cadence,
and exact endpoint state. TCG VM results are not Steam Deck measurements.

Both runs complete normally and match all 19 endpoint fields at frames 5 and
25, including bit-exact player position, HUD, animation counters and 50/2/2
cadence. Both execute 68 updates in the 20-image measurement window. Movement
ON advances from 189 to 257 native calls and 6,818 to 9,302 callbacks, with zero
declines or local reentries; OFF remains at zero.

| Metric | OFF | ON | Change |
| --- | ---: | ---: | ---: |
| Execution CPU ms / update | 191.311 | 186.493 | -2.52% |
| Process CPU ms / update | 801.199 | 806.607 | +0.67% |
| New images / second | 1.0906 | 1.0409 | -4.56% |

This does not demonstrate an overall speedup. Keep movement OFF; no release
default changes or new patch follow from this measurement. Do not repeat this
unchanged pair or present its execution-only saving as a global improvement.
The complete comparison is `runs/movement-gamma-comparison-linux-20260917.json`.

Evidence: `runs/movement-frontiers-*-test-20260917.log`,
`runs/movement-aot-{windows-test,linux-final-test}-20260917.log`,
`runs/movement-owner-{first,off}-windows-20260917/result.json`, and
`runs/movement-gamma-{off,on}-linux-20260917.log`.

## Further related scope

The first/alternative/second-pass contact classification family
`075356..0755C6`, `0757A0..075980`, `075980..075ECC` shares live contact records
with this owner. The read-only inventory finds 1,011 normal and 83 delay-slot
instructions plus 52 math calls, but only 9/4,023 historical exclusive samples.
That is a possible context-sharing boundary, not evidence of a large standalone
win. Preserve the external TOUCH boundary and live contact-list semantics.
The separate active collision-list/hierarchy/polygon-construction scope
`028EC2 -> 052518 -> 028B00 -> 0287A0` remains another substantial candidate in
`native-movement-resolver-next-20260917.md`; neither is implemented here.
