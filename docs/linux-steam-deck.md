# Native Linux / Steam Deck port

Starting point: the replacement Windows candidate `57210a0`, on `main`.
The previous `v1.0.0-rc1-windows` candidate is superseded. Linux uses a distinct
`build-linux` and eventual `out/linux`; the accepted Windows game in
`out/experimental` is not replaced by Linux builds. Neither this work nor
dependency preparation modifies r354 or personal saves.

This machine currently has no WSL and is not elevated. The build route uses
a pinned Windows-hosted Zig cross-compiler targeting x86_64 Linux / glibc2.31.
No Windows reboot or system-wide installation is required. All native ELF
objects, including the retained generated guest source, must be compiled for
Linux; Windows .obj/.lib reuse would be invalid.

Dependencies downloaded from their publishers and SHA256 verified:

| Dependency | Version | SHA256 |
| --- | --- | --- |
| Zig Windows x64 | 0.16.0 | 68659eb5f1e4eb1437a722f1dd889c5a322c9954607f5edcf337bc3684a75a7e |
| SDL source | 3.4.16 | 7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68 |
| FFmpeg LGPL shared Linux x64 | 8.1.2-52-g5a03dfa0f6 | c9fccd62f756986a657b18b863518c0b2e61686577579fdcb240c4cd7fc9416b |
| Pinned Katana source archive | 178448be | 85e5bd27952552f0957559cca82bde5b4024cd71e50e94650cd922f42430cd66 |

The SDK archive is extracted only into `.local/linux-sdk`. This is a port-local
adaptation, not an update to the old Katana development repository.

Compiled as Linux ELF: the guest CPU/memory/runtime archive, SDL3 (X11,
Vulkan, ALSA, HIDAPI and udev), and the existing Vulkan GPU renderer using an
SDL surface. The shaders and GPU draw rules are shared with Windows. Win32
exclusive fullscreen remains guarded on Windows; SDL owns Linux display modes.

The complete retained guest and native host now link into a Linux ELF game.
Emerald Coast was started through the private levelmatrix scenario in an
isolated Ubuntu VM, with Vulkan on Mesa llvmpipe. The captured frame contains
Sonic, the stage, rings and its original mission prompt. This is software QEMU
emulation: it establishes a native rendered level, not Steam Deck performance
or a complete campaign playthrough. The diagnostic deadline expired during
the slow stage startup; its internal FirstVisibleGameFrame acceptance flag
was not reached even though the actual frame was captured.

Evidence: `.local/menu-preview/linux-game-frame500.bmp` and
`.local/menu-preview/linux-game-v4.log`. The software GPU exposes a 128 MiB
storage-buffer range. The shared renderer now limits its fragment arena to
the device's advertised range and uses that same capacity for gather/resolve.
Hardware supporting the previous capacity retains it.

The native SDL setup verifies the supported PAL GDI/tracks, extracts 2,070
files and reconstructs the bound startup image; the installed output passed
its SHA checks. It installs the precompiled game and required libraries in a
versioned application directory, creates an application-menu and desktop
shortcut, and preserves existing personal saves. The end user runs no
analyzer, Katana export, compiler, Python interpreter or SDK.

Native host checks also pass for sealed content reads, traversal/symlink/root
boundary rejection, save writer locking, byte-exact Windows v2 save interchange,
backup recovery, newer schema handling, independent Story/Chao slots, and SDL
virtual controller disconnect/reconnect, axes, remapping and persisted settings.
These use isolated test data, not the user's saves. The installed content store
is separate from user state: by default `~/.local/share/SARecomp-content` and
`~/.local/share/SARecomp`, respectively.

Integrated native hosts: SDL window/input/audio, Vulkan rendering, FFmpeg
movies, portable menu rasterization, startup progress, configuration/restart,
profiles and error dialogs. The graphics contract check rendered 13 frames;
input checks covered disconnect/reconnect, axes, remapping and persistence.
Audio checks used SDL's dummy driver; they do not establish actual device
hotplug. A physical Steam Deck and real dock/undock still require the user's
hardware test. On detected handheld Deck hardware, the game enforces
1280x800 fullscreen; original 4:3 is an explicit content-aspect choice.

## Packaging

`tools/package-installers.py` stages an explicit file list and produces native
Linux/Deck `.run` installers and a Windows `.exe` installer. The `.run` wrapper
extracts an xz archive into a private temporary folder and starts SDL setup.
The Windows envelope uses NSIS 3.12 solely to extract and launch the same
native setup UI; it adds no second wizard. Both use user-level installation.
Linux/Deck setup remains a window in Desktop Mode; only the game is fullscreen.

No administrator account, sudo password or system-wide package installation
is needed. In Steam Deck Desktop Mode, open Konsole and run the downloaded
file directly through the shell (this also works without an executable bit):

```sh
sh "$HOME/Downloads/SonicAdventureRecompiled-SteamDeck.run"
```

For desktop Linux, substitute `SonicAdventureRecompiled-Linux-x86_64.run`.
The installed application resides in `~/.local/share/SARecomp-app`; the game
content and personal save directories remain separate as described above.

The payload contains the game, required libraries, setup/configuration helpers,
menu/setup art, fonts, icon, compact install recipe and required license notices.
It excludes original GDI/tracks and installed content, personal saves, caches,
logs, crash capsules, maps/PDBs, source, AOT archives and development tools.
Linux application copies have development symbols removed before packaging;
the original linked files are retained locally for diagnosis. Fonts needed
only by the Linux text renderer are omitted from Windows. Windows bundles
only the four Visual C++ runtime DLLs required by the inspected imports.

The shortcut's exact display name is **Sonic Adventure Recompiled** on every
platform. Its wordmark icon is embedded in Windows executables and included
as PNG/ICO where needed. See `assets/icons/README.md` for asset provenance.

Release fault serialization reserves memory and writes no capsule before
the user chooses Export diagnostics in the error dialog. An explicit export
adds the capsule alongside the human-readable report; it never attaches save
files or a full memory dump. The isolated enhancement check passed the
before-consent/no-file and after-consent/byte-exact export assertions.

All three installer envelopes passed complete installation checks (v3 payload).
Both `.run` files extracted and installed all 2,070 original files plus their
verified application payload in the Linux VM as UID 1000, without elevation.
The Deck edition saved Vulkan, 1280x800, fullscreen and VSync defaults.
The final Windows NSIS executable completed the same native installation
path with exit code 0. These checks use isolated user data and deliberately
skip real desktop/start-menu shortcut writes. Evidence:
`.local/menu-preview/linux-deck-full-installers-v3.log` and the Windows
full-install output under `.local/menu-preview/windows-full-install-v2.log`.
Earlier setup-only artifacts under `out/linux-setup-dev` are development
artifacts and must not be distributed.

The final Linux/Deck payload contains 23 files including its integrity manifest;
Windows contains 29. The Windows package is 280,856,712 bytes; Linux is
316,978,518 bytes and Deck is 316,975,374 bytes (v4, with the input fix below). Linux's x86-filtered xz packing
replaced the first 656,680,598-byte gzip package without changing any game
bytes. NSIS uses solid LZMA for Windows.

Size comparison: the [official Unleashed Recompiled release](https://github.com/hedge-dev/UnleashedRecomp/releases) inspected on
2026-09-14 offers a 50.9 MiB Windows ZIP and 35.1 MiB Flatpak ZIP. The difference
is primarily our generated native program: the Windows `.text` section alone
is 1,644,410,880 bytes, about 86% of its executable. Linux `.text` is
1,403,146,851 bytes. Removing development symbols saved a further 62,021,072
bytes from the Linux game plus approximately 13 MB from its two helpers.
No unsafe code elimination, guest-code regeneration or image unpacker was
introduced to chase the other project's download size. A substantially
smaller executable requires a separate code-generation/size optimization.

Windows checks: the complete native installation accepted the original GDI,
extracted all 2,070 files, verified the copied application, saved defaults and
started the installed copy without content-root arguments. It rendered the
title sequence and ran to the controlled 45-second diagnostic deadline,
with no runtime graphics contract failures. Capture:
`.local/menu-preview/windows-installed-game-frames/frame-600.bmp`.
The hidden setup smoke check also passed notice-first, keyboard, controller
Back and Desktop-window assertions.

## Steam Deck input correction and current performance

The first physical Gaming Mode test exposed a static Options screen that did
not receive controller input. The Linux render consumer waited indefinitely
for a new graphics packet. While a modal screen remained unchanged there was
no packet to wake the SDL event owner, so input could not trigger its redraw.
The same wait also prevented autonomous idle presentations.

The Linux consumer now waits on a condition variable with a maximum eight-ms
event-pump interval, shortened by its next presentation deadline. New work and
shutdown wake it immediately. SDL polling stays on the render owner; gameplay
bindings and the Windows event loop are unchanged.

`sonic-linux-menu-input-tests` reproduces the old failure without new draws or
test-side SDL event pumping. The corrected real Linux platform/graphics pair
passes static-menu navigation, accept/back, guest-input suppression, all 14
mapped buttons, both sticks, both triggers, pause, neutral disconnect and
reconnection. It uses a virtual Steam gamepad under hidden Xvfb, not physical
Deck hardware. Evidence: `.local/menu-preview/deck-menu-before.log` and
`.local/menu-preview/deck-menu-after.log`. The incremental game link rebuilt
no retained guest translation units. The v4 Linux/Deck packages contain this
fix and passed the payload audit; the unchanged full-install mechanism was
tested with v3. The Windows installer is unchanged.

The installed Linux v3 game also starts without content-root arguments and
plays all 200 nonblack frames of the initial SFD movie. That software-VM probe
ended at its controlled 90-second deadline before the gameplay acceptance
gate, not a crash (`.local/menu-preview/linux-installed-game-v3.log`).

The user's physical Deck reports approximately 40 simulation FPS in Sonic's
Emerald Coast with the Recompiled 60-Hz target. This is below the required
update rate and explains the slowdown. No 60-FPS Deck performance claim follows
from the installer/input tests or software-VM rendering. Performance work
continues separately; the input correction has no measured simulation gain.

Primary references: [Zig downloads](https://ziglang.org/download/),
[SDL Linux build notes](https://wiki.libsdl.org/SDL3/README-linux),
[SDL Vulkan API](https://wiki.libsdl.org/SDL3/CategoryVulkan),
[FFmpeg build publisher](https://github.com/BtbN/FFmpeg-Builds),
[Valve Steam Linux Runtime](https://github.com/ValveSoftware/steam-runtime).
