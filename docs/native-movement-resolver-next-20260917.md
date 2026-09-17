# Next native boundary: movement and contact resolution

Read-only scope after the object activation/lifetime work. No movement, collision,
game cadence or installed September 17 update is changed by this investigation.

The next substantial semantic owner is `8C073018`, with its main body after the
short level/state selection at `0730A0`. The retained containing unit is
`unit-v8C073018-8C073018-457a60bdbd02a56b.cpp`, 3,484,616 bytes, SHA-256
`f18e090b5b8b0f6899c122dc28234f7226464391c7e12339b702d3e9c30caf92`.
Its last retained instruction/resume address is `074210`; the next callee starts
at `074214`. Canonical/P2 entries are aliases, not different algorithms.

Existing evidence:

- Historical Linux Gamma profile: 9.99% inclusive in
  `runs/linux-gamma-original-profile-20260915/perf-families.txt`.
- Windows active chains: 17/256 include this owner in
  `runs/active-chain-gamma-20260916/aggregate.json`.
- Post-update Linux: 33/4,023 exclusive samples in
  `runs/native-cpu-current-linux-gamma-perf-symbols.txt`.

These profiles use different builds and hosts. Inclusive work includes already
native descendants and cannot be added to their savings or presented as a
forecast. The large main body still performs multi-stage position/contact
resolution, vector work, and position/angle/status updates through retained
AOT. The semantic name is inferred from its source accesses and callees.

The containing unit directly references these distinct retained callees:
`028BFE`, `029B00`, `049A4E`, `04F7E0`, `055C9A`, `055CEC`, `06CC62`,
`074214`, `074712`, `074C30`, `074E24`, `074FAE`, `075100`, `075154`,
`075356`, `0757A0`, `075980`, `078DF0`, `078F40`, `10CF98`, `10D038`,
`638FD0`, `63A69C`, `63A88C`, `63FFC0`, `640068` (all under `8C`).
This inventory is not a proved full indirect closure. Native TOUCH, triangle,
matrix and vector work already cover some descendants. Preserve those routes
and real external boundaries; replacing only the tiny initial selector is not
the intended optimization. Reconstruct the complete resolver's live state,
RAM effects, rounding and callback order before choosing a native boundary.

A smaller alternative is `028EC2` with `052518 -> 028B00 -> 0287A0`: selection
and maintenance of active collision-object lists, recursive hierarchy transforms
and collision-polygon construction. Historical evidence is 2.84% inclusive Linux,
11/256 Windows chains, and 74/4,023 newer Linux exclusive samples across these
families. This list construction differs from the rejected NEAR query replacement
(`028BFE/0522C0`) and existing native TOUCH consumer (`029B00`). No measured gain
or completed implementation exists for either candidate.
