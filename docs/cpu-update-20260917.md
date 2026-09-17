# Native CPU update, 2026-09-17

The user requested continued native CPU work and the next patch around 05:20
Europe/Berlin on September 17. This report tracks the qualified update; it is
not a claim that the 20–25 ms Steam Deck target has been achieved.

## Candidate identity

The Linux/Steam Deck update applies to the September 16 CPU update:

- Base executable: `edcc7c027cfe666b186b92a88bad545c48da9144adebd81c0a054d366c1d0884`.
- Target executable: `7dbdf58e16dc910d4f1b39998e7557851106b5c8d18da8bbfb80453f9a137869`.
- Target size: 1,743,275,568 bytes.
- Self-extracting package: 246,835,864 bytes.
- Package SHA-256: `e4111c57553502091035e09fcfb4620ae2ff509d8b7da195c441d7b3b29685e0`.
- Internal patch identity: `native-cpu-20260917`.

The validated staging package is `out/cpu-update-20260917/validation-update.run`.
The final delivery path is recorded after the remaining installed-game checks.
No GDI, installed content, personal save or development dependency is packaged.
Run the update as the normal user in Desktop Mode, without sudo or reinstalling.
The package preserves existing Steam/desktop paths, settings and diagnostic
policy, retaining the previous program and manifest as rollback backups.

The shared Windows candidate is `out/performance-candidate-20260915/game.exe`,
SHA-256 `0334413ab28a538591b766be2c9bf7582c582e125ae0d15f6b5353e188b0a206`.
Its incremental link passes the retained closure and FPU audits. The original
r354 baseline and old Katana workspace have not been modified.

## Implementation and performance evidence

See [shared native CPU defaults](native-cpu-defaults-20260917.md) for the
qualified group and exact matched Windows result: 6.64% less execution CPU,
7.57% fewer cycles in Gamma Original with identical game updates and state.
The palette-only complete-loop change saves 2.21% CPU / 2.52% cycles on Windows.
Component comparisons cover complete CPU/RAM state, original PAL arithmetic,
observer revocation, rejected inputs, audio lifecycle and tail consumption.

Normal-start Windows checks use no per-feature enabling overrides. Gamma
Original and Recompiled complete frames 5..125 at 25.002 and 60.029 new images/s;
Chaos 4 Original completes its 5..65 entry window at 24.996. These are short,
hidden offscreen checks on the desktop, not full fights or Deck predictions.
They confirm positive native animation, model-memory, render and palette counts;
Original retains PAL 50 / release 2 / delta 2 and Recompiled 60 / 1 / 1.
Evidence: `runs/native-cpu-installed-win-evidence.json` and per-run result files.

Fresh Linux configuration was also checked after deleting only the three
RAM/transfer/performance-default cache selections: all become ON, and Ninja
reports no compilation needed for the already matching candidate. A fresh
build no longer depends on historical cache entries to retain these paths.

The direct Linux comparison against the September 16 binary did not reach
Gamma Recompiled gameplay in the bounded VM run. It ended after about 1,267 seconds with
a forced stop and no valid frame window. It supplies no usable CPU/FPS ratio.
Do not interpret this as a guaranteed speedup or a hardware crash in the old
release. See `runs/native-cpu-base-linux-gamma-command.log`. Earlier VM pairs
are documented separately with their workload/concurrency limitations.

The direct Original-mode comparison succeeds on the same two-vCPU / 6-GiB
TCG VM, with identical 800x500 / 50% software-renderer settings and a quiet host:

| Gamma frames 5..25 | September 16 | September 17 |
|---|---:|---:|
| Window wall time (ms) | 59,030 | 39,912 |
| New images/second | 0.338811 | 0.501102 |
| Execution CPU ms/game update | 241.975850 | 239.482273 |
| Whole process CPU ms/game update | 1,531.791816 | 1,157.871994 |
| Game updates | 68 | 66 |

That is 47.90% more new-image throughput, 1.03% less execution CPU/update and
24.41% less whole-process CPU/update **in this VM**. The authored overload
behavior performs two fewer updates; ending player coordinates consequently
differ. This is not exact matched-work proof, a 48% simulation-kernel speedup,
or a Steam Deck forecast. The retained Windows comparison is the exact-state
CPU evidence. Original timing is unchanged in both Linux runs. Evidence:
`runs/native-cpu-{base,installed}-linux-original-gamma-summary.json`.

## Actual package installation

The complete self-extracting package updated both previously installed test
launch paths in the Linux VM. Both new executables match the target hash above
and share the published inode. Their actual previous programs and manifests
remain as `game.pre-native-cpu-20260917` and the corresponding manifest backup.

Story, Chao and settings sentinels are unchanged. Existing diagnostic policy
and all non-program manifest entries remain unchanged. Applying the same
package again succeeds as already installed without replacing either program.
The separate September 16 control binary remains byte-identical.

Evidence: `runs/native-cpu-actual-install-20260917.log`, the retained verification
script and `/home/sonic/preloaded-v1/native-cpu-install-20260917/verification.json`.
The actual installed program completes Gamma Original gameplay frames 5..25
and the Chaos 4 Original stage-entry frames 5..25. Both end at the expected
diagnostic boundary, with no forced termination. The executable's status code 1
is the expected SDK diagnostic-stop convention; the harness returns success.
Both retain PAL 50 Hz, release 2 and delta 2. Gamma confirms 503 native hierarchy
calls, 7,296 pose blends, 319,221 complete-memory calls, 15,213 render captures
and 22,511 closed palette batches without per-feature enabling overrides.

These are short correctness/normal-start checks in the software VM, not full
boss playthroughs or Steam Deck FPS. Evidence:
`runs/native-cpu-installed-linux-original-{gamma,chaos4}-summary.json`.

The installed Recompiled Gamma run also completes frames 5..25 after restarting
the same VM with four vCPUs and `LP_NUM_THREADS=2`. It performs exactly 20 game
updates, retains 60 Hz / release 1 / delta 1, and ends at the expected diagnostic
boundary without forced termination. It uses normal-start native defaults and
the same authenticated installed executable. This is a function check on a
changed test-machine configuration, explicitly excluded from the two-vCPU
performance comparison above. The harness now records CPU count and software
renderer thread settings to make that distinction visible in future reports.
Evidence: `runs/native-cpu-installed-linux-recompiled-fourcpu-summary.json`.

## Matching diagnostics switches

New policy-only ON/OFF packages are 21,144 bytes each. They authenticate only
this update's program identity and contain no executable replacement. The ON,
OFF and repeated OFF switches pass on both actual updated launch paths; game
hashes/inodes/modes, manifests and Story/Chao/settings sentinels are unchanged.
The previous CPU binary is rejected, and original test preferences are restored.
Evidence: `runs/native-cpu-diagnostics-linux-20260917.log`.

The installed Recompiled Gamma run also timed out before its gameplay window
(1,268 seconds). Its setup matches the earlier successful explicit-ON prototype;
there is no valid measurement from this run. A ten-second audio-worker sample
finds about 41% in effect-block processing and 33% in the native QSound kernel.
This is a worker-specific VM observation, not a measured share of the whole game
or proof of a new regression. The independent Original checks above complete.
