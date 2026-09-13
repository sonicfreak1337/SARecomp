# Bounded inverse arithmetic specialization

The earlier forwarding-only experiment was rejected. This candidate instead
specializes the retained integer arithmetic at 221 constant-operation,
constant-register sites in the XMTRX branch of the original 4x4 inverse.
It is port-local, opt-in, and preserves the original r354 archive.

## Scope and source contract

The original owner 8C638FF0 forms cofactors/adjugate and scales them by the
reciprocal determinant from 8C64F32C. R4=0 selects XMTRX/XF; nonzero R4
selects an in-place 16-float RAM matrix. Singular input returns R0=0 and
writes the original 0x7F7FC99E sentinel to all sixteen targets. The candidate
does not replace this algorithm or its singular policy.

Only four-argument FPU calls in [8C639066,8C6393DA) are changed: 141 Multiply,
47 Subtract and 33 Add. The source-bound unit is
`unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp`, SHA256
`79ecc1ebbdf3e06c517d5edcc42850c08eb50c7dfe6fe4359c59b0e626472558`.
RAM inverse, determinant, five-argument calls, memory effects, register-bank
changes, instruction accounting, legality checks, safepoints and host FPU
epochs remain byte-identical outside those call statements and one include.
Reversing the substitutions must recover the exact original source.

`prepare-inverse-arithmetic.py` extracts the actual retained integer product
and sum implementations from SDK `src/runtime/fpu.cpp`, SHA256
`e0a2ec1ad05dcb884b69d7b30708a5b8184e09e6c2e77e55259f6ba6cc8126af`.
Their bodies change only by forceinline decoration. The wrapper specializes
operation/register indices and reduces the nontrapping status publication:
clear previous causes, set the current cause, accumulate sticky flags and
publish the destination bits.

PR mode or any exception enable rejects before mutation. The exact original
helpers classify operands/results and retain their rounding rules, including
RM 2/3 behavior. Difficult inputs and boundary results fall back to the
retained five-argument helper. No fast-math option, epsilon determinant,
changed exception policy, host-rounding shortcut or duplicated epoch is used.
The existing ten-entry link-ownership and native closure audits still apply.

## Component and native evidence

The excluded-from-all target `sonic_inverse_arithmetic_tests` links the real
retained runtime. It passed 338,208 cases: 12,858 accepted fast cases, 325,350
fallbacks and 191,317 reference traps. Coverage includes special/boundary
floats, signed zero, cancellation, all RM values, DN/PR/enables, prior status,
dependent operations, source=destination, and matching/nonmatching nested
retained epochs. Every selected template is compared after each operation.
Architectural state, exceptions, accounting, sentinel RAM, callback logs and
MXCSR are checked explicitly. Rejection must leave state unchanged.

Log: `.local/menu-preview/inverse-arithmetic-test-01.log`.
The arithmetic-only 221-operation skeleton, 5,000 rounds in four alternating
pairs, takes 12.24–12.32 ms via the retained helper versus 8.45–8.60 ms inline,
with equal fingerprints. That roughly 31% kernel reduction is **not** an
inverse-function or game speedup. FMOV, FSCHG, stack and other work are omitted
from that skeleton deliberately; the original game unit retains all of them.

Actual object relocations change from 425 void/516 bool FPU references to
204 void/737 bool references, exactly the 221 selected fallbacks. Component
native disassembly confirms inline integer classification/arithmetic/status
publication, with retained calls on rejected branches. The game object grows
from 2,916,994 to 3,020,130 bytes. Evidence:
`.local/menu-preview/inverse-native-evidence.json` and
`.local/menu-preview/inverse-kernel-native.txt`.

## Bounded game comparison

Each run uses the same hidden/muted hardware-isolated Emerald Coast forward
fixture, copied saves, normal input remapping, 1280x720, D3D11, 100%, VSync off
and 144 output target. Each runs 60 gameplay seconds, excludes the first ten,
and has no sampler or image capture. All three complete the planned stop
without runtime failure. Every clock witness remains PAL50 / release2 /
delta2, owned by the original checkpoint with stored TV word1.

| Run | New draws/s | Output/s | Execution CPU ms/boundary | Raw cycles/boundary |
| --- | ---: | ---: | ---: | ---: |
| inverse-control-ec-01 | 16.212 | 143.737 | 60.237 | 265,282,752 |
| inverse-candidate-ec-01 | 18.062 | 143.188 | 53.636 | 236,395,895 |
| inverse-control-return-ec-01 | 17.021 | 143.110 | 57.267 | 251,507,276 |

Execution CPU per boundary is 6.34–10.96% lower than the two controls; raw
cycles are 6.01–10.89% lower. Process CPU/boundary is 63.769 ms versus
68.437/68.544 ms. The return control reused the original control object and
needed only a relink. Executable hashes are recorded in each result JSON;
the control file hashes differ after relinking, with the same source and
selected object retained.

The PC was in active use, and time-bounded forward runs travel different
distances when throughput differs. These results support a useful candidate;
they are not a whole-game guarantee or a precise causal percentage. New draw
boundaries are also not a count of all original task-update passes.

## Integration decision

Keep INVERSE selected in the current experimental build after the comparison;
do not expand to other arithmetic families from this one measurement. CMake
default remains OFF, which restores the untouched selected AOT member.
DIRECT and prepared-RAM experiments remain disabled. No user setting, save,
game cadence or immutable baseline file changes. This is not baseline
promotion and does not implement 60-Hz rendering or simulation.

Final retained-candidate relink: `.local/menu-preview/build-inverse-retained.log`,
ownership/native closure PASS, 1,910,121,984 bytes. Current experimental
`out/experimental/game.exe` SHA256:
`06b46378ec11fe37eaaa4bf66a30f8d6ac9bc57c3dec353fd40f4e780284823d`.
The later standalone timing-component build did not modify that executable.
The subsequent [read-only update diagnostic](original-update-live-20260913.md)
incrementally relinks the adapter and records its own executable hash. It keeps
this exact INVERSE implementation selected; its trace is not another clean
performance comparison.
