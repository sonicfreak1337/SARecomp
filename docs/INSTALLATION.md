# Installation

[← Back to the project](../README.md)

You must provide your own copy of **Sonic Adventure**. The setup program is
English-only; the game's supported text and voice languages are selectable in Options.

## Supported game files

Only **Sonic Adventure PAL v1.003 (1999), MK-5100050**, is supported.
Sonic Adventure DX, other Dreamcast revisions and other regions cannot be used.

Provide the `.gdi` descriptor and all three accompanying tracks. Extract the
complete archive first and keep every track filename exactly as referenced
inside the `.gdi`. Select the `.gdi` in setup, not an archive or an individual track.
Setup verifies the track layout, sizes and SHA-256 hashes before installation.

The main data track, **track 3**, must have this SHA-256:

```text
189f89cb695ff81b40878463f326ead759437794d733329ebf3943196ad59b45
```

<details>
<summary>Full supported track identities</summary>

| Track | Size in bytes | SHA-256 |
| --- | ---: | --- |
| 1 | 26,721,072 | `cb45a8de12af5b13abb559fc8acc714744a4e22965fe67158cdbd0037cace093` |
| 2 | 13,994,400 | `57eff24181c0581e969a40ec4972336da0891dcd093f814d7dd577c64e0142a6` |
| 3 | 1,185,760,800 | `189f89cb695ff81b40878463f326ead759437794d733329ebf3943196ad59b45` |

These identities come from the installer's
[supported-disc definition](../src/setup/supported_disc.hpp). Track contents
are decisive; do not edit or substitute files to bypass a failed verification.

</details>

## Get the right package

Use the Windows, Linux or Steam Deck **player installer** from
[v0.50.0-beta.1](https://github.com/sonicfreak1337/SARecomp/releases/tag/v0.50.0-beta.1).
All three full installers include the latest CPU update. No separate
performance patch is needed for this release.

Automatically generated source-code ZIPs are not installers.
A `.run` CPU update is also not a full
installer: it requires one of the existing builds named in its release notes.

## Windows

1. Run the **Windows installer** (`.exe`).
2. Follow setup, select the supported `.gdi` and wait for installation to finish.
3. Start **Sonic Adventure Recompiled** from the desktop or Start menu.
4. Set your preferred display, renderer, timing and languages in configuration
   or in-game Options. Restart when a setting asks you to.

Keep the installed program and its companion files together. **Alt+Enter**
toggles fullscreen/windowed presentation.

## Linux

Use the Linux installer for **Linux Mint (including Cinnamon), Ubuntu and
other compatible 64-bit distributions**. No particular desktop environment
is required. Compatibility is expected where the requirements below are met;
Mint has not yet been tested separately. Linux installer checks use our Linux VM.

Requires **x86-64 Linux**, **glibc 2.31+**, **X11/XWayland** and a working
**Vulkan 1.3** driver. Wine, Proton and a Vulkan SDK are not required to play.

1. Download the **Linux installer** (`.run`).
2. In its file properties, enable the executable permission, then run it.
   Alternatively, open a terminal in the download folder and enter `sh `
   followed by the installer filename.
3. Run setup as your normal user, **without sudo**. Select the supported `.gdi`.
4. Launch **Sonic Adventure Recompiled** from the applications menu or desktop.

## Steam Deck

Install in **Desktop Mode**, using the **Steam Deck installer** (`.run`).
No administrator or sudo password is needed.

> [!WARNING]
> **Always launch the game through Steam Gaming Mode.** Starting the game in
> Desktop Mode can freeze the Steam Deck. Desktop Mode is for installation;
> return to Gaming Mode before starting the game.

1. Download and extract the installer package and your game files.
2. Right-click the `.run`, open **Properties → Permissions**, enable
   **Is executable**, then run it.
3. Complete setup and select the supported `.gdi`.
4. In Steam, choose **Games → Add a Non-Steam Game to My Library** and select
   the installed **Sonic Adventure Recompiled** application.
5. Return to Gaming Mode and launch it from your library. Leave
   **Force the use of a specific Steam Play compatibility tool** disabled.

The handheld preset uses **1280 × 800**, **16:10**, **fullscreen** and Vulkan.
Compatible external resolutions and wider aspect ratios are available while
an external display is connected. Handheld display choices are constrained to
keep the internal screen usable. Existing settings are preserved on updates.
For the best current Steam Deck performance, use **Game timing → Original**
and **VSync → Off** in Options.
New Deck test installations default to Original; **Recompiled** remains
selectable in Options. The port is a **work in progress**, and further
performance improvements are planned.

## Updating

Close the game first and read the update's supported-version list. Installers
and supported patches preserve existing saves and settings. A runtime patch
updates the installed program without needing the GDI again; continue using
your existing desktop or Steam shortcut afterward.

Use only diagnostic switches that match the installed build. Diagnostics are
off for normal play. Never mix older patches or diagnostic packages into a
newer installation unless its release notes explicitly support that combination.

## Troubleshooting

- **Disc rejected:** check the PAL revision, all three tracks, filenames,
  extraction and hashes above. Setup does not support patched or modified tracks.
- **No launch on Deck:** confirm that the installed application was added to
  Steam and forced Proton compatibility is off.
- **Black screen after a display change:** allow the confirmation timeout to
  return to the previous configuration. Do not confirm an unusable mode.
- **Slow gameplay:** include timing mode, renderer, character, stage and the
  game's SIM FPS in a report. Display FPS alone does not measure simulation speed.
- **Crash:** record the build and exact scene or transition. A diagnostic
  report can be exported when offered; sharing it is optional.

Report reproducible problems through the
[issue form](https://github.com/sonicfreak1337/SARecomp/issues/new/choose).
Do not upload disc images, extracted game content or personal saves to issues.
