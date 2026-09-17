"""Keep the native closure audit with explicit source-bound port objects."""
import pathlib
import sys
source = pathlib.Path(sys.argv[1]).read_text()
changes = [
    # Windows PNG decoding for the identity-bound supplied menu artwork. This
    # admits one named OS component, retaining the rest of the closure audit.
    ('constexpr std::array<std::string_view, 27> allowed_platform_owners{',
     'constexpr std::array<std::string_view, 28> allowed_platform_owners{\n    std::string_view{"gdiplus"},'),
    ('std::string_view{"gdi32.dll"}, std::string_view{"comdlg32.dll"},',
     'std::string_view{"gdi32.dll"}, std::string_view{"gdiplus.dll"}, std::string_view{"comdlg32.dll"},'),
    ('constexpr std::array<std::string_view, 4> allowed_first_party_owners{',
     'constexpr std::array<std::string_view, 8> allowed_first_party_owners{\n    std::string_view{"sonic_minicart_aot"},\n    std::string_view{"sonic_startup"},\n    std::string_view{"sonic_vulkan"},\n    std::string_view{"sonic_sony_input"},'),
    ('constexpr std::array<std::string_view, 1> allowed_direct_objects{\n    std::string_view{"main.cpp.obj"},',
     'constexpr std::array<std::string_view, 5> allowed_direct_objects{\n    std::string_view{"native_port_sound_bank.cpp.obj"},\n    std::string_view{"native_port_audio.cpp.obj"},\n    std::string_view{"native_port_platform.cpp.obj"},\n    std::string_view{"native_port_graphics.cpp.obj"},\n    std::string_view{"main.cpp.obj"},')]
for before, after in changes:
    if source.count(before) != 1:
        raise RuntimeError("Pinned link-audit layout changed; review required")
    source = source.replace(before, after)
options=sys.argv[3:]
allowed={'--ram-read-experiment':'sonic_ram_reads', '--fpu-call-experiment':'sonic_fpu_calls',
    '--ram-regions':'sonic_ram_regions',
    '--compact-aot-control':'sonic_compact_aot_control',
    '--compact-aot-compact':'sonic_compact_aot_compact',
    '--fpu-runtime-fast':'sonic_fpu_runtime',
    '--internal-diagnostics':'sonic_internal_diagnostics'}
if len(options)!=len(set(options)) or any(option not in allowed for option in options):
    raise RuntimeError("Unknown or duplicate native link audit option")
if ('--ram-read-experiment' in options and len(options)>1) or (
    '--compact-aot-control' in options and '--compact-aot-compact' in options):
    raise RuntimeError("Conflicting experimental archive owners")
if options:
    owners=[allowed[option] for option in options]
    before = 'constexpr std::array<std::string_view, 8> allowed_first_party_owners{'
    if source.count(before) != 1:
        raise RuntimeError("Unexpected experimental archive owner boundary")
    source = source.replace(before,
        'constexpr std::array<std::string_view, '+str(8+len(owners))+'> allowed_first_party_owners{'+
        ''.join('\n    std::string_view{"'+owner+'"},' for owner in owners))
pathlib.Path(sys.argv[2]).write_text(source)
