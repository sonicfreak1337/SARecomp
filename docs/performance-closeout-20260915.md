# Performance round closeout, 2026-09-15

The user requested stopping after this round. The profiling work ended without
a remaining performance experiment or a newly produced installer. The user then
explicitly requested updated installers before stopping; that packaging and
installation validation is recorded in `installer-v5-20260915.md`.
The larger Steam Deck performance objective remains unresolved. New Deck
installations still default to Original timing; existing explicit settings
and saves are preserved.

## Final diagnostic profile

`runs/global-current-profile-20260915` exercised Gamma Emerald Coast with
Recompiled timing, Vulkan offscreen rendering at 1280x800, 100 percent render
scale, isolated input and the retained mesh/corner optimizations. The run was
hidden and muted, used copied saves, completed its normal 60-second gameplay
window with stop reason 2, and reported no runtime failure or forced stop.

Executable SHA-256:
`0e6fdeca6de6eee7cbfff73b7792515776860a188dab7e5cdab0b43f89a4ab8f`.
The resolver verified the matching executable, linker map and execution thread.
The sample contains 1,943 observations over 30,014.2 ms, with 127.367 ms total
suspension and 1.7301 ms maximum suspension. Module attribution: game 1,646,
ntdll 193, VCRUNTIME 99 and MSVCP 5. These are wall-sampled instruction locations,
not exclusive CPU percentages or invocation counts.

Remaining game locations include FPU arithmetic, immutable-write range checks,
model corner building, dispatch and coverage admission. The 32 bounded external
stacks include audio-command acknowledgements, title-deadline waits, comparison
or copy callers and allocation. Nearest DLL exports and compiler EH labels do
not establish that C++ exception handling is a bottleneck. This profile provides
no basis for deleting correctness checks or changing audio observation points.

The instrumented run recorded 50.263 new draws/s and 18.310 execution-thread CPU
ms/title boundary. This is neither a paired improvement measurement nor a Steam
Deck result. The actual Linux guest flags were also checked: Release, effective
`-O2`, strict floating-point behavior; no accidental unoptimized build was found.
No additional production optimization was promoted from this diagnostic round.

## Retained outcome and limits

The verified model projection and RAM-copy experiments were retired for lack
of a useful measured gain; see `model-projection-experiment-20260915.md`.
The current candidate retains the fixed-frame benchmark support, not those
experimental game paths. The accepted user executable was not replaced.

The completed size work is in `package-compression-20260915.md` and
`performance-media-20260915.md`: about 49.5 MB less across measured compressed
Linux package components, including 6.54 MB from the latest lossless dictionary
change. This is component evidence, not a measurement of a newly built installer.
The requested large performance gain has not been demonstrated. The subsequent
installer request superseded the earlier packaging gate, not that limitation.

After the separately requested installer rebuild, work stops at the user's
request. Resume only on a new user instruction; do not interpret the unfinished
performance objective as permission to continue past this stop.
