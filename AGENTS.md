# Sonic Adventure: Recompiled

This is the separate, private Sonic port project. Remote:
https://github.com/sonicfreak1337/SARecomp. The former KatanaRecomp repository
is not the development target. SA2 work is stopped.

The user accepted r354 / 0.49.9 on 2026-09-11 as the new working baseline.
All seven stories are complete; currently reported bugs are user-confirmed
fixed. Preserve that product, its AOT pack, native semantics and save data.

## Baseline protection

- `.local/baseline/r354` is an independent byte-verified snapshot, never a
  build/output directory. No edits, cleanup, hardlinks, regeneration or
  automatic replacement there. `baseline/r354.json` binds every saved file.
- The original r354 under the old Katana workspace remains another copy.
- Katana is a pinned SDK dependency. Do not edit/rebuild the old Katana tree
  as a side effect of Sonic enhancements. Any needed port-local adaptation
  must be explicit and preserve the original behavior when disabled.
- Enhancements are experimental and opt-in. Use new output/build directories
  and separate `KATANA_USER_DATA_ROOT` roots. Never write experiments into the
  baseline or the user's original save namespace. Seed test saves by copy.
- Baseline promotion is an explicit user decision, never a consequence of a
  successful compile. Keep the original presentation mode available.

## Current feature

Real 16:9 and ultrawide 21:9: Hor+ world rendering at the actual pixel aspect,
consistent render culling, undistorted HUD anchored at the actual screen
edges (explicit latest user requirement), and complete screen fades. Keep
movie aspect and title simulation cadence intact. Do not stretch the final
4:3 image or broaden gameplay activation/collision/event logic.

## Work and verification

- The next enhancement is an optional Vulkan renderer alongside D3D11, moving
  toward Linux support. Keep D3D11 and the accepted baseline available. Vulkan
  is not implemented by the CPU profiling batch; do not claim it fixes the
  measured guest-execution bottleneck without measurements.

- Work autonomously within the user's authorization; no repeated permissions.
- Prefer incremental performance builds; retain the r354 compiled AOT pack
  for adapter-only changes. Keep 144 presentation FPS independent of title
  cadence. Do not regenerate AOT for display settings.
- Use small relevant visual/boot checks, no full level matrix by default.
- The user currently uses the PC: all tests hidden and muted using
  KATANA_PORT_BACKGROUND_TEST=1; no focus, keyboard/mouse injection or visible
  game window. Obtain visual evidence through native frame capture instead.
- Before any edit, inspect the file. Source edits use the synchronous local
  `codex.exe --codex-run-as-apply-patch` endpoint. Read-only inspection may be
  delegated to existing Sage; no automatic fan-out.
- Before a retained Ninja build use the maintained Ninja recovery helper and
  its absolute bound binary. Never falsify timestamps, objects or build logs.
- Local git commits belong here. Push only this Sonic project to its verified
  private remote; never original disc images. Installed assets are authorized
  for development, but the eventual end-user package requires installation
  from the user's original media. Personal saves stay local.
