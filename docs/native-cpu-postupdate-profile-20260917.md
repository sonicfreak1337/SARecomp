# Execution profile after the native CPU update

This is a source for the next optimization, not an additional change in the
September 17 patch. The profiled executable is the installed Linux candidate
`7dbdf58e16dc910d4f1b39998e7557851106b5c8d18da8bbfb80453f9a137869`.

Gamma Emerald Coast, Original timing, 800x500 / 50% software rendering,
normal-start native defaults, diagnostics/provider telemetry off. The owned
test VM has four vCPUs, 6 GiB RAM and `LP_NUM_THREADS=2`. This differs from the
two-vCPU before/after comparison; no throughput comparison to those runs is
valid. A 199-Hz `cpu-clock:u` sampler observes only the execution thread for
30 seconds. The game completes its 5..45 window and normal diagnostic stop.

The profile contains 4,023 samples with none lost, spread across 929 symbol
rows. Grouping exact `fn_<source>_runtime_entry` families yields 2,263 samples
(56.25%) across 343 generated guest-function bodies. This share includes real
game calculations as well as their compiled representation; it is not all
removable overhead. The largest generated family, `8C0912C0`, has 52 samples
(1.29%). The largest individual symbol is memcpy at 2.04%. FPU binary arithmetic,
dispatch, address translation and memory/observer helpers are also distributed
among many small symbols. The sampling noise does not support precise savings
forecasts from an individual one-percent symbol.

The existing 41 prepared RAM-region units define 894 functions. Of the sampled
generated-body work, 1,506 samples (66.55%) fall inside those units; 757 are
outside. Membership does not establish that the fast prefix was admitted or
what proportion of a body executes inside it. Therefore neither adding all
remaining units nor repeating an already rejected register-ABI experiment is
justified by this profile alone. The generated bodies already use a
`NativeAotRegisterFile` with register masks; adding generic register caching is
not a new finding.

The next bounded investigation should locate remaining work within these
already-hot bodies and distinguish actual prefix execution, fallback and
instruction/exception bookkeeping. Retain the current checks, baseline and
matched CPU/RAM oracle while investigating broader operation fusion. The
separate instruction-accounting audit documents its functional consumers.

Evidence:

- `runs/native-cpu-current-linux-gamma-profile-summary.json`.
- `runs/native-cpu-current-linux-gamma-perf-report.txt` and `-perf-symbols.txt`.
- `runs/native-cpu-current-linux-gamma-profile-groups.json`.
- `runs/native-cpu-current-linux-gamma-region-coverage.json`.
- `build-linux/generated/region-writes/preparation.json` and its 41 prepared files.

The source mapping uses actual function definitions, not filename address
ranges. No additional game source, timing policy or release flag was changed
for this investigation.
