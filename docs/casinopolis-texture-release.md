# Tails Casinopolis: released texture resolution

The user crash in session 1789238274496, PID 3840, source frame 16056 is
`0x53414704` (Basic texture contract), at `8C0376D0` / return `8C0370E2`.
Model `8C1D9AA0` selects texture 8 from TEXLIST `8C1C6BD4` (28 entries at
`8C1C6A84`). Its descriptor `8C765028` is row 269 of the current 2048-row,
68-byte SDK registry at `8C7608B4`.

The complete selected row is `FFFFFFFF FFFFFFFF 00000000 08000000`, followed
by thirteen zero words. This is a released SDK row. Retail release 64E660
preserves texture bookkeeping and clears the reference count with MOV.W;
requiring a zero-filled descriptor would be incorrect.

Both a current PVM view and a native TextureSet binding remain for the same
TEXLIST/header. The TextureSet has one host lease. The live texture resolver
correctly gives the SDK PVM view precedence and finds no live descriptor.
The sentinel resolver previously ignored that precedence, tried to resolve
the native catalog, then turned the expected missing handle into Malformed.
The Basic renderer consequently aborted instead of skipping the released draw.

`sonic_texture_sentinel.hpp` now gives a current PVM view the same precedence
for released/null/sentinel states as the ordinary resolver. A native-only
TextureSet still resolves its descriptorless catalog. Unaligned or out-of-range
rows, live resources and nonzero reference counts are not accepted as freed.
No guest texture is restored, overwritten or made live by this correction.
The release routine and save data remain unchanged.

This applies through the shared texture-state resolver to Basic models,
screen/world primitives and the other callers, without a Casinopolis address
exception. `has_current_native_pvm_view` supplies the same fresh header check
to both paths. Normal live draws do not gain a second owner scan.

## Evidence and limits

- `texture-sentinel-01.log` passes the complete selected capsule image with
  overlapping PVM/native ownership and verifies no cache resurrection or
  guest-memory writes. Four cases run the production SDK release plan before
  draw resolution, including retained TSP and upper-halfword bookkeeping.
  P1/P2 aliases, native-only catalogs and malformed row boundaries are covered.
- `build-casino-camera-01.log` recompiles the adapter/manifest and links with
  zero AOT recompiles. Native link audit passes (1,909,914,624 bytes).
- `runs/casino-texture-sentinel-01` starts Tails Casinopolis hidden/muted,
  renders the race introduction and exits at the requested 15-second scenario
  deadline, frame 408. Capture 900 was inspected. This is an entry/render smoke
  check, not replay of the user's long run to frame 16056.
- The r354 executable/AOT quick integrity audit and git whitespace check pass.

The same build contains the shared-camera interpolation work described in
render-interpolation.md. Its analytic D3D11/Vulkan reference tests pass; the
new actual-game orbit was not launched because the user had started PID 17444.
That process was left untouched. This records the earlier build only:
interpolation has since been withdrawn and compiled out. The independent
Casinopolis released-texture fix remains in the new build.
