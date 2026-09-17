# Native CPU groups update, September 17, 2026

The user requested another patch for a Deck test and then explicitly requested
testing related optimizations together. This update enables common object
activation/lifetime/distance and the combined model-ownership/SIMD-projection
group in both Original and Recompiled timing. Original game cadence, activation
distances, guest-visible model output and mutable callback boundaries remain.

This is a follow-up to the already delivered morning CPU update, not a new
installer or a claim that the 20–25 ms Deck target has been reached. The immutable
r354 baseline, original Katana workspace and personal saves are untouched.

## Why these paths belong together

Complete model ownership admits and exposes the output span that SIMD projection
writes. The batch publishes into that span before the model owner commits its
guest-visible state. Closed model-memory access was already enabled in the
morning update. The new group selects ownership and projection together via
`SARECOMP_NATIVE_MODEL_GROUP`; individual overrides and
`SARECOMP_NATIVE_CPU_PATHS=0` retain comparison/diagnostic control.

Projection still reads its source points from admitted RAM; this change does
not claim to remove the additional input capture. It enables already implemented
arithmetic and ownership together after measuring their actual composition.
Component evidence is in the model-pipeline (145 cases), projection-layout
(4,096 cases) and object-activation reports (revision F, 1,262 Windows cases and
242 changed Linux cases on top of the prior 1,250-case Linux suite).

## Direct comparison with the delivered patch

Four-vCPU / 6-GiB Linux TCG VM, llvmpipe with two raster threads, muted hidden
Gamma Emerald Coast, Original timing, 800x500 with 50% rendering. No other build
or bulk transfer ran during the measurement windows. Each 5..25 window has
20 new images and 68 game updates. All 19 checked first/last endpoint fields
match: scene, timing, player coordinates/state, HUD and common native counters.
This is endpoint evidence, not an assertion of complete RAM equality.

| Configuration | Execution CPU ms/update | Process CPU ms/update | New images/s |
| --- | ---: | ---: | ---: |
| Delivered Sep17 morning update | 188.403209 | 779.646435 | 1.086012 |
| New object family, both model features OFF | 183.564159 | 783.447969 | 1.111296 |
| Object family plus model group | 168.350259 | 752.907387 | 1.094451 |

The model group saves 8.29% execution CPU versus the object-only build. Together
the groups save 10.64% execution CPU versus the delivered update; VM image
throughput improves only 0.78%. These figures are not additive with older toggle
experiments, do not establish a gain in every stage, and do not predict Deck FPS.

The measured control executable is SHA-256
`7dbdf58e16dc910d4f1b39998e7557851106b5c8d18da8bbfb80453f9a137869`.
The measured candidate is
`9019624096073b6455942f71d337608fa7b9013e8373f16e7e8a151c7acc5c80`,
with explicit model switches. The final build changes their startup policy to
ON; it is a different executable identity. The measured candidate is not the
published package payload.

Evidence: `runs/native-objects-group-comparison-20260917.json` and the
`previous`, `current-assets` and `model-group` summaries. The group run records
35,323 model calls, 17,715 direct outputs, 23,205 projection batches and 800,017
projected vertices. These cumulative counters include startup, not just the
measured window. Model admission declines: zero; projection declines: one.

## Additional combination checked

Direct collision memory inside admitted collision parents and fused
cross/length/normalization children already compose through the collision
closure switch; standalone collision-memory overrides are unnecessary.
Adding that closure to the model group also matches all 19 endpoints, with
11,695 fused cross, 26,983 length and 22,959 normalization calls. It saves only
0.68% further execution CPU while costing 1.09% more process CPU and 0.80%
image throughput. This run establishes no clear additional end-to-end benefit;
collision closure remains OFF. Evidence:
`runs/native-objects-group-collision-comparison-20260917.json`.

The final Linux build retains shipped BASE RAM regions, prepared transfers and
native memory comparison. Extended RAM/FPU, prepared RAM access, matrix bulk,
model packets and unrelated rejected experiments remain OFF. Windows retains
its shipped RAM policy. This isolates the tested combination from older
development-cache experiments.

## Final delivery verification

The Linux/Deck payload is SHA-256
`b5599d23ab3bcf2a0f54207409ff31109b20c84b42782b4f2036fc8c5d447ecc`,
1,743,328,160 bytes. The wrapper is 218,757,784 bytes (208.62 MiB), SHA-256
`735e0ef125cd7886c6cdd7b46ecb07254ce4c4ba9d2f371285f7f38374a259a9`.
Patch ID: `native-groups-20260917`. Staging:
`out/cpu-update-native-groups-20260917/validation-update.run`.
It requires the morning Sep17 executable, runs as the normal user in Desktop
Mode without sudo, and preserves existing shortcuts and diagnostic preferences.
Only the executable delta, bounded apply script, metadata, README and Zstandard
helper/license are packaged. No GDI, installed content or personal data is included.

The actual wrapper updates both isolated Linux test launch paths to this exact
target, preserves the prior programs/manifests as rollback backups, preserves
Story/Chao/settings sentinels and all other manifest rows, and passes a repeated
installation without replacing the new programs. The previous Sep17 control
installation remains unchanged. Evidence:
`runs/native-groups-final-install-20260917.log` and
`/home/sonic/preloaded-v1/native-groups-install-20260917/verification.json`.

The Windows program is `out/native-objects-windows-20260917/game.exe`, SHA-256
`d881534416b05516fe6bde6385769694df00ddd9b9836b8c067c9a604857bb54`,
1,911,770,624 bytes. Incremental link and retained/FPU ownership audits pass.
Both platform builds bind provider source identity
`sha256:3ee6e15d33e7e324059714ec00fbe229cb9002e0e4804627f13a9654484db188`.
The dispatcher body remains identical when refreshing its manifest identity.

Windows Gamma gameplay 5..25 passes with native object/model/projection defaults.
Original retains PAL 50 / release 2 / delta 2; Recompiled retains 60 / 1 / 1
and exactly 20 updates. The Recompiled verification run checks 23,130 projected
batches against the retained implementation while the model owner is active.
Evidence: `runs/native-groups-final-windows-verification-20260917.json`.
Two initial Windows checks stopped during the stage introduction because the
invocation omitted `--wait-for-gameplay`; they verify that introduction only,
not Recompiled gameplay cadence. The subsequent properly gated checks above
are the gameplay evidence. No performance ratios are reported from these
short final function checks.

Actual installed Linux Gamma gameplay 5..15 also passes both modes. Original
retains 50 / 2 / 2, with 29,603 native model owners, 20,021 projection batches
and 136,648 direct object-distance calls without enabling overrides. Recompiled
retains 60 / 1 / 1 and exactly ten updates for ten new images. It verifies
20,003 projection batches against the retained computation while the model
owner is active. Both exit at the expected diagnostic boundary without forced
termination. These are short correctness checks, not complete-stage tests or
additional performance claims. Evidence:
`runs/native-groups-final-linux-gameplay-20260917.json` and the corresponding
per-mode summaries.

Matching diagnostics ON, OFF and repeated OFF packages pass on both installed
launch paths. Program hashes/inodes/modes, manifests and Story/Chao/settings
sentinels remain unchanged. The previous executable is rejected, and the test
policy is restored. Each package is 21,144 bytes. ON SHA-256:
`37233d16673f07a863fa07383ce282c45e9708fa1194552a41887022f276589b`;
OFF SHA-256:
`104fc5ca41a915ba1252442895340b5bcb49b2a5b9f50c62f59c7e1872492e13`.
Evidence: `runs/native-groups-final-diagnostics-20260917.json` and its command log.

Distribution filenames below `out/patches/`:

- `SonicAdventureRecompiled-CPU-Update-2026-09-17-v2.run`
- `SARecomp-Diagnostics-ON-2026-09-17-v2.run`
- `SARecomp-Diagnostics-OFF-2026-09-17-v2.run`

The distributed bytes must match the validated staging hashes above. Adjacent
JSON sidecars bind source commit, publication time and final verification.
The earlier morning update and its matching diagnostic packages stay intact.

The earlier intermediate fixture (901962...) was retired after preserving its
measurements and host executable copy to reclaim VM disk space. Its missing-assets
first attempt was a test-fixture assembly error, repaired before measurements;
the final fixture copies existing assets.
