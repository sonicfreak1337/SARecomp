# Configuration and save language options

`sonic-config.exe` is an English Win32 program next to `game.exe`. It writes
the same atomically published INI as the native Options menu. The launcher
opens it only when `setup_complete` is absent/false; cancellation exits before
game initialization. `KATANA_PORT_BACKGROUND_TEST=1` bypasses the popup.
No controller setting is disabled by configuration.

Display mode, resolution, renderer, aspect, render scale and VSync are host
settings. Numeric output FPS selection was retired on 2026-09-13. Game timing
selects Original (scene-owned update/output cadence) or Recompiled (60-FPS
gameplay target, 60-FPS output without VSync, display-paced output with VSync).
Changing timing or VSync requires a restart. Legacy FPS and anisotropy INI
values are ignored. Text, voice and subtitles additionally have an explicit
**Use game setting** state. That state performs no guest-memory reads/writes
and continues every original entry. Explicit language choices apply at startup
and after a save load; the next normal game save persists them. There is no
forced save at a partially initialized loading boundary and no host patch to
an original VMU file. Save/Chao provider identities and save locations stay intact.

## PAL v1.003 source contract

The active save record is `8C7988E0 + U32[8C161BE4] * 4A0`, index 0–2.
Three records occupy `DE0` bytes. Options byte `+251` encodes text in mask 70
(1–5 for Japanese/English/French/Spanish/German), voice in mask 0C (1 Japanese,
2 English), and subtitles in bit 02 (zero means on). Other bits are preserved.

The common serializer `8C0884A0` is intercepted before it copies the selected
record, adds current progress and computes its checksum. Both `8C0114F0` and
`8C011524` reach this serializer. It does not overwrite byte +251 from globals.
Its original body still owns the full record update, CRC and caller return.

At `8C0885C0`, only PR `8C011750` admits the load merge: the caller has completed
the SDK read, original validation and full three-record copy. Other callers
and out-of-range indices are left alone. `8C0544E2` at PR `8C054872` applies
initial text/voice/subtitle globals after the title's BIOS language setup.
`8C08A4B2` at PR `8C04C9D8` preserves subtitle preference after its primary load.

ADVERTISE getters are source-bound at `8088CA94`, `8088CAF8`, `8088CBC0`,
runtime `8C901A94`, `8C901AF8`, `8C901BC0`. They invoke their exact original AOT
body through the runtime PC, preserving scratch registers, MACL, T, stack and
floating-point effects, then replace only the result. The subtitle getter's
0=on convention differs from the global/configuration's 1=on convention.

Every hook is pinned to its original byte hash in the native manifest.
The provider refresh admits only this complete reviewed family (seven hooks),
validates already compiled entries, and updates dispatch/audit metadata. No
game partition or Katana SDK is regenerated. The immutable baseline is intact.

## Verification

`sonic_language_tests` compares all option-mask combinations and the exact
retail setters. It checks wrapper CPU effects against the original getters,
the confirmed load boundary, initial globals, save merge and index bounds.
It computes the original CRC, writes/reopens a separate native VMU provider,
validates all three records with original `8C088708`, then reads language
through the original getters with host overrides disabled. Retail code runs
in the existing test-only interpreter; no interpreter is linked into game.exe.

The CRC leaf `8C088634` reads record bytes +4 through +49F, starts at FFFF,
uses reflected polynomial 8408 and final XOR FFFF. The original serializer
stores its zero-extended 16-bit result in the first 32-bit word. The validator
checks three records and rejects the first mismatch.

The config control test saves/reloads Vulkan, borderless, ultrawide, German
text, English voices and enabled subtitles; invalid dimensions cannot replace
a valid file. Its own window is briefly painted outside the virtual desktop,
without activation or a taskbar entry, for a native capture. It is not a
screen capture of the user's applications.

Final checks: `SONIC_LANGUAGE_TESTS_OK` reports 13,824 mask combinations,
36 original-setter cases, matching getter ABI, native VMU reopen and original
CRC validation with host overrides disabled. Local log:
`.local/language-save-verified.log`. Dialog capture:
`runs/configuration-final/configuration.bmp`.

The initial fullscreen startup exposed a menu-order bug: fullscreen detached
the native menu before the Options extension queried it. `window_menu()` now
retains access to that owned menu while detached and restores it on Alt+Enter.
The exact reported configuration (Vulkan, 3182 x 1332, borderless, German text)
was copied to `runs/config-start-regression`. Hidden gameplay passed the
15-second probe; its native check explicitly installs Options into the
detached menu, exercises choices and restores the same menu on windowed return.
No user configuration or save was changed by that test.

Final incremental build: `.local/vulkan-build-13.log`, 34.945 seconds,
zero AOT recompiles, native link audit passed. The baseline executable/metadata
hash check passed separately. These checks do not repeat the user's stories.
