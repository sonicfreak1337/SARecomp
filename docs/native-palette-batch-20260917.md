# Bounded native palette-lighting batch

The qualified batch is enabled at normal startup for both timing modes.
Internal `SARECOMP_NATIVE_PALETTE_BATCH=0` retains the previous complete owner.
PAL owner 8C037350 already admits its complete RAM footprint and has no guest
callbacks. Its new optional arithmetic batch evaluates four normals per SIMD
group, preserving the exact ordered multiply + three FMAs of FIPR, then the
separate FMUL and FADD. All original palette lookups, ordered stores, register
end state and even-count normal read-ahead remain in the qualified owner.

Admission requires AVX2/FMA with OSXSAVE, DN, the original two rounding modes,
no enabled FPU exceptions, sticky Inexact already set, bounded finite normals
and transformed light (absolute value <=16), and scale magnitude 1–256. These
bounds exclude arithmetic overflow, FMUL/FADD underflow and integer overflow;
the only possible arithmetic flag is already sticky. Final FTRC clears Cause.
Inputs outside this subset retain the complete existing arithmetic. Nothing
uses fast-math, reassociation, approximate reciprocal or dropped RAM writes.
The separate SIMD translation unit is only entered after runtime CPU checks.

Windows and Linux component tests pass 2,000 batches / 72,720 normals across
both rounding modes and register banks, signed zero, subnormals, cancellation,
positive/negative scale and tail lengths. Floating results, converted integers
and FPSCR match the retained runtime bit for bit; 12 rejected contexts preserve
guest and output state. Windows whole-owner comparison with original PAL SH4
bytes passes 488 cases, exercising 86 batches / 2,106 normals and comparing all
architectural state, RAM and ordered stores.

The first revision replaced only per-normal arithmetic inside the original
paired loop. Its matched Windows run saved only 0.46% CPU and slightly increased
cycles, so that micro-result did not qualify a useful gain. The final revision
also closes the complete publication loop when the authenticated product write
capability is available: all admitted color words are written in their original
order, and the exact final CPU registers are published once. Unknown observers
retain the original paired loop and its individual visible writes. Both odd and
even counts preserve their distinct pointer/register tails and normal read-ahead.

Windows and Linux complete-memory comparisons pass 588 cases across all five
native families. They exercise 48 closed palette loops and 2,188 direct words,
comparing all CPU state and the entire 16 MiB RAM image, including observer
revocation. These extend the original-PAL arithmetic/owner checks above.

Windows executable `986583e713324466da1a13a5d72aedb1ebb771fb84d6a0c8c8bdb4ed4e6d3bff`
completed matched Gamma Recompiled windows 5..1605. Palette ON versus OFF costs
14.423828 versus 14.750000 execution CPU ms/image (-2.21%) and 63.159689 versus
64.792254 million execution cycles/image (-2.52%). Both execute 1,688 updates,
end at tick 1,814 and HUD 10,070, and match final XYZ bits exactly. Presentation
is 56.44 new images/s in both hidden offscreen runs. The ON run executes 476,680
closed batches / 16,954,741 normals, with 52 admitted-owner arithmetic fallbacks.

Evidence: `runs/palette-batch-{win,linux}-test-a.log`,
`runs/palette-batch-win-owner-test-a.log`,
`runs/palette-batch-closed-{win,linux}-test-a.log`, and
`runs/palette-closed-win-{on,off}-gamma/result.json`.
Linux game comparison is recorded separately; component parity and desktop
CPU savings are not a Steam Deck frame-rate claim.

The first Linux Gamma Recompiled pair (same SHA-256
`e3aeb9e3560478545703d76a50775d617c5fa0eb7aadf2ccf1f726568ecf1572`)
completes 20 updates in each mode. CPU/update is 408.637335 ms ON and
483.573765 ms OFF; image throughput is 0.277543 versus 0.253563/s. Both match
starting/final XYZ bits and HUD exactly, with a one-tick absolute start offset.
Both exit through the expected host deadline with no forced stop. However,
the host compiled during the OFF window, so this is provisional VM evidence,
not a qualified percentage gain. The delivery comparison uses a quiet host.
Evidence: `runs/palette-closed-linux-{on,off}-gamma-command.log` and VM probe
summaries under `/home/sonic/preloaded-v1/probe-palette-closed-{on,off}-gamma`.
