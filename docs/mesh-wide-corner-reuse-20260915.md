# Exact mesh-wide corner reuse

Internal performance follow-up to the Vulkan/media and Linux comparison work.
No installer or baseline promotion. Original timing stays available, and new
Deck installations still select Original until the performance issue is resolved.

## Change and boundaries

The previous indexed path reused an authored corner inside one polygon. The
new source plan additionally identifies equal **point index + raw UV pair**
across a mesh. Different model points and either side of a UV seam remain
distinct. It stores representatives and the original logical triangle order.
Every draw rebuilds its current vertices after reading live positions, normals,
lighting and material. No transformed vertex or GPU resource persists here.

Fully unclipped eligible meshes build each unique vertex once and copy the
precomputed index stream. A mesh needing near clipping uses the existing
per-triangle builder, which can mix indexed and expanded clipped geometry.
The expanded logical-stream limit is unchanged; no geometry is omitted to
obtain the gain. An oversized or invalid source plan falls back as before.

Admission remains Recompiled gameplay, TitleBasic, live unobserved direct
reads, a valid model transform, smooth shading and no ENV/exceptional SDK
colour path. Original timing and the excluded paths retain their old builder.
The source cache still compares the full descriptor, topology and UV bytes,
guest FPSCR mode and host FP mode. Its existing 16 MiB / 512-entry and 512 KiB
per-plan limits include all new allocation capacities.

`SARECOMP_MESH_SHARED_CORNERS=0` is the private same-binary control.
`SARECOMP_INDEXED_CORNERS_VERIFY=1` rebuilds the original vertices and compares
all 76 bytes. Bulk verification requires the normal cached source-plan mode;
source-plan verification deliberately exercises the original topology parser.
Neither switch is a product setting. Both benchmark helpers record these modes.

## Correctness evidence

- Windows and Linux source-plan tests: 33 cases passed, including cross-polygon
  identity, separate points with equal UVs, UV seams, source mutations, changed
  live attributes, 0/3/6 interleaved clipped vertices, rounding and cache limits.
- Existing Windows corner-index tests: 41 cases passed.
- Hidden Emerald Coast bulk oracle: 65,792,757 rebuilt logical vertices match
  byte-for-byte, plus 1,511,801 per-triangle reuse checks. It exercised 1,235,899
  bulk meshes and finished at the expected deadline without a contract failure.
- The preliminary per-triangle variant also passed 31,955,075 reused-vertex,
  22,447,813 topology-triangle and 67,148,592 UV checks. Its 0.14% native FPS
  difference was noise; the retained candidate adds bulk construction instead.

The ordinary model shaders do not use physical vertex IDs. Triangle/index
order and each triangle's provoking vertex retain their original attributes.
These checks are not a complete story playthrough or a Deck hardware test.

## Native Windows measurement

Same executable, hidden Vulkan offscreen rendering at 1280x800 / 100%, VSync
off, Recompiled timing, indexed corners enabled, isolated forward input and
private copied saves. First ten gameplay seconds are excluded. No compile or
Linux gameplay ran concurrently with the throughput pair.

| Emerald Coast | Shared off | Shared/bulk on | Change |
| --- | ---: | ---: | ---: |
| New game frames/s | 52.9236 | 54.7455 | +3.44% |
| Execution-thread CPU ms/frame | 17.9915 | 17.3293 | -3.68% |
| All-process CPU ms/frame | 21.3283 | 20.3205 | -4.73% |
| Execution cycles/frame | 79,157,046 | 75,927,905 | -4.08% |
| p95 frame interval, ms | 21.2806 | 21.1727 | |
| p99 frame interval, ms | 49.0943 | 45.7927 | |

Both runs pass with expected HostDeadline (2). Neither reports a source-plan
eviction/decline or graphics contract failure. Stored vertices per logical
corner fall from 0.556 to 0.489, about 12% less vertex storage. This is a single
pair with frame-indexed movement; do not extrapolate it to a +50% game gain or
claim monitor presentation FPS. Windows WSI on this host still requires the
explicit hidden offscreen test path documented in the earlier report.

Artifacts: `runs/mesh-bulk-{control,native,verify}-emerald/`.
Executable SHA-256:
`dcb43906cc86443c46bf51a97d7a8a24c1a5ab84eaa114e128903a49635bcdbb`.

The second-character check used Gamma Emerald Coast in reverse order (on,
then off), with the same executable and configuration:

| Gamma Emerald Coast | Shared off | Shared/bulk on | Change |
| --- | ---: | ---: | ---: |
| New game frames/s | 52.1486 | 51.7870 | -0.69% |
| Execution-thread CPU ms/frame | 17.2403 | 17.3786 | +0.80% |
| All-process CPU ms/frame | 20.9334 | 21.3876 | +2.17% |
| Execution cycles/frame | 76,113,801 | 76,384,733 | +0.36% |

Both complete normally, with no source-plan eviction/decline or contract
failure. The candidate exercises 1,760,563 bulk meshes. Gamma does **not**
establish a performance gain; these small differences are within ordinary
single-run variation. Stored/logical vertices improve from 0.596 to 0.559.
Keep the private off control for future Deck comparison rather than claiming
a universal speedup. Artifacts: `runs/mesh-bulk-{control,native}-gamma/`.

## Linux measurement

The native Linux pair uses QEMU TCG / llvmpipe and the existing 16:10 VM probe,
800x500 at 50% render scale, Recompiled timing, isolated input and private user
data. Candidate ran first, then control, without concurrent compilation or
Windows gameplay. The VM is not Deck hardware; keep older 4:3 results separate.

| Emerald Coast, after first gameplay sample | Shared off | Shared/bulk on | Change |
| --- | ---: | ---: | ---: |
| New game frames/s | 0.556338 | 0.572564 | +2.92% |
| Execution-thread CPU ms/frame | 357.855 | 357.583 | -0.08% |
| All-process CPU ms/frame | 3434.511 | 3339.363 | -2.77% |

Both complete with the expected deadline and no forced termination. At only
32/34 measured new frames, this single pair is not a precise speedup estimate.
It does not establish a Linux execution-thread reduction; that metric is flat.
Artifacts: `runs/linux-mesh-bulk-{control,native}-emerald/{summary.json,game.log}`.

The timing-only ELF is stripped into a dedicated test copy. Every allocated
ELF section was checked for equal name, type, address, size, flags, alignment
and contents against the unstripped build. No runtime section changed.
Stripped candidate SHA-256:
`67e4742eac28e5d6b6e0677488206452158237fca123a7c10aac93cb118245a1`.

The frozen r354 snapshot, accepted Windows executable and personal saves were
not modified. This change does not close the stable-60 Deck objective.
