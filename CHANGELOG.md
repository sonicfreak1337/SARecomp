# Changelog

## Unreleased

### Fixed

- Windows setup uses software rendering and a native error dialog when startup fails. Setup also keeps a startup log for troubleshooting installations that do not open (#14).

## 0.52.1 — 2026-10-18

### Fixed

- Fix the Linux / Steam Deck crash after Sonic's opening cutscene when entering the Chaos 0 encounter (#3).
- Restore missing Linux / Steam Deck execution-boundary corrections used by the Windows build.

### Changed

- Replace the withdrawn 0.52.0 Linux and Steam Deck AppImages. Windows continues to use the 0.52.0 installer.
- The separately reported Linux crash at the SEGA splash screen with failure code 4 remains under investigation (#3).

## 0.52.0 — 2026-10-18

### Added

- Added more debug tools.

- Import international Sonic Adventure saves from standard Flycast VMU `.bin` files through Options → Profiles. Import includes Story and available Chao data, shows a preview and backs up the current profile before replacing it.

### Fixed

- #3: Fix Linux and Steam Deck crashing when the intro ends or is skipped before the title screen.

- Restore native level startup on Linux and Steam Deck.

### Changed

- Withdraw the broken 0.51.0 Linux and Steam Deck AppImages.

## 0.51.0 — 2026-10-17

### Fixed

- #14: Windows setup now displays extraction, startup and setup-process errors instead of closing silently.

- #11: Diagnostic reports now show the actual game version.

- #3: Support valid symlinked Linux game-content and save folders, including mounted home directories. Startup errors now identify the folder that could not be opened.

### Added

- Native AppImage packages for Linux and Steam Deck replace the `.run` installers.

- #11: Opt-in crash reports include more graphics-failure details from release builds. Reports do not automatically include personal saves or full memory dumps.

- #5: Windows setup supports choosing an installation folder and an optional portable mode that keeps the runtime, imported game content, saves and settings together. Steam Deck setup finishes without launching the game in Desktop Mode.

- #6 / #10: Broader support for SDL-mapped gamepads on Windows, including compatible Xbox, PlayStation, Nintendo, Steam and third-party controllers.
