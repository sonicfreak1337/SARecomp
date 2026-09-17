# Steam Deck test installer, 17 September 2026

Requested complete private test installation for friends. This packages the
delivered September 17 v2 native CPU update; no separate performance patch is
required. The movement/contact replacement under development is not included.
This is a test build, not a promotion to the final release candidate.

## Distribution

`out/installers/SonicAdventureRecompiled-SteamDeck-Test-2026-09-17.run`

- 267,669,346 bytes (255.27 MiB).
- SHA-256: `e5d40fee53888b1688315aeed5cbfbb665930ea2a0cc1ed150f4ffeb0bdd7650`.
- Original English setup, user-owned PAL GDI and tracks, installation in Desktop
  Mode as the normal user, without sudo. No disc, installed original assets,
  personal saves, caches or crash reports are distributed.
- Defaults: Vulkan, fullscreen, 1280x800, native Deck aspect, Original timing,
  diagnostics OFF. Explicit existing settings and saves retain their established
  installation behavior.

Game source commit: `be39cd8dab6a57486bd0fc18e5defa6d9fef9d55`.
Source executable SHA-256:
`b5599d23ab3bcf2a0f54207409ff31109b20c84b42782b4f2036fc8c5d447ecc`.
Packaged stripped executable SHA-256:
`f7023f123e6fcc4d76361cf05eba131db8344197e38528eab5e6165e1b187f8b`.
All 27 allocated ELF sections, flags, addresses, sizes and entrypoint match the
delivered executable exactly; only non-runtime debug/symbol material is removed.

Matching optional policy-only switches are in `out/patches/`:

- `SARecomp-Diagnostics-ON-Deck-Test-2026-09-17.run`
- `SARecomp-Diagnostics-OFF-Deck-Test-2026-09-17.run`

These authenticate the stripped executable. Do not substitute the earlier v2
switches, whose different executable hash would be rejected. Diagnostics can
cost performance; OFF remains the normal setting.

## Verification and limits

The actual self-extracting installer completed a fresh full installation in the
Linux VM as UID 1000. All 25 payload files and all 2,070 files reconstructed from
the local original disc match their manifests. No diagnostics policy was
installed. Deck defaults and preservation of explicit timing/music settings pass.

The installed executable completed a hidden, muted Gamma Emerald Coast gameplay
check through frames 5..15 and stopped at the requested host boundary without
forced termination. It retained PAL 50-Hz clock, two release slots and logical
delta 2; native object, model, animation and projection paths were active. This
is an installed-product correctness check, not a full-stage test or a new Deck
performance result. The VM-only reduced test resolution does not alter setup
defaults. Supporting local reports:

- `runs/deck-test-installer-source-20260917.json`
- `runs/deck-test-installer-strip-proof-20260917.json`
- `runs/deck-test-installer-install-20260917.json`
- `runs/deck-test-installer-gameplay-20260917.json` and `.log`
- `runs/deck-test-installer-diagnostics-20260917.log`

The user's recent Steam Deck crash is unresolved: diagnostics were OFF, no
capsule or scene/transition was supplied, and play continued after saving.
Packaging this requested test build does not claim that crash is fixed.
