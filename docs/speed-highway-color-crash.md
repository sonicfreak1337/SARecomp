# Speed Highway 2 SDK color abort, 2026-09-11

The user run ending at 21:45:20 (PID 44516) provided a complete capsule:
`katana-crash-session-1789154910149-44516.log`. At frame 25,212 the native
SDK model hook `8C6214FA`, called from `8C621446`, aborted with `53414701`.
The provider transcript identifies source line 33642 in the pre-fix adapter:
the finite-only read of constant ARGB words at `8C88F5A8..B4`.

The model is `8C1B7ED8`, referencing material `8C1B7A84`. Its authored flags
are `2671A400`, selecting textured IgnoreLight in SDK620 before any runtime
ConstantAttr override. The old capsule does not retain the four color words
or the runtime override, so it cannot establish their exact bit patterns.
The failing finite-only check was already present in the frozen r354 source;
the capsule does not prove that widescreen culling produced the color state.

## Original behavior and correction

Retail `620A72..620B02` and `6385F0..63867E` consume all four ARGB components.
Control `0x10` replaces them; otherwise `0x20` adds them. FMOV preserves raw
bits, and FADD follows the SH-4 FPSCR. Neither owner requires finite inputs.
The experimental adapter now uses checked word reads and SH-4 addition.
Geometry, pointers, texture ranges and the separate legacy/title owners keep
their existing validation.

Bypassing the early check alone would send a nonfinite material to the host
renderer. For exceptional SDK620 textured colors, the port now converts the
constant header color before combining it with the existing packed intensity.
The source path `620B6C/76 -> 6064F0/6063F0` copies raw header ARGB; the
IgnoreLight writer `61CEDC` supplies intensity one. The conversion matches
Flycast's upper-16-bit float table and byte multiplication divided by 256.
Packed intensity bytes are consumed directly, without quantizing them twice.

The existing SDK620 lit float and SDK638 unlit conversion paths remain in
place. Other exceptional vertex colors are converted after their existing
native lighting calculation, before host interpolation. Native material RGB
is neutral on these paths so color is not applied twice. This closes the
nonfinite transport gap; it is not a new implementation or verification of
the entire SDK638 lighting pipeline. Established finite rendering is retained,
apart from using the original FPSCR for additive material color arithmetic.

The checked local Flycast source is
`reference/flycast-oracle-v27-1-src/core/hw/pvr/ta_vtx.cpp` in the former Katana
workspace, read-only: `float_to_satu8`, `float_to_satu8_math`, and
`vert_face_base_color`. The test contains the table's mathematical oracle;
the port reuses its existing conversion, now factored into `sonic_sdk_color.hpp`.

## Verification and limits

- `sonic_sdk_color_tests`: 768 comparisons against both retail SH-4 color
  sequences, including replacement precedence, both rounding modes, DN modes,
  infinities, either-sign NaNs and the reported FPSCR `4106D`.
- All 65,536 Flycast table indices, each with three low-word patterns:
  196,608 comparisons. Intermediate packed intensity bytes and missing-memory
  rejection are covered.
- Thirty-two DN=0 arithmetic cases that raise the original SH-4 denormal
  exception are excluded from the color-output comparisons; raw FMOV and
  DN=1 handling of those inputs are covered.
- Final incremental build: 47.5 seconds, zero AOT recompiles, native link
  audit passed. The r354 snapshot and personal saves remain unchanged.
- A hidden, muted 15-second Speed Highway entry probe at 1720x720 reached
  its planned deadline (`reason=2`, frame 555), with no runtime/graphics
  abort. The inspected frame 1500 shows normal first-act gameplay and HUD.
  Evidence: `runs/speed-highway-color-fix`. This is a short boot/render check,
  not a reproduction of the later second-act crash.

These focused checks establish the identified read/transport correction.
They do not replay the user's entire 17-minute run or establish a cause for
the separately reported station-hall monitor crash.
