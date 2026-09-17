# Next native boundary: object activation and lifetime

Read-only scope investigation after the model transaction work. Nothing in
this document changes object activation, gameplay distance or the delivered
September 17 update.

The existing execution-thread profile contains three related generated bodies:
`8C0912C0` (1.29% exclusive samples), `8C091928` (1.14%) and `8C09105A`
(0.895%). These are separate samples, not inclusive call-tree costs. A large
global saving is not established by those percentages.

Inspection of the PAL bytes identifies a concrete shared operation rather
than another generic register/cache experiment:

- `09105A..091087` reads a position and tests squared distance against FR7.
  It exits early after X, XY or XYZ. The last two additions use FMAC. Preserve
  the exact early-exit FR/R4/FPUL/FPSCR/T state, rounding, exceptional values
  and authored read-ahead (Z is read in the first branch's delay slot).
- `0912C0..0914E1` selects the current reference position, then walks the
  16-byte records at `8C79A320` using the count at `8C79A31C`. It filters
  record/type flags, selects the distance limit and calls `09105A` for eligible
  entries. Positive results call `09846E`, the already identified loaded-event
  callback registrar, and initialize the returned task's transform/state.
  Records marked with saved state use `record+12` and can call `098A82`.
- `091928..0919D1` tests a live task against up to three reference positions,
  using the same distance owner, and can set its callback at `task+16`.
  The third reference has its own status byte and distance constant.

This is consistent with the common SET-object activation/lifetime layer;
that semantic name is an inference from its accesses and registrar consumer.
Do not call it a proved collision broad phase or remove distant objects from
the iteration using an unreviewed spatial cache.

The useful next implementation is a typed native distance operation plus
complete activation/lifetime owners. Within each callback-free stretch,
admit RAM once and iterate normally in C++. Retain task creation and cleanup
calls at their original points. Reacquire memory/observer capabilities after
each such call and reread the same mutable fields that the original reads.
Do not capture the entire table across callbacks, restart the original owner
after mutation, change activation radii, or flatten arbitrary task callbacks.

An independent original-byte oracle should cover inactive and live records,
all early distance exits, signed counts, saved-state records, constructor
failure and callback mutations. Start with the same CPU/RAM/observer contract
as the existing native owners. Only then compare actual Gamma gameplay and
a boss; do not infer savings from instruction count alone.

Source witnesses: retail RAM at
`.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin`;
`src/main.cpp` names registrar `8C09846E`; the profile and source mapping are
in `docs/native-cpu-postupdate-profile-20260917.md`. `0912C0` also needs the
literal island `091580..09158B`; `091928` needs `091A00..091A17`. Verify the
complete source closure and all callees before producing a replacement.
