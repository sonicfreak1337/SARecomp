"""Reuse the pinned platform validation, save codec and public ABI on Linux.

The Win32 implementation and source archive remain untouched. Only the host
implementation block is replaced; the save byte format is copied verbatim.
"""
import hashlib
import sys
from pathlib import Path

source_path, output = map(Path, sys.argv[1:])
raw = source_path.read_bytes()
if hashlib.sha256(raw).hexdigest() != "ec90e85fe3bb38625295f9f1fa79ebfab31bab7ca7bcc09faef5668f1b4eea22":
    raise RuntimeError("Pinned platform source changed")
source = raw.decode("utf-8")
begin = source.index("\n#ifdef _WIN32\n\nnamespace {")
end = source.index("\n#endif\n\nNativePortReadOnlyFile::NativePortReadOnlyFile", begin)
codec_start = source.index("constexpr std::array<std::byte, 16u> save_magic{")
codec_end = source.index("[[nodiscard]] bool path_missing", codec_start)
codec = source[codec_start:codec_end].replace("ERROR_ARITHMETIC_OVERFLOW", "EOVERFLOW")
includes = '''#include "katana/io/input_provenance.hpp"
#include "controller_state.hpp"
#include <SDL3/SDL.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
'''
prefix = source[:begin].replace('#include "native_port_input_policy.hpp"', includes)
output.mkdir(parents=True, exist_ok=True)
(output / "native_port_platform.cpp").write_text(prefix + '\n#include "native_port_platform_posix.inc"\n' + source[end+len("\n#endif"):], encoding="utf-8")
(output / "native_save_codec.inc").write_text(codec, encoding="utf-8")
