# Original timing performance regression

The user measured about 14 SIM FPS in Emerald Coast and 13 in Chaos 0 on
Steam Deck with **Original** timing. The new v5 installer had selected
Original for new Deck installations, as requested. That exposed a preexisting
port bug: `sonic_native_gameplay_math_active` and seven per-family default
switches were still coupled to the experimental 60-Hz implementation.
Original consequently selected slow translated owners, including palette
lighting, normals, matrix operations, collision, animation and mesh reuse.
The same erroneous policy was shared by Windows and desktop Linux.

The former v5 packages have been withdrawn from `out/installers` to an
explicitly marked internal archive. They are not release candidates.

## Correction and scope

Original gameplay now independently admits the same native owners in the
authenticated main states 4/5/9, scenes 15/16, no-transition, original 2/2 scope, with a
known 50/60-Hz video owner. This is a derived per-frame state, cleared on
restore and on another authored cadence request. It does not write guest
timing, registers, simulation data, saves or the original mode constructor.
Each native owner retains its source/memory/FPU guards and translated fallback.
Loading, movies, scripts and the private fixture keep their existing ownership.

The seven native defaults no longer ask whether Recompiled timing is enabled.
Recompiled still changes eligible scheduling to 60 Hz / 1/1; Original keeps its
authored clock and 2/2 pair. The diagnostic retained-math override allows
comparisons within one executable without changing the selected clock.
Benchmarks can now select Original on Linux, wait for the corresponding
gameplay state and report whether native math is actually active.

Windows was rebuilt in five incremental steps; Linux in four. No guest AOT
export, baseline replacement or old Katana change was required.

## Actual-game evidence

All probes use hidden/muted, isolated input and separate user data. Windows
uses 1280x800/100%; Linux uses 800x500/50% with llvmpipe in QEMU TCG. There is no
physical Deck measurement of the corrected code yet.

| Current Windows executable, Emerald Coast | New draws/s | Execution CPU ms/title boundary |
| --- | ---: | ---: |
| Original, retained math, Vulkan offscreen | 19.3370 | 50.3125 |
| Original, native math, Vulkan offscreen | 25.0016 | 30.3125 |
| Original, native math, D3D11 | 24.9986 | 30.1042 |
| Recompiled, native math, Vulkan offscreen | 57.0401 | 16.7708 |

Windows probes cover boundaries 30 through 180 and complete normally
(exit 1 / HostDeadline 2). Original reports 50 Hz / release 2 / delta 2
throughout; Recompiled reports 60 Hz / release 1 / delta 1.
The native runs report actual admitted palette/normal/matrix/collision owners.
The original v5 binary also reproduces the slow Windows path: 19.1048 new
draws/s, 50.9375 ms CPU/boundary, with all native math disabled.

| Same current Linux executable, Original, frames5..15 | Retained math | Native math |
| --- | ---: | ---: |
| New draws/s | 0.392449 | 0.733514 |
| Execution CPU ms/frame | 1055.13352 | 692.44988 |
| Process CPU ms/frame | 3587.94749 | 2670.03041 |

Both Linux probes complete at the exact requested window and normal
HostDeadline. The native run reduces execution CPU/frame by 34.4%; the VM's
software renderer and emulated CPU make its absolute FPS unsuitable for Deck
claims. These bounded moving-stage runs are not identical world-state
trajectories: the authored original update loop can perform additional work
when a slow host misses its timing. The counter/cadence evidence demonstrates
the policy defect and its removal, not a new per-instruction equivalence proof.
The existing guarded native implementations and their differential checks
were not changed.

One preliminary Linux harness lacked the installed menu assets and exited at
`menu-background-identity` before gameplay. The corrected harness supplies
the same installed assets/resources for both measurements; this was not a
runtime failure caused by the math correction.

Evidence: `runs/{original-math-*,recompiled-math-*}-20260915`,
`.local/menu-preview/linux-original-{retained-v2,native-v1}.log`, and the
owned VM's `/home/sonic/timing-math-fix-v1`.

## Unleashed Recompiled comparison

The current upstream revision is still
`cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c`, matching the earlier research.
Its [recompiler configuration](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecompLib/config/SWA.toml)
enables register localization and redundant ABI-save elimination independently
of the user's FPS setting. Its [application update](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecomp/app.cpp)
handles elapsed time separately, and its
[frame-rate patches](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecomp/patches/fps_patches.cpp)
retain fixed-rate behavior where needed. Its
[swapchain wait](https://github.com/hedge-dev/UnleashedRecomp/blob/cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c/UnleashedRecomp/gpu/video.cpp)
is consumed once through a pending-wait flag.

The relevant correction here is to separate execution efficiency from timing
policy. Our retained register localization and Vulkan resource reuse remain
active; no foreign gameplay constants, unverified timing patches or different
project's FPS results were substituted for Sonic Adventure evidence.

## Patch safety

The separate Linux/Deck patch requires the existing v5 program as its binary
delta reference. It includes its own Zstandard 1.5.7 decoder, built for the
existing glibc 2.31 floor, and needs neither sudo, GDI nor additional packages.
It updates authenticated older installed launch paths too, since an existing
Steam shortcut may still target a previous version directory. Each complete
new program is SHA-256 verified before publication. Original program and
manifest backups are retained, and files are replaced atomically.

Seven fixture checks cover both launch paths, idempotence, damaged programs,
damaged deltas, missing v5, a running game, publication failure/rollback/retry,
and recovery after an interrupted manifest publication. Saves, Chao markers,
settings and the existing shortcut remain unchanged. An initial repair test
found a same-inode rename failure; repair now publishes only the missing
manifest. All checks pass after that correction.

The complete final patch also passed against the actual installed v5 Deck
program and an older installed Steam launch path in the VM. Both programs
match the new Linux SHA-256, both original program backups match their old
identities, and existing settings and Story/Chao markers remain unchanged.
This test uses the complete self-extracting artifact, not only fixture data.
The patched older Steam launch path also completes hidden Linux Emerald Coast
with Recompiled timing, native math active, release/delta 1/1 and normal
HostDeadline termination. It uses the retained older SDL/media dependencies.

## Replacement artifacts

The delivery directory `out/installers` contains only the corrected packages.
The old v5 files are retained solely in
`out/withdrawn-installers-v5-timing-bug`, marked `DO-NOT-DISTRIBUTE.txt`.
Package audits report no original content, personal saves or development files.
The authoring tool rejects known withdrawn Windows/Linux program hashes,
including `--reuse-stage`; rejection was checked against all three v5 stages
without writing any replacement archive.

| Edition | Filename | Bytes |
| --- | --- | ---: |
| Steam Deck | SonicAdventureRecompiled-SteamDeck.run | 267,244,722 |
| Linux x86-64 | SonicAdventureRecompiled-Linux-x86_64.run | 267,244,470 |
| Windows x64 | SonicAdventureRecompiled-Windows-x64.exe | 247,805,616 |

Artifact SHA-256:

- Deck: `e1ca3381e6d30548739f4d0ef0f2b0630f49306fadb08eade427d55b8b7274c2`
- Linux: `3f8feae95f6180b20301e20eed3c143107655e4a8ea4426efb0ba90523e41463`
- Windows: `5be56f7f4727796cfbfe76d3d313ebcc9ad823eef08031427653783b76e8ca80`

Both Linux editions contain the identical corrected game:
`7d4fb2b71694dead0ae7f76f105bb975429978a7dce3ce6cad44aace220c6921`.
Windows contains:
`4522f14f8b6f4272757c84e00f2b6bb0d96832acd5011436e4964ab68782eead`.

All three complete installers passed installation over isolated existing v5
installations. The tests verified all 2,070 original files and all application
payload files (29 Windows, 25 Linux/Deck), unchanged existing settings, retained
previous content and edition defaults. Linux/Deck setup ran as UID 1000,
without sudo. The initial acceptance harness incorrectly required content
directory reuse; setup intentionally publishes a fresh verified content set.
The corrected check verifies the new content and retention of the old set.
This adjustment did not change the shipped installer or repeat extraction.
Evidence: `.local/menu-preview/installer-v6-*acceptance*` and the VM's
`/home/sonic/installer-v6/acceptance-result.json`.

Existing Deck/Linux installations use the separate
`out/patches/SonicAdventureRecompiled-v5-PerformancePatch.run` (114,115,224 bytes),
SHA-256 `079468d27d6c87d198d6e7431ea9fd8085e030e28deed1ce81ca26918d4b39f5`.
Close the game, then run this file as the normal user in Desktop Mode. No
reinstallation or original media are required. The same patch updates a
supported desktop Linux installation; it is not a Windows patch.

The original-timing issue is fixed in the common implementation. Stable
physical Deck 60 FPS and the user's specific Chaos 0 scene remain separate
hardware acceptance checks, not claimed as completed by these probes.

## Subsequent Deck feedback and delivery hold

The user subsequently measured approximately 17 SIM FPS in Emerald Coast,
10–15 in Windy Valley, 23–26 in Egg Hornet and 20 in the following cutscene.
The timing-policy fix does not resolve Deck performance. The renewed objective
is substantial further improvement, with no new patch until a large measured
frame-rate gain is demonstrated. These artifacts record the corrected-policy
comparison baseline; they do not constitute Deck performance acceptance.
