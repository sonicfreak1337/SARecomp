"""Bind one port-local DualSense correction to the exact retained SDK source."""
import hashlib
import sys
import zipfile
from pathlib import Path

archive, destination = Path(sys.argv[1]), Path(sys.argv[2])
expected = {
    "native_port_platform.cpp": "ec90e85fe3bb38625295f9f1fa79ebfab31bab7ca7bcc09faef5668f1b4eea22",
    "native_port_input_policy.hpp": "d2ac1e94a926449ddbb17366e69d10734d808a0b1c4fc567b8e1f99382c64137",
}
with zipfile.ZipFile(archive) as sdk:
    sources = {name: sdk.read("src/runtime/"+name) for name in expected}
for name, value in sources.items():
    if hashlib.sha256(value).hexdigest() != expected[name]:
        raise RuntimeError("Pinned camera platform source changed: "+name)
source = sources["native_port_platform.cpp"].decode("utf-8")
anchor = "#include \"native_port_input_policy.hpp\""
if source.count(anchor) != 1:
    raise RuntimeError("Camera platform include layout changed")
source = source.replace(anchor, anchor+'\n#include "sonic_camera_input.hpp"\n#include "sonic_presentation.hpp"')
anchor = "            if ((capabilities.wCaps & JOYCAPS_HASZ) != 0u &&\n"
if source.count(anchor) != 1:
    raise RuntimeError("Camera platform Sony axis layout changed")
correction = """            // Port-local opt-in correction, after identity-bound Sony admission.
            // XInput, Original camera, movement, buttons and trigger semantics
            // retain the pinned SDK paths. Replay returns before live polling.
            if (identity->kind == NativeGamepadSourceKind::DualSense &&
                ::sonic::presentation::settings().camera_style == ::sonic::camera::Style::Recompiled) {
                const auto axes = (capabilities.wCaps & (JOYCAPS_HASZ | JOYCAPS_HASR)) == (JOYCAPS_HASZ | JOYCAPS_HASR)
                    ? ::sonic::camera::dualsense_right_stick(info.dwZpos, info.dwRpos,
                        capabilities.wZmin, capabilities.wZmax, capabilities.wRmin, capabilities.wRmax)
                    : ::sonic::camera::RawStick{};
                destination.right_stick_x_raw = axes.x;
                destination.right_stick_y_raw = axes.y;
                static bool reported = false;
                if (!reported) {
                    std::fprintf(stderr, "SONIC_CAMERA_CONTROLLER source=dualsense axes=Z/R raw=%d,%d\\n", int(axes.x), int(axes.y));
                    reported = true;
                }
            }
"""
source = source.replace(anchor, correction+anchor)
sources["native_port_platform.cpp"] = source.encode("utf-8")
destination.mkdir(parents=True, exist_ok=True)
for name, value in sources.items():
    target = destination/name
    if not target.exists() or target.read_bytes() != value:
        target.write_bytes(value)
print("SONIC_CAMERA_PLATFORM_READY sdk_source_verified=1 mapping=dualsense_Z_R original_and_xinput=unchanged")
