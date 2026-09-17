# Nonblocking title audio status

The September 17 native CPU candidate enables this path by default;
`SARECOMP_ASYNC_AUDIO_STATUS=0` retains synchronous status for diagnosis.
See `native-cpu-defaults-20260917.md` for the combined qualification. The implementation builds
for Windows and Linux; no pinned SDK source or game cadence is changed.

The main-thread wait trace in `runs/model-packets-off-wait-stack.txt` reaches
the audio queue's ACK wait through `voice_snapshot`, called by
`synchronize_native_adx_streams` / ADXT status. Repeated VM waits were about
20–25 ms. A status call inspected every bound ADX voice; the time call did
that and then synchronously queried its requested voice again. The existing
once-per-title-frame pump guard did not cover these queries.

`sonic_audio_status.cpp` registers a bounded publisher on the existing shared
audio execution domain. After the engine's worker service, it observes at
most eight voices using the SDK's inline worker query. It creates no second
audio thread, queue, decoder or mixer. Title reads use atomic publications
and never submit a command or wait for an ACK. A coherent last sample is
used if the bounded version read overlaps a publication.

All payload words are atomic, with sequentially consistent version checks;
there is no C++ data race hidden behind a seqlock. Generation-bound handles
and per-registration revisions reject stale results. The lifecycle command
ACK seeds the initial sample. Start is ordered before registration, and
unregistration before stop/release. Restore recreates the publisher and
registers restored voices. Publisher destruction precedes engine destruction;
terminal cleanup never dereferences an already-cleaned engine.

Device completion normally wakes the consumer. For disconnected/silent
devices, a title read may request a nonblocking atomic service wake if the
publication is at least 10 ms old, at most once per 10 ms. This is a bound
on requests, not a guaranteed update latency under worker load. Serial
reference mode retains synchronous SDK queries. Failed domains remain errors.

The existing guest status mapping is retained, including paused PLAYING,
DECODE_END while mixed audio remains unplayed, and PLAY_END only after the
audible tail drains. Only the title thread publishes guest RAM.

## Verification so far

- Windows dedicated and serial component tests pass, including unavailable
  audio-device recovery. Linux dedicated SDL-dummy component tests pass.
- 50,000 dedicated-mode reads submit zero commands and add zero ACK waits.
  Tests cover capacity, invalid/foreign-thread access, pause/resume, completion
  without advancing a title frame, tail drain, slot/generation reuse, stop,
  a fresh publisher epoch and terminal cleanup.
- Windows Gamma gameplay on/off completes the same 5..125 image window,
  both at about 60 images/s. Producer CPU is 14.3229 ms/image in both runs;
  this establishes no Windows CPU gain. Evidence: `runs/audio-status-windows-*`.
- The clean same-executable Linux Gamma pair completes 20 updates each:
  ON 434.9343 producer CPU ms/update, 0.26713 new images/s, 74.869 s window;
  OFF 456.4594 ms/update, 0.13372 new images/s, 149.562 s window. That is
  4.72% less producer CPU and 99.76% more new-image throughput in this VM.
  Total run times are 473.43 and 944.40 seconds, including startup.
  Both use the same native animation/pose, RAM/transfer paths and executable,
  with model packets/closed-memory leaves OFF and no intrusive sampler.
- Player XYZ bits, HUD timer, release slots, logical delta and video cadence
  match at both 5..25 boundaries. ON game ticks are 172..192, OFF 173..193:
  a one-tick introductory offset, but exactly 20 updates in both windows.
  Raw evidence: `runs/audio-status-linux-{on,off}-summary.json`.
  QEMU/TCG plus llvmpipe exaggerates cross-thread scheduling costs. This
  establishes a removed wait on Linux, not doubled Steam Deck FPS.

Linux executable SHA-256:
`766215589a09cad6d629044de002ce578a38332c8196922e4d13b2827c40d82b`.
No installed patch was replaced at that experiment checkpoint. The subsequent
combined candidate and actual installation checks are in `cpu-update-20260917.md`.

## Original cadence and boss follow-up

With the newer closed-memory candidate `a944a94bbd9aeab3f62ac6db68d9e148067be9e2a602fd4b9a56838e46054fac`,
Gamma also completes in Original mode. Both sides retain 50 Hz video,
release slots 2 and logical delta 2. The ON window produces 20 images /
65 updates in 40.581 s; OFF produces 20 images / 67 updates in 47.042 s.
The authored overload behavior causes different update counts and endpoint
positions. Consequently this pair is NOT a matched CPU improvement: ON costs
252.9873 producer CPU ms/update, OFF 236.4887. The observed 15.92% image-rate
increase is VM throughput with differing workload, not a Deck forecast.
Evidence: `runs/native-memory-linux-original-gamma*-summary.json`.

The Chaos-4 stage-entry/cutscene probe completes on Linux with the publisher
enabled, 20 images / 65 updates, normal diagnostic stop and retained Original
cadence. The Windows Original Gamma and Chaos-4 probes also complete.
These are bounded admission/lifecycle checks, not full boss/story playthroughs.

## Isolated DSP bit-operation follow-up (not retained)

A September 17 scratch prototype replaces the packed-ring exponent loop with
`countl_zero` and the portable signed-shift formula with C++20 arithmetic shift.
It passes all 16,777,216 packed input patterns, 65,536 decode words, one million
bounded signed shifts, and 256 complete DSP blocks / 131,113 frames with identical
outputs and every byte of state. On Linux its 44,032-frame component benchmark
takes 34.523 ms versus reference 32.561 / 33.719 ms. This supplies no useful gain;
the product header and delivered patch are unchanged. Do not promote this
rewrite merely because it uses explicitly native bit operations.
Evidence: `runs/audio-bitops-prototype-linux-test.log` and the isolated sources
under `.local/audio-native-bitops`.
