# Instruction-local PreparedRead32 differential plan

2026-09-13. Read-only audit against the pinned SDK headers and
`.local/sdk-leaves/memory.cpp`. Only this report was written. No tests,
builds, game execution, source changes, network or private-field access.

## Oracle and reusable setup

Use the **original generated translate/preflight/read lambda and instruction
envelope**, compared with the proposed prepared variant. Do not substitute
guest_read_u32_at alone for the original oracle: a write-only watchpoint
allows a direct read counted as unobserved, whereas the general Memory path
counts it observed even though the write callback does not run.

Implementation follow-up: `tools/test_prepared_read.cpp` now passes 41 cases.
For MMU tests, install a RuntimeAddressSpace, select AddressTranslationMode
Mmu/NoMmu and write its MMUCR as well as CpuState::mmucr. A bare CpuState with
only MMUCR changed does not activate that translation. Keep a separate
bare-CPU fallback case. The initial mmio_boundary_epoch is 1; a successful
MMIO read advances it to 2. See the experiment report for measured results.

The first witness is the exact envelope at generated
`unit-v8C029400-8C029B00-9bed0201322da5d9.cpp:3164`, source PC 8C02947C,
opcode 6432, R4 <- [R3], 2 guest cycles. Its translate/read lambdas are at
lines 125/190. Preserve the envelope's original flush/reload, MMIO epoch,
attempt/catch/complete and safepoint calls. The other required witness is
R0 <- [R14] at 8C036BE0 in the second unit named in the follow-up report.

Concrete retained setup exists in the old workspace under
`private/diagnostics/r312-prepared-ram-analysis-20260908a/prepared-probe.cpp`:
Memory(0), map_dreamcast_main_ram, independent checked/prepared CPUs,
sentinel destination registers and RAM, then reset_performance_counters.
Its historical prepared implementation is a different experiment; reuse
the setup pattern, not its old helper or incomplete state comparator.
The adjacent prepared-probe-writer.cpp uses synthetic MOV.L opcode 6212
followed by RTS/NOP. No game image is necessary for that component fixture.

Current `tools/test_render_culling.cpp:14` supplies a small PlatformServices
stub; lines 72-78 show two independent Memory/LinearMemoryDevice owners.
Its comparator omits fault/provenance/access state needed here. Do not run
the culling test or import its retail-image dependency for this task.

Small public-API RAM fixture, with one 64-KiB backing repeated twice:

```cpp
CpuState cpu{.memory = Memory{0u}};
auto ram = std::make_shared<LinearMemoryDevice>(0x10000u);
cpu.memory.map_region("ram0", 0x0C000000u, ram);
cpu.memory.map_region("ram1", 0x0C010000u, ram);
cpu.memory.bind_direct_linear_alias_window(0x0C000000u, 0x20000u, *ram);
cpu.memory.set_lookup_mode(MemoryLookupMode::Indexed);
cpu.write_sr(sr_md_mask);
cpu.mmucr = 0u;
ram->write_u32(0x100u, 0x13579BDFu);
auto guard = cpu.memory.direct_linear_memory_guard(false);
cpu.memory.reset_performance_counters();
```

Only physical Memory regions are necessary: guest_read_u32_at translates
physical/P1/P2 addresses to them. The generated fast translator passes P1/P2
through and folds physical addresses to P1 for direct_linear_guard_offset.
Calling that offset API with physical 0C000100 directly must fail; feeding
it 8C000100 or AC000100 after the original translator is correct.

Alternatively `map_dreamcast_main_ram(Memory&)` in dreamcast_memory.hpp:149
returns a shared LinearMemoryDevice and supplies the product 16-MiB/four-
mirror contract, as used by the retained probe. Keep both CPU owners alive;
never copy CpuState wholesale because it owns noncopyable Memory.

## Minimal differential matrix

Start every case from equivalent fresh fixtures. Compare an ordered vector
of callback snapshots, result/destination state, all four access counters,
CPU architectural/provenance state and MMIO epochs. Separately assert prepare
does not change any of those, and a guarded-consume miss leaves its output
sentinel and every counter untouched.

| Case | Setup / addresses | Required result |
| --- | --- | --- |
| Ordinary aliases | 0C000100, 8C000100, AC000100; also mirrored 0C010100 | Same word and counters; prepare succeeds for all after translation. |
| Privilege/MMU admission | Each alias with write_sr(0) or write_sr(sr_md_mask), and mmucr bit0 clear/set; empty UTLB initially | P1/P2 direct only in privileged mode, even with MMU enabled. Physical user access with MMU off remains eligible. Other failures use the unchanged guest reader and same exception. |
| Bounds | 8C01FFFC succeeds; 8C020000 is beyond window; 8BFFFFFC is below window | Last word succeeds; out-of-window falls back without prepared load/counter effects. |
| Alignment | 8C000101 and 8C000102, Strict then Permissive | Guard always misses unaligned words. Full original/prepared exception or fallback result matches; permissive Memory policy is not permission to broaden the guard. |
| Backing-wrap refusal | Separate backing size 2, physical regions 0C000000 and 0C000002, span 4; aligned four-byte read at 8C000000 | Public binding is legal; guard rejects width greater than backing. Fallback raises CrossRegion. For normal power-of-two backing >=4, an aligned word cannot straddle a backing boundary. |
| Read observer | add_watchpoint(0C000100,4,Read,callback); also a nonoverlapping Read watchpoint | Any Read watchpoint disables fresh direct guards globally. Matching watchpoint receives one original read event; nonoverlapping one receives none but the access is still counted observed. |
| Write-only observer | Same address, MemoryWatchpointAccess::Write; reacquire guard after adding | Fresh read guard succeeds; no write callback. Direct result and counters remain identical to original generated reader. |
| Full trace and guest sink | set_trace_handler; separately set_guest_memory_access_sink | Both disable direct reads; one trace/access event per successful fallback read. Sink records exact guest origin, virtual/physical address and attempt/retirement values. |
| Epoch invalidation | Save guard/token; map an unrelated region, clear/rebind window, toggle lookup Reference/Indexed, add/remove watcher, replace full trace/sink or guest-write observer | Old guard immediately fails via public current/read APIs. Reacquire for subsequent instructions. No direct generation value needs to be written or inspected privately. |
| Non-invalidating toggles | set_alignment_policy; set_mmio_access_tracking; set_mmio_trace_handler; set_mmio_interrupt_state_sink | These setters do **not** refresh direct generation in this pinned implementation. RAM guard stays current, read succeeds, no MMIO/dirty callbacks or epoch changes. |
| Move/reconstruction | Move-construct Memory; separately assign a fresh equivalently mapped Memory with different bytes into existing owner | Old source/destination guards reject while their owner objects remain alive; fresh guard reads new bytes. Never dereference a guard after owner destruction. |
| Preflight miss plus provider flush | Services::consume_guest_cycles mutates CPU source register and/or RAM via captured fixture pointer; set pending cycles beforehand and force initial miss | Envelope releases registers, flushes, reloads, then evaluates the new address for the actual read. Record exactly one flush-side mutation and the post-flush origin/address/result. |
| Actual MMIO fallback | map a small MmioMemoryDevice outside RAM; width-aware read handler logs/can reject | Prepare misses; exactly one device read; successful access advances mmio_boundary_epoch once, triggers installed MMIO trace/dirty sink, and retains envelope safepoint. Error/catch behavior matches when rejected. |

The epoch-mutation/changed-bytes tests are helper rejection/late-read tests,
not permission to insert callbacks or SR/MMU mutation into a successfully
prepared instruction. Add one late-read assertion by preparing then changing
ram->writable_bytes before consume: consumption must see the new word,
proving prepare did not fetch early. Do not attribute this synthetic mutation
to any admitted generated success envelope.

## Exact counters, observer order and exception state

Counter field order below is `(indexed_region_hits, reference_region_probes,
unobserved_accesses, observed_accesses)`, exposed by
Memory::performance_counters; reset via reset_performance_counters.

- Prepare success/miss and guarded-consume miss: `(0,0,0,0)` delta.
- Successful fresh direct word, including write-only watchpoint, guest-write
  observer and MMIO-only diagnostics: `(1,0,1,0)` delta.
- Successful Indexed fallback in the 64-KiB fixture with full trace or any
  watchpoint: `(1,0,0,1)` delta. With guest-memory sink alone: `(1,0,1,0)`.
- Reference fallback with the first RAM region and no access observer:
  `(0,1,1,0)`; second region `(0,2,1,0)`. This is why a fixed region order and
  whole-page mappings make the fixture easier to assert.
- Failed direct probe never counts as a read. Exception-path resolution
  probes vary by failure point: assert exact equality against the original
  envelope, not a blanket zero for every fault. Capture read-phase counters
  separately from any exception-entry diagnostic work.

Access event order is full trace first, then matching watchpoints in
registration order (memory.cpp:3100); GuestMemoryAccessSink follows the
ordinary Memory read accounting/observers. Capture vectors of operation,
address, width, value and region name. For the guest sink additionally record
instruction source/runtime PCs and valid flag, virtual/physical addresses,
attempt/retirement counters, size, scalar validity and backing offsets.
Independent fixtures have different backing pointer values: compare each
against its own expected ram.get(), not raw pointer equality across CPUs.
Use preallocated storage in noexcept sink callbacks.

GuestInstructionOrigin and MemoryAccessError expose the origin, reason,
operation, address, width and region. Keep source PC distinct from runtime
PC in at least one fixture. Use the envelope's
enter_memory_exception_with_provenance(cpu,error,runtimePC,opcode), declared
in exception.hpp:91, rather than a custom fault handler.

Compare r/r_bank, fr/xf, PC/PR/GBR/VBR, read_sr/read_fpscr, MAC/FPUL,
SSR/SPC/SGR/DBR/TRA/TEA/EXPEVT/INTEVT, PTEH/PTEL/PTEA/TTB/MMUCR/UTLB,
trap_pending, exception_generation, last_exception_cause,
exception_in_delay_slot, last_exception_instruction_pc/physical_pc/owner_pc,
last_exception_generation and every MemoryFaultProvenance field. Compare
attempted/retired counts, pending/total cycles and active instruction/block
virtual/physical metadata. Use explicit comparisons, not memcmp padding.
Seed unrelated fields and destination with sentinels to detect damage.

For the isolated 6432 successful envelope before terminal block finalization,
expect attempted +1, retired +1 and pending cycles +2 after any preflush.
A thrown read leaves destination unchanged and attempted +1/retired +0;
compare the original exception-entry cycle/register outcome. The constructor
and complete() APIs are public in runtime.hpp:289 and do not call services.

## Public mutation APIs and limitations

Memory supports map_region but no public unmap/replace-region or Memory
snapshot/restore method in the pinned header. Test changed backing through
move assignment/reconstruction, not invented APIs. Replacing bytes through
LinearMemoryDevice::writable_bytes does not invalidate a guard; guards cache
addresses, not values. This is not a full save-state restoration test.

Public generation witnesses are the guard's generation value,
direct_linear_memory_guard_current(guard,false), its bool conversion and
direct_linear_guard_read_u32. Never modify their copied public fields to
manufacture an otherwise impossible mapping; never access private counters
or generation_source_. The source-owner identity check belongs to
direct_linear_memory_guard_current, while inline consumption trusts the
guard's still-live Memory owner; tokens must never cross owners.

Source anchors: memory.hpp:305-434,747-858,877; memory.cpp:484-503 (moves),
705-792 (binding/current),1122-1176 (watchers/trace),1183-1247 (MMIO-only
setters),1253-1315 (write observer/guest sink),1417-1455 (read),2893-2943
(lookup counters),3045-3072 (eligibility). The pinned APIs are sufficient for
this bounded component matrix. No new SDK/header API or full-game fixture
is required. This report supplies a plan, not executed equivalence evidence.
