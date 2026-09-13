# In-game PC enhancement package

Requested 2026-09-12. This branch starts at `41e4e19`, retaining immutable
r354 and the accepted standalone port. All UI follows the effective original
text language: Japanese, English, French, Spanish and German. Sound Test
remains the original ADVERTISE screen 5, returning to new Options screen 8.

## Implementation ledger

- [x] Versioned settings, migration of the existing INI, live camera/audio.
- [x] Sonic-style Options replacement; controller/keyboard/mouse navigation,
      remapped prompts, translations, restart labels and preserved Sound Test.
- [x] Out-of-process display confirmation/rollback, including failed startup.
- [x] Action remapping, mouse camera, per-axis sensitivity/inversion,
      configurable deadzones and original-camera return delay.
- [x] XInput and native Sony SDL3 input, with independent analog triggers.
      Vibration controls were removed from Options at the user's request on
      2026-09-13; the internal device backend is retained.
- [x] Original baked tutorial/button hints follow physical bindings. The
      catalog covers all 30 character/language archives, replacing 327 control
      regions in 272 entries, plus the common SUMMARY Next/Back bar. Native
      menu/quit prompts follow remapping. See tutorial-page-integration.md;
      the sampled in-world jump hint is already action-neutral.
- [x] Separate music/voice/effects, background mute, subtitle size/backplate,
      original language persistence.
- [x] Consistent story+Chao profile snapshots, immutable versioned backups,
      explicit restore/import/export preview and overwrite confirmation.
- [x] Focus/disconnect pause, hotplug, resume/monitor/audio changes, voluntary
      diagnostics export without saves or raw personal paths.
- [x] VSync/output caps at unchanged guest cadence. VSync On disables and
      bypasses the manual cap; Off/Automatic retain the saved value.
- [x] Render scale up to 100%, optional anisotropy, output-resolution UI and retained Original mode.
- [x] Relevant hidden/muted component and actual-game checks, quick baseline
      integrity audit and incremental build.

These boxes describe implemented behavior, not exhaustive hardware or story
acceptance. Physical monitor-frequency changes, device unplugging and actual
standby were not performed while the user uses the PC. See
[options-finalization.md](options-finalization.md) for the final build evidence,
hidden-window measurement limits and remaining acceptance work.

## Boundaries already established

Render interpolation was withdrawn on 2026-09-12 after the user confirmed
whole-environment glitches disappear when it is disabled. The prototype is
compiled out and removed from Options; existing INI keys cannot re-enable it.
144 FPS presentation remains independent of the original simulation.
The user also removed replay development from the remaining scope and accepted
the subtitle implementation. Preserve both existing behavior and subtitle
appearance; no new replay work or subtitle redesign is required for completion.

HUD customization was removed at the user's request. Keep the accepted fixed
4:3, 16:9 and 21:9 layouts. Old HUD scale/margin INI keys are ignored on load
and omitted on save; subtitle readability settings remain independent.

The user reported bugs at 200% render scale and requested a 100% maximum.
Menu and validation now cap at 100%; existing 101..200% settings load as 100%.
Supersampling above native resolution is no longer an offered feature.

ADVERTISE screen 8 readiness: exact module binding, main state 11; controller
work current/ready screen 8 with no transition; task at `8C9645D0`, callback
`8C9078E0`, work state 2, interaction 0, display flag `FFFFFFFF`. A host modal
freezes original tasks only at a completed frame. Sound Test/back execute
original `8C90177E(5/7)` and the original follow-up interaction state 9;
normal activation/fades continue after the modal. No original SDK video reset.

Subtitle classification belongs only to context type 1 in `8C054A00`, with
draw callers `8C054AF0` / `8C054B9A`; other text stays unmodified. The existing
text rasterizer is a completed bitmap and is not a generic controller-glyph
mapping boundary. Input hints require proven owners, not arbitrary A/B text
or global atlas replacement.

Microsoft's [XAG 107](https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/107)
informs digital alternatives, remapping and updated prompts. Menu clicks
activate on release inside the pressed control and can be cancelled by moving
away. Dangerous save operations require a second, explicit confirmation.

## Verification policy

Use isolated settings and copied saves. Never delete or restore the user's
active profile as a test. Display tests are hidden; no OS input injection,
real resolution switch or focus grab. Inject logical input at the same menu
controller boundary and use native frame captures. Full matrices remain
unnecessary; renderer/pacing tests use matched representative scenes and
specific cuts, with honest distinction between synthetic cadence and physical
monitor verification. The dated entries below are historical work notes; the
implementation ledger and finalization report describe the current scope.

## Integration evidence to date

`runs/options-native-02` exercised the real Options replacement, volume save,
original Sound Test and return to Options in hidden/muted Vulkan. Modal guest
instruction/frame deltas stayed zero. `OPTION.ADX` was authenticated and loaded.
The intentional 60-second host deadline is not a crash. Component tests cover
five-language navigation/input, whole Story+Chao snapshots, invalid imports,
preview digest checks and before-restore backups. Real child-process display
tests pass acceptance, rejection, crash, hang timeout, orphan recovery and
launch failure. These are bounded checks, not a completed package acceptance.

The reported Options entry/exit flash requires a separate display boundary:
ADVERTISE runtime `8C9079BE`, source `808929BE`, 0x38 bytes, SHA-256
`634269bce4e226bfd5c6296653363581c690e2f55c38810eb90348c78dcbefdf`.
`907A40` registers update `9078E0` and display `9079BE` independently. The
replacement suppresses the display subtree while the exact live task owns it,
including work states 1 (fade in) and 3 (fade out). It does not require ready
screen 8, because that condition fails on both transition edges. Original
update, audio, controller transitions, cleanup and Sound Test remain active.

The full feature/status inventory is in `recompiled-features.md`.

The incremental transition build passed in 33.353 seconds with zero AOT
recompiles (`.local/menu-preview/build-transitions-02.log`). The component
test confirms all four task states suppress only the owned display and leave
CPU/task/controller RAM unchanged; unrelated owners continue unchanged.
`runs/options-transition-back-01` reaches suppression in phase 1/frame 759,
phase 2/frame 769, opens native Options at 771, returns through the original
destination 7, then suppresses phase 3/frame 781 and reaches phase 0/frame 791.
It continues to the original character selection and ends at its planned
host deadline, with no graphics/runtime contract failure. After the user closed
their run, `runs/options-transition-sound-01` passed the Sound Test route and
return to native Options twice on the same executable. It saves music volume
90, preserves zero modal guest ticks, suppresses entry/exit phases and ends at
the planned 50-second deadline. Both run directories contain `result.json`.
The separate r354 executable/AOT metadata verification passed.

The user also confirmed that the original Options menu no longer flashes
on entry or exit. The transition bug is accepted as fixed.

Host-only input polling now bypasses recording, replay and the pending
checkpoint gate. Physical controller history is separate from replay state;
replay initializes physical discovery lazily only when a host dialog needs it.
Options, display confirmation and title quit use this path. The actual
patched-platform test in `.local/enhancement-tests/input-poll-01` verifies two
authored replay entries with 24 intervening host polls, recording exactly two
guest polls and restoring the poll scope after an owner-thread exception.
Keyboard/mouse recording after remapping remains separate unfinished work.

New visual report: Emerald Coast sky/horizon has a hard vertical seam in the
user's ultrawide capture `codex-clipboard-5216695b-315a-476e-b0eb-c74c1658ba82.png`.
The immutable STG01 sky model at offset `11EAE0`, identified by SA Tools'
`Skybox Top` object `11EB08`, closes its eight strips at U=1275 (5*255).
TitleBasic's `037990`/`037CDC` vertex writers load literal `3B808083` from
`038F10` and FLOAT/FMUL signed UVs. The native path incorrectly used 1/256
for both this owner and the resident SDK. `sonic_model_uv.hpp` now preserves
the original title factor and FPSCR rounding; resident models, sprites, UI
and world projection retain their existing contracts. The actual retail
instruction oracle passed 72 UV cases, including negative/repeated values
and both supported rounding modes, along with the existing SDK color checks.
The hidden/muted Vulkan run `runs/sky-uv-vulkan-01` reached its planned
20-second stage deadline without a graphics contract failure. Captures
`frame-1490.bmp` and `frame-1670.bmp` show the continuous Emerald Coast
sky/horizon after the correction. This is visual evidence, not a pixel-matched
comparison: the earlier orbit recording's presentation indices had different
camera poses.

The user's further reports cover cutscene/ending effects and the emblem-get
screen's incomplete dimming. A common Interface color-plane rule now expands
untextured, constant-depth rectangles that span the authored screen width.
It validates both triangles and their shared diagonal and accepts full-width
cinematic bars/vertical wipes. World/HUD roles, textured pictures, local panels
and overlapping triangles are excluded; original 4:3 packets are unchanged.
The presentation checks pass at 1280x720, 2560x1080 and 3440x1440. The real
GPU tests `.local/enhancement-tests/overlays-vulkan-02` and
`.local/enhancement-tests/overlays-d3d11-01` additionally pass at 1260x540:
all nine edge/center samples are dimmed, while the same world-role geometry
retains its aspect. The user also confirmed the emblem dimming fix in-game
on 2026-09-12. Ending captures and separately textured effects remain to check.

Presentation overrides now run after texture and sampler assignment, so
the optional anisotropic filtering setting actually reaches world packets.
The 100% scale cap and legacy 200% migration pass the foundation test at
`.local/enhancement-tests/foundation-scale-cap-01`. The incremental build
`.local/menu-preview/build-scale-uv-overlays-01.log` took 74.133 seconds,
reported zero AOT recompiles and passed the native link audit.

The Options root now includes About. Its three credit lines are exactly the
user's supplied English wording; the heading and Back control follow the
game's text language. Eleven root entries fit on one page, and the credits
page uses the existing Sonic background, panel and input/navigation model.
Previews: `.local/menu-preview/about-english.png` and
`.local/menu-preview/title-about-german.png`.
The combined executable was rebuilt at 17:46 on 2026-09-12. The incremental
log `.local/menu-preview/build-about-final-01.log` reports 66.809 seconds,
zero AOT recompiles and a passing native link audit (1,909,822,976 bytes).
The existing foundation suite passes at
`.local/enhancement-tests/foundation-about-01`; the baseline executable and
AOT metadata hashes still pass `tools/verify-baseline.py --quick`.

## Physical input and movie audio continuation

The quit dialog now consumes the already sampled physical input with the menu
Confirm/Cancel bindings, instead of decoding gameplay-mapped A/B and separately
calling GetAsyncKeyState. It retains title ownership, neutral rearming and
modal guest freeze. Prompt glyphs reflect the opening input device and the
assigned actions. Primary pointer clicks use output-scaled button bounds and
release-inside semantics. Alt is tracked so Alt+Enter cannot confirm quitting
or become gameplay Start. Intro movie skipping now applies the normal input
transform, preserving legacy replay ownership and the original skippable
sequence guard.

`sonic_movie_audio.hpp` wraps the existing pinned FFmpeg provider explicitly.
It applies Master/background-mute gain to copied PCM, leaving decoder-owned
bytes and all video/timestamp/EOS metadata unchanged. Unity forwards the
original pointer unchanged. Sofdec contains a mixed soundtrack, so individual
music/voice/effect sliders do not separate its already mixed audio. New gain
affects newly decoded PCM; previously queued audio drains normally.

`SONIC_MOVIE_AUDIO_TEST_OK` verifies identity at unity, exact half gain,
background mute, unchanged timing/video/stream ends and single-close ownership
including a failed open. `SONIC_QUIT_TESTS_OK` additionally verifies remapped
physical buttons/keys, mouse cancellation, Alt+Enter and all five languages.
Both logs are in `.local/menu-preview/*-final-01.log`.

Actual hidden/muted integration `runs/quit-remapped-movie-01` passes: 368,064
real intro audio samples have gain min/max 0.500, 123 decoded frames were
presented, then the original diagnostic Start edge skipped the intro. Physical
X/Y controls open/cancel/confirm the quit dialog, both modal intervals consume
zero guest instructions/frames, and the process exits normally with code 0.
This tests a bounded intro interval, not full-movie EOS or a physical device.
The combined incremental build `.local/menu-preview/build-input-movie-final-01.log`
took 75.777 seconds with zero AOT recompiles and passed the native link audit
(1,909,828,096 bytes). The broader Options goal remains incomplete: replay
capture after remapping, audio/monitor/standby recovery, render interpolation,
native-output UI and remaining visual acceptance still require work.

## Interpolation implementation and visual correction

The incremental build `build-motion-game-01.log` added port-local frame/draw
metadata, owned render snapshots and intermediate model/view transforms in
72.826 seconds with zero AOT recompiles. It did not establish whole-game
correctness: the user immediately reported glitchy movement, not an FPS issue.
A private Emerald Coast orbit confirmed that only part of the world was being
interpolated, mixing camera instants within one frame.

The correction requires complete world coverage and latches original-pose
fallback for an incomplete camera/scene epoch. The component and both GPU
tests pass, including unproven-world rejection and resource mutation order.
`docs/render-interpolation.md` records the failure, evidence and remaining
scope honestly. No whole-game interpolation or performance improvement is
claimed. The original user playtest was left untouched; subsequent tests use
isolated hidden/muted processes and copied saves.

The corrected game build completed in 68.697 seconds, zero AOT recompiles,
with the native link audit passing. A 20-second hidden/muted Emerald Coast
camera orbit ended normally and verified the full-scene fallback: no partial
intermediate poses were emitted. Native Station Square acceptance and complete
camera/world interpolation are still pending. The r354 quick audit passes.

## Audio-device and sleep-time recovery

The preceding turn made concrete progress by correcting partial interpolation
and gathering native/GPU evidence. This turn adds recovery at the common PCM
endpoint while retaining voices/decoders and the original audio-domain ABI.
Unplayed PCM, pause and monotone media position survive reopening; unavailable
hardware uses a bounded silent real-time queue.

The real stream/domain error-injection test passes removal, silent startup,
replacement failure, callback lifetime, bounded retry and pause/resume.
Sleep-time tests retain ordinary stalls and prevent missed/duplicate events
or modal time from creating timer debt. The incremental game build took
67.388 seconds with zero AOT recompiles and passed the link audit.
audio-device-recovery.md records scope and limitations. Physical hardware
hotplug/suspend and monitor recovery remain separate acceptance work.
The overall seven-part goal remains active.

The final hidden/muted Vulkan integration run audio-recovery-native-01 passes
real FFmpeg intro playback (123 video frames, 368,192 audio samples), mapped
skip, two host quit dialogs with zero guest ticks, and clean exit code 0.
There is no spurious sleep-time rebase in the normal run. The r354 quick
audit and git whitespace check pass; no diagnostic game process remains.

## Recovery visibility

Audio and Profiles now expose localized read-only status in Options. Audio
aggregates current PCM endpoints, distinguishes automatic retries from a
quarantined stream requiring restart, and drops stopped streams from status.
Backup errors no longer disappear in a catch-all handler. Atomic publication
and pruning have separate outcomes, and cleanup retries do not duplicate
already committed backups. Voluntary diagnostics include only state codes
and counters, never personal saves or device identifiers.

Fault-injection and actual copied-VMU tests pass; 4:3 German and 16:9 English
status views were visually checked. See recovery-status.md for exact evidence.
This closes those implementation gaps, not the entire enhancement goal.

The new game was built in 71.389 seconds with zero AOT recompiles and passed
the native link audit. recovery-status-native-01 passes two hidden/muted
Options/Sound-Test transitions with zero guest ticks inside Options and
continued suppression of the old menu. It ends at the expected diagnostic
deadline; no game process remains. The quick r354 integrity audit passes.

The motion regression follow-up corrected a false task identity at the
resident scheduler boundary. Multiple objects previously inherited their
shared callback address; they now use the actual task argument scoped through
nested calls. The retained AOT remains unchanged. The new incremental build
and hidden Emerald Coast camera orbit pass; incomplete world coverage stays
entirely on the current original pose. Full interpolation remains unfinished.
See render-interpolation.md for the native evidence and remaining coverage.

The next incremental build corrects the new Tails Casinopolis texture abort:
an overlapping native TextureSet no longer overrides a current SDK PVM view's
released state. The capsule-state regression and production SDK release cases
pass, followed by a hidden/muted Casinopolis introduction smoke check. The long
user route is not replayed. See casinopolis-texture-release.md for exact evidence.
Shared-camera interpolation also passes independent D3D11 and Vulkan analytic
image comparisons; its actual-game binding check remains pending while the user
tests the new executable. The seven-part enhancement goal remains unfinished.
