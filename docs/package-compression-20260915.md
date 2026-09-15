# Smaller Linux and Steam Deck packages

The Linux/Deck package builder now uses an x86 + LZMA2 preset-4 stream with a
32 MiB dictionary instead of the previous 4 MiB default. The XZ/tar installer
format and extraction procedure are unchanged. Windows already uses a 16 MiB
NSIS dictionary and keeps that setting. Package result JSON now records the
compression configuration, so future size comparisons can identify it.

This is separate from the already validated lean media libraries described in
[the media performance report](performance-media-20260915.md). No new installer
was made for this comparison; the existing installers remain in place pending
meaningful performance progress. It changes no installed assets or game logic.

## Complete executable measurement

`tools/measure-payload-compression.py` measures components without staging or
creating an installer. It checks the input identity between variants and fully
decompresses each result, comparing byte count and SHA-256 with the source.

The input is a separately stripped diagnostic Linux game copy, 1,678,964,368
bytes, SHA-256 `3af3797993ec20f9ebf2857b06e191ff68c0dd4c25f4b405b50282428995d51b`.
The source binary and r354 were not modified by stripping or compression.
This copy predates removal of the rejected RAM-copy trial; it is a measured
component, not a release-candidate or final installer identity.

| Complete game component | Previous 4 MiB dictionary | New 32 MiB dictionary |
| --- | ---: | ---: |
| Compressed bytes | 250,624,564 | 244,082,980 |
| Compression wall time, authoring PC | 180.140 s | 264.640 s |
| Decompression and SHA verification | 14.868 s | 14.569 s |
| Decoded bytes / SHA | Exact | Exact |

The complete component is **6,541,584 bytes smaller (2.61%)**. This costs more
authoring time and about 28 MiB additional decoder dictionary capacity. The
single measured pair shows no decompression penalty on the Windows authoring
PC; it is not a measured Deck install duration. No game/compiler workload ran
alongside this comparison. Source/destination hashing is included in the times.

A prior 256 MiB prefix screen compared 4, 16 and 32 MiB dictionaries. Their
compressed sizes were 58,083,752 / 57,269,516 / 57,031,720 bytes, all with exact
roundtrips. The full-file result above replaces any estimate from that prefix.
Artifacts: `.local/size-compression-20260915/{prefix,full}/result.json`.

The separately measured Linux media stream is 42,921,836 bytes smaller. Together
the measured compressed components save 49,463,420 bytes. Solid tar compression,
file ordering and the final executable can change a complete installer's exact
size, so this sum must not be reported as a measured new installer size.
Installed media-library savings remain 136,487,288 bytes on Linux/Deck and
104,873,472 bytes on Windows; dictionary changes do not reduce installed size.
