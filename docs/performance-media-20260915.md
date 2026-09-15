# Vulkan submission cost and smaller media runtime

Status: internal performance candidate; **no new installer produced**. The
accepted Windows executable under `out/experimental` is unchanged. Neither
the frozen r354 baseline, guest AOT, original media nor personal saves changed.
New Steam Deck installs default to Original timing until the performance issue
is resolved; explicit existing settings survive reinstalling.

## Vulkan changes

- Fixed-size pipeline keys retain the original 18 fields and warmup format.
- Descriptor writes for a newly allocated, unbound set are submitted together.
- Draw/capture sets use dynamic constant-buffer offsets. Within one submission,
  identical resource bindings reuse immutable descriptor sets. Every draw still
  uploads its current constants. An identical fog table can reuse its upload.
- Every key includes the backing buffers/ranges, texture view/layout, sampler,
  and all Type2 resources. Entries are cleared only after the submission fence
  and descriptor-pool reset. At 4,096 keys, ordinary allocation continues;
  no draw is discarded.
- Redundant pipeline, viewport and scissor commands are omitted. All graphics
  paths update this state and each new command-buffer recording resets it.

The private controls `SARECOMP_VULKAN_DESCRIPTOR_CACHE=0`,
`SARECOMP_VULKAN_STATE_CACHE=0`, and `SARECOMP_VULKAN_SEPARATE_WRITES=1` allow
same-binary comparisons. They are not user-facing options. Draw order, shaders,
blend/depth rules, simulation cadence and instruction semantics are unchanged.

## Measurements and limits

The Linux test environment is QEMU TCG with two virtual CPUs, 6 GB RAM and Mesa
llvmpipe. It is useful for finding costs, **not predicting Steam Deck FPS**.
The hidden probe uses fresh private user data, isolated forward input, Vulkan,
640x480 at 50% render scale, VSync off and Recompiled timing. It waits for active
gameplay rather than timing the lengthy introduction on this slow host.

| Actual Emerald Coast, same Linux executable and lean media libraries | Caches off | Caches on |
| --- | ---: | ---: |
| Steady newly drawn game frames/s | 0.519489 | 0.648099 |
| Execution-thread CPU ms/draw | 388.218 | 366.276 |
| All-process CPU ms/draw | 3659.373 | 2941.595 |
| Frames / full gameplay interval | 32 / 61.495 s | 39 / 60.099 s |

This pair has **24.8% more new game frames** and 19.6% less process CPU per draw.
It is not a 24.8% guest-execution optimization: llvmpipe is a software renderer,
TCG magnifies costs, and frame-indexed movement covers different distances.
Both probes complete and the stop frontier is the expected HostDeadline (2).
Artifacts: `runs/linux-caches-{control,final}-game-01/{summary.json,game.log}`.

The submission-only fixture reduces Linux render-owner CPU from 47.449 to
12.613 ms/frame with descriptor reuse; the subsequent state-cache pair is
12.648 versus 11.372 ms/frame. On the RX 7900 XTX the corresponding offscreen
descriptor pair is 2.344 versus 1.953–2.148 ms/frame, and state reuse is 2.246
versus 2.148 ms/frame. These are small synthetic draw workloads, not Sim FPS.

The first actual Windows pair is 53.002 new frames/s with both caches off and
51.170 on. Execution CPU is 18.055 versus 18.746 ms/frame. This does **not**
establish a Windows gameplay improvement. Keep this result alongside the
favorable VM/renderer measurements; do not claim a universal or Deck +50% win.
Artifacts: `runs/vulkan-caches-offscreen-{control,candidate}-02/result.json`.
The repeated enabled run (`candidate-03`) reaches 52.941 frames/s, 18.021 ms
execution CPU and 20.926 ms process CPU per frame, essentially matching the
disabled control. All three complete without contract failures. This supports
the absence of the first pair's apparent regression, not a Windows speedup.

The fresh 30-second Windows execution profile has 1,931 samples with 89.622 ms
total suspension. It points to distributed guest memory, dispatch and FPU work,
not one new 50% hotspot. Its nearest-symbol/COMDAT attribution is evidence for
investigation, not exclusive CPU percentages. The matching executable SHA is
`1226c65572f52d6269708bd527a2bb8a12bd43ab081bb1cae5d749c753133b9a`.
Artifacts: `runs/vulkan-current-execution-profile-01/execution-ip*.json`.

### Correctness checks

Thirteen renderer capture frames compare byte-for-byte with descriptor reuse
off/on, and again with state reuse off/on, on Windows and Linux. Coverage
includes updated/destroyed textures, persistent meshes, winding, clipping,
fog, depth and Type2 accumulation/resolve. A 140,000-draw fixture also exercises
upload/pool rollover; it is not proof of 4,096 distinct cache keys.

The current Windows desktop refuses Vulkan surface capabilities with result
-13 even for the earlier accepted build. An explicit hidden-only
`SARECOMP_VULKAN_OFFSCREEN_TEST=1` test mode therefore skips monitor presentation
while submitting the real GPU scene. It verifies actual window visibility,
requires background-test mode and logs activation. The first full-game attempt
failed because it checked the configuration's initial-visibility preference
instead of the already hidden window; the corrected candidate completes.
These results are not monitor-presentation measurements. Linux continues to
exercise the ordinary Xvfb swapchain path. No desktop/display settings changed.

## Media runtime size

`tools/build-lean-ffmpeg.py` builds the same pinned FFmpeg revision
`5a03dfa0f607ee6156a59bdad3987cd3b858ee5d` with only SA1's readers. It keeps
ADX, MPEG-1/2 video, ADX/MPEG-PS/MPEG-video demuxing/parsing, audio resampling,
pixel conversion and runtime x86 SIMD selection. The MPEG-video demuxer is
required by the MPEG-PS elementary-stream probe, including Sofdec sources.

| Five runtime libraries | Previous bytes | New bytes | Saved bytes |
| --- | ---: | ---: | ---: |
| Linux / Steam Deck | 139,796,704 | 3,309,416 | 136,487,288 |
| Windows | 109,179,904 | 4,306,432 | 104,873,472 |
| Linux libraries, same x86 + LZMA2 preset-4 compression | 43,869,772 | 947,936 | 42,921,836 |

The compressed row measures only the libraries. It is not a newly measured
installer size. Original assets are neither transcoded nor reduced in quality.
The package builder now verifies the five library hashes/sizes and includes
the LGPL license, exact source/configuration metadata, compatibility patch and
build recipe. It does not include compilers, headers or development tests.

The Linux target remains x86-64 / glibc 2.31, without a new system dependency.
The two configure fixes concern a removed sysctl header and a GNU-linker PE
workaround; codec implementation is unchanged. Windows DLLs retain ASLR and
NX flags. Baseline dependency libraries remain intact.

### Media verification

The comparison tool uses the actual native game codec provider, including its
custom AVIO and Sofdec header handling. On **both operating systems**, all
4,207 sources have matching first-32-sample fingerprints between the full and
lean libraries: 109 music ADXs, 10 videos and 4,088 AFS voice entries. The
fingerprints cover metadata, timestamps, durations, sample counts and decoded
PCM/RGBA bytes. They are not a full-length comparison of every source.

`SLH448_1.SFD` was additionally drained to EOF on both operating systems: 200
video frames, 9,188 audio chunks, matching metadata/sample fingerprints and
normal stream completion. Linux actual-game probes and the internal Windows
Emerald Coast candidate also run with the lean libraries.

Artifacts: `runs/media-decoding-windows-{control,lean}`,
`runs/linux-media-decoding`, `.local/menu-preview/linux-media-size.json`.

## Reproduction

Build only the dependency, without exporting or compiling guest AOT:

```text
python tools/build-lean-ffmpeg.py --platform linux --jobs 3
python tools/build-lean-ffmpeg.py --platform windows --jobs 3
```

Runtime staging is under `.local/lean-ffmpeg/runtime-{linux,windows}`.
`--stage-only` strips dedicated runtime copies of an existing dependency build.
The selected package payload must later be installed and tested normally; this
round intentionally leaves the delivered installers unchanged.

## Additional executable-size check

The stripped current Linux game has about 1.403 GB of machine code. Debug
symbols have already been removed from release staging; stripping the test
copy again is not a new installer-size saving. Runtime unwind tables, constant
data and code cannot simply be omitted from the package.

The next bounded check used LLVM's
[safe identical-code folding](https://reviews.llvm.org/D48146), which uses
[address-significance information](https://reviews.llvm.org/D48155) to retain
distinct addresses where required. The pinned Zig driver rejects `--icf`, so
the check replayed its verbose linker arguments through the already installed
LLD 19.1.5 in a separate ignored directory. No build configuration changed.

Matched LLD 19 links with the same existing inputs:

| Internal unstripped ELF | Bytes |
| --- | ---: |
| ICF off | 1,741,105,488 |
| Safe ICF | 1,741,080,080 |
| Difference | 25,408 |

This tiny saving does not justify a second production linker path. Neither
experimental ELF was promoted or packaged, and no performance claim is made.
The useful retained size saving remains the media-library reduction above.
Artifacts: `.local/size-linker-20260915/` (explicit argument lists and link logs).
