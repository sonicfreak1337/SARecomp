# Binding-aware original tutorial pages

The native tutorial now updates its controller/keyboard diagram, camera and
movement labels, and baked gameplay button symbols. It follows the active
Xbox, PlayStation or keyboard presentation and the actual gameplay bindings;
the host menu's separate Confirm/Cancel actions are not substituted.

The source audit covers all 30 character/language TUTOMSG archives and all
785 entries. The authored catalog binds 327 control regions in 272 entries:
A 90, B 35, X 144, movement stick 42, camera 10, device diagram 5 and one
independent shoulder-pair instruction. No standalone Y icon occurs. Exact
archive and PVRT hashes, ordinals, dimensions and non-overlapping rectangles
are retained in src/sonic_tutorial_control_spans.inc. See the page-owner audit
for the original binary/table evidence and matching method.

Original prose is read from the installed, identity-checked archives. Only
the measured control regions are drawn anew. Longer labels shift the original
following pixels into available transparent space; overflowing lines contract
within their original quad. Glyphs are supersampled and resolved back to the
original strip dimensions so ordinary text retains its original filtering.
The device diagram is a separate high-resolution image. Character pictures,
panels, original music, page animations and event-gated progression remain
owned by SUMMARY. No source archive or save is modified.

Original-camera LR labels show the mapped shoulders. Recompiled-camera labels
show the actual camera stick, mapped look keys or enabled mouse; swapped sticks
are honored. Movement instructions always show movement input, including Big's
fishing aim. Japanese Big's fishing-exit LR remains a shoulder instruction,
even with the Recompiled camera. A slash denotes independent alternatives,
not a newly invented simultaneous button chord.

## Runtime ownership and lifetime

Replacement is admitted only at the SUMMARY strip leaf with PR 8C9017BE,
source 8C798718 and four vertices. Guards verify the exact active module and
generation, current language/character page root, displayed page, strip list
and iterator record, active TEXLIST and ordinal, first registered descriptor,
live token/generation/handle, archive identity and complete PVRT identity.
The existing immediate draw transaction, vertex geometry, UVs, colors,
depth, sampler and guest texture publication remain original.

At most two CPU source archives and 64 replacement textures are retained for
one tutorial generation/settings tuple. Rebinding, glyph/device style, camera
configuration and module-generation changes retire the old owned resources
through the renderer's existing deferred retirement path. Teardown releases
them with the other dynamic surfaces. Missing authority or allocation failure
retains the original drawable; it never aborts a working tutorial.

## Verification

- Component test decodes all 30 installed archives and rasterizes all 272
  affected entries, cycling Xbox, PlayStation and keyboard layouts. It checks
  owner/payload rejection, source dimensions, remapped gameplay bindings,
  swapped sticks, fishing shoulders, independent shoulder alternatives and
  exact untouched source pixels. Representative PNGs were inspected.
- The existing bottom-bar tests retain five languages, three styles and both
  cropped widths, including long labels and unbound-B fallback.
- runs/tutorial-pages-01 passes a hidden/muted original Sonic tutorial visit.
  The first page binds all five expected entries. The capture shows Triangle
  for remapped Jump and Square for remapped Back/Spin Dash.
- runs/tutorial-pages-keyboard-02 passes the same live owner chain and shows
  WASD, Space (named LEER by this Windows keyboard layout), X and B. The final
  small presentation correction uses a slash between alternative shoulders;
  the earlier captures display a plus there.

All game runs use copied saves and planned diagnostic deadlines; there was no
physical input, sound, window focus or user-save mutation. These runs prove the
Sonic English page owner's integration. Other character/language artwork is
covered by the source inventory and raster checks, not by a full gameplay
matrix. Complete page-navigation/re-entry and physical controller delivery are
not claimed. No new replay system or gameplay FPS change is part of this work.

Retained Ninja -j2 build: .local/menu-preview/build-tutorial-page-final.log.
The link audit passes at 1,910,005,248 bytes; no frozen AOT unit was compiled.
The final component log is .local/menu-preview/tutorial-pages-final.log.
The quick protected executable/AOT-metadata integrity audit passes.
Adapter source digest:
0876f6205698df02ac4f367ff9d6d2e57a1b0c05f047fd22c64ce47f3aff10d0.
