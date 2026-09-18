SONIC ADVENTURE RECOMPILED
Native CPU Update - September 18, 2026

INSTALLATION
1. Close Sonic Adventure Recompiled.
2. On Steam Deck, switch to Desktop Mode.
3. Run the CPU Update .run file as your normal user. No sudo is needed.
4. Wait for the success message, then use your existing Steam or desktop shortcut.

This update supports the September 17 v2 CPU Update and the September 17
Steam Deck / Linux Test Installer. It replaces only the game program and
its installation manifest. No GDI, game reinstallation or extra packages
are needed. Saves, Chao data, settings and the diagnostics preference stay intact.

Keep approximately 2 GB free in the installation drive, plus space for the
download and temporary extraction. The previous program and manifest are
retained as backups. Applying the same update again is safe.

NATIVE CPU WORK
The update extends the shared native collision-world, render/motion and
inverse-trigonometry groups. Original and Recompiled use the same qualified
native CPU groups while preserving their respective game timing.

DIAGNOSTICS
The matching Diagnostics-ON and Diagnostics-OFF .run files are optional
internal switches. They change the next launch's diagnostic policy only.
Normal play uses OFF. Turn ON when collecting a reproducible problem and
return to OFF afterward. Functional memory and module-lifetime handling
remain active in both modes.

This remains a test update. Linux VM measurements are not Steam Deck FPS
measurements, and the previously reported unreproduced Deck crash is not
claimed fixed by this performance update.

Port by SoNiCFReaK. Powered by KatanaRecomp.
Zstandard 1.5.7 is included under its BSD license.
Source: https://github.com/facebook/zstd/tree/v1.5.7
