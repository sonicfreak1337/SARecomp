# Backup and audio recovery status

Native Options now includes read-only status rows on Audio and Profiles.
Values and actionable explanations use the effective text language, including
live language changes. Status updates do not dirty settings, navigate to a
different page or change the guest simulation.

## Save protection

Automatic backup errors remain nonfatal, but are retained as a visible failed
state. Retry stays on the existing two-second polling interval. The source VMU
is read-only throughout backup and cleanup.

The atomic publication of a new whole-VMU copy is the success boundary.
Failure to remove an older automatic copy reports "saved; cleanup pending",
not a failed backup. Cleanup retries without making duplicate snapshots.
Manual and before-restore copies are excluded from automatic retention.
Initialization and a committed restore invalidate the observed-save cache.

## Audio output

Each actual PCM endpoint publishes its state: connected, silent/retrying,
restart required, or inactive. The menu aggregates all current endpoints;
one working music stream cannot conceal a failed movie or effects stream.
Stopped and destroyed streams relinquish their status. Only a driver-close
failure that requires permanent quarantine recommends restarting the game.
Ordinary disconnection continues to use the bounded automatic reconnect path.

Counters are process-local and are not settings or save data. The voluntary
diagnostic report includes these states, numeric endpoint error codes and
backup/reconnect counters. It does not include device identifiers, paths,
save contents, save digests or VMU file names.

## Verification

- recovery-status-tests-01.log: authentic copied Story+Chao VMU; blocked
  backup directory, bounded retry and recovery; locked old copy after a
  successful publication; cleanup without duplicates; invalid source
  preserved; diagnostic privacy and read-only status in all five languages.
- recovery-status-audio-01.log: actual public audio stream and worker with
  hidden-process driver fault injection; disconnect/reconnect, permanent
  quarantine, multiple simultaneous streams and status lifetime.
- Menu captures backup-failure-4x3-de.png and audio-failure-16x9-en.png show
  readable status and recovery instructions without misleading value arrows.

These logs/captures are under .local/menu-preview. This does not constitute
physical audio hotplug/standby or whole-package acceptance.

The incremental game build build-recovery-status-game-01.log passed in
71.389 seconds with zero AOT recompiles and a successful native link audit.
The resulting out/experimental/game.exe is 1,909,890,560 bytes.
runs/recovery-status-native-01/result.json passes the hidden/muted native
Options-to-original-Sound-Test transition twice. Guest instructions and
frames remain frozen in the host menu; the old menu stays suppressed on
entry/exit. The expected diagnostic deadline stops at frame 1307 (reason 2,
exit code 1), not a crash. The quick r354 audit and whitespace check pass.
