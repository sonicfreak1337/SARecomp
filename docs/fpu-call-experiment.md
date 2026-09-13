# Bounded FPU forwarding experiment

The later INVERSE mode is a separate arithmetic-specialization experiment;
see [its contract and game comparison](inverse-arithmetic-experiment.md).
DIRECT remains rejected. Neither mode changes the default OFF setting.

The hardware-isolated profile in `docs/execution-thread-profile-20260913.md`
found real work in the four-argument `fpu_binary` forwarding overload. The
pinned SDK body only calls the public five-argument overload with
`std::nullopt` and discards its result. The retained native wrapper has an
extra call/return, a 72-byte stack adjustment and optional-argument copying.
The sample is across all callers; its 26/1297 observations do not predict
the gain from changing one unit.

`SARECOMP_FPU_CALL_EXPERIMENT` selects OFF (retained archive), CONTROL
(byte-identical separately compiled unit), or DIRECT. Both experimental
variants use identical compiler settings and preserve the rest of the AOT
archive. The source manifest and SHA bind exactly
`unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp`. Its 425 four-argument call
statements become direct five-argument calls with the result discarded:
274 Multiply, 82 Subtract, 67 Add and 2 Divide. The 516 existing five-argument
calls stay intact. Changed sites belong to the entries 8C638FF0 (419),
8C6397F8 (3) and 8C639B1C (3); the whole unit contains ten public entries.

The preparer checks the actual archived forwarding body, then reverses its
own call-only substitutions to recover the identical original source.
Modes, operands, exceptions, FPSCR, register order, host FPU epochs and
instruction accounting are not changed. There is no new arithmetic helper,
weaker floating-point flag, cache, timestep change or interpolation. A link
audit requires all ten entries to have the new archive as sole owner and
rejects the old selected member. Normal native closure auditing still runs.

A prior candidate would skip two integer-only reciprocal epochs under
proven operand/mode conditions. It was not implemented: the profile did not
establish either reciprocal site was hot. Removing the observed forwarding
work at 425 sites is the better bounded first comparison.

## Native evidence and result

Both variants compiled one selected AOT unit and passed the ten-entry
ownership audit plus native closure audit. COFF relocations confirm CONTROL
contains 425 four-argument and 516 five-argument references; DIRECT has zero
four-argument and 941 five-argument references. At the first changed callsite,
DIRECT emits a single full-width zero store to the fifth argument's stack
slot, then calls the retained five-argument body. The forwarding frame and
partial-byte/overlapping-load sequence are absent. Object size increases from
2,916,994 to 2,920,724 bytes because argument preparation moves into callers.
Evidence: `.local/menu-preview/fpu-call-native-evidence.json`.

One clean pair used the hardware-isolated, normally remapped forward profile,
hidden/muted Emerald Coast, 1280x720, D3D11, 100%, VSync off and 144 output.
Each ran 60 gameplay seconds; first ten seconds were excluded, no IP sampler.
Both completed the planned stop without a runtime failure.

| Run | New draws/s | Output/s | Execution CPU ms/boundary | Raw cycles/boundary |
| --- | ---: | ---: | ---: | ---: |
| fpu-calls-control-ec-01 | 16.844 | 143.277 | 57.552 | 254,229,569 |
| fpu-calls-direct-ec-01 | 16.420 | 143.719 | 59.670 | 262,140,535 |

CONTROL executable:
`9ff926a70d8321523f2eaf0aaf77ddecdc26008c7459bf14f04ca82cb74feb3a`.
DIRECT executable:
`4d3af803dbbbcbab0be25faa456d9c7d61d27e226f71e949d92d5f72a598e13c`.

DIRECT is about 2.5% slower in new draws and 3.7% more execution CPU per
boundary in this pair. A moving fixture on an actively used desktop cannot
assign every difference to the call change; it nevertheless provides no
evidence for promotion. All cadence witnesses stay 50/2/2. Process CPU
(69.196 versus 68.964 ms/boundary) is nearly unchanged and must not be used
to disguise the slower execution thread.

**Decision: OFF restored, no product speedup claimed.** Do not expand this
call rewrite across the game or repeat a matrix to chase a marginal result.
The static removal is real, but its value did not survive the game check.
The experiment remains independently reproducible, disabled by default.
Restoration log: `.local/menu-preview/build-fpu-calls-off.log`, native closure
PASS, 1,910,016,000-byte executable and original selected AOT ownership.
