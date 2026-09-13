# SUMMARY tutorial page ownership

2026-09-13. Bounded read-only source audit. Only this report was written.
No source/build/game/input/save/hardware/network changes. Existing contact
sheet and runs/tutorial-native-01/frames/frame-900.bmp were visually inspected.

## Exact seam and ownership

Display **8C901800** calls strip-list iterator **8C90176A** at 8C901890.
The iterator calls **8C07E582** at 8C9017BA, leaving **PR 8C9017BE**.
The helper resolves the current TEXLIST ordinal, constructs four interleaved
24-byte vertices, restores PR at 8C07E912 and tail-jumps to **8C63EC48** at
8C07E922. Thus the existing textured native stream family can replace these
draws using PR 8C9017BE, r4 8C798718, r5 4. r6 is the original descriptor
word 0; r7 is the original alpha mode.

The Sonic picture is a separate owner: **8C9012E8** binds its picture
TEXLIST at 8C901364 and calls 8C07EAA0 at 8C9013AE/3D0. The page panel is
separate 8C07F1E0 at 8C9018E0. Keep those pictures/panels, original animation
and progression, and the existing bottom bar PR 8C9014CE path.

Reuse tutorial-prompt-audit.md's active SUMMARY generation, exact archive/PVRT,
first-matching registered texture owner and host-resource lifetime guards.
Preserve original prepare/finish immediate state, colors, UVs, scaling,
depth and strip order. Change only authorized input-bearing host images.

## Exact table and live page

Initialization 8C900BE4 calls **8C0584E6**, the current-character getter
already byte-checked by the adapter against literal **8C78B39D**. Its return
supplies the table slot: 0 Sonic, 1 Tails, 2 Knuckles, 3 Amy, 4 Big, 5 E102,
proven by table archive names. Do not import another character enumeration.

Guest text language is u32 **8C754B3C**. The language/character root is
`u32(8C90BBA8 + language*4) + character*24`. Exact language bases are:

| Value | Archive language | Base |
| --- | --- | --- |
| 0 | Japanese | 8C90B968 |
| 1 | English | 8C90B8D8 |
| 2 | French | 8C90B9F8 |
| 3 | Spanish | 8C90BB18 |
| 4 | German | 8C90BA88 |

Each 24-byte character root contains:

| Offset | Type | Meaning |
| --- | --- | --- |
| +0 | u16 | Page count |
| +2 | u16 | Page threshold used for picture mode |
| +4 | pointer | Array of 20-byte page records |
| +8,+12 | pointers | Two TEXLIST slots |
| +16,+20 | pointers | Corresponding archive stem strings |

8C901660 loads the nonnull TEXLIST/name pairs through 8C099690.
8C9019E0 writes the chosen root to page work1+16 and work2+16.
Display 8C901800 reads work1+16. Exact live task chain:

```
controller = u32(8C90E1F4)
controllerWork1 = u32(controller + 32)
pageTask = u32(controllerWork1 + 20)
pageWork1 = u32(pageTask + 32)
pageWork2 = u32(pageTask + 44)
root = u32(pageWork1 + 16)
displayedPage = u32(pageWork2 + 8)
pendingPage = u32(pageWork2 + 12)
page = u32(root + 4) + displayedPage*20
```

Use displayedPage: 8C9018FC changes +8 after the old page scales out.
8C901BAA/BB0/BB6 read current, set current and set pending respectively.
8C901A90 skips pages whose event condition is unmet. Enumeration does not
authorize bypassing that original progression.

A 20-byte page has u16 base x/y at +0/+2; event condition +4 (FFFF means
unconditional; others tested through 8C0905CE); panel width/height +6/+8;
unclassified +10; two strip-list pointers +12/+16 paired with root TEXLISTs
+8/+12. A strip list has s16 count followed by six-byte records
`{s16 ordinal, s16 xOffset, s16 yOffset}`. Verified ordinals are nonnegative.

8C90176A computes x=page.x+xOffset*scaleX, y=page.y+yOffset*scaleY.
pageWork1+44/+48 hold scaleX/Y; +40 holds z, with -10 added for strips.
Default z is 10000. 8C07E582 uses full UVs and selected texture dimensions.

At the native leaf the restored iterator registers additionally retain:
r9=root; r10=root+12 for the first TEXLIST or root+16 for the second
(postincrement); r12=strip-list; r13=strip index;
r14=strip-list+2+index*6; r11=8C07E582. These are exact row witnesses to
validate alongside the live task chain and registered texture authority.

## Sonic English first page

Root **8C90B8D8**: count 7, threshold 5, pages **8C90A268**, TEXLISTs
null/**8C909B0C**, archive stems null/**tutomsg_sonic_e**.
Page 0: x/y 210/96, condition FFFF, panel 390x144, strip lists
null/**8C90A118**. That list has five entries:

| Index | Ordinal / asset | Offset | Stable guest x/y | Original meaning |
| --- | --- | --- | --- | --- |
| 0 | 0 / padmanu | 0,8 | 210,104 | Dreamcast diagram and connector lines |
| 1 | 1 / sprf_01_e | 136,0 | 346,96 | LR: Rotate Camera |
| 2 | 2 / sprf_02_e | 136,24 | 346,120 | Stick: Maneuver Character |
| 3 | 3 / sprf_03_e | 136,64 | 346,160 | X/B: Spin Dash |
| 4 | 4 / sprf_04_e | 136,96 | 346,192 | A: Jump Attack |

Stable depth is 9990. Ordinal 0 is 128x128, 1-4 are 256x32. This matches
the existing native capture. The connector lines are baked into padmanu,
not separate guest primitives; account for endpoints when replacing it.

TUTOMSG_SONIC_E.PRS encoded SHA256 is
`b7b0305bc9b9d2828c82ae43e823d56a4900d0ad7df93ee339150a534eb1ee2c`,
80577 bytes, 32 entries. padmanu PVRT SHA256 is
`77777f20e3d8dd83db09e6e25163251f43d08ac5af0fa8c466f7a47928e7805b`.
Use each ordinal's exact catalog identity. padmanu and Sonic TEXLISTs are
shared across other character roots; archive/GBIX alone is insufficient.

The bounded remaining English Sonic enumeration is:

| Page | Page record | Strip list | Ordinals |
| --- | --- | --- | --- |
| 1 | 8C90A27C | 8C90A138 | 5,6,7,8 |
| 2 | 8C90A290 | 8C90A152 | 9,10,11,12,13 |
| 3 | 8C90A2A4 | 8C90A172 | 14,15,16,17,18 |
| 4 | 8C90A2B8 | 8C90A192 | 19,20,21,22,23,31 |
| 5 | 8C90A2CC | 8C90A1B8 | 24,25,26 |
| 6 | 8C90A2E0 | 8C90A1CC | 27,28 |

The inspected contact sheet shows input-bearing strips 6,10,12,15,17,
20,21,22,23 beyond page-0 ordinals 0-4. Other listed strips are headings,
generic prose or empty artwork; retain their pixels. Mixed prose/input
strips 17/23 require preserving their meaning when rerasterized. Page 2/3
event conditions are 008D/008E. All other characters/languages are enumerable
through the same roots and record layouts without scanning game code.

## Evidence and limits

SUMMARY.bin SHA256 checked:
`93969e279339fd1a9687e2cee7d842e544a41eb777283f2073a04adf1d531949`.
Binary offsets equal guest address minus 8C900000. Source disassembly is
the supplied private/diagnostics/r330-progress-graphics-timing-20260909a/
SUMMARY.disasm.txt. Resident disassembly is private/analysis/
sonic-adventure-pal-v1003/r289-primary-coverage-work-v1/primary-00000000.disasm.txt.
All 466 words of 8C07E582..8C07E926 were compared with original
postpal-main-ram-native-ready.bin; interval SHA256:
`5f10d8bf389170b5e3c9e199a2bb08a05b4c3bded3d62a813ac3c11c35e79863`.
Literal range 8C07EA60..8C07EA78 SHA256:
`0c06d98b58037ef0775c39b9510379b1025a91afb755169dfed4ca028894835a`.

The static seam and table ownership are closed. No new live capture proves
these register/task-chain guards yet. No controller, full-language,
full-character, transition or re-entry test was performed. This does not
prove the logical action contract of every later instruction or camera
mode; resolve those through the input/gameplay owner before labeling them.

## Complete TUTOMSG control-region inventory

The additional bounded asset audit decoded all 30 character/language
archives with the existing decode_pages.py and bundled Pillow runtime.
All 785 entries were checked against catalog ordinal, dimensions and full
PVRT SHA256; all encoded archive SHA256/lengths also matched. Every contact
sheet was visually inspected, including non-strip artwork. No prose was
translated or rewritten. Metadata is in
`.local/analysis/tutorial/control-spans.json` (schema version 1).

The JSON contains 272 span-bearing entries and **327 non-overlapping
regions**: A 90, B 35, X 144, Move 42, Camera 10, Diagram 5 and ShoulderPair 1.
No Y icon occurs outside the five diagrams; tiny padmanu markings are covered
by their whole Diagram rectangle. Each archive binds encoded SHA/length,
and each selected entry binds ordinal, name, PVRT SHA, dimensions and
original-pixel `{token,x,y,w,h}` regions. Bounds are half-open. Neighboring
slashes, colons, plus signs and prose remain outside the semantic regions.

Sonic English opaque icon cores supply the shared A/B/X/Move templates.
Tolerant matching accounts for compositing over blue highlight backgrounds
and small antialias variations: maximum accepted RGB mean errors were
A 6.061, B 1.392, X 2.557, Move 4.017 on a 0-255 scale. All matches passed
the tighter anchor/core tests; no unaccounted strong red/yellow/blue/green
glyph pixels remained in any 32-high strip. Per-span match errors are
retained in the JSON. Visual contact-sheet inspection found no additional
unmatched grayscale-stick or colored-button symbols. Tall Big cast gauges
depict fishing reels, not controller buttons. No catalog mismatches or
overlapping/out-of-bounds rectangles occurred.

Language rows cannot share English ordinal assumptions: French/Spanish
Sonic's Shake stick is ordinal 31; Japanese Amy has a second Move symbol in
ordinal 12. Original Move always means the movement-stick input, including
aiming, rotating, rod manipulation and pushing down in fishing instructions;
do not relabel it as the camera stick.

The ten Camera regions cover only literal LR in Sonic/Tails first-page
Rotate Camera strips. One further written control label exists: Japanese
Big ordinal 32 `bprf_42`, region x=3,y=5,w=27,h=17, means LR to exit fishing
mode. It is separately tagged **ShoulderPair**, for the original gameplay
LeftTrigger/RightTrigger pair, and must not receive a right-stick camera
replacement. No other standalone written button labels were found in the
30 inspected sheets. This distinguishes visible input identity; the parent
integration still owns runtime action behavior and camera-mode decisions.

JSON SHA256:
`9a3ed301212327ef9740c9f0ce4ab7f8e264a80c80174b4fd039f02b4c9bea49`.
This is static asset coverage, not a new live runtime or page-navigation
test. Original blue highlight pixels surrounding each glyph must be
preserved when the integrator composes replacement images.
