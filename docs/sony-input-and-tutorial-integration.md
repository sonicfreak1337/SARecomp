# Sony input and tutorial navigation, 2026-09-13

Integrated in the Sonic port on top of `19f8338`. The r354 baseline and
compiled AOT pack remain unchanged. Interpolation stays disabled. All runs
were hidden and muted; no controller output, OS input injection, focus change
or personal save modification was performed.

## Direct Sony input and original vibration

The port now uses the pinned SDL 3.4.16 HIDAPI driver for official DualShock 4,
DualSense and DualSense Edge controllers. This owns **both input and output**:
Bluetooth vibration can switch Sony's report format, and SDL continues reading
the enhanced input instead of leaving WinMM stuck on the previous format.
The enhanced-report hint is `auto`, so effects trigger that change rather
than unconditional initialization.

Only Sony HIDAPI families are enabled before SDL initialization; DirectInput,
RawInput, XInput, WGI, GameInput and unrelated HID families are disabled inside
SDL. The existing native XInput backend and its logical-slot/alias policy stay
in place. SDL creates neither a window nor an audio device. The official
`054c/0ba0` DS4 wireless adapter remains admitted: unlike a minimal homemade
USB-output implementation, SDL has explicit dongle connection handling.
The shipped implementation was checked in
[SDL 3.4.16's PS4 driver](https://github.com/libsdl-org/SDL/blob/release-3.4.16/src/joystick/hidapi/SDL_hidapi_ps4.c).

When SDL initializes successfully, it is authoritative for Sony input even
with no connected Sony controller. There is no simultaneous WinMM candidate
for the same device, and an enhanced Bluetooth controller does not depend on
continued DirectInput enumeration. Complete, case-normalized HID paths define
identities; VID/PID alone never merges two physical controllers. Endpoint
tokens are monotonic, so a late vibration callback cannot reach a replacement
controller after disconnect/reconnect.

Analog L2/R2 and both sticks enter the same normalized input contract as Xbox,
before the existing remapping/deadzone layer. Missing/incompatible SDL retains
the legacy Sony input path. Its documented digital L2/R2 button bits work as
binary controls; Z is no longer reused as a combined trigger.

Original guest PuruPuru requests continue through the established native
deadline engine. User strength is applied once. Its callback chooses exactly
one correlated XInput route or Sony endpoint. The worker is joined before
closing Sony objects or unloading either library. The Sony mutex serializes
output against discovery, disconnect and teardown. SDL receives a secondary
65000-ms timeout; the engine owns the real, usually much shorter, deadline.
The frozen SDK's separate, unused direct `set_gamepad_vibration` API remains
XInput-only; actual Sonic vibration uses the four bound PuruPuru leaves.

The DLL is loaded from the executable directory with restricted dependency
search and an exact version check. CMake checks its pinned SHA-256. Headers,
runtime and zlib license are included under `third_party/sdl3`; the package
also carries the DLL and license. No source was rebuilt inside Katana.

## Original tutorial navigation

SUMMARY's common bottom bar now displays the current **gameplay A and B**
bindings, with X as the Back fallback if B is unbound. These are distinct from
the host menu's Confirm/Cancel bindings. The original accepts A for Next and
either B or X for Back, although its baked artwork advertises X.

This is a host texture substitution at the proved textured-stream leaf,
not a guest texture rewrite. Matching requires the exact live SUMMARY module,
callback/vertex source, active TEXLIST, ordinal-zero publication, selected
first registry row, registered handle/generation, complete archive identity
and complete PVRT identity. The original immediate-state transaction, colors,
depth, sampler, geometry, queue order and guest texture publication remain.
Unknown or mismatching state keeps the original bar.

The cached native bar follows text language and current glyph style, with
five languages and Xbox/PlayStation/keyboard labels. Rasterization is 4x and
keeps text inside the 352/375 visible source pixels of the original 512-wide
quad. Long bindings can use two rows. Rebinding, language/style changes and
module generations invalidate the image. Replacement/teardown uses the
existing deferred texture retirement owner.

This commit closed the **common navigation bar**. The later page integration
also handles the baked control diagrams and instruction glyphs in all 30
TUTOMSG archives; see tutorial-page-integration.md. Dynamic in-world hints
have a separate data path: the inspected homing-attack hint already names the
action rather than a physical button, so it remains original. No global A/B,
GBIX or `padmanu` replacement was introduced. Subtitles remain unchanged.

## Evidence and limits

- `sony-input-02.log`: synthetic SDL transport passed independent analog
  triggers, neutral sticks, buttons, two identical-model controllers, report
  transition, disconnect/reconnect, stale endpoint rejection, failed-stop
  retry and teardown, including official DS4-dongle admission. Xbox devices
  are rejected by the Sony backend.
- `sony-poll-01.log`: actual native platform test passed; host polling still
  preserves replay cursor, recording count and poll sequence isolation.
- `sdl-runtime-01.log`: packaged DLL loaded and returned version `3004016`;
  no SDL gamepad subsystem was initialized by this probe.
- `tutorial-prompt-01.log`: 5 languages x 3 styles passed, including remapped
  gameplay A/B rather than host Confirm/Cancel, crop bounds and long labels.
  PNGs under `.local/analysis/tutorial/native-prompts-01` were inspected.
- `options-sony-tutorial-01.log`: full existing Options component suite passed.
- `runs/options-sony-tutorial-01`: actual 50-second hidden Vulkan Options ->
  Sound Test run passed. Guest work remained frozen inside the native menu;
  the old Options artwork stayed suppressed on entry and exit.
- `verify-baseline.py --quick`: PASS; both protected baseline executable/AOT
  hashes still match. No full matrix or physical controller test was run.

The later original tutorial visits confirm the common bar's live owner match
and remapped PlayStation/Japanese keyboard labels. See tutorial-native-visit.md
for captures, loader proof and remaining limits. The earlier Options/Sound Test
run itself does not claim tutorial coverage.
Bluetooth and USB packet delivery on the user's hardware are not claimed from
the fake transport tests. The existing physical Sony controls remain available
for the next ordinary playtest; there is no pending permission request.

Builds use retained Ninja with `-j2`. `build-sony-tutorial-02.log` passed the
native link audit after explicitly admitting the new source-bound Sony
library; `build-sony-tutorial-03.log` adds preserved official dongle admission.
No AOT translation unit was compiled. Adapter source digest:
`520bf060ba82d23267ab62745b77b041e1e8e3a62ff72c3f30e2c5d7fd537873`.

This batch is input/UI work. It establishes no new gameplay FPS gain or
60-Hz simulation claim. The measured guest CPU bottleneck and timing audit
remain the next optimization work after the outstanding original hint owners.
