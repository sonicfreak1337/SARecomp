# Private Linux profile-guided optimization experiment

This is not a performance patch or installer. `SARECOMP_LINUX_PGO` defaults
to `OFF`. The control is the diagnostics-default-OFF Linux program, SHA-256
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.
The installed product, personal saves, accepted Windows executable and r354
are outside this experiment. The 20–25 ms goal remains open.

**Result: game instrumentation is rejected for delivery.** The standalone
Linux compiler/profile round trip works, but both game training variants
stall before a valid gameplay window. No game USE build, speedup claim,
performance patch or installer is produced. The control is restored exactly.

## Toolchain qualification

The pinned Zig compiler reports Clang 21.1.0. `-fprofile-generate` alone
successfully links a program but does not link a profile writer. A successful
build therefore did not establish working PGO. The private training build
explicitly links the matching upstream LLVM 21.1.0 compiler-rt profile runtime
and retains its registration constructor with `-u,__llvm_profile_runtime`.

`tools/prepare-linux-profile-runtime.py` authenticates the official source
archive, compiles the 19 upstream profile sources unchanged for
`x86_64-linux-gnu.2.31`, and records compiler/source/library identities. Its
runtime is not a product dependency. The USE build does not link it.

The installed Windows LLVM 19.1.5 profile reader lacks zlib support. Generate
uses LLVM's `-enable-name-compression=false`; it does not rewrite the raw
profile format. The qualification probe ran twice on Linux, wrote two real
profiles, merged with nonzero branch/function counts and compiled with
Clang 21 profile-use. Emitted IR contains the measured `branch_weights` and
`function_entry_count` metadata. Six Linux control/use checksums agree,
including four inputs outside the two training inputs. This validates the
pipeline, not a game-performance gain.

Primary references:

- [Clang PGO documentation](https://clang.llvm.org/docs/UsersManual.html#profile-guided-optimization)
- [Pinned compiler-rt profile sources](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.0/compiler-rt/lib/profile/CMakeLists.txt)
- [LLVM name compression switch](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.0/llvm/lib/ProfileData/InstrProf.cpp)
- [Profile-guided machine function splitting](https://github.com/llvm/llvm-project/blob/llvmorg-21.1.0/llvm/lib/CodeGen/MachineFunctionSplitter.cpp)

## Bounded game build

`cmake/LinuxPgo.cmake` selects 119 translation units: 42 common profiled AOT
units (including the retained inverse specialization), execution/runtime,
native title/gameplay, services, draw preparation, Vulkan, platform and audio.
It leaves the other game modules and SDL unchanged. The selected sources are
byte-identical copies at stable paths shared by GENERATE and USE, occupying
their original archive positions. Original object caches remain available.
`generated/pgo/scope.tsv` records the full source inventory.

GENERATE uses LLVM's default non-atomic counters and disables value profiling.
Shared host counters may lose concurrent increments; these are approximate
optimization hints, never performance or gameplay counters. There are no
value-profile callbacks on indirect guest calls. USE requires an existing merged profile and
treats mismatched instrumentation hashes as errors. Both preserve the existing
strict floating-point, guest memory, exception and timing flags. There are no
source-level gameplay substitutions, skipped updates or rendering shortcuts.
Training throughput is not a candidate performance measurement.

USE also enables `-fsplit-machine-functions` (separately controllable with
`SARECOMP_LINUX_PGO_SPLIT_COLD`). LLVM moves profile-cold machine blocks out
of hot functions to reduce instruction-cache fragmentation. The same six
Linux qualification checksums agree with this flag. Any game result belongs
to the combined PGO/splitting candidate, not to either flag in isolation.

The first atomic/value-profile training program booted and queued Gamma, but
did not reach gameplay. A short diagnostic sample found repeated guest calls
at `8C65F288`, `8C65F2D0`, `8C65F2F2` and `8C661240`. The owned test was
terminated externally with SIGKILL after SIGTERM did not finish shutdown;
the complete harness attempt took 859.3 seconds and returned -9 with no
gameplay samples. Its empty profile is not used. The harness's own
`forced_stop` flag is false because its watchdog did not initiate the stop;
`expected_stop=false` and the explicit external termination are authoritative.
This does not establish the cause of the stall or a product-performance result.
The second attempt removes both atomic counters and indirect-call value
instrumentation. Undefined-symbol inspection confirms the title/runtime
objects no longer call `__llvm_profile_instrument_*`. It progresses farther
through loading, but again provides no gameplay samples. A second 147-sample
diagnostic finds `8C661240`, `8C65F334` and `8C65F2F2`; it is externally
terminated after 417.9 seconds. Neither short diagnostic is a performance A/B
or establishes a root cause. Reducing instrumentation did not qualify game
training, and the failure is not an excuse to alter the guest wait protocol.

| Training variant | SHA-256 | Bytes | Valid gameplay samples |
| --- | --- | ---: | ---: |
| Atomic + value profiles | `0cd30803e5ef8dfaab0be3b3ce7444d1462a6c3754c9e17e486575275ffbb008` | 1,734,301,256 | 0 |
| Non-atomic counters only | `f9243cec883b9a7b75e28365dc8fdfa9b315e653acd2e2f71c888b593ee2d46a` | 1,734,688,288 | 0 |

Do not broaden instrumentation or compile USE from incomplete/empty training
files. Windy training was not started because Gamma failed. Retained fixture
profiles must never substitute for a game training profile.

The pinned upstream `Inputs/instrprof-value-prof-real.c` fixture also runs
under Linux with this writer, atomic instrumentation and indirect calls. It
finishes in 0.122 seconds and produces a readable 48,624-byte raw profile.
Both indirect sites have recorded targets; its deliberately large target set
exhausts the default value-node buffer, with the expected warnings. This
rules out a completely unusable value-profile writer, not a game-specific
instrumentation interaction.

## Restoration

The retained Linux cache is `SARECOMP_LINUX_PGO=OFF`. Existing original
objects were re-archived and linked in nine build actions, without recompiling
the game or analyzing/exporting AOT. Canonical stripping retains `.comment`.
The resulting 1,678,965,680-byte executable matches D2 exactly. Host and VM
isolated slots are restored from that control; failed training binaries are
not retained as runnable candidate outputs. Source/object caches and reports
remain available for a future investigation with new evidence.

The fresh restored Gamma run passes: image boundaries 5..25 produce 20 new
images and 68 guest updates, ending at guest tick 258. Every sample retains
PAL 50 Hz, release 2, logical delta 2 and native gameplay math. The process
returns the expected host deadline (exit 1, stop reason 2), with no forced
stop. Total harness duration is 252.7 seconds. Execution CPU is 239.258 ms
per guest update in this software-emulated VM, consistent with the earlier
control runs. This is restoration evidence, not a performance improvement
or a Steam Deck frame-rate prediction.

Evidence: `runs/linux-pgo/`, especially both failed summaries, toolchain
qualification, source identities, restoration records and
`gamma-restored-summary.json`. No game process remains running after the check.
