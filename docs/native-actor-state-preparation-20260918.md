# Remaining native work and actor-state preparation

The fresh Gamma Emerald Coast profile runs with all five private groups ON:
movement/contact, model submission, land display, object collision and original
camera update. This replaces the older, pre-land/object/camera distribution as
the basis for selecting further work. It establishes **no new performance gain**.

The hidden Linux VM run uses Original timing, Deck aspect and two software
raster workers. The executable is the existing camera-operation candidate,
SHA-256 `9ff104baff27941026fa52ff8197d8ff845920e4fd692764e538d2a1f3822866`.
It reaches the requested stop without forced termination. The 30-second,
99-Hz execution-thread recording finishes with exit 0 and no lost samples.
The longer frame window keeps recording separate from shutdown. The probe
now avoids copying the large private ELF into perf's build-ID cache and records
the exact recorder arguments. This does not prove why the older recorder exited 1.

There are 1,226 stack samples, 1,208 with a game leaf; one game leaf is unresolved.
The matching ELF's executable segments are byte-verified. Exact symbol-name
rows are also supported now; unknown synthesized PLT names remain unresolved.
Raw stack leaf counts reproduce the independently resolved report exactly.

| Retained owner and descendants | Samples | Share |
| --- | ---: | ---: |
| Main task traversal `0986CC` | 1,079 | 88.01% |
| Actor root `0CBD40` | 225 | 18.35% |
| Actor state root `0FDC20` | 93 | 7.59% |
| Model/motion wrapper `040942` | 131 | 10.69% |
| Land root `0519C0` | 83 | 6.77% |
| Original camera root `019F4A` | 41 | 3.34% |

These inclusive counts overlap and include necessary game work. In particular,
the two actor roots cover 318 unique samples (25.94%), of which 209 already
include native descendants. Their combined percentage is **not** an expected
speedup. The task traversal's 88% is not removable traversal overhead either.
The next substantial scope is actor state processing composed with the existing
movement and model operations, not another isolated copy or FPU microbenchmark.

## State-transfer support

The author now accepts explicitly reviewed BRAF entry sets and direct BRA
transfers to other original entries. It still rejects undeclared computed
branches. A state transfer preserves the current frame and PR; it is different
from a function call. The target is captured before the delay slot. An unknown
target resumes before either the branch or its delay instruction takes effect.
The original continuation author adds the branch itself as a restart point,
not the nonexistent call continuation at PC+4.

Indexed byte writes used by state records retain the ordinary checked store
path, including their original delay-slot provenance. Explicit original
precision-polymorphic FPU scopes can be declared and are checked against the
retained lexical guards. Read-only RuntimeOnly blocks can request an original
local resume router; existing callers retain their prior default behavior.

Seven invalid author inputs are rejected. A generated standalone executable
passes 50 cases on Windows and in the Linux VM: forward/backward targets,
unknown entries, target-register modification in the delay slot, direct branch,
delay-load faults and indexed byte writes. Register/PR results, target, write
origin and exactly-once delay execution are compared. This is a focused branch
mechanics test, not a complete actor-state gameplay comparison.

The actual `0FDC20` closure review authenticates the original source manifest,
RAM image and 32 state/helper owner definitions. All 13 signed table entries at
`0FDCB4` match the original BRAF target set. Twelve branch transfer sites, exact
FPU scopes and local fault routes now author successfully. The original
precision-polymorphic scope starts at `0FC3F0`. The resulting inventory is
`runs/native-state-transfers-20260918/actor-scope.json`.

**This actor group is not integrated into the running game yet.** Next, compose
its runtime operation with the existing native movement/model groups, preserving
real foreign callbacks. A completed state transfer must not use the ordinary
call path's PR-return check or replay its delay instruction on a child fallback.
Then compare the complete state operation against retained AOT and run a bounded
combined gameplay comparison. Do not promote it or publish a patch based on
author generation or the standalone branch test alone. Continue the larger
actor-family work, including the measured `0CBD40` chain.

## Preserved behavior and evidence

All 231 existing generated render/contact files and both complete private
original-continuation C++ files are byte-identical to the built candidate.
The gameplay binaries were not rebuilt for this preparatory tool change.
There is no new default, timing change, installer or patch. The delivered
September 18 patch remains SHA-256
`173c0754bad9b429ef5e31260d17f34a07bce1dc69b09f119bb1ed24d3c27003`.
Baseline and personal saves are untouched.

The raw profile, recorder invocation, sample reports and resolved stacks are
compressed together in `runs/all-native-profile-gamma-20260918/evidence.tar.gz`
(424,736 bytes), with individual hashes in `evidence-manifest.json`. Expand it
in that directory before using the retained resolver scripts. `overview.json`
is the compact result. The state review and both platform test logs are under
`runs/native-state-transfers-20260918`.
