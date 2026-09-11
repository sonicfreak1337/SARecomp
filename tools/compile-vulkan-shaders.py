"""Compile shared HLSL to SPIR-V with explicit Vulkan interface/depth adaptations."""
import argparse
import hashlib
import pathlib
import re
import subprocess
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
DXC_URL = "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2607/dxc_2026_07_29.zip"
DXC_SHA = "a1dfb116ba3eeae6a1582291b53a8e7bf65ad760676bd3194685c8f7367cd241"

def replace_once(source, before, after):
    if source.count(before) != 1:
        raise RuntimeError("Pinned shader contract changed: " + before)
    return source.replace(before, after)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    compiler = ROOT / ".local/toolchain/dxc/bin/x64/dxc.exe"
    if not compiler.exists():
        archive = ROOT / ".local/archives/dxc-1.9.2607.zip"
        archive.parent.mkdir(parents=True, exist_ok=True)
        if not archive.exists():
            urllib.request.urlretrieve(DXC_URL, archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != DXC_SHA:
            raise RuntimeError("DXC archive digest mismatch")
        with zipfile.ZipFile(archive) as z:
            z.extractall(ROOT / ".local/toolchain/dxc")
    source = (ROOT / "src/renderer/pinned/native_port_graphics.cpp").read_text()
    shaders = dict(re.findall(r'constexpr char (native_graphics\w*shader_source)\[\] = R"\((.*?)\)";', source, re.S))
    work = args.output.parent / "vulkan-shaders"
    work.mkdir(parents=True, exist_ok=True)
    jobs = [("draw_vs", "", "draw_vertex_main", "vs"),
            ("draw_ps", "", "draw_pixel_main", "ps"),
            ("composite_vs", "", "composite_vertex_main", "vs"),
            ("composite_ps", "", "composite_pixel_main", "ps"),
            ("overlay_ps", "", "performance_overlay_pixel_main", "ps"),
            ("capture_ps", "_type_two_capture", "draw_type_two_capture_main", "ps")]
    jobs += [(f"resolve_ps_{n}", "_type_two_resolve", "type_two_resolve_pixel_main", "ps") for n in (32, 64, 128, 256)]
    output = ["// Generated from pinned HLSL. Do not edit.\n#pragma once\n#include <cstdint>\nnamespace sonic::rendering::shaders {\n"]
    for name, family, entry, stage in jobs:
        hlsl = work / (name + ".hlsl")
        code = shaders[f"native_graphics{family}_shader_source"]
        if name == "draw_vs":
            # D3D's fixed one-pixel point size is an explicit SPIR-V builtin.
            code = replace_once(code, "struct DrawVertexOutput {",
                'struct DrawVertexOutput {\n    [[vk::builtin("PointSize")]] float point_size : PSIZE;')
            code = replace_once(code, "    DrawVertexOutput output;", "    DrawVertexOutput output;\n    output.point_size = 1.0;")
        if name == "draw_ps":
            # D24 is optional in Vulkan (AMD exposes D32). Store its UNORM24
            # value explicitly so sampled opaque depth matches the D3D oracle.
            before = "    return output;\n}\n\nstruct CompositeVertexOutput"
            code = replace_once(code, before, "    output.depth = round(saturate(output.depth) * 16777215.0) / 16777215.0;\n" + before)
        if name == "capture_ps":
            # Compare in the same depth format as the stored opaque sample.
            # FXC/DXC log approximations must not reject coplanar fragments.
            code = replace_once(code, "if (final_depth < opaque_depth) discard;",
                "if (round(saturate(final_depth) * 16777215.0) / 16777215.0 < opaque_depth) discard;")
            # D3D's masked color output has no attachment in the Vulkan gather.
            code = replace_once(code, "    float4 color : SV_Target;", "")
            code = replace_once(code, "    output.color = float4(0.0, 0.0, 0.0, 0.0);", "")
        hlsl.write_text(code)
        spv = work / (name + ".spv")
        command = [str(compiler), "-spirv", "-fspv-target-env=vulkan1.3", "-fvk-use-dx-layout",
                   "-fvk-b-shift", "0", "0", "-fvk-t-shift", "4", "0",
                   "-fvk-s-shift", "12", "0", "-fvk-u-shift", "16", "0",
                   "-T", stage + "_6_0", "-E", entry, "-Fo", str(spv), str(hlsl)]
        if stage == "vs":
            command += ["-fvk-invert-y"]
        if family == "_type_two_resolve":
            command += ["-D", "NATIVE_TYPE_TWO_SORT_CAPACITY=" + name.rsplit("_", 1)[1]]
        subprocess.run(command, check=True)
        binary = spv.read_bytes()
        words = [f"0x{int.from_bytes(binary[i:i+4], 'little'):08x}u" for i in range(0, len(binary), 4)]
        output.append(f"inline constexpr std::uint32_t {name}[] = {{\n")
        output += [",".join(words[i:i+8]) + ",\n" for i in range(0, len(words), 8)]
        output.append("};\n")
    output.append("}\n")
    args.output.write_text("".join(output))

if __name__ == "__main__":
    main()
