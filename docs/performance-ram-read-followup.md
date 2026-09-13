# RAM read preparation follow-up

Read-only audit for Eggman, 2026-09-13. No source implementation, build, game
start, device operation, AOT extraction/regeneration or agent fan-out.
Only this report is authored. The dispatch-source memo is already integrated
and is not proposed again.

## Decision

**The duplicate work survives native optimization.** Both requested AOT
units contain a successful preflight followed by a reader that repeats the
architectural address translation and guard/range validation. This is not
merely duplicated C++ that Clang already eliminated.

Recommend one bounded first experiment: **reuse the translated address for
ordinary single-word GPR reads, while retaining the existing public guarded
reader for the actual load, generation/range validation and accounting.**
This is a deliberately smaller version of the earlier full prepared-offset
proposal. It removes the second architectural translation, not every repeated
bounds check. It does not read early or change the SDK/header ABI.

GO for that source-bound prototype and its later native-code comparison;
not a demonstrated game speedup or permission for a broad RAM rewrite.

## Inspected inputs and scope

Repository: `C:/Users/ultim/Desktop/Sonic Adventure Recompiled`.
Branch: `enhancements/ingame-settings`.
HEAD at start: `c3f8d0bb35790f6c9f3064c9c02637bfeaaad8f6`.
Root's tutorial integration was dirty at entry; it subsequently committed
while this audit remained read-only. Observed later HEAD:
`07938aa29ff87cf9bb276a32c06664728b41ca2b`.
No tutorial, CMake, adapter or manifest changes belong to this report.

The two inspected sources are under `.local/working-product/generated/code/`:

| Unit | Bytes | SHA-256 |
| --- | ---: | --- |
| `unit-v8C029400-8C029B00-9bed0201322da5d9.cpp` | 3851719 | `d99c58370239f5009140005a238795a5dd6a0e434ba9ffe068857deaa556b9d1` |
| `unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp` | 3517814 | `c34ed098e7625b5a432263ebf286ae486cb86b534dad0cdb97d4991f2253e45a` |

These hashes match their individual records at lines 1163/1175 of the
working generated-artifact manifest. This is not a new full-bundle audit.

Other inspected contracts:

- `.local/baseline/r354/sdk/include/katana/runtime/memory.hpp` SHA
  `8e194176d954262a97110f1e2e3bc8ca6d32c44d33e52c965b7eb45607b7b640`.
- The adjacent `runtime.hpp` SHA
  `74661d49e5556055b6fd7d05e42d89c7fc4f8aed9167b21012f204de752c52a3`.
- `.local/sdk-leaves/memory.cpp` SHA
  `56806312c7d8fcdd5d5d33c0678397757ba0c52830566333f872cc8ba63db6f5`,
  matching the existing pinned-leaf recipe.
- Existing `docs/performance-next-audit.md`, `docs/dispatch-memoization.md`,
  `runs/perf-before-windy/ip-resolved.txt` and current import/preparation seams.

## Native evidence, not just generated source

Read-only disassembly used installed LLVM objdump 19.1.5 at:
`C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/llvm-objdump.exe`.
Commands used `-d --no-show-raw-insn --start-address=... --stop-address=...`
on bounded regions; output was inspected in memory, not exported.

Native authority is the retained historical reference:

- `.local/performance-reference/game.exe`, SHA
  `c15acd3199022ac41cac3721e2cfb7175e0c8fa5415524c3a2de67cdb343c9f4`.
- Its `game-native-port.map`, SHA
  `e613ecae7e16194330159775ee84856ab2665dd3a9579689c386869ebc125a78`.

The following are preferred-base **native VAs**, not guest PCs or sampled
ASLR addresses. Image base is `0x140000000`; subtract it for an RVA.
The evidence proves the behavior in this retained optimized binary. It is
not a claim that Root's latest in-progress product was disassembled.

| Guest witness | Successful native path |
| --- | --- |
| `0x8C02947C`, ordinary `R4 = read32(R3)`; first unit source 3164-3195 | `0x19FB259D3` calls preflight `0x140394AF0`; success branches through `0x19FB25A44`; instruction-attempt constructor at `0x19FB25A83`; reader call `0x19FB25AB4 -> 0x146A92420`. |
| `0x8C036BE0`, ordinary `R0 = read32(R14)`; second unit source 1392-1423 | Inlined preflight at `0x1A0299E45..0x1A0299F20`, successful range tail `0x1A02DDFE9..0x1A02DE015`, instruction bookkeeping `0x1A029A11C..0x1A029A17F`, then `0x1A029A1B3 -> 0x142BAF3E0`. |

The shared preflight at `0x140394AF0..0x140394BEB` includes privilege/MMU/
segment translation and guard-generation, alignment, window and backing-wrap
checks. Both units' lambda symbols map to this address; linker folding means
the attribution of a shared helper is not exclusive guest-function ownership.

The first unit's reader repeats architectural translation at
`0x146A92440..0x146A924B0`, then guard/range checks, then performs the load at
`0x146A92533` and the two successful-read counter increments at
`0x146A92536` and `0x146A9253D`.
The second unit repeats the same structure at
`0x142BAF3F9..0x142BAF46F`; its load is at `0x142BAF4EC`, increments at
`0x142BAF4F0` and `0x142BAF4F7`.

An additional first-unit FPU witness, guest `0x8C02943A`, calls the preflight
at `0x19FB21277` (and again at `0x19FB212A2` for a pair), starts the
instruction at `0x19FB2134D`, checks FD/PR/SZ, then calls the reader at
`0x19FB21577`. It confirms the duplication but is **excluded from the first
implementation** because its exception and paired-read ordering is broader.

The prior trace attributes 44 and 29 samples to these units. Those counts
do not establish the exclusive cost or invocation rate of the two witness
instructions. No percentage saving is inferred from them.

## One implementation seam

Use a manifest-bound preparation step for separate build copies of these
two units, following the existing port-local generated-source preparation
pattern. A possible new recipe is `tools/prepare-ram-read-aot.py`; it is a
proposal, not an existing file. Do not overload the dispatch-memo seam.

Only the selected replacement AOT members should be compiled/linked before
their retained archive members. Verify the link map contains exactly one
effective definition and the intended replacement owners. Do not rely only
on archive names or symbol-resolution success to establish the replacement.
Root owns the CMake/link-audit change. Keep original signatures and an
unmodified/off preparation path; no old Katana or baseline modifications.

The transformation is structural, not a runtime guest-PC allowlist:

1. Find an ordinary, non-delay-slot instruction with one four-byte preflight,
   one `katana_direct_ram_read_u32` into a GPR, the original attempt/catch/
   completion skeleton, and no intervening callback or CPU-mode mutation.
2. Preserve the old preflight boolean's meaning and uses. Alongside it retain
   the original guest address and the successfully translated direct address.
3. At the original load position, try the retained SDK guarded reader with
   that translated address. If preparation failed, the original address no
   longer matches, or the guarded reader misses, call the original generated
   reader with the current guest address. Never reuse the pre-flush address
   on a preflight-miss path.
4. Leave stores, FPU loads, sign-extending widths, paired/post-increment reads,
   delay slots, ambiguous instruction shapes and all other units unchanged.

The two files contain 174/225 textual single-width preflight declarations.
These are **not** 399 qualified sites: the shape also occurs in instructions
and delay-slot contexts excluded above. Count and assert only fully matched
instruction envelopes during preparation; do not silently transform a loose
global regex match. The two ordinary witnesses above are minimum fixtures.

Conceptual instruction-local token and consumption (not applied code):

```cpp
struct PreparedRead32 {
    std::uint32_t guest_address{};
    std::uint32_t direct_address{};
    bool valid{};
};
// prepare: unchanged translate + direct_linear_guard_offset(..., 4, ignored).
// It reads no guest data and changes no access counters.

std::uint32_t value = 0;
if (prepared.valid && prepared.guest_address == current_guest_address &&
    direct_linear_guard_read_u32(guard, prepared.direct_address, value)) {
    // Publish to the original GPR here, within the existing instruction try.
} else {
    value = original_read_u32(origin, current_guest_address, original_allow);
}
```

Use an inline/local implementation, not a new out-of-line per-read service.
The original helper remains available and unmodified for fallback. A token
lives for this single instruction; it is not a TLS cache or a retained pointer.

## Required invariants

- **Attempt/exception order:** preflight still precedes pending-cycle flush
  on a miss. Active instruction PC/provenance, attempted count and pending
  cycles are set before the actual read. Destination update remains after
  successful read; completion/retirement and the existing memory-error catch
  remain in their original positions. Do not prefetch a value in prepare.
- **Architectural state:** successful prepare-to-consume regions cannot
  mutate the address registers, SR privilege or MMUCR translation mode, or
  cross a guest/provider callback. `ExplicitGuestInstructionAttempt` only
  updates the inspected bookkeeping fields; it does not call a provider.
  PC relocation is not permission to translate the data address differently.
  Leave shapes with an unestablished boundary unchanged.
- **Aliases/alignment:** preserve the generated P1/P2 privileged rule, the
  other-segment MMU/user checks and the existing physical folding rules.
  Do not use `unrelocate_code_address` for data addresses. The guarded reader
  retains four-byte alignment, full physical-span and backing-wrap checks.
  No host-tolerated unaligned load substitutes for guest semantics.
- **Memory epoch:** retain the same Memory-owned guard. The public reader
  checks its live generation before dereferencing. Mapping/backing/lookup/
  observer mutations invalidate that guard; scope/move/restore must not keep
  tokens alive. No raw pointer persists across a call or instruction boundary.
- **Observers/accounting:** successful reads still increment indexed hits
  and unobserved accesses exactly once through the SDK helper. A guard miss
  does not consume a read or change counters. Fallback retains its observer,
  MMIO epoch and post-instruction safepoint behavior, including register
  flush/reload and address re-evaluation after a provider flush.
- **Writes/lifecycle:** unchanged. Do not touch DirectLinearWriteBatch,
  guest write observers, code-tracker generations, bytes-changed detection,
  ordered write notifications or loaded-image retirement. A read optimization
  must not claim that code/data alias tracking can be removed.
- **Fallback:** failed optimization eligibility means the current executable
  path runs. It does not introduce a stage/PC stop or an admission requirement.

## Why not consume a raw prepared offset immediately?

The guard's `account_successful_read` and counter pointer are private. A bare
`memcpy(guard.read_bytes + offset)` would omit required counters; changing the
pinned class or using private-field/const-cast tricks is not the small seam.

There **is** a public `Memory::account_prevalidated_unobserved_accesses` API
(`memory.hpp:773-779`, leaf `memory.cpp:694-702`). It is not a drop-in synonym
for guard accounting: it rejects any access observer and MMIO tracing/tracking,
whereas direct-read enablement is read-observer-specific
(`memory.cpp:3045-3071`). A write-only watchpoint can leave direct reads valid
while the aggregate-account API refuses them. It also adds an out-of-line
call and checks. Consequently a full raw-offset/accounting fusion needs a
separate cost/order comparison and precise refusal fallback; this report
does not recommend that broader variant as the next smallest experiment.

The recommended partial reuse intentionally preserves the second guard/
bounds pass. Report its native result honestly as translation reuse, not
complete elimination of duplicated memory preparation.

## Later checks and acceptance, owned by Eggman

No checks below were executed in this audit.

- Preparation assertions: exact source hashes/manifest records; unchanged
  unselected bodies; both witness envelopes; no replacement in delay/FPU/
  store shapes; original fallback/catch/flush/completion text retained.
- Component differential checks: compare original versus prepared ordinary
  read for physical/P1/P2 aliases, privilege/MMU modes, aligned and misaligned
  addresses, window end/backing wrap, read watchpoints and write-only
  watchpoints, trace/sink changes, guard invalidation and Memory move/restore.
- Compare full CPU exception/provenance/counter state and ordered observer
  events. A preflight-miss callback which changes address registers or memory
  must use post-callback state. Successful RAM reads must not add MMIO events
  or write notifications. Do not manufacture mode mutation inside the
  successful callback-free envelope and call that an admitted use.
- Reuse Root's component/gate orchestration. The inspected Options/save
  enhancement test is not already a RAM-equivalence fixture; do not claim
  its success supplies this proof or add another full game matrix.
- Inspect the replacement native witnesses: the second SR/MMU/segment
  translation should disappear on successful prepare, with load position,
  guard-generation/range checks and successful-read accounting retained.
  Reject the candidate if added token handling/calls/spills erase the benefit.
- Then use the existing bounded EC/Windy performance route with identical
  settings, original cadence and aligned world intervals. Compare native
  code size and main-thread/whole-process costs separately; diagnostic flags
  must match. Keep the original/off implementation and executable identities.

Expected benefit is **small and unmeasured** for this deliberately partial
change. The profitable instruction share and frequency are not established;
neither the 44+29 samples nor source-match counts support an FPS percentage.
The native-code evidence justifies a bounded experiment, not a new 10-25%
promise. No further expansion or standing monitoring is requested.
