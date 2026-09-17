Sonic Adventure Recompiled - Native CPU Update 2026-09-17

Requires the CPU Update dated 2026-09-16.
Close the game. Run this .run file in Desktop Mode as your normal user.
No sudo, GDI, reinstallation or extra packages are required.
Use your existing Steam or desktop shortcut after the update.

Saves, Chao data, installed game content and settings are preserved.
The internal diagnostics ON/OFF preference is preserved.
About 1.8 GB of free installation space is needed for verified reconstruction.
The previous executable and its manifest are retained as rollback backups.
An unsupported or modified installation is left unchanged.

This update enables native animation hierarchy and pose calculations, complete
render-context operations, bounded vectorized lighting and direct publication
of admitted model data. Audio playback status no longer requires a synchronous
worker round trip on each poll. Qualified collision owners can run in scripted
scenes as well as gameplay, subject to their original functional checks.

Both Original and Recompiled use these CPU paths. Original retains its authored
scene cadence; Recompiled retains its existing game timing. No simulation
updates, game objects or collision checks are removed to increase frame counts.
Measured desktop and Linux VM improvements do not guarantee a Deck frame rate.

Port by SoNiCFReaK. Powered by KatanaRecomp.
Zstandard 1.5.7, BSD license; https://github.com/facebook/zstd/tree/v1.5.7
