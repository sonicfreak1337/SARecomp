# Linux / Steam Deck CPU update, 2026-09-16

The user explicitly requested this update for yesterday's installed patch.
That supersedes the previous delivery hold; it does not establish the requested
large performance gain or the still-open 20–25 ms/frame goal.

## Delivery

`out/patches/SonicAdventureRecompiled-CPU-Update-2026-09-16.run`

- Download size: 243,835,544 bytes.
- Package SHA-256: `3cf612ed2568765790b5e9656ec45fbf6fabcb2dbb205b223dcc889d822fbbb6`.
- Installed executable: 1,680,565,104 bytes.
- Executable SHA-256: `edcc7c027cfe666b186b92a88bad545c48da9144adebd81c0a054d366c1d0884`.

Close the game and run the file in Steam Deck Desktop Mode, as the normal user.
No sudo, GDI, reinstallation or extra packages are needed. Existing Steam and
desktop shortcuts remain valid. Reconstruction needs approximately 1.7 GB of
free application space, in addition to temporary package extraction space.

The supported current executables are the native-math update
`7d4fb2b71694dead0ae7f76f105bb975429978a7dce3ce6cad44aace220c6921`
and the diagnostics update
`d2d6e6d35e2664586486d6dceb262b91ca03b974acecd59f09636b0eedc4807b`.
The latter retains `game.pre-diagnostics-v1`, which supplies the authenticated
native-math reference for the binary delta. Missing or damaged references fail
before publication. The downloaded package contains no original disc content.

## Runtime changes

The program includes the previously qualified B04B RAM/ALU/FP regions with
per-region page proofs, prepared indirect transfers across the shared dispatcher,
and native memory comparison. The RAM and transfer paths now activate on normal
startup, before threads or cached environment reads. Developer environment
overrides still allow direct comparisons with the original paths.

`SARECOMP_LINUX_PERFORMANCE_DEFAULTS=ON` requires both compiled paths. The
normal source build default remains OFF; the delivered candidate explicitly
enables it. Original and Recompiled both retain native gameplay math. This
change does not alter game timing, rendering settings or saved progress.

The new private shared-register procedure pilot is **excluded**. Its fresh
Gamma comparison did not establish a benefit; its build switch remains OFF.
See [the experiment report](linux-procedure-registers-20260916.md).

## Installation checks

All 15 patch fixture cases pass with the bundled Linux Zstandard decoder.
They cover both launch paths, repeated application, damaged inputs, a running
game, symlink rejection, interruption recovery, rollback, distinct backup
names, and the previous diagnostics executable's separate delta reference.

The actual final self-extracting package successfully updated isolated copies
of both supported 1.68 GB programs in the Linux VM. Both resulting executable
hashes match the delivery hash above. Their previous executable hashes remain
available as `game.pre-performance-20260916`; corresponding manifest backups
use the same suffix. Existing older backups are retained. Rollback restores
the actual previous executable, including D2, rather than the delta reference.

Story/Chao/settings sentinels and the existing diagnostics policy remain
unchanged. The update never enters user-content or save roots. Diagnostic
preference is preserved; absent a preference, diagnostics stay OFF. The old
standalone diagnostics patches target D2 and must not be used to downgrade
this newer executable. The internal policy file and developer override still
work on the new program.

The first installed-launch fixture omitted the original `assets` directory
and stopped with `menu-background-identity`. The complete unchanged installed
assets/resources/libraries were then supplied; no game or patch change was
made to bypass the resource identity check.

With those files present, the actual patched D2 installation completed the
Windy Valley 5..25 title-image window and reached the expected host stop
(`completed=true`, `expected_stop=true`, `stop_reason=2`, no forced stop).
Startup confirms `ram_regions=1`, `prepared_transfers=1`, diagnostics OFF,
without overriding those defaults from the harness. Gameplay confirms the
Original PAL video clock at 50 Hz, release slots 2, logical delta 2 and native
gameplay math enabled. This is a short installation/gameplay check, not a
new full-story validation.

Evidence retained locally:

- `runs/cpu-update-patch-fixtures-20260916.log`
- `runs/cpu-update-full-patch-install-20260916.log`
- `runs/cpu-update-installed-verification-20260916.log`
- `runs/cpu-update-installed-windy-20260916-v2.log`
- `runs/cpu-update-installed-windy-20260916-v2.json`
- `runs/cpu-update-install-verification-20260916.json`
- `runs/cpu-update-build-20260916.log`

## Performance scope

This VM uses TCG CPU emulation and software graphics. Its throughput is not
a Steam Deck FPS estimate. Earlier CPU-cost pairs have workload differences;
they do not prove a large global frame-rate improvement. The update is the
explicitly requested current candidate, not a claim of reaching 60 FPS or
20–25 ms on the Deck. Hardware feedback is still needed.
