# Options package completion audit, 2026-09-13

Read-only source audit at `3561d33`, branch `enhancements/ingame-settings`.
Only this report was written. No build, game, hardware query, device change,
network access or helper was used. Existing reports describe prior tests;
this audit does not rerun them. r354 and personal saves remain untouched.

The authorized package is largely implemented. Two concrete residual defects
are suitable for a bounded correction now. The old Sony/rumble/tutorial TODOs
are superseded; physical acceptance remains separate from implementation.

## Concrete source gaps

### P2: mouse buttons 3–5 bypass release suppression after Options closes

`src/sonic_menu_runtime.cpp:38` considers only mouse buttons 1/2 in `neutral()`.
The modal destructor arms `release_input`; `observe()` clears it as soon as
that predicate succeeds (`:133`). But `sonic_input_bindings.hpp:14` admits
all five mouse buttons, `sonic_input.cpp:35` reads any bound mouse button,
and `native_title_adapter.cpp:17072` passes the resulting menu-consumption
flag directly into the gameplay input transform.

Concrete sequence: bind gameplay A to Mouse 4, close Options with the keyboard
while Mouse 4 remains held, then release the keyboard. The next otherwise
neutral sample clears suppression and sends the already-held Mouse 4 to A.
The same sequence with held Mouse 1/2 stays suppressed. This is a source-
proven predicate inconsistency, not a reproduced physical-controller failure.

Small closure: require every supported mouse button to be up before clearing
the existing release latch. Keep the current axis/keyboard policy. Verify
buttons 3/4/5 held across menu close, release, then a fresh press through the
actual release-latch/transform path. `tools/test_enhancements.cpp:107` already
checks mouse release inside the menu, but that Model test does not exercise
the runtime's separate post-menu latch.

### P2: one unreadable profile prevents selecting any healthy profile

`src/sonic_menu_runtime.cpp:192` loops over all profile IDs and calls
`profiles::profile_preview(id)` before publishing the choice list. There is
only an outer operation catch (`:218`). `sonic_profiles.cpp:137` returns an
empty optional only for a missing save; validation/read failures throw.
`current_bytes()` correctly rejects incompatible schemas and invalid volumes,
and requires a usable recovery copy when the primary envelope is damaged.

An inactive profile with damaged primary and no usable `.bak`, an incompatible
schema, or an unreadable save therefore aborts the whole Switch Profile action.
The user receives the generic error and cannot select a healthy or empty
profile through that list. This does not justify weakening save validation.

Small closure: isolate preview failure per profile and show a localized
unavailable/error row that cannot activate that profile; continue listing
healthy/empty profiles. Keep unreadable bytes untouched. Extend the existing
copied-VMU corruption fixtures in `tools/test_enhancements.cpp:147` with one
healthy, one empty and one unreadable inactive profile, and exercise the list
construction rather than only `profile_preview()` in isolation.

## Already implemented; do not reopen stale TODOs

| Area | Current source and actual scope |
| --- | --- |
| Options/display | `sonic_menu.cpp` exposes five-language pages, live changes, discard/reset confirmations and disabled output-FPS control under VSync On. `sonic_presentation.cpp` validates/serializes settings and separates restart fields. `sonic_restart.cpp` implements display trial/rollback. |
| Input | `sonic_input.cpp`, `sonic_sony_input.cpp` and `prepare-camera-platform.py` provide remapping, independent analog triggers, stick/deadzone policy, Sony SDL input/output, stable endpoint tokens and existing XInput routes. Missing SDL retains the documented legacy input fallback. |
| Original vibration | Bound PuruPuru integration and `sonic_rumble.hpp` supply timed original requests, strength, stop/suppression and teardown. The unused frozen direct-vibration API being XInput-only is not a missing Sonic rumble route. |
| Tutorials | Common navigation and page substitutions are integrated. `tutorial-page-integration.md` records 327 regions in 272 entries across all 30 archives; runtime artwork is identity/owner guarded. The old report's “bottom bar only” limit is obsolete. |
| Audio | Per-program bank routing, authenticated ADX routing and endpoint recovery/status are present (`sonic_audio_settings.hpp`, `prepare-audio-buses.py`, `sonic_audio_recovery.inc`). Already-mixed movie audio intentionally follows Master. |
| Profiles/reliability | Whole-VMU validation/import/export, previews, automatic/manual/before-restore copies, bounded backup retry/status and diagnostic privacy exist. `launcher/main.cpp:1128` commits staged restoration after the old provider is destroyed. Focus/controller-loss policies, resume rebasing and display recovery are implemented. |

## Remaining acceptance and scope limits

- Physical Sony USB/Bluetooth input, analog trigger delivery, two-controller
  reconnect identity and vibration delivery remain hardware-unverified.
  Synthetic transport/deadline tests are documented; implementation exists.
- Real audio unplug/replug, actual system suspend/resume and visible VSync
  scanout across monitor refresh changes remain hardware-unverified. Driver
  fault injection and hidden window/display recovery are prior evidence,
  not missing recovery implementation or proof of visible scanout.
- Tutorial inventory/raster coverage is broader than live gameplay coverage:
  the integrated original Sonic English first page was visited, while complete
  page navigation/re-entry and every character/language were not played.
- `inworld-input-hint-audit.md` proves the sampled homing hint already names
  the jump action. Other literal button hints are not exhaustively proven,
  but no concrete wrong remaining record was identified here. Do not invent
  a global hint replacement task from that uncertainty.
- Separately textured ending effects retain representative gameplay acceptance
  limits. They are not evidence of missing Options/input/audio/profile code.

Replay development, render interpolation, HUD customization and subtitle
redesign stay withdrawn. Existing accepted subtitle settings/behavior stay.
Performance profiling and whole-game FPS claims belong to Root's separate
work. No broad game matrix is needed to close the two source defects above.

## Integration closure

Root fixed both defects after the audit. The modal teardown and input observer
now share `InputReleaseGate`, covering all five supported mouse buttons.
`profiles::catalog()` isolates preview failures per row; the real Options
projection localizes unavailable profiles and disables activation. Selection
is validated again before requesting a restart. Save validation and recovery
rules remain strict; listing never repairs or writes a profile.

`sonic_enhancement_tests` passed with a fresh isolated root
`.local/options-closure-20260913-01`: each mouse button held through keyboard
menu close, release, and a fresh press went through the real input transform.
The real profile projection and menu model were exercised in five languages
with healthy, empty, corrupt, and incompatible copied profiles. Keyboard,
controller samples and mouse clicks could not select invalid data. Healthy
and empty choices required the existing confirmation; all save bytes and the
read-only source fixture remained unchanged.

Incremental `game` build and native link-closure audit passed (10 steps,
retained AOT, RAM-read experiment OFF). Build log:
`.local/menu-preview/build-options-closure-game.log`. The package's known
implementation gaps from this audit are closed. The physical acceptance
limits above still apply; no real controller or audio device was disturbed.
