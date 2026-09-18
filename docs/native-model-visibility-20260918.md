# Native visibility and material submission

Continuation of the complete model-submission group in
`native-model-submission-20260918.md`. The same private
`SARECOMP_NATIVE_MODEL_SUBMISSION=1` switch selects this work; it remains OFF
by default. The delivered September 18 update is unchanged.

## Connected operation

The PAL `03718C` owner performs both sphere visibility and material setup.
The composed hierarchy now runs that entire owner against its live admitted
RAM capability, with one nontrapping FPU body, then proceeds into transform,
projection, palette lighting and drawing. Original 4:3 uses zero horizontal
expansion; widescreen preserves the existing two widened X comparisons and
unchanged vertical/near/far bounds. No simulation cadence is changed.

The earlier composed-model admission omitted four writes: GBR+28, +32, +36
and +40. It incorrectly described cull as read-only. The parent now admits
all six material publications before mutation, including GBR+44 and +52,
and protects parent/model source bytes and captured inputs. The closed-child
interface explicitly passes the live shared operation. Any declined child
revokes it before retained execution.

At the real GBR of `8C8FFE00`, the first three publications are also the mask
words at `8C8FFE1C..24`. These aliases are intentional: the retail owner
stores them and subsequently reads them in order. The native body preserves
those live ordered accesses, rather than snapshotting them or rejecting the
normal title layout. Other input/source aliases retain conservative rejection.

## Verification

Windows and Linux each pass 253 new full CPU/16-MiB RAM reference cases.
The reference executes the actual PAL cull instructions. For widescreen,
only its two X-comparison source operands are changed to the existing wider
bounds. Cases cover normal title GBR and separate storage, both supported
rounding modes, alternate FR bank, rejected spheres, exceptional operands,
all six protected writes, source/input aliases and both enclosing model roots.
The enclosing-root cases retain mocked transform/draw children and the
existing native palette/context implementations; they do not claim a new
full original-model-draw oracle. Windows also passes the existing 73 shared
model cases after the admission/interface change.

The corrected Windows 4:3 Gamma pair matches all sixteen selected endpoint
fields and has a byte-identical captured frame. In its window, 5,811 model
calls stay within the hierarchy operation: foreign callbacks fall from 9,507
to 3,696 with zero model revocations and zero native-owner resumes. Contact
processing is ON in both sides, isolating the model group. Captured runs are
excluded from performance qualification.

An earlier local admission revision rejected the normal mask alias and
revoked 3,225 model operations. That diagnostic revision is superseded; its
numbers are not performance evidence. Temporary diagnostic prints and the
call-inventory switch have been removed from the retained source.

Evidence:

- `runs/model-visibility-alias-components-windows-20260918.log`
- `runs/model-visibility-alias-components-linux-20260918.log`
- `runs/model-visibility-shared-windows-20260918.log`
- `runs/model-visibility-gameplay-comparison-windows-20260918-b.json`
- `runs/model-visibility-executables-20260918.json`

The Linux stripped executable has the same 27 allocated ELF sections as its
unstripped build. The Windows incremental output still occupies
`out/model-submission-windows-20260918/game.exe`; its current SHA is in the
visibility manifest above. The earlier report's SHA describes the older
revision, not the current contents of that reused development output path.

## Linux gameplay comparison

The capture-free Gamma pair uses one executable, Original timing, Deck aspect,
diagnostics OFF and contact processing ON on both sides. All sixteen selected
endpoint fields match at boundaries 5 and 25, including the same 68 game
updates. There are no native resumes or model revocations. The model group
closes 8,348 calls; foreign hierarchy calls fall from 13,584 to 5,236.

- Execution CPU/update: 223.9954 to 182.0492 ms, **18.73% lower**.
- Whole-process CPU/update: 1151.2808 to 1047.7386 ms, **8.99% lower**.
- New game images/s: 0.76095 to 0.94127, **23.70% higher**.

These are short four-vCPU QEMU/TCG measurements with software Vulkan, not Deck
FPS or an end-to-end comparison against the delivered update. Contact is ON
in the control already. `LP_NUM_THREADS` was not explicitly set in either
run, so both use the same driver default and the recorded setting is null.
Do not compare their absolute costs to the earlier two-raster-thread runs,
or sum these percentages with earlier group-toggle results. Evidence:
`runs/model-visibility-gamma-comparison-linux-20260918.json`.

The bounded Knuckles pairs both complete without a native resume or model
revocation. Sky Deck starts with a one-HUD-tick offset, differing player
coordinates and differing accumulated palette work. Its timing percentages
are excluded, without rebasing counters or rerunning an unchanged pair.
Lost World matches all sixteen endpoint fields and 68 updates: execution
CPU/update changes -1.02%, process CPU/update +1.30%, and new images/s -3.78%.
That is no useful improvement for Lost World. Evidence:

- `runs/model-visibility-knuckles-sky-deck-comparison-linux-20260918.json`
- `runs/model-visibility-knuckles-lost-world-comparison-linux-20260918.json`

Keep the composed model and contact groups private-OFF. Gamma's additional
gain is real within its matched pair, but does not establish a global benefit
or justify another delivered update by itself. Do not repeat these unchanged
pairs or full component suites. The next useful work must change the connected
operation rather than accumulate standalone toggles.

## Remaining boundary

The short Gamma call inventory finds the remaining hierarchy callback at
`040784 -> 0D209C`. This is a character-node callback with authored matrix
rotation/translation branches, not an unused diagnostic call. It is retained.
Do not generalize from this one character to all callback owners or remove
the callback without preserving its full semantics.
