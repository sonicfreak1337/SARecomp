# Producer-side sound commands

Two port-local experiments remove avoidable title/audio-worker round trips.
Both are internally OFF until combined release qualification:
`SARECOMP_SOUND_METADATA_CACHE=1` and `SARECOMP_DEFERRED_MIDI_NOTES=1`.
The same authenticated SoundBank preparation builds on Windows and Linux;
no original SDK source or public class layout is edited.

The metadata cache stores immutable collection facts (`has_program` and
`has_sequence`) in 512 producer-owned entries. Keys include slot, generation,
kind, bank and item, including negative results. Load/unload/reset/restore
invalidates before mutation, even when restored handles reuse an epoch.
Exceptions are not cached. Worker-side public-API mutation permanently retires
the cache. Full execution-domain and queue health are checked before a hit;
domain-only terminal errors cannot be hidden by a cached answer. Serial mode,
nonstandard domain configuration and foreign callers retain the original API.
`SARECOMP_SOUND_METADATA_VERIFY=1` compares each hit with its original query.

Only the title's two MIDI note-on callers that discard VoiceHandle may enqueue
opcode 0x7001. The sole existing audio worker executes the original Core
operation and owns the resulting note. All ordinary SDK callers retain real
return handles; serial reference remains synchronous. Queue FIFO, handle
generation, note-off/stop ordering and terminal errors remain enforced.
Program-selection rejection and sequence rollback stay on their prior path.

This deliberately moves note execution behind the return of those two void
title operations. A later worker failure becomes visible through the next
execution-domain operation, rather than at the original hook. Queue saturation
can still block; this is not a wait-free command path or a second audio engine.

## Verification

- Windows dedicated and serial tests pass; Linux dedicated test passes.
  In dedicated mode, 4,000 repeated metadata queries enqueue zero commands;
  serial mode retains all 4,000 synchronous commands. Tests cover both query
  kinds, positive/negative results, lifecycle/restore, slot generations,
  exceptions, foreign producer rejection, terminal failures and note ordering.
- Fixed-clock Windows rendering produces 24,576 bit-identical PCM samples
  between synchronous and deferred notes. Existing volume, DSP-tail muting
  and restore checks still pass.
- A hidden Windows Gamma probe verifies every metadata hit against its
  original result and completes normally with deferred notes enabled.
- A separate 600-image Windows pair reaches the same 60-image/s cap. ON costs
  14.6354 producer CPU ms/image; OFF 14.6094, effectively unchanged. Cycles
  are 64.6899 versus 63.7225 million. This establishes no Windows speedup.
- The Linux Gamma pair also establishes no useful gain: ON costs 448.5146
  producer CPU ms/update versus 423.5076 OFF (5.9% more), while image throughput
  differs by only 0.85%. Both windows contain 20 updates. This experiment stays
  OFF and is excluded from the requested patch. The VM is not Deck hardware.

Evidence: `runs/sound-command-{windows,linux}-test-*.log`,
`runs/sound-command-windows-serial-test-b.log`,
`runs/sound-pcm-windows-test-a.log`,
`runs/audio-commands-windows-verify/result.json`, and
`runs/sound-command-windows-{on,off}/result.json`.

Linux preparation now regenerates SoundBank from the authenticated SDK ZIP
instead of copying a potentially stale Windows-generated file. The qualified
prepared source matches byte-for-byte on both platforms:
`547dc60e31977bc58d30c35c2b3d1b8ea1f8db9957928de3ba330826c50aef2e`.
