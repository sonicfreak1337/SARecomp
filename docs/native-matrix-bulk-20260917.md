# Whole-matrix stack transfers

This internal experiment replaces the repeated MOVCA/FSCHG/FMOV-pair replay
inside the already-native matrix stack with complete 64-byte host copies.
`SARECOMP_NATIVE_MATRIX_BULK=1` enables it; the normal-start default is OFF.
The shared native CPU override and closed-memory capability still apply.
Windows and Linux use identical source, independently of Original/Recompiled
timing. The delivered September 17 patch is unchanged.

## Semantics

The complete original leaf admission remains: authenticated PAL instruction
bytes, scalar FPU mode, privileged CPU state, writable non-code destinations,
and disjoint control/source/output ranges. Only the registered, current product
observer admits the raw write capability. Unknown or revoked observers retain
every original scalar write, including intermediate values and source tags.

The two MOVCA stores in each save are overwritten by the sixteen XF words.
Within the admitted callback-free leaf no observer, guest load or scheduler
sees their intermediate values. One 64-byte copy publishes the exact final
matrix, while memory counters still account all eighteen original writes.
The depth and pointer stores remain ordered. Complete matrix reads use the
existing guard's grouped raw load, accounting sixteen original accesses.

Odd FMOV pair indices select XF, in ascending little-endian word order.
Two FSCHG operations restore the original writable FPSCR bits and clear
reserved bits. The replacement does no
floating-point arithmetic and preserves NaN/subnormal payloads, signed zeros,
both bank modes, all final GPRs, T, FPSCR and the original return PC. Null-input
push saves two matrices; explicit-input push saves one and loads its source.
Full-stack push and zero-remainder pop retain their original early returns.
The original wrapped unsigned pop-depth subtraction is unchanged.

## Component evidence

Windows and Linux each pass 588 full-16-MiB-RAM/register comparisons across
the native leaf families. The matrix cases now include nonnumeric raw payloads
and exact read/write memory counters. A follow-up source review added explicit
reserved-FPSCR normalization and enabled-exception/status payload cases; the
final component results are recorded below. The new path witnesses eight pushes,
four pops, twelve saved matrices and eight loads per component run. Product
observer, arbitrary observer and revoked observer generations are compared;
the latter two retain their ordered original events. No original source or
baseline was modified.

Evidence: `runs/matrix-bulk-{win,linux}-component-20260917.log` and corresponding
build logs. These correctness checks do not establish a performance gain.
Separate same-executable game comparisons follow with projection batching OFF.

The initial performance-comparison Windows SHA-256 is
`9e8a48d1af4ce465bcab7eec2e95b622084b04119be28e9b0e945a04e1a4ef86`.
and Linux SHA-256 is
`16a9a201803d55f321946dc6e34b987558e601799004853a14bdc32248755f2f`.
Both retain the previously documented experimental extended RAM prefixes.

## Paired gameplay results

Projection batching is OFF throughout. Original timing, isolated input,
diagnostics/telemetry OFF, 800x500 Vulkan at 50% rendering are shared by each
same-executable ON/OFF pair. Windows uses 300 new images (5..305). The Linux
four-vCPU TCG VM uses 40 new images (5..45), 136 updates and LP_NUM_THREADS=2.
All runs stop at their expected deadline without forced termination.

| Platform / scene | OFF execution CPU | ON execution CPU | Image throughput change |
|---|---:|---:|---:|
| Windows / Gamma Emerald Coast | 22.29167 ms/image | 22.34375 ms/image | Original cadence capped |
| Linux / Gamma Emerald Coast | 223.01093 ms/update | 220.04589 ms/update | -4.12% |
| Linux / Chaos 4 entry | 223.21385 ms/update | 231.04263 ms/update | -1.51% |

Gamma Windows and both Linux pairs match boundary player position, HUD ticks,
cadence and native/memory counters. Linux Gamma saves only 1.33% execution CPU
while whole-image throughput worsens; Chaos 4 costs 3.51% more execution CPU.
The Windows Chaos 4 pair has different start ticks and subsequently diverging
scene/respawn states. Its apparent 6.04% CPU reduction is not comparable work
and is explicitly excluded from any speedup claim.

Evidence: `runs/matrix-bulk-windows-comparison-20260917.json` and
`runs/matrix-bulk-linux-comparison-20260917.json`, with their referenced runs.
These are VM/desktop measurements, not measured Deck FPS.

## Final correction and disposition

After those measurements, the two bulk paths were corrected to clear reserved
FPSCR bits exactly as the original pair of FSCHG instructions does. Both final
Windows and Linux component runs again pass all 588 comparisons, now including
reserved bits, exception enables and status flags. Logs:
`runs/matrix-bulk-{win,linux}-final-component-20260917.log`.
The final Windows Gamma probe with Recompiled selected also passes. Its short
functional run is not a new paired performance measurement.

Final Windows SHA-256:
`68436e67494fdee8fb06556410f2a3af2179008dd441ddca3c5509882bacea86`, at
`out/matrix-bulk-windows-20260917/game.exe`.
Final Linux SHA-256:
`d6af269a24900cb9a56f762dc3b1c225fce6b6ac158b524e3cc151c5723d70c3`, at
`build-linux/game`. The final Linux component was executed in the VM; the full
Linux gameplay pairs above used the earlier explicitly recorded binary.

The experiment remains internally OFF. It has exact component evidence, but
no useful global performance gain. No patch or installer is produced from it.
The confirmed delivered native CPU paths remain enabled and unchanged.
