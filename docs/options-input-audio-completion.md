# Options input and audio completion

This port-only continuation preserves the frozen r354 SDK, AOT pack and VMU
namespace. Interpolation, HUD customization and replay development remain
withdrawn. Accepted subtitles are unchanged.

## Input corrections

Physical analog triggers can be assigned to either logical trigger without
losing analog strength. Digital alternatives produce full strength; stale
disconnected packets cannot activate a trigger. The actual input transform is
tested, including serialization, reconnect and Sony/Xbox identity.

Escape can be bound after a localized assign/cancel confirmation, defaulting
to cancellation. Mouse confirm/cancel bindings on buttons 1–5 activate on
release. Cancel takes precedence over confirmation, including a rebound left
button over a destructive confirmation. These rules also cover title quit.

Evidence: `.local/menu-preview/input-mapping-before.log` reproduced the
cross-trigger failure; `input-mapping-after-02.log`, `input-menu-audit-02.log`
and `input-quit-audit.log` pass the corrections.

## Mixed sound banks

`tools/audit-audio-buses.py` checks the size and SHA-256 of all 122 installed
MLT files against the original catalog. Seventeen Japanese/English pairs
expose their internal program layout. Chao ALIFE character containers combine
bank 3 (P_ effects) and bank 6 (V_ voices). ALT containers relocate voices to
bank 1 and effects to bank 4. Sky Deck's language variants have 153 identical
sample splits and two different announcement programs, bank 1 programs 29/30.

The prior whole-container classification incorrectly changed character
effects with the Voice slider and Sky Deck announcements with Effects.
`tools/prepare-audio-buses.py` now authenticates and adapts the pinned
SoundBank source locally. Every created note retains its program's bus,
including notes created by the sequencer. Restore derives the bus again from
the validated program coordinates; the serialized SDK layout is unchanged.

The audio worker reads bus settings once per mix block. Master and background
mute apply after the bank's DSP output, covering reverb tails without scaling
the master twice. Original guest gain, envelopes, pan, pitch and sequencing
are retained. No title callback addresses or new game assets are required.
ADX routing remains per authenticated Music/Voice owner. Mixed movie audio
continues to use Master because the original stream is already mixed.

`tools/test_audio_buses.cpp` loads the actual authenticated retail banks and
captures final PCM through the existing synthetic endpoint. The observer is
available only to an explicitly injected hidden-process test API; the real
endpoint still receives mandatory silence in background tests. No audio is
saved to disk or played. The comparison retains the authored DSP startup
tail: muting a note must exactly match the same bank rendered without that
note, rather than pretending a freshly loaded DSP has no history.

Six representative voice/effect cases pass: ALIFE Amy banks 3/6, ALT Gamma
banks 4/1, Japanese Sky Deck announcement 29 and English stage effect 26.
Each checks actual PCM isolation, half Master gain, restored active-note
routing and fully silent Master. Logs are `audio-bus-amy-{fx,voice}-05.log`,
`audio-bus-alt-fx-05.log` and `audio-bus-{alt-voice,sky-voice,sky-fx}-06.log`
under `.local/menu-preview`.

The initial fixture checks exposed two test issues, retained in earlier logs:
background audio was already zeroed before the endpoint observer, and ALT
program 0 is a silent placeholder. The final observer is before mandatory
muting, and the ALT voice fixture uses authored program 2.

The five-language Options/settings/profile regression also passes in
`options-audio-input-final-02.log`, using an existing isolated test-save copy.
Physical device unplug/standby and original in-game tutorial glyph coverage
are separate from these component results.

The retained build in `build-options-audio-input-01.log` passes native link
audit (1,909,914,112 bytes) without rebuilding AOT translation units.
`runs/options-audio-input-01/result.json` passes the hidden/muted real-game
Options/Sound-Test round trip. Original menu display is suppressed through
both fades and the modal advances no guest instructions/frames. The run ends
at its planned 50-second diagnostic deadline. The quick r354 integrity audit
passes. Controller hardware feedback still needs the separate input audit;
the existence of a vibration-strength slider alone did not prove gameplay rumble.

## PuruPuru integration

The subsequent [owner audit](rumble-owner-audit.md) found that native input
never exposed the PuruPuru capability, so the original title returned before
creating its request task. Four byte-bound SDK entry replacements now cover
capability, timeout configuration, request and stop. Only the second accessory
slot of each connected compatible controller is exposed; VMU slots and saves
are untouched. The original player-to-port lookup and delayed request task
remain in the retained AOT, including its compiled `8C109D60` entry.

The request packing and timeout translation follow the authenticated retail
SDK and the audited Flycast implementation. A native deadline worker applies
the two motor levels and explicitly stops them even if guest progress stalls.
It sleeps while idle; unchanged per-frame policy does not wake it. It never
executes guest code. Menu/focus suppression, controller replacement/disconnect
and shutdown cancel old effects; the worker is joined before the XInput DLL
is released. Hidden tests never attach the real motor transport.

`sonic_rumble_tests` passes signed-strength normalization, frequency/timeout
contracts, both inclination flags, VMU-slot isolation, strength applied once,
an automatic timed stop without frame polling, suppression, rebind and teardown
with a synthetic endpoint. `input-rumble-02.log` and `poll-rumble-01.log` pass
actual input remapping and platform-poll isolation. Direct Sony haptic output
remains unsupported and is stated in all five menu languages. No physical
vibration test was performed on the user's controller.

The DualSense Z axis is no longer also read as a combined trigger. Verified
Z/R camera input is preserved. Dedicated U/V analog trigger admission remains
unproven; digital shoulder/keyboard alternatives and XInput analog triggers
remain available. Mouse side-button use now updates prompt style; bindings
with only an alternate key no longer display an empty label.

## First audio CPU reduction

The DSP still runs for every loaded effect bank. Zero-valued sends avoid an
unnecessary `llround`, and static pan/level factors are computed once per audio
block with the original multiply order. Original delay lines, rounding and
output validation remain. The eight complete PCM fingerprints in each of two
retail fixtures are identical before/after, including restored voices and tails.

`audio-cpu-before/after-{amy,sky}.log` report 390.625 -> 375 and 406.25 -> 375
process CPU ms respectively. These single, short observations have coarse CPU
clock resolution; they are not a demonstrated whole-game speedup. The paired
stage experiment and remaining bottlenecks are recorded separately.

Stage benchmark schema v2 now labels title-boundary FPS and new-draw FPS
separately from output FPS. It also records applied video Hz, release slots
and logical delta. None of these counters is falsely labeled as a proven
gameplay simulation-update count.

## Integrated build and matched stage check

`build-options-integrated-02.log` links `out/experimental/game.exe`
(1,909,923,840 bytes) successfully, including all four new native request
bindings. No AOT translation units were recompiled. The latest source identity
is `616621b2542fbce3aff031a2d7682474ecd2876734cba348f1f180edcadb8fc5`.
`options-integrated-final.log`, `input-integrated-final.log`,
`rumble-final-01.log` and the quick r354 executable/AOT-metadata audit pass.
Development save/load, focus loss, system suspend and window destruction also
cancel motor output; a restored state never resurrects a transient effect.

One paired 60-second Emerald Coast probe used hidden/muted D3D11, 1280x720,
input profile 3, VSync Off, original camera and the same diagnostic timing mode.
Both runs reached their intended stop with no contract/crash frontier.

| Observation | Before | Integrated |
| --- | ---: | ---: |
| New drawn frames / second | 12.989 | 13.336 |
| Presentation / second | 143.904 | 143.995 |
| Process CPU ms / title boundary | 86.264 | 90.398 |
| Process CPU core equivalents | 1.123 | 1.204 |

Evidence: `runs/options-integration-{before,after}/result.json`. The small
throughput rise comes with increased total CPU cost in this one pair. This
does **not** establish a whole-game performance improvement or stable 30/60
simulation FPS. The time-driven movement probe can also reach different
content as throughput changes; diagnostic timing adds work to both sides.
The useful result is preserved PCM plus reduced local routing work, not a
blanket FPS claim.

The integrated run consistently witnesses active video 50 Hz, release slots 2
and logical delta 2. The original nominal release cadence and measured actual
throughput remain different quantities. Further performance work should focus
on guest/AOT execution and independently prove gameplay updates before any
60-Hz enhancement. Interpolation remains disabled.
