# Exact title UV decoding

**Experiment rejected for the standard path.** Correctness passed, but the
matched gameplay measurements showed less than one percent improvement.
Production source and options were restored; only the exhaustive original
UV instruction test remains. The local experiment patch is
`.local/research/model-uv-experiment.patch`.

The retained model renderer enters a host FPU epoch for every authored UV
pair. The post-collision Gamma execution profile
`runs/touch-owner-profile-gamma-d3d11-01/execution-ip-resolved.json`
still contains samples in both epoch setup and individual 16-bit guest reads.
Those samples identify work to investigate; they are not an exclusive UV
CPU-time attribution.

## Implementation

The tested version of `src/sonic_model_uv.hpp` supplied one immutable 512-KiB lookup table for the
full signed-16-bit domain in each supported rounding mode. Its initializer
uses integer arithmetic for the exact original product
`int16 * (0x808083 * 2^-31)`, retaining literal `0x3B808083`, nearest/even
rounding and truncation toward zero. Every nonzero result is finite and
normal, so DN needs no separate table. The lookup reads the current guest
rounding mode; reserved modes use the previous decoder. Neither initialization
nor lookup changes the host FPU environment or guest registers.

Only the Recompiled TitleBasic gameplay draw path selects the table. Original
and resident SDK draws retain their previous conversion. The table selection
and one-time initialization guard occur outside the corner loop; no guest
geometry or texture data persists in this cache.

The same path can read the two authored little-endian coordinates through
one existing four-byte direct-RAM proof. The reader still validates the live
mapping and backing-store range, and reads the current bytes on each call.
Unaligned pairs use the existing memcpy reader. Observed/scalar access and a
failed four-byte proof retain the two original 16-bit reads, including order,
partial outputs and failure behavior. No read is widened into unproved memory.

Private comparison flag `SARECOMP_MODEL_UV` / benchmark `--model-uv`:

- `retained`: existing two reads and epoch conversion.
- `table`: existing two reads and exact lookup.
- `packed`: bounded paired read and exact lookup (experiment default).

These private flags were removed along with the experiment. There is no
new user-facing option.

## Correctness evidence

`sonic_sdk_color_tests` passed:

- 262,144 UV-pair cases against the original retail FLOAT/FMUL instructions:
  every signed-16-bit coordinate, both supported rounding modes and DN on/off.
- 196 host/guest FPU-state cases, including reserved-mode fallback and changed
  ambient rounding/status/denormal controls.
- 18 paired-reader cases covering unaligned/boundary reads, failures, observer
  ordering, partial outputs and immediate visibility of changed guest bytes.
- Existing 768 retail color cases, 196,608 color table cases and range checks.

Log: `.local/menu-preview/model-uv-component-test.log`.
The read-pair fixture is not the production memory mapper: generation/alias/
backing-wrap admission relies on the unchanged, independently inspected
`SonicGuestReader::offset_of` and live direct-memory guard. The source review
found no concrete correctness issue; it did not substitute for running tests.

The experimental incremental game build passed provider/FPU link audits in
74.374 seconds, with zero retained AOT recompiles.

## Matched game measurements

All five runs used EXE SHA-256
`a9b0b59c9a214cf4959c48ef976bdfe0d8292cb279cd36cb9e0f24a87867367b`.
They were hidden/muted Gamma Emerald Coast runs at 3440x1440 D3D11,
Recompiled timing, VSync off, native collision/matrix owners and indexed
corners enabled. Each walked forward for 60 seconds with the first 10 seconds
excluded. All passed, completed normally and were not forcibly stopped.
Presentation remained approximately 60; the table reports actual new draws.

| Run suffix (prefix `model-uv-`) | New draws/s | Execution CPU ms/draw | Cycles/draw |
| --- | ---: | ---: | ---: |
| retained-gamma-d3d11-01 | 49.57009 | 18.39758 | 81,370,389 |
| table-gamma-d3d11-01 | 49.93586 | 18.30232 | 80,518,956 |
| packed-gamma-d3d11-01 | 50.19156 | 18.37685 | 80,410,693 |
| packed-gamma-d3d11-02 | 50.07419 | 18.28231 | 80,358,343 |
| retained-gamma-d3d11-02 | 49.98643 | 18.41406 | 80,583,637 |

The repeat reversed the retained/packed order. Averaging those two pairs
gives approximately 0.41% less execution CPU time, 0.73% fewer cycles and
0.71% more new draws. The second pair alone improved new draws by 0.18%.
This does not justify the additional permanent lookup path/512-KiB table.
No 60-FPS or headroom claim is made.

The production UV decoder, reader call order and provider identity were
restored to commit `93e4b8d`. The strengthened original-decoder test continues
to enumerate all int16 coordinates and both rounding modes, so later attempts
can reuse the independent retail oracle.

Final restored executable SHA-256:
`c11f2c741504b15324277d6e469516994320ef23c7c79587179f2268b6152d82`.
Its provider identity is again
`39a6aff2f1adebf4fcab765b29faa9a5a39bbb8139e8c4f30befbd456ea0b86c`.
The restored build took 73.956 seconds with zero AOT recompiles and passed
both FPU link audits plus the native closure/link audit. The retained UV/color
oracle passed again (`.local/menu-preview/model-uv-restored-component-test.log`).

`runs/model-uv-restored-sonic-01` started Sonic Emerald Coast with copied
saves, hidden/muted D3D11 1280x720 and Recompiled timing. The inspected
`frames/frame-750.bmp` shows ordinary character, environment, textures and
HUD; the run ended at its own deadline, guest frame 416, with no runtime
contract failure. This is a short integration check, not an entire stage run.
All test processes were finished afterwards.
