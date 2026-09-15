# Installer rebuild v5, 2026-09-15

**WITHDRAWN — DO NOT DISTRIBUTE.** Original timing incorrectly disabled native
gameplay math in these builds. The packages below have been moved out of the
delivery folder. Their tested replacements and the existing-installation
patch are recorded in [the regression fix](original-timing-performance-fix-20260915.md).
The remainder of this document is historical evidence, not a release approval.

The user explicitly requested updated installers before work stops. This
supersedes the earlier instruction to wait for a large performance gain before
packaging. It is not a claim that Recompiled mode now sustains 60 FPS on Deck.
No further performance experiment or guest AOT export was started.

## Packages

All files are under `out/installers/`. Sizes below are measured complete
installer files, including their wrappers, rather than summed components.

| Edition | Installer | Bytes | Previous v4 bytes | Saved bytes |
| --- | --- | ---: | ---: | ---: |
| Steam Deck | SonicAdventureRecompiled-SteamDeck.run | 267,253,694 | 316,975,374 | 49,721,680 |
| Linux x86-64 | SonicAdventureRecompiled-Linux-x86_64.run | 267,254,558 | 316,978,518 | 49,723,960 |
| Windows x64 | SonicAdventureRecompiled-Windows-x64.exe | 247,911,435 | 280,856,712 | 32,945,277 |

SHA-256:

- Deck: `844c174a892142b7065a7d2de6e6ca4802ac847dea1d0bdca7fc84cadeb7fc8a`
- Linux: `e4fce9133cbc2a05aa3eeb660db1bd61a903fcbab324038827e5022a77936556`
- Windows: `f81d4ac064a6d9d5eb30953c976e12d70d136b36d88a3175c99e355925bd80fb`

Linux/Deck use the verified 32 MiB XZ dictionary; Windows retains NSIS solid
LZMA with a 16 MiB dictionary. All use the verified lean FFmpeg runtime.
The payload audit found 26 files on Linux/Deck and 30 on Windows, including
integrity manifests. Original media, installed original content, personal
saves, crash reports and development files are excluded. Required licenses,
the FFmpeg build recipe and configuration metadata remain included.

The Linux game is identical in both editions: 1,678,962,752 bytes after stripping,
SHA-256 `5230777fad5de2a68ef17985b5c53276873a8d51b1b2d64d9eedb67040224f01`.
The unstripped developer binary is
`0b3afc852b2126f0a3a893e2c8f245455187c49e882e2ac175f5982a673ece95`.
The Windows game is 1,911,726,080 bytes,
SHA-256 `0e6fdeca6de6eee7cbfff73b7792515776860a188dab7e5cdab0b43f89a4ab8f`.
The accepted `out/experimental/game.exe` and r354 snapshot were not replaced.

## Installation checks

Both final `.run` files were executed through `sh` in the headless Ubuntu VM,
as UID 1000, without sudo. Each installed all 2,070 original files from the
supported PAL GDI and verified the complete application payload. Separate test
namespaces suppressed real desktop/start-menu changes. The installed game hashes
match each other and the staged binary.

The Deck installation wrote Vulkan, 1280x800, fullscreen, VSync on and Original
game timing. Desktop Linux wrote Vulkan, 1280x720, windowed and Recompiled timing.
The defaults checks on both installed setup executables also changed a timing
and audio preference, then reapplied setup defaults and verified that the
existing choices survived. Existing installations are not forced back to
Original; their saved timing selection remains authoritative.

The final Windows NSIS envelope returned 0 after full installation. NSIS does
not forward the child setup's stdout handle, so an initial test expecting the
console success marker was corrected to verify the installed artifacts: all
2,070 original files and all 29 manifest-listed application files matched their
expected lengths and SHA-256. The envelope's hidden setup UI smoke check and
the installed defaults/reinstallation check also returned 0.

The installed Windows game completed Emerald Coast's exact diagnostic frame
window 5..35 and stopped normally with HostDeadline (exit 1 / stop reason 2).
The test used only its newly installed content and libraries. One preliminary
launch printed usage because a diagnostic flag lacked its required content
argument. A no-argument launch then confirmed correct installed-content
discovery but intentionally failed the benchmark guard: Windows automatic
input recording is mutually exclusive with isolated test input. The successful
stage check used the supported explicit installed-content invocation. Neither
test-harness correction required a product change or repackaging.

The installed Linux/Deck game also completed Emerald Coast's exact diagnostic
frame window 5..15 with ten measured new draws, a valid frame window and normal
HostDeadline shutdown (exit 1 / stop reason 2, no forced termination). The VM
used reduced-resolution hidden Vulkan rendering and isolated input. The entire
run took 533.7 seconds under TCG/llvmpipe and overlapped installer validation;
its timing must not be used as performance comparison evidence. The generated
FirstVisibleGameFrame product gate is not the acceptance criterion for this
hidden scenario; the actual scenario's completed rendered-frame window is.

Evidence is local under `.local/menu-preview/installer-v5-*`,
`.local/windows-full-install-v5/` and `/home/sonic/installer-v5/` in the owned VM.
These checks cover installation and a short gameplay path, not a physical
Steam Deck performance or full-story acceptance run.

The owned VM was shut down after collecting its results. No game, build,
packaging job or performance investigation remains running.
