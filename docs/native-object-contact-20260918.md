# Native object collision and the combined CPU groups

The shared object-collision pass is now a complete native operation, selected
internally with `SARECOMP_NATIVE_OBJECT_CONTACT=1`. It stays OFF by default.
This is common object/actor collision, separate from the already-native
terrain collision-world producer and movement/NEAR/TOUCH operation.

## Connected scope

The public owner `0342E0` clears the contact results, runs the live category
lists through `033F00`, `03415E` and `034250`, then finishes the original
`02F81A` tail. The group includes broad bounds, shape-pair dispatch, shape
transforms and narrow tests, contact filtering and hit registration. It keeps
the original list order, category rules, live counts and flags. It does not
reduce the number of game updates, omit collision pairs or change distances.

`tools/object-contact-owners.json` adds 53 authenticated owners: 8,909 reachable
instruction PCs, 512 call sites and 117 original lexical FPU scopes. Fifteen
more owners in this closure already belong to the native movement/contact
group. The combined implementation now has 114 owners in total.

The operation shares admitted RAM and source proofs across its internal
calls. Foreign callbacks remain real calls and revoke the borrowed view.
Code/data alias fences, original rounding/FP bank behavior, supported-memory
checks and the original fault continuations remain functional. The source
proof set grows from a 64-bit mask to a size-bound bitset; source identities
are still checked per entered owner. No persistent collision-data cache or
cross-callback permission cache is introduced.

Public entry hooks remain separate from inventory membership so that the
retained continuations can execute without re-entering the enhancement.
After an actual retained return or computed tail, the bridge publishes that
owner's transfer site, including when a tail child completes the return.
The private original bodies expose no fabricated global dispatcher entries.

## Verification

Windows and Linux each pass 105 cases: 92 original-instruction comparisons,
nine complete retained-AOT executions and four actual unaligned-access fault
continuations. The cases cover near/far shape pairs, all four reviewed shape
types, empty and paused passes, all nine list categories, mixed categories,
filter flags and multiple shape records. Nearest rounding and the alternate
FP bank with round-to-zero are included. Every CPU register and all 16 MiB
of RAM are compared, including intermediate real foreign-call boundaries.
The AOT checks follow genuine sparse entries from the retained dispatcher.

The hidden, muted Windows Gamma Emerald Coast pair has all four related
groups OFF versus ON: movement/contact, model submission, land display and
object collision. All sixteen selected state fields match at both endpoints.
Frame 270 is byte-identical:
`f351cb258cecce1622bc275cc6a06b47b9aeeef6eac57f85f2df24616c846498`.
The interval executes 48 native object passes and 96,375 internal contact
calls, without a contact/hierarchy resume or model-proof revocation. This
capture check is excluded from performance evidence.

Both actual game targets build incrementally. The executable manifest is
`runs/object-contact-executables-20260918.json`; all 27 allocated ELF sections
match between the build output and the stripped Linux copy. Current outputs:

- Windows: `out/model-submission-windows-20260918/game.exe`, SHA-256
  `6b09df44b46b2252ac847df5e8957ee02d09b18d39b36d6efafa236d5131f05d`.
- Linux build: `build-linux/game`, SHA-256
  `12cb1bc9ba76ec96653b65a5a486f85e167ae48c322e495f3fcb02ca6a4ed653`.
- Linux staged: `out/object-contact-linux-20260918/game`, SHA-256
  `f5cca22a5bf9eb117d7ea008aecb61aae00ae2452bc3deb1019593385ea20cd0`.

The private Windows filename is reused; the manifest above replaces its
earlier artifact binding. Released patches/installers are not overwritten.

## Bounded Linux comparison

Each pair uses the same executable, Original timing, Deck aspect, two
software-Vulkan workers, diagnostics/telemetry OFF and the fixed frame window
5 to 25. The four groups are disabled together in the reference and enabled
together in the candidate. Each side completes without forced termination,
with the expected probe stop. All sixteen selected endpoint fields and 68
game updates match in both scenes.

| Scene | Execution CPU/update | Process CPU/update | New game images/s |
| --- | ---: | ---: | ---: |
| Gamma Emerald Coast | -15.34% | -3.28% | +0.05% |
| Knuckles Lost World | -13.37% | -7.23% | +11.59% |

The native object pass runs 68 times in each window. The combined contact
operation makes 126,975 internal calls in Gamma and 190,962 in Lost World.
There are no contact/hierarchy resumes or model-proof revocations. Gamma
execution CPU/update falls from 189.18 to 160.15 ms in this QEMU/TCG VM;
Lost World falls from 319.45 to 276.73 ms. These VM costs are not Deck frame
times. The reduced execution work is measurable in both scenes, whereas
whole-frame throughput improves only in Lost World.

These are combined-package results, not additional percentages to sum with
older experiments or isolated measurements of the newest 53 owners. They
do not establish the requested 20–25 ms Deck target. Keep the internal
switches available for the next candidate; do not promote a default, rerun
these completed pairs unchanged, or create another release solely from this
short VM comparison. Continue the connected native work with the useful CPU
reduction retained. The already-delivered September 18 patch still has SHA-256
`173c0754bad9b429ef5e31260d17f34a07bce1dc69b09f119bb1ed24d3c27003`.

Evidence: `runs/object-contact-final-tests-windows-20260918.log`,
`runs/object-contact-tests-linux-20260918.log`,
`runs/object-contact-gameplay-comparison-windows-20260918.json`,
`runs/object-contact-{gamma,lost-world}-comparison-linux-20260918.json`,
the corresponding source summaries/game logs, inventory, executable manifest
and incremental build logs. No personal saves are included.
