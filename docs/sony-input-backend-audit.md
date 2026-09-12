# Direct Sony input/output backend audit

Follow-through: the port now uses the complete Sony SDL3 backend described in
[the integration report](sony-input-and-tutorial-integration.md). The minimal
USB-only boundary below records the initial audit, not the shipped limitation.

2026-09-12; read-only source audit for Eggman. Repository branch
`enhancements/ingame-settings`, requested base `19f8338`. Only this document
was written. No controller handles opened, state queried, reports sent,
builds/game tests run, inputs injected, or frozen SDK archives changed.

## Implementable boundary

Direct **USB** DualSense and DualShock 4 rumble can be added to the existing
native rumble transport. **Bluetooth rumble cannot safely be bolted onto the
current WinMM input owner as an output-only feature.** Sony Bluetooth effect
traffic changes the controller into enhanced report mode; preserving input
then requires an enhanced HID input owner too. Keep direct Bluetooth rumble
unavailable until that larger boundary exists; the existing correlated
XInput transport remains usable. This is a concrete protocol/input ownership
limitation, not a permission request.

Digital L2/R2 are implementable using the existing identity-bound Sony button
layout. They supply binary trigger actions; they do not establish analog U/V
mapping or analog pressure support.

## Exact local integration points

The mutable owner is `tools/prepare-camera-platform.py`; generated line
numbers below describe the inspected `build-performance/generated/camera-platform/native_port_platform.cpp`.

* `NativeJoystickIdentity` around 744 currently holds WinMM index, hash of
  DirectInput instance GUID, and Sony kind. Extend it with exact device path,
  VID/PID, and validated transport metadata. Do not replace the existing
  identity with a VID/PID hash, joystick index, name, or first matching HID.
* `enumerate_direct_input_gamepad` around 795 already creates the exact
  DirectInput object and reads `DIPROP_VIDPID` and `DIPROP_JOYSTICKID`.
  Before `device->Release()`, read `DIPROP_GUIDANDPATH` into
  `DIPROPGUIDANDPATH`: `dwSize=sizeof(value)`, `dwHeaderSize=sizeof(DIPROPHEADER)`,
  `dwObj=0`, `dwHow=DIPH_DEVICE`. Microsoft explicitly documents opening
  this returned interface path with CreateFile for direct HID operations.
  Failure should remove output capability, not the controller's input.
* `initialize_physical_input`, `consume_joystick_identity_refresh` (~3299)
  and the final physical candidate placement (~2750..2835) are the catalog,
  refresh and slot-binding seams. The discovery worker may publish immutable
  metadata; the owner must synchronize transport handle replacement with the
  rumble worker. Do not put raw owning handles into the existing swapped
  identity vector: destruction currently happens on a discovery worker.
* The current engine attachment owns `&xinput_`; replace this with a lifetime
  stable transport router that owns XInput and Sony endpoints. Reserve
  disjoint endpoint numbers (e.g. XInput 0..3, Sony >=4). Never reinterpret
  an arbitrary unsigned endpoint as a WinMM slot or HID handle.
* Before `engine().bind`, resolve exactly one output route for the selected
  logical identity. Prefer its already established XInput alias if retaining
  current behavior; otherwise use its validated USB HID endpoint. Never
  output through both routes for one physical controller.
* Current constructor attachment requires `xinput_.set_state`; remove that
  dependency for a Sony-capable router. Preserve both existing hard guards:
  replay mode and `KATANA_PORT_BACKGROUND_TEST` suppress physical output.
  `Engine::policy` already applies the user gain. The Send callback receives
  scaled 16-bit values and must only quantize, not apply that gain again.
* Public `set_gamepad_vibration` (~2840) is separately XInput-only. If it is
  retained as a public capability, route it consistently or document its
  separate status. Do not call it from the deadline callback: it enforces
  owner-thread access, and it independently applies gain.

The identity/path API contract is primary Microsoft documentation:
[DIPROPGUIDANDPATH](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416637(v=vs.85)).

## Admission and Windows transport

Admit Sony VID `054C`, DualSense PID `0CE6` (Edge `0DF2` has the same main
output family), and DualShock 4 PIDs `05C4`/`09CC`. Existing PID `0BA0` is the
Sony wireless adapter, not an ordinary wired DS4; keep it outside the minimal
USB allowlist until its controller-connected/lifetime behavior is handled.

Open only the path obtained from the selected DirectInput instance, using
shared read/write access, `OPEN_EXISTING`, and `FILE_FLAG_OVERLAPPED`.
Validate `HidD_GetAttributes` against that identity; validate a Generic
Desktop Game Pad/Joystick collection with `HidD_GetPreparsedData` /
`HidP_GetCaps`. Free preparsed data on every path. A failed/shared-access
denied/unrecognized device must remain input-only, not abort startup.

Identify the actual bus from device-interface/PnP metadata. HIDAPI resolves
`DEVPKEY_Device_InstanceId`, locates its devnode, gets its parent and reads
`DEVPKEY_Device_CompatibleIds` to distinguish USB/BTHENUM/BTHLEDEVICE. Unknown
bus means unsupported output. A product called Wireless Controller is often
USB; a HID path containing VID/PID does not by itself establish USB.

Construct a zeroed report buffer of the validated `OutputReportByteLength`,
put the logical report at its beginning and leave padding zero. Windows HID
WriteFile expects the collection's maximum output length, including report
ID. Require enough room for the intended format, cap allocation, and reject
unexpected transport/report contracts. SDL uses 48 logical bytes for PS5 USB;
current Linux uses a larger 63-byte USB structure. This is why hardcoding
the Windows WriteFile count from a Linux struct is wrong. DS4 USB's logical
report is 32 bytes. HIDAPI's Windows transport explicitly pads short reports.

Sources: pinned
[HIDAPI Windows implementation](https://github.com/libsdl-org/SDL/blob/26b37f5d1c3518125ff8b269b5bdfcf452870500/src/hidapi/windows/hid.c)
(`hid_internal_detect_bus_type`, `hid_open_path`, `hid_write`) and
[Linux Sony driver](https://github.com/torvalds/linux/blob/cba2348ab114391f5b1a00fa65c5b739f13f0563/drivers/hid/hid-playstation.c).

## Minimal USB report bytes

Offsets include report ID. All bytes not specified are zero; do not enable
LED, audio-volume, microphone, adaptive-trigger, or player-light fields.

| Device / mode | Logical length | Fields |
| --- | --- | --- |
| DS4 rumble or stop | 32 | `[0]=05`, `[1]=01` (rumble-valid only), `[4]=high>>8`, `[5]=low>>8` |
| DS5 legacy active | 48 | `[0]=02`, `[1]=03` (compatible vibration + select haptics), `[3]=high>>9`, `[4]=low>>9` |
| DS5 legacy stop | 48 | `[0]=02`, remaining bytes zero; clears emulation and returns audio haptics |
| DS5 improved active, optional | 48 | `[0]=02`, `[1]=02`, `[3]=high>>8`, `[4]=low>>8`, `[39]=04` |

DS5 effect payload begins at 1, DS4 effect payload at 4. Low frequency maps
to the left motor, high frequency to the right. SDL deliberately halves
legacy DS5 amplitude to approximate Xbox strength. Both USB formats have
no CRC or sequence field.

The minimal implementation may use the legacy DS5 contract without firmware
feature queries. Improved mode is optional: SDL enables it for Edge or
firmware >= `0x0224`; the USB firmware report is feature ID `0x20`, firmware
little-endian bytes 44/45 of the report. Do not treat a failed query as
verified new firmware. SDL's unknown-firmware Bluetooth assumption is not
required for this USB implementation.

SDL emits a rumble-start effects packet selecting haptics (`[1]=02`) before
the first nonzero rumble packet. Preserve that start transition if matching
SDL's behavior. With no feature ownership of LEDs/audio volume/triggers,
only the haptics selection bits need to change. Stop must reach the same
handle that received the preceding active packet.

Primary source:
[SDL PS5 effects](https://github.com/libsdl-org/SDL/blob/26b37f5d1c3518125ff8b269b5bdfcf452870500/src/joystick/hidapi/SDL_hidapi_ps5.c)
(`DS5EffectsState_t`, `UpdateEffects`, `RumbleJoystick`, `InternalSendJoystickEffect`)
and [SDL PS4 effects](https://github.com/libsdl-org/SDL/blob/26b37f5d1c3518125ff8b269b5bdfcf452870500/src/joystick/hidapi/SDL_hidapi_ps4.c).

## Bluetooth packet contract and the input blocker

For a future complete HID backend, the exact output formats are:

* DS5: 78 bytes; `[0]=31`, `[1]=(sequence&15)<<4`, `[2]=10`; effects start
  at offset 3, so compatible flags/motors/improved flag are at 3/5/6/41.
  Advance sequence modulo 16 per report, following Linux. Current SDL uses
  zero sequence but the corrected same three-byte header. Older copied
  examples using `[1]=02` and payload at 2 are obsolete for this purpose.
* DS4: 78 bytes; `[0]=11`, `[1]=C0|interval`, `[3]=01`; payload at 6,
  high/low motors at 6/7. SDL's default interval is 4 (`C4`). Rumble-only
  mask `01` avoids changing LED state. No DS5-style sequence field.
* Both append IEEE CRC32 in little endian at bytes 74..77. Input to CRC is
  prefix byte `A2` followed by report bytes 0..73. Equivalent bitwise code:
  initial `FFFFFFFF`, reflected polynomial `EDB88320`, final complement.
  CRC covers the transport header and zero padding too. No CRC over the
  Windows descriptor padding beyond the 78-byte logical Bluetooth report.

SDL records DS5 Bluetooth basic reports as DirectInput-enabled and treats
enhanced mode as irreversible within the connection. Even an invalid effects
packet can trigger the transition. Feature reads of serial/firmware can also
enable enhanced reports. DS4 has the corresponding enhanced mode transition.
SDL can do this because it also owns input report decoding; our WinMM backend
does not. A zero-strength test write is therefore not a passive capability
probe. The safe current boundary is USB-only plus existing XInput aliases;
full Bluetooth requires implementing/validating HID report input, hotplug and
logical-slot continuity together. No physical transition was attempted here.

See the pinned SDL PS4/PS5 sources above (`SetEnhancedMode`,
`UpdateEnhancedModeOnApplicationUsage`, `TickleBluetooth`) and Linux
`dualsense_init_output_report` / `dualsense_send_output_report` for CRC/sequence.

## Cancellation, endpoint replacement and lifetime

`Engine::Send` may run on its timer worker or the owner making a request,
under the Engine mutex. Transport mutation therefore needs its own clear
synchronization; no callback may query guest state or recursively call Engine.
Avoid lock inversion: never hold a transport mutex while calling bind/detach,
because those can synchronously call Send. Do not hold the identity publication
mutex over output I/O.

Maintain each handle, report buffer, event and OVERLAPPED object in a stable
endpoint object. Serialize writes per endpoint. Use a bounded wait for pending
WriteFile and check completed byte count. On failure/timeout, request
`CancelIoEx(handle,&overlapped)`, then observe completion before reusing/freeing
the buffer, OVERLAPPED or event. Cancellation alone neither stops motors nor
proves the packet failed: it can race with successful completion. Quarantine
pending endpoint storage if completion cannot be drained promptly; never
return with a still-pending stack buffer.

On physical removal, logical unbind, changed path, or handle-generation change:
stop/unbind through the old route before retiring it; invalidate queued old
writes; drain/cancel old I/O; only then expose the replacement route. If the
same GUID/path reconnects, force a new endpoint generation and Engine reset,
otherwise an old deadline could target the new connection. Keep an unchanged
healthy endpoint across ordinary catalog refreshes.

Shutdown ordering: stop deadlines and join/detach Engine while router and
handles still exist; drain remaining output I/O; close handles/events; unload
libraries last. Output failure removes capability until a deliberate refresh
or reconnect, rather than retrying writes every frame. Best-effort stop failure
must not be described as proven motor shutdown.

[Microsoft CancelIoEx contract](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex)
explicitly requires completed I/O before OVERLAPPED reuse and documents the
cancel-versus-success race. These constraints are stronger than merely adding
a timeout around synchronous WriteFile.

## Digital L2/R2 without axis guessing

The current `add_joystick_buttons` (~947) skips zero-based buttons 6 and 7.
It already maps Sony buttons 0..5 and 8..11 by fixed ordinals. The original
DualSense descriptor/report documentation identifies HID Button usages 7/8 as
L2/R2, in both USB and basic Bluetooth reports. Linux's Sony driver separately
decodes the same L2/R2 digital bits for both DS5 and DS4, in addition to their
analog values. Microsoft documents `dwButtons` as JOY_BUTTON1..32 flags.

Therefore extend the **existing admitted Sony layout**, gated by at least
eight advertised buttons: `dwButtons & (1u<<6)` gives binary left trigger,
`dwButtons & (1u<<7)` right trigger. Populate `left_trigger_raw` /
`right_trigger_raw` with 255 or 0 and let existing normalized fields and
`src/sonic_input.cpp` synthetic LT/RT binding logic handle actions. Set these
after the old combined-Z block, or replace that block for these identities;
otherwise stale axis-derived values can overwrite the digital contract.
For DS4 as well as DS5, Sony Z is a right-stick coordinate in the documented
layout and must not synthesize triggers. Preserve XInput analog decoding.

This is a source-supported semantic mapping for the stock Sony layout, not a
physical test of this user's Windows driver or of third-party remappers.
It closes binary trigger controls and binding prompts, including simultaneous
L2+R2. It must not be labeled analog trigger completion. No U/V values or
neutral ranges are inferred. A hidden, hardware-free fixture should exercise
none/L2/R2/both, right-stick movement with neither button, and non-Sony exclusion.

Primary sources:
[original DualSense report documentation](https://github.com/nondebug/dualsense/blob/b87450eb1c4ba8d74948f13b43d4c1eaa8ffa4f5/README.md),
[Linux Sony input decoder](https://github.com/torvalds/linux/blob/cba2348ab114391f5b1a00fa65c5b739f13f0563/drivers/hid/hid-playstation.c),
[Microsoft JOYINFOEX](https://learn.microsoft.com/en-us/windows/win32/api/joystickapi/ns-joystickapi-joyinfoex).

## Bounded verification for integration

Use pure report fixtures for USB offsets, lengths/padding, start/stop and gain;
fake transports for identity generation replacement, failed writes, detach
ordering and cancellation races. No game/hardware tests were performed in
this audit. Any later game tests remain hidden/muted under
`KATANA_PORT_BACKGROUND_TEST=1`, which must continue to suppress motor output.

## Follow-up: SDL3 as the complete Sony backend (2026-09-13)

**Recommendation: use SDL3 HIDAPI for Sony input and rumble together.** This
closes the Bluetooth report-mode boundary without maintaining a second custom
HID input/output implementation. Keep native XInput and keyboard ownership;
replace only admitted Sony input candidates and their rumble endpoint.
This follow-up is also source-only: no code, dependency installation, build,
controller access or game test. The existing direct-HID packet notes above
remain reference material, not a requirement to implement those packets.

The inspected SDL source is the same pinned commit
`26b37f5d1c3518125ff8b269b5bdfcf452870500`. Pin a tested SDL3 dependency including
the corrected PS5 Bluetooth header, rather than accepting an arbitrary DLL
called SDL3.dll. The public API names used here exist in SDL3, not SDL2.

### Filter before initializing SDL

Use checked `SDL_SetHintWithPriority(...,SDL_HINT_OVERRIDE)` calls before the
first joystick/gamepad initialization, then keep these process-level hints
stable. The concrete configuration is:

| Hint | Value / purpose |
| --- | --- |
| `SDL_HINT_JOYSTICK_HIDAPI` | `0`: default all HIDAPI families off |
| `SDL_HINT_JOYSTICK_HIDAPI_PS4` | `1` |
| `SDL_HINT_JOYSTICK_HIDAPI_PS5` | `1` |
| `SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT` | `0x054c/0x05c4,0x054c/0x09cc,0x054c/0x0ce6,0x054c/0x0df2` |
| `SDL_HINT_JOYSTICK_DIRECTINPUT` | `0` |
| `SDL_HINT_JOYSTICK_RAWINPUT` | `0` |
| `SDL_HINT_XINPUT_ENABLED` | `0` |
| `SDL_HINT_JOYSTICK_WGI` | `0` |
| `SDL_HINT_JOYSTICK_GAMEINPUT` | `0` |
| `SDL_HINT_JOYSTICK_GAMEINPUT_RAW` | `0` |
| `SDL_HINT_HIDAPI_LIBUSB` | `0`: use native Windows HID paths |
| `SDL_HINT_JOYSTICK_ENHANCED_REPORTS` | `auto`: switch Bluetooth on actual effects use |

Explicitly set `SDL_HINT_JOYSTICK_HIDAPI_XBOX`, `_XBOX_360`,
`_XBOX_360_WIRELESS`, `_XBOX_ONE`, and `_GIP` to `0` too if the dependency
includes those drivers; their family overrides must not be inherited from
environment settings. The PID allowlist also excludes third-party controllers
that happen to advertise a PS4/PS5-compatible layout. Add `054c/0ba0` only when
including the separately handled official DS4 dongle deliberately.

The HIDAPI master hint is a default, not an unconditional global kill switch:
PS4/PS5 `IsEnabled()` read their own hints first. In `HIDAPI_GetDeviceDriver`,
`SDL_ShouldIgnoreJoystick` runs before family selection and opening the
read/write HID driver handle. That calls `SDL_ShouldIgnoreGamepad`, which
enforces the allowlist. Thus filtering only after `SDL_GetGamepads()` would
be too late: HIDAPI may initialize/open devices during discovery already.
The lower HID enumeration can still inspect metadata of other devices; this
configuration restricts SDL controller ownership, not all enumeration reads.

The hint exception for Steam virtual devices and a Wine-specific bypass are
visible in `SDL_ShouldIgnoreGamepad`. Windows is the current target; also
apply an explicit Sony VID/PID check to returned gamepad IDs before opening,
and do not enable SDL's Steam-virtual-controller environment override.
If another in-process SDL user already initialized joysticks, these filters
are shared global state; establish one initialization owner instead of
reconfiguring a live subsystem behind that user.

Evidence: [HIDAPI default and overrides](https://wiki.libsdl.org/SDL3/SDL_HINT_JOYSTICK_HIDAPI),
[allowlist syntax](https://wiki.libsdl.org/SDL3/SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT),
[pinned hints](https://github.com/libsdl-org/SDL/blob/26b37f5d1c3518125ff8b269b5bdfcf452870500/include/SDL3/SDL_hints.h),
[driver admission/open order](https://github.com/libsdl-org/SDL/blob/26b37f5d1c3518125ff8b269b5bdfcf452870500/src/joystick/hidapi/SDL_hidapijoystick.c),
[gamepad filtering](https://github.com/libsdl-org/SDL/blob/26b37f5d1c3518125ff8b269b5bdfcf452870500/src/joystick/SDL_gamepad.c).

### No SDL window, explicit owner updates

Initialize only `SDL_INIT_GAMEPAD` on the process main thread. It initializes
joysticks and events, not video/audio. No SDL window, SDL renderer or SDL audio
device is required; the native host window/message pump stays intact. Preserve
the existing executable entry point (`SDL_MAIN_HANDLED` / `SDL_SetMainReady`
as appropriate to the chosen headers), rather than adopting SDL's main wrapper.
The current native owner thread must actually be the main thread at init/quit;
an arbitrary DirectInput discovery worker is not an acceptable init owner.

For polling integration, disable SDL gamepad and joystick events and call
`SDL_UpdateGamepads()` explicitly before taking each physical snapshot. Enumerate
IDs with `SDL_GetGamepads` and free its allocated array with `SDL_free`; open
new admitted IDs once, check `SDL_GamepadConnected`, close retired objects.
UpdateGamepads calls UpdateJoysticks, which also performs detection and rumble
maintenance. It is documented thread-safe, but keeping all catalog/input work
on the host owner keeps snapshots and slot placement deterministic.

Continue updates in the existing host/menu/pause poll path; guest replay must
not accidentally substitute physical data into its recorded snapshot. SDL's
Windows `JOYSTICK_THREAD` concerns detection/raw input messages; it is not a
replacement for `SDL_UpdateGamepads` or an independent rumble-expiry guarantee.
Polling mode should suppress both joystick and gamepad event queues to avoid
accumulating unused input events. Preserve native focus/pause policy: in the
source, SDL suppresses unfocused input only when it owns windows, so a no-SDL-
window integration cannot delegate the game's focus policy to SDL.

Evidence: [Init and subsystem dependencies](https://wiki.libsdl.org/SDL3/SDL_Init),
[manual gamepad updates](https://wiki.libsdl.org/SDL3/SDL_UpdateGamepads),
[polling without gamepad events](https://wiki.libsdl.org/SDL3/SDL_SetGamepadEventsEnabled),
[main-entry integration](https://wiki.libsdl.org/SDL3/SDL_SetMainReady), and
`SDL_PrivateJoystickShouldIgnoreEvent` in the pinned joystick source below.

### Exact path correlation and persistence after enhanced mode

There is an exact Windows path bridge in this configuration:
HIDAPI converts the device interface's UTF-16 path to UTF-8; SDL's HIDAPI
`GetDevicePath` returns it; opening a joystick copies it with `SDL_strdup`;
`SDL_GetGamepadPath` forwards to the opened joystick's stored path. SDL's own
DirectInput backend uses `DIPROP_GUIDANDPATH`, converts its path to UTF-8 and
uppercases it for matching. Therefore convert the native DirectInput path and
compare the **complete interface path case-insensitively**, retaining collection
suffixes and interface GUID. Do not match only VID/PID, partial serial text,
display names, controller order or bus guesses. Check VID/PID too and require
a unique match. Copy the SDL path while the gamepad object is owned; a raw path
pointer must not outlive close. The unopened `GetGamepadPathForID` ultimately
uses a driver pointer with an explicit lifetime FIXME, so copy under suitable
SDL joystick locking or use the owned opened-gamepad path.

Preserve the existing DirectInput GUID-derived logical ID when that full path
matches, but SDL ownership cannot continue to depend on WinMM reporting the
device. After SDL switches Bluetooth to enhanced mode, DirectInput may no
longer expose usable input. Keep the SDL candidate, its assigned logical slot,
path, and SDL instance generation until SDL reports disconnection. A device
already in enhanced mode when the game starts may have no DirectInput match;
admit it independently through its validated SDL Sony identity and full path
in a disjoint stable identity domain, or it will remain unsupported.

USB/Bluetooth path changes and reconnects produce a new transport generation.
Do not equate the SDL instance ID with a persistent cross-launch identity.
Stop/unbind the old generation and neutralize the slot before replacement.
For each admitted SDL Sony path, skip the corresponding WinMM candidate to
avoid double button transitions; keep existing Sony-to-XInput alias deduplication
for remappers. Do not require analog trigger correlation to use the direct SDL
Sony endpoint, and do not make both SDL and XInput rumble the same controller.

Evidence: [GetGamepadPath API](https://wiki.libsdl.org/SDL3/SDL_GetGamepadPath),
the pinned HIDAPI/Windows and gamepad sources above, and
[SDL's DirectInput path normalization](https://github.com/libsdl-org/SDL/blob/26b37f5d1c3518125ff8b269b5bdfcf452870500/src/joystick/windows/SDL_dinputjoystick.c).
The exact bridge is source-supported for Windows native HIDAPI, not a portable
guarantee for arbitrary SDL backends; the public path API is implementation-dependent.

### Input and Bluetooth report transition

Use SDL's standardized SOUTH/EAST/WEST/NORTH, shoulders, sticks, D-pad,
BACK/START buttons, LEFTX/LEFTY/RIGHTX/RIGHTY and LEFT_TRIGGER/RIGHT_TRIGGER
axes. Trigger axes from `SDL_GetGamepadAxis` range 0..32767; normalize/clamp
and convert to native 0..255 raw trigger fields. Preserve host Y-axis inversion
conventions; skip the WinMM Z/R corrective branch for SDL samples. This gives
real analog Sony trigger input without guessing WinMM U/V or double-applying
the camera correction.

The PS5 HIDAPI driver decodes both simple packets and CRC-validated Bluetooth
report `31`; PS4 likewise handles simple and enhanced reports. With enhanced
reports `auto`, basic Bluetooth input works first, and a rumble call enables
enhanced mode while SDL continues decoding the new input format. Establish
SDL as input owner before exposing rumble capability. USB is handled by the
same API. Query `SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN` from gamepad properties;
an open gamepad or accepted VID/PID alone is not a rumble-capability result.

SDL explicitly documents that enhanced Bluetooth reports break DirectInput
for other applications and persist until the controller is power-cycled.
SDL protects this game's input through its own decoder, but does not remove
that device-level consequence. Also, SDL's PS4/PS5 enhanced initialization
can update controller LED/player-light state; this is broader than the custom
rumble-only reports above. `SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED=0` disables
player lights but is not a general promise to leave all LEDs untouched.

For hidden/muted tests or replay-only runs, skip physical SDL initialization
when physical input is not needed, and never call rumble or enable enhanced
features. Merely guarding Send is insufficient to promise no initialization
feature traffic, particularly for USB devices.

Evidence: [enhanced-mode contract](https://wiki.libsdl.org/SDL3/SDL_HINT_JOYSTICK_ENHANCED_REPORTS),
pinned PS4/PS5 decoder sources above, and
[SDL3 gamepad axes/properties](https://github.com/libsdl-org/SDL/blob/26b37f5d1c3518125ff8b269b5bdfcf452870500/include/SDL3/SDL_gamepad.h).

### Rumble thread safety and lifetime with the existing Engine

`SDL_RumbleGamepad` is documented safe from any thread. The current Engine Send
callback may therefore call it directly through a stable router-owned gamepad
object; quantization, USB/BT packet packing and SDL's HID rumble queue stay
inside SDL. Pass the already gain-scaled 16-bit low/high values without an
additional user gain. Return SDL's bool result to the engine.

SDL's duration is software-managed: callers must process events or call
UpdateJoysticks/UpdateGamepads for expiry and resend. The pinned source caps
duration at 65535 ms and resends every 2000 ms during updates. With the existing
Engine's explicit independent deadline and Send signature lacking duration,
a nonzero send may use 65000 ms as an SDL fallback ceiling (the original
AST maximum is 64000 ms); the Engine must still send explicit zero at its
actual deadline. Do not pass duration zero as a portable infinite-rumble
contract: the source clears expiry/resend bookkeeping for that value.
Changing amplitude replaces the SDL request; it must not replace the Engine's
original deadline. Continue owner updates for long effects so device watchdogs
receive resends. Zero intensity explicitly stops and does not require waiting
for the next input frame to invoke the API.

API thread safety does not make a freed SDL_Gamepad pointer valid. Keep
ownership and closing serialized with the router; remove/unbind endpoints
under the Engine's synchronization while the old object still exists. Its
timer callback must finish before closing that object. Never hold the router
mutex while calling Engine bind/detach, which can call Send and reacquire it.
Never acquire Engine locks from SDL event callbacks while holding SDL locks.
SDL disconnect keeps an opened object as disconnected until close; check
`SDL_GamepadConnected`, drop capability and reset generation on removal.

Clean shutdown: block/stop Engine, detach and join its worker, close all owned
gamepads, then balance `SDL_QuitSubSystem(SDL_INIT_GAMEPAD)` on the main thread,
and only afterward unload a dynamically loaded SDL library. Avoid global
`SDL_Quit` when another subsystem owner may exist. SDL handles internal HID
queue/handle cleanup; the host still owns ordering and cannot claim a physical
stop succeeded after disconnect merely because local teardown completed.

Evidence: [RumbleGamepad thread/duration contract](https://wiki.libsdl.org/SDL3/SDL_RumbleGamepad),
[joystick locks, lifetime and maintenance](https://github.com/libsdl-org/SDL/blob/26b37f5d1c3518125ff8b269b5bdfcf452870500/src/joystick/SDL_joystick.c),
[resend/duration constants](https://github.com/libsdl-org/SDL/blob/26b37f5d1c3518125ff8b269b5bdfcf452870500/src/joystick/SDL_sysjoystick.h).

The implementable scope is now closed: filtered SDL Sony discovery/input,
path-correlated identity with independent SDL continuity, standardized analog
controls, and lifetime-safe routing into the existing deadline engine. Physical
USB/Bluetooth reconnect and report-transition behavior is still untested here;
it should not be claimed validated from this source audit alone.
