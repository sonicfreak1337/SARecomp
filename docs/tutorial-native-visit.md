# Original tutorial visit and remapped navigation

The private `tutorial-sonic` scenario now exposes a proven original SUMMARY
entry. It requires ADVERTISE main state 11, its exact active cleanup owner,
and current character 0. It does not set story progress or select another
character. Existing credits entry and cleanup semantics are retained.

Original result 104 branches from 8C054054 to 8C054176. That branch calls
ADVERTISE cleanup, then writes main state 18. The main jump table at
8C053BA8 selects 8C054392 for state 18. It loads `summary.prs` normally and
calls header +0x0C, 8C900B60. Subsequent state 19 owns tutorial updates.
The diagnostic provider reproduces the cleanup and state publication only;
it never calls a PRS initializer without its original loader.

Original boot SHA:
`b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af`.
Transition bytes 8C054176..8C054182:
`7750405ee77d4a6eb5e1bebf03639e8d041c62b9298e029d0fafb7f3c053593c`.
Loader bytes 8C054392..8C0543A6:
`30bcab2beae3ef49913bc388faa5b993b287bf36223603a76aff6d5ea2443b29`.
These are additional identity/ABI guards, not new general SDK address rules.

`tools/test-tutorial-runtime.py` uses copied saves, a fresh settings root,
hidden/muted Vulkan and a 40-second host deadline. No physical input or
replay is involved. Both `runs/tutorial-native-01` and
`runs/tutorial-native-jp-01` pass the original state transition, live texture
provenance match and absence of recorded runtime failures.

The first run deliberately maps gameplay A to Triangle and B to Square.
Native `frame-900.bmp` visibly shows Triangle:Next / Square:Back. The second
uses keyboard prompts and Japanese host text; Space and B, plus Japanese
Next/Back labels, are visible and legible. Windows names Space as LEER on
this host. Both private entries load the original English artwork (375-pixel
visible bar); the second run is not proof of the Japanese 352-pixel archive
or of the normal save-language merge. Separate raster tests cover both widths.

The captures also confirm the remaining scope: the original character
controller diagram still depicts Dreamcast buttons. This report closes the
common navigation bar's actual-game match, not every baked tutorial page.
No complete tutorial page-navigation or re-entry sequence is claimed.
