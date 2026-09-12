# In-world input-hint boundary: bounded audit

Sage supplied a partial read-only path audit; Root continued it against the
original bytes. No message substitution was implemented. This is a concrete
boundary/result report, not a claim of full hint coverage.

The representative monitor uses update 8C061300/display 8C0613BC. Its opening
path 8C061208 selects a request through 8C05EF8A, and 8C05F0D6 submits the
12-byte request through 8C05FA4A with callback 8C05F7A0. The callback takes a
text pointer from an 8-byte page record and invokes 8C05F64A with context
8C78C458. At 8C05F674 that helper calls the original 8C054F2E string rasterizer.

The later 8C055C20 queue, 8C055B32 texture publication and 8C054A00 display
operate on already rasterized 60-byte contexts. They have no semantic input
action to remap. Subtitles must not be globally altered at these boundaries.

## Meaning of the candidate control bytes

Original primary disassembly was checked for the actual consumers:

- Initial byte 0x09 selects 8C055050: measure each line and center it
  separately, then render via 8C054FEC.
- Initial byte 0x07 selects 8C0550FC: measure the maximum paragraph line
  width, use a common centered left edge, then render the individual lines.
- 8C054D6C handles 0x0A as a newline. Other values below 0x20 return without
  drawing; printable values go through the language-specific glyph lookup.

Therefore 0x07 and 0x09 are layout controls, not A/B/X/Y/L/R tokens. Replacing
them with button names would corrupt ordinary prose layout.

Root decoded the installed STG01.PRS read-only into a local analysis copy:
1,954,056 bytes, SHA-256
`4c4478ef6bc50c53a85e2c08d88bed8fcfcfdf47c9db378f7ad8f693f8b29538`.
Its 0C910CA0 page array references English strings at 0C911188 and 0C9111C4.
The following page array references 0C911200, beginning with 0x07 and the
plain instruction to press the jump button again for a homing attack.
The complete NUL-terminated 66-byte record has SHA-256
`cea4bc6bfd70e1a32d686a3c3e12ecace473492584f1999b076f06e2d074e1a9`.
It contains a newline and ordinary ASCII, no physical Dreamcast button token.
The analogous Japanese examples use the 0x09 line-layout prefix.

This sampled instruction already refers to an action (jump), so it stays
correct when that action is remapped and should remain original. This does
not establish that every other monitor or special font glyph is neutral.
For any future proven literal button hint, bind the complete active module,
page record and original bytes before the string rasterizer. Never replace
arbitrary A/B letters or globally swap a same-named font/texture. Full owner
admission for such a replacement and its bounded translated layout still
require a concrete affected record.
