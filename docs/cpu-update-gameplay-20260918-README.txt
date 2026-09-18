SONIC ADVENTURE RECOMPILED
Native Gameplay CPU Update - September 18, 2026 (v2)

INSTALLATION
1. Close Sonic Adventure Recompiled.
2. On Steam Deck, switch to Desktop Mode.
3. Run SonicAdventureRecompiled-CPU-Update-2026-09-18-v2.run.
4. Wait for the success message, then use your existing Steam shortcut.

No sudo, GDI or game reinstallation is needed. This update supports the
September 17 Steam Deck/Linux Test Installer, the September 17 v2 CPU
Update and the September 18 morning CPU Update.

The update replaces the program and updates its installation manifest.
Story saves, Chao data, installed game content, settings and the diagnostic
preference are preserved. The previous program and manifest are kept as
backups. Applying the same update again is safe.

Keep about 2 GB free on the installation drive, plus space for the download
and temporary extraction.

NATIVE CPU WORK
Connected player-state, movement/contact, camera, actor and world/model
display operations now run together in the native CPU group. Both Original
and Recompiled timing use this group; game speed and settings are unchanged.
The experimental interpolation, model packets and unrelated rejected
experiments remain disabled.

DIAGNOSTICS
Use the matching SARecomp-Diagnostics-ON-2026-09-18-v2.run and
SARecomp-Diagnostics-OFF-2026-09-18-v2.run only when needed. These small
switches change the next launch's internal diagnostics policy without
replacing the game. Normal play uses OFF. Functional memory and module
handling remain active.

This is a test update. Linux VM performance results are not Steam Deck FPS.

Port by SoNiCFReaK. Powered by KatanaRecomp.
Zstandard 1.5.7 is included under its BSD license.
Source: https://github.com/facebook/zstd/tree/v1.5.7
