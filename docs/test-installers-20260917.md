# Three-platform test installation, 17 September 2026

The user requested matching current Linux and Windows installers alongside the
already delivered Steam Deck test installer, then retirement of obsolete files.
These are private test builds, not final-release promotion. English setup accepts
the supported PAL GDI plus its tracks. The installers contain no original disc,
installed original assets, personal saves or crash reports.

## Published files

| Edition | File in `out/installers/` | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Steam Deck | `SonicAdventureRecompiled-SteamDeck-Test-2026-09-17.run` | 267669346 | `e5d40fee53888b1688315aeed5cbfbb665930ea2a0cc1ed150f4ffeb0bdd7650` |
| Linux | `SonicAdventureRecompiled-Linux-Test-2026-09-17.run` | 267669650 | `d1efd635ea4abd1e82c704fcabbf19542fc8a4d155a317ab48b6e4996793d833` |
| Windows | `SonicAdventureRecompiled-Windows-Test-2026-09-17.exe` | 248289981 | `78fbfdbfd01590d6ba085c855972d41d32982f44e4f45318158d9215c3ec78b6` |

`INSTALLATION.txt` is the common short English guide supplied beside the three
installers. No extra performance patch is required.

## Runtime identity and defaults

Linux and Deck contain the same runtime as the September 17 v2 CPU update, with
only non-runtime ELF debug/symbol sections removed. Source commit `be39cd8`,
unstripped SHA-256 `b5599d23ab3bcf2a0f54207409ff31109b20c84b42782b4f2036fc8c5d447ecc`,
packaged SHA-256 `f7023f123e6fcc4d76361cf05eba131db8344197e38528eab5e6165e1b187f8b`.
The recorded 27-section comparison verifies identical allocated runtime sections.
The movement experiment is absent from these two packages.

Windows was incrementally linked from the current development source. Its game
SHA-256 is `79fe7ef0b281ccb03964ce8b304fc7fae4dd065bc9e0eb34345d13f734a6c4a6`,
provider identity `aea78670b105bc58f100692730852fc842fe7581d9c9aa9da6ca5b8ad9c687a9`.
It retains the same active CPU-group policy, but includes the separately gated
movement experiment with its default OFF. It is not byte-identical to the old
Windows v2 executable or the Linux source snapshot. Lean runtime DLLs are bundled;
build tools, symbols, disc data and test fixtures are excluded.

All editions default diagnostics OFF. New Deck installs use Vulkan, fullscreen
1280x800 and Original timing. New Linux PC installs use Vulkan, windowed 1280x720
and Recompiled timing. Windows uses D3D11, windowed 1280x720 and Recompiled timing.
Explicit pre-existing settings and saves retain the established reinstall policy.

## Actual installation and gameplay checks

The generalized `tools/test_deck_installer.py --edition` performed full disc
installation from each real packaged installer. Linux/Deck ran as UID 1000
without sudo. Windows ran as the normal user. Verification includes every
payload file (25 Linux/Deck, 29 Windows), all 2,070 reconstructed original files,
edition defaults and preservation of explicit timing/music settings on reinstall.

Installed executables using their installed content and bundled libraries
completed hidden, muted Gamma Emerald Coast checks, gameplay frames 5..15:

- Deck: Original, PAL video clock 50, release slots 2, logical delta 2.
- Linux and Windows: Recompiled, video clock 60, release slots 1, delta 1.
- All reached the requested host deadline (stop reason 2), without forced kill.
- Object/model/animation/projection native paths are active. Windows movement
  calls remain zero at normal defaults.

These are functional installation checks, not full-story or full-stage tests and
not performance comparisons. The Linux installed-game check overlapped unrelated
host work; its throughput must not be reported as a regression or speedup.

Evidence: `runs/deck-test-installer-*-20260917.*`,
`runs/linux-test-installer-install-20260917.json`,
`runs/linux-test-installer-gameplay-20260917.json`,
`runs/windows-test-installer-install-20260917.json`, and
`runs/windows-test-installer-gameplay-20260917/result.json`.

The recent Steam Deck crash reported without capsule or reproducible location
remains unresolved. These packages do not claim to fix it. Matching optional
diagnostic switches for the stripped Linux/Deck executable use the filenames
`SARecomp-Diagnostics-{ON,OFF}-Deck-Test-2026-09-17.run`.

## Obsolete artifact retirement

After qualification and publication, 25 obsolete packages or duplicate staging
and isolated Windows-installation outputs (9.56 GiB logical size) were moved into
`C:/Users/ultim/Desktop/SARecomp - Zum Loeschen - Installer 2026-09-17`.
The user requested a desktop folder for manual deletion. A manifest records all
old and new paths, also retained in `runs/cleanup-installer-moved-20260917.json`.
Current packages, v2 update, matching diagnostic switches, user-created archives,
baseline, personal saves and active incremental build caches were preserved.
Moving on the same volume does not itself free disk space. The installed Windows
test fixture and packaging-stage paths in older evidence are now retired; their
qualification reports remain in `runs/`.
