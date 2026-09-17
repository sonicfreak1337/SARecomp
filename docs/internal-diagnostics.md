# Internal runtime diagnostics

Normal play defaults to diagnostics **off**. There is no Options-menu setting.
The private ON/OFF patches set `.sarecomp-diagnostics` beside the installed
program. The launcher reads it once, before starting any worker threads.
Changes take effect on the next launch. A missing or malformed file selects off.
Developers can override the file with `SARECOMP_INTERNAL_DIAGNOSTICS=0` or `1`.

The switch controls:

- Continuous crash-capsule breadcrumbs, provider transcripts, memory-access
  notes and closure probes, including automatic Windows input recording.
- Dispatch observation journals and repeated immutable pack-storage audits.
- Repeated checks that the port's own write observer has not been replaced.
- The renderer's complete vertex-value/normal/depth-capability audit. Actual
  vertex transforms, preprocessing and GPU uploads are identical in both modes.
- The periodic graphics telemetry snapshot, which otherwise waits for render
  replies solely to print a report.

Memory bounds, buffer/index limits, executable write tracking, module ownership
and lifecycle generations, callback resolution, exceptions, audio and guest
timing stay active. They perform work needed to execute the game. Explicit
developer benchmark/capture flags remain available independently so an OFF run
can be measured. Fault-time reporting still works; release crash reports still
require the user's confirmation. OFF does not retain a rolling pre-crash history.

## Patches

`out/patches/SARecomp-Diagnostics-ON.run` and
`out/patches/SARecomp-Diagnostics-OFF.run` target Linux and Steam Deck.
Run with the game closed, in Desktop Mode, as the normal user. No sudo or GDI.
The first use updates a previously native-math-patched installation; subsequent
switches write only the policy file and retain the executable inode. Both use
the existing Steam/desktop launch paths. Saves, Chao data and settings are not
opened by the patch. Executable and manifest updates are verified and reversible.

The shared policy is also compiled into the Windows performance candidate.
The accepted `out/experimental` build and r354 snapshot are unchanged.

The [2026-09-16 CPU update](cpu-update-20260916.md) preserves this policy but
replaces the Linux executable with a newer identity. The original standalone
ON/OFF patch packages remain bound to the D2 executable below; they must not
be used to downgrade the CPU update. Its internal policy file and developer
override continue to work normally.

The [September 17 native CPU update](cpu-update-20260917.md) has new, small
policy-only ON/OFF packages. They admit only its exact executable SHA-256 and
contain no program delta or decoder. An older or modified build is rejected;
they cannot downgrade the program. Both switches and repeated OFF were applied
to the two actual updated Linux launch paths. Executable hashes/inodes/modes,
manifests and Story/Chao/settings sentinels stayed unchanged. An old September 16
installation was rejected without changes. Test preferences were restored.
Evidence: `runs/native-cpu-diagnostics-linux-20260917.log`.

## Verification and performance

The Linux VM is a TCG software-graphics environment, not a Deck FPS estimate.
Nine policy cases pass on Windows and Linux. Patch fixtures cover repeated
ON/OFF switches, two existing launch paths, unchanged saves/settings, damaged
inputs, running games, symlink rejection and rollback after publication failure.
All 13 Vulkan renderer fixture images are byte-identical with diagnostics ON/OFF.

Both final self-extracting patches were also applied to an actual installed
Linux application with the supported native-math executable. The ON patch
reconstructed and verified the new program; the subsequent OFF patch retained
the exact executable inode and size. Each mode launched Windy Valley through
the installed program, without an environment override, and reached the expected
host stop after 15 title frames. Startup logs confirm enabled=1 then enabled=0.
The test installation is left OFF. This is a short installation/boot check,
not a new full-story validation.

The Windy Valley window 5..15 measured 888.13 execution CPU ms/frame ON and
833.34 OFF (about 6% less CPU time). ON enables extra journals which were not all
active in the previous release, so this does not establish a release improvement.
All four stage runs retained active_video_hz=50, release_slots=2, logical_delta=2
and native_gameplay_math=1.

Gamma Emerald Coast, Original timing, identical 800x500 viewport / 50% render
scale and title-frame window 5..25, native math and caches enabled:

| Build | Execution CPU ms/frame | Process CPU ms/frame | New frames/s |
| --- | ---: | ---: | ---: |
| Previous native-math build | 839.70 | 3358.09 | 0.5651 |
| Internal diagnostics OFF | 864.60 | 3447.78 | 0.5475 |

Both complete normally with the expected host stop, no forced termination.
This approximately 3% difference is **not evidence of an improvement**. The
20–25 ms target remains open; removing these diagnostics does not resolve the
measured execution bottleneck. Do not present this as the promised large Deck
performance patch. The explicit diagnostic-switch request is a separate delivery.

The final Linux program is SHA-256
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.
The patch base is
`7d4fb2b71694dead0ae7f76f105bb975429978a7dce3ce6cad44aace220c6921`.
Source preparations authenticate the frozen SDK source before making the small
port-local substitutions. No guest AOT unit is regenerated or recompiled.

Evidence: `runs/internal-diagnostics-linux/` and the Linux build logs under
`runs/build-internal-diagnostics-linux*.log`.

A subsequent [selected-AOT statistics experiment](linux-aot-statistics-experiment-20260916.md)
also completed Linux gameplay checks. Its runtime and compile-time variants
remain disabled because neither established a useful overall gain. It does
not change these patches or make their OFF mode omit every instruction counter.
