# Installed SADX: actual execution and render scheduling

This is a read-only comparison with the installed Steam executable, not a
benchmark of SADX and not a new performance result for this port. The executable,
configuration, and saves were not changed. No SADX code or assets were imported.

## Exact reference

`C:/Program Files (x86)/Steam/steamapps/common/Sonic Adventure DX/Sonic Adventure DX.exe`
is a PE32 x86 executable, image base `00400000`, 90,922,000 bytes, SHA-256
`e4330c00d7ee3910ecc1acab789ffd2a2d571c506984b99a8de4d3663e697fd6`.
Steam app 71250, installed build 411939. Addresses below were independently
decoded from that executable. The public
[SADX Mod Loader](https://github.com/X-Hax/sadx-mod-loader) targets the 2004
PC release; its addresses are not interchangeable with this Steam binary.

The earlier `sadx-timing-reference.md` only examined the 60-Hz counter and
limiter. That does not explain our guest-execution cost. Copying SADX's busy
polling would not reduce CPU work.

## Object execution

| Steam address | Observed behavior |
| --- | --- |
| `004F3C10..004F3CB3` | Execute one bucket. Load next task before invoking task callback at +10h. Set/clear current callback global `05BAF520`. 49 x86 instructions / 164 bytes. |
| `004F3E20..004F3EB2` | Execute bucket 7, then 0..6, with additional DX-specific branches. |
| `004F49A0..004F49ED` | Execute children from parent +0Ch, likewise next-before-callback. 27 x86 instructions / 78 bytes. |
| `004F3CC0`, `004F3D40`, `004F3EC0` | Display bucket, child display, and all-bucket display. Display callback is at task +14h. |

The diagnostic call at `004F3C5F` is only a self-link error path.
`00404EF0` formats text into `0629C100`; it is not a timing or scheduling call.

Our task owner `8C0986CC` has 33 translated guest PCs and 1,241 generated C++
lines. Its successful non-null loop contains only 11 guest instructions / 18
guest cycles: most inclusive time belongs to the invoked callbacks. The large
source expansion illustrates instruction, memory and dispatch scaffolding,
but source-line counts are not a CPU measurement. Replacing this outer loop
alone would leave its expensive descendants unchanged. No speedup is claimed.

## Catch-up updates and drawing

SADX's `004C81C0..004C832D` decrements the remaining-update counter at
`05B9D838` before calling `004C8330`. N updates therefore see N-1 through 0.
The update body calls ExecuteAllTasks at `004C8352`. Separate display passes
set that counter to zero and invoke DisplayAllObjects, e.g. `004C8570`.

The queue processor `004A2A30..004A384F` visits 2,048 buckets. Its check at
`004A2A9B` bypasses all queue loops when remaining updates are nonzero.
Drawing/animation preparation has earlier guards too: `004A9660` branches
directly to its epilogue on a nonzero counter, before motion/model work.
Equivalent entry guards occur at `004A2010`, `004A44B0`, `004A88F0`, `004A9DF0`.
The outer main loop resets queue state (`004AC258`), runs the scene handler
(`004AC267`), then processes the queue (`004AC2B8`).

This is relevant, but it is not an unimplemented concept in our Dreamcast port:

- Dreamcast entries `8C03700C`, `8C037098`, and `8C038D00` test update index
  `8C754E08` and return before drawing when nonzero. The Dreamcast counter
  counts up; SADX's remaining-update counter counts down. Copying the numeric
  predicate into another scheduling convention would be wrong.
- The original AOT entry guards at `03700C` and `037098` are retained.
  `src/sonic_render_culling.cpp` also preserves the `038D00` guard before
  performing native widescreen culling. Glyph and 3D sprite providers retain
  their own update-index guards.
- Existing Linux samples have 302 additional native palette calls in each of
  two successive images while the intervening update counts are respectively
  four and three. This supports drawing being gated already, rather than all
  drawing simply being repeated on every catch-up update. It does not prove
  every custom/native provider has the right scope.

Thus neither dropping catch-up updates nor globally skipping motion/transform
calls is justified. Those functions also supply gameplay joints and collision.
The original timing behavior is documented in `original-update-live-20260913.md`.

## Consequence for optimization

SADX executes game-object functions as native x86 procedures. Our remaining
translated functions still publish instruction/cycle state, perform checked
RAM operations and cross generic dispatch/register boundaries. Current active
profiles show this work distributed across many callbacks. A global change
needs to reduce that shared execution work, or replace complete substantial
owners; another limiter, a native outer task loop, or additional output frames
does not address it. The existing lack of net performance gain remains explicit.

The follow-up RAM audit also rules out two tempting repeats: the immutable
range classifier already has a 256-byte cell index, and the prepared runtime
already bypasses repeated `aot_contract_valid()` auditing with diagnostics OFF.
Mapping-generation checks and executable-write notification remain functional.
Prepared transfers, read groups, preloaded reads, and observer caching already
cover the simple cache variants; none is presented as a newly discovered fix.

There are longer original execution spans worth separating from the outer
task loop: `8C0CC090..8C0CC138` performs 85 consecutive instructions, including
60 memory operations, before the branch at `8C0CC13A`; it repeatedly updates
object flags through live pointers. `8C073D54..8C073DDA` mixes vector arithmetic,
loads, stores and a vector swap before branching. These are examples of scope,
not measured hot-block rankings: an inclusive parent profile does not prove
either path executes frequently. Their pointer dependencies, ordered writes,
possible aliases and interior resume entries prevent treating them as a single
unchecked memcpy or merely increasing the earlier adjacent-read-group size.

The next meaningful execution experiment must cover complete memory-bearing
regions, aggregate successful instruction accounting, and publish precise state
at faults and external calls. It needs actual region hotness and equivalence
evidence first. This static comparison alone does not establish a safe runtime
shortcut or a percentage gain.

The [public SADX decompilation](https://github.com/doldecomp/sadx) was also
checked at `4c2b833be9b4b63d57d761beb647cb8d9acbedb6`. Its available C/C++ sources
at that revision are SDK/runtime/library work, not a complete native gameplay
implementation we can directly adopt. It targets a different release as well.

## Fixed-callee follow-up

A separate read-only audit checked the actual prepared-transfer dispatcher and
the 41 profiled AOT units. They already contain 2,886 direct
`fn_*_runtime_entry(cpu, context)` call sites, zero remaining `static_call`
sites, 2,342 `runtime_only_call` sites and 18 exact-guarded fallbacks. These are
static source counts, including generated repetitions, not call frequencies.
The exact-guarded examples also already have an admitted direct-call path.

That path preserves register publication, chainability and pending selection,
the call-depth guard, exception generation, return PC and current memory guard.
Remaining dynamic dispatch also selects native replacement hooks and overlays;
substituting an AOT function pointer would bypass functional owner selection.
The old exact Windows profile attributes 18/1,316 game samples to
`dispatch_native` and 15/1,316 to entry lookup, including dynamic calls. It does
not support a new fixed-literal-call optimization, especially after prepared
transfers. Do not repeat that pilot without new evidence.

Effective source: `build-linux/generated/transfer-plans/native-port-dispatch.cpp`.
Representative retained direct call:
`unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp:2067`.
The implemented follow-up and measured limitations are recorded in
`linux-ram-regions-experiment-20260916.md`.
