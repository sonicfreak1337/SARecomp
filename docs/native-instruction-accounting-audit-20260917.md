# Remaining instruction accounting: bounded source audit

This is a next-step investigation, not an enabled optimization or a measured
gain. Do not change the qualified September 17 delivery based on this alone.

The remaining compiled SH4-shaped bodies still publish attempted/retired
instruction counts and pending guest cycles at fine granularity. For example,
the prepared `unit-v8C0400A0-8C04124E-c3a8c709f8ba2806.cpp` increments all three
around guest PC 8C0400AA. Potentially faulting operations additionally use
`ExplicitGuestInstructionAttempt`: active source PCs, an exception generation
witness, pending cycles, attempted count and conditional retirement.

The native-port service does not emulate a Dreamcast cycle scheduler here:
`NativePortAotServices::consume_guest_cycles` advances its sequence and uses
the cycle total as a host lifecycle/deadline polling budget. Its
`observe_guest_block_completion` is empty. Therefore pure instruction-count
publication is a plausible remaining cost, but guest cycles cannot simply be
removed: that would alter host polling/stop responsiveness. Current exception
PCs and the exception-generation checks also have functional uses.

Known counter consumers found in the pinned local SDK:

- Crash/stall reports, saved development state and memory-access provenance.
- The dynamic interpreter's instruction budget (must remain counted).
- Exception state preservation and native handoff validation.
- Native-port cycle commit and host polling (must preserve their existing work).

An arbitrary memory observer may inspect the CPU during a write. Consequently,
blind source deletion or an SDK-wide macro is not a qualified implementation.
A future candidate needs a native-port-only contract, a way to preserve the
counted diagnostic/observer path, and exact CPU/RAM/exception comparisons before
a matched game benchmark. Prefer batching proven straight-line bookkeeping at
the next observable boundary, retaining exact totals, over zeroing counters.
The existing RAM regions already do this in their admitted spans; establish
what remains outside those spans before extending the transformation. Do not
repeat the rejected six-procedure register-ABI or full hardware-FPU experiments.

Evidence examined (local pinned SDK / generated preparation):

- `include/katana/runtime/runtime.hpp`: ExplicitGuestInstructionAttempt.
- `src/runtime/native_port_runtime.cpp`: consume_guest_cycles and finalize.
- `src/runtime/dynamic_interpreter.cpp`: fallback instruction budget.
- `src/runtime/memory.cpp`: write/access provenance counters.
- `build-linux/generated/region-writes/unit-v8C0400A0-8C04124E-c3a8c709f8ba2806.cpp`.

The executable size audit finds about 1.404 GB of `.text`, 88.6 MB of unwind
frames and 44.6 MB of exception tables. Removing non-loaded symbols/debug data
would save download/install space but does not by itself reduce execution CPU.
Do not present stripping as a frame-rate optimization.
