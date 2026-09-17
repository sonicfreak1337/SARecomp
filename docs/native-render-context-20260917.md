# Complete native render-context owners

The internal `SARECOMP_NATIVE_RENDER_CONTEXT=1` experiment replaces PAL
`8C605CEC` (capture) and `8C605D4A` (commit) with complete semantic C++
operations shared by Windows and Linux. It now defaults ON in the September 17
native CPU candidate; `0` retains the old owner for internal diagnosis.
These routines copy the renderer header and selected packet cursors around
model drawing. They contain no floating-point operations or guest callbacks.

The retained AOT unit gets two entry bridges; interior resume addresses and
rejected contexts retain its original body. The return passes through the
existing AOT epilogue with the exact RTS source and `pc=pr`. The preparer
authenticates the whole retained source manifest, both owner bodies and any
preceding RAM-region preparation. Three code/literal ranges are independently
authenticated against the PAL RAM image. No pinned SDK or baseline is edited.

Each call admits the complete source, destination and control ranges before
mutation. Source code, pointer slots and branch flags cannot overlap writes.
Data buffers can overlap: live loads and stores retain the original order.
The registered product observer allows one whole-operation RAM capability;
unknown stable observers receive all individual writes through Memory.
General observers, active traps, watched memory, missing permissions and
unqualified translation contexts fall back before mutation. An exception
after mutation propagates out; it never restarts the original owner.

`tools/test_render_context.cpp` executes the original PAL bytes as its oracle.
On Windows, 192 cases match the complete architectural CPU state, all 16 MiB
of RAM and ordered writes. Cases cover both branches, P0/P1/P2 aliases,
overlapping buffers, an end-of-RAM buffer and observer-capability revocation.
Ten invalid cases reject without mutation. The product capability is reached
in all 64 expected cases. Evidence: `runs/render-context-win-test-b.log`.
Instruction retirement/provenance counters are outside this semantic native
boundary, as for the existing native owners; no Dreamcast scheduler exists
inside the product NativePortAotServices. The title still owns game cadence.

Independent source review found no missing CPU/FPU or alias guard. The
Windows game links incrementally and passes its existing closure/FPU audit.
The same 192 positive / 10 rejection cases pass on Linux
(`runs/render-context-linux-test-a.log`).

Windows executable `68afb4556a9a1b05278ef4b5dbc5c8b9b6ae885f6a96bd5c59c2bb05fe5d0d5a`
completed matched Gamma Recompiled windows 5–1605. Control versus render-owner
ON is 15.791016 versus 15.419922 execution CPU ms/image (-2.35%); image output
is effectively unchanged at 56.39 versus 56.37/s. Both have 1,688 updates,
identical final XYZ bits and HUD timer. The ON run records 205,385 captures,
205,385 commits, 410,770 direct operations and zero fallbacks. The Original
5–125 smoke test also passes, retaining PAL 50 / release 2 / delta 2.
Evidence: `runs/native-owner-win-{control,render,original}/result.json`.

Linux executable `12666ce5fc24b7797169773a43f3bec83cbfd2f535ccf7533a68367ec9521e86`
completed Gamma Original 5–25 with the render owner plus all-scene collision
ON/OFF: 239.909556 versus 249.602803 execution CPU ms/update (-3.88%), and
0.510934 versus 0.490172 images/s (+4.24%). This is a combined comparison;
68 versus 67 updates, different start ticks/XYZ, and small final XYZ/timer
differences prevent treating it as an exact equal-work speedup. Both retain
PAL 50 / release 2 / delta 2; the ON path has zero render fallbacks.
Evidence: `runs/native-owner-linux-{on,off}-gamma-original-summary.json`.
These are QEMU/TCG measurements, never Steam Deck FPS predictions.
