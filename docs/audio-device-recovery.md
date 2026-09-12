# Audio endpoint recovery and standby time

The experimental Sonic port reopens its output after a default-device change
or hardware failure. This applies to the common PCM endpoint used by title
audio, Options music and Sofdec movies. The retained SDK public stream ABI,
format/snapshot contract and single audio-domain worker remain intact.

tools/prepare-audio-output.py authenticates the complete pinned
native_port_audio.cpp (SHA-256
0153b9d08fcecf5aba5ddf8872ae2e6342d1f6330c737684c0487bdebac71499)
and its private execution-domain header before adding the port-local
extension. The native link audit admits this one named direct object.

## Behavior

- Endpoint notifications publish only an atomic epoch. Device calls, queue
  migration and COM lifetime stay on the original audio-domain owner.
- Migration samples the device cursor and copies only unplayed PCM. Old
  callbacks are fenced before reset/unprepare/close. The replacement receives
  the remaining samples; paused streams pause it before their first write.
- A device-position base keeps the media cursor monotone across reopening.
  Requeued PCM does not count as new submitted buffers or samples.
- With no available output, the bounded queue advances silently in real time,
  preserving the movie audio master clock. Open retries are limited to once
  per second unless Windows reports a new device change.
- Write/prepare/position failures retain voices and decoders. A second failure
  during replacement requeue enters silent mode without another reopen loop.
- If a driver refuses to close, the retained SDK callback fence and ownership
  quarantine prevent freeing driver-owned memory. That stream remains silent
  to avoid repeated allocations. Normal user stop remains nonfatal.

The implementation follows Microsoft's [notification lifetime rules](https://learn.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdeviceenumerator-registerendpointnotificationcallback)
and [wave-output buffer retirement](https://learn.microsoft.com/en-us/windows/win32/multimedia/devices-and-data-types).

## Standby

Resume events clear held keyboard/mouse state and notify audio recovery.
sonic_host_resume.hpp separately compares host elapsed time with
[Windows working-state time](https://learn.microsoft.com/en-us/windows/win32/sysinfo/interrupt-time),
which excludes sleep. Sampling is about 10 Hz except explicit resume events.
This also detects a missing power broadcast. Ordinary slow frames still count.

Measured sleeping time shifts title timer/periodic epochs; producer deadlines
and camera/interpolation history reset. Movies retain their pause/play time
boundary. Options and quicksave mark already-excluded modal time, avoiding a
second subtraction. The simulation frequency is not changed.

## Evidence and limits

tools/test_audio_recovery.cpp drives the real public stream/audio worker with
a hidden-process-only WinMM substitution. It checks partial migration,
paused requeue, removal, silent startup/rate, reopen, failed writes (including
replacement failure), bounded retry, failed-close callback fencing and stop.
Sleep checks cover ordinary stalls, duplicate/missing notifications and modal
double-count prevention. .local/menu-preview/audio-recovery-final.log passes.

The incremental build build-audio-recovery-game-02.log took 67.388 seconds,
zero AOT recompiles, and passed the native link audit (1,909,876,224 bytes).
The initial link lacked the precise-clock import in the pinned Windows
libraries; final code resolves the OS entry once and retains the documented
lower-resolution working-time fallback.

The hidden/muted Vulkan game run runs/audio-recovery-native-01 also passes:
123 decoded and presented intro frames, 368,192 decoded audio samples at
Master 50%, normal mapped Start skip, two quit dialogs with zero guest ticks
and a clean process exit. No spurious sleep rebasing was logged during this
ordinary runtime. Its result.json is the native smoke-test evidence.

Physical unplug/replug and actual machine standby were not performed on the
user's desktop. Monitor replacement and wider package acceptance remain
pending. The follow-up recovery-status.md records the localized status UI,
multi-stream error visibility and voluntary diagnostic counters.
