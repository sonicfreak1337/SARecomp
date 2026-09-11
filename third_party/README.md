# Additional renderer dependencies

The Vulkan renderer uses the following pinned upstream components. Their
copyright notices and full licenses are included in this source tree and
in the development build's `licenses/` directory. Existing FFmpeg notices
remain beside the executable.

## volk

Copyright (c) 2018-2026 Arseny Kapoulkine. MIT license.

- Source: https://github.com/zeux/volk
- Commit: `e640c6ea6420bdaf6248e85f736ab0b99491ae58`
- Included unchanged: `volk.h`, `volk.c`, `LICENSE.md`.
- `volk.h` SHA-256: `dc2cb753d467353380dc4384ca479551a2a527b32b80527f8aee312de6510a78`
- `volk.c` SHA-256: `64159146b843e1b3d9f5dc14035b2716e16868b9e5ec4d7aef8f57f6b674a875`
- Source license: `volk/LICENSE.md`; packaged license: `licenses/volk/LICENSE.md`.

## Vulkan-Headers

Copyright 2015-2026 The Khronos Group Inc. Apache-2.0 OR MIT for the
included Vulkan API headers; upstream notices are retained.

- Source: https://github.com/KhronosGroup/Vulkan-Headers
- Commit: `ee2ec5fd83dafce291024683b50dc89219333076`
- Included unchanged: upstream `include/`, `LICENSE.md`, and `LICENSES/`.
- Upstream ZIP SHA-256: `6dc2fb9e72afd2e90887a57e7ca799acb6100b98a6b222c8c52e91082d74cd44`
- Source licenses: `vulkan-headers/LICENSE.md` and `vulkan-headers/LICENSES/`.
- Packaged licenses: `licenses/vulkan-headers/`.

## Shader compiler (build tool only)

Microsoft DirectXShaderCompiler `v1.9.2607`, official Windows release
`dxc_2026_07_29.zip`, is downloaded into the ignored local toolchain.
Its archive SHA-256 is verified before extraction:
`a1dfb116ba3eeae6a1582291b53a8e7bf65ad760676bd3194685c8f7367cd241`.

Release: https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2607

The compiler's own license files stay with that toolchain. Neither the
compiler nor its DLLs are distributed with the game. Compiled SPIR-V is
embedded in the executable; players need the Vulkan loader supplied by
their graphics driver, not a Vulkan SDK installation.

## Port-owned pinned graphics adaptation

`src/renderer/pinned/` starts from the project's archived Katana source
`178448be` (`katana-source-178448be.zip`). The graphics implementation is
adapted locally; the command-stream and UI-edge headers are copied unchanged.
This is an explicit Sonic port source dependency, not a modification to the
archived SDK, original repository, baseline product, or generated game code.
