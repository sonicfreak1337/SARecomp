# Changelog

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
