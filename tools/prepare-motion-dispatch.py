"""Retain current native hook bindings; render-interpolation tracing is retired."""
import hashlib
from pathlib import Path
import sys

source_path, destination = map(Path, sys.argv[1:3])
manifest = source_path.parent.parent / ".katana-generated-artifacts"
lines = manifest.read_text().splitlines()
if lines[0] != "katana-codegen-artifacts-v2":
    raise RuntimeError("Unrecognized dispatch manifest")
if lines[1] != "generation\tsha256:" + hashlib.sha256(("\n".join(lines[2:])+"\n").encode()).hexdigest():
    raise RuntimeError("Invalid dispatch manifest generation")
relative = source_path.relative_to(manifest.parent).as_posix()
records = [line.split("\t") for line in lines[2:] if line.split("\t")[0] == relative]
data = source_path.read_bytes()
if len(records) != 1 or int(records[0][1]) != len(data) or records[0][2] != "sha256:"+hashlib.sha256(data).hexdigest():
    raise RuntimeError("Dispatch source differs from its SDK-authored generation")
source = data.decode()
for header in ("katana_port.hpp", "native-port-dispatch-internal.hpp"):
    original = '#include "../include/'+header+'"'
    if source.count(original) != 1:
        raise RuntimeError("Unexpected dispatch include boundary")
    source = source.replace(original, '#include "'+header+'"')
# Interpolation withdrawn: the original prototype injection is retained below
# for reference only. The generated hook bridge remains necessary for Options.
# source = '#include "sonic_motion_owner.hpp"\n#include "sonic_presentation.hpp"\n'+source
anchor = """    require_active_context(cpu);
    const auto result = dispatch_native(
        *active_native_context, target, cpu.pr, std::nullopt, false,
        initial_source);"""
if source.count(anchor) != 1:
    raise RuntimeError("Unexpected native call-dispatch boundary")
# source = source.replace(anchor, """    require_active_context(cpu);
#     const sonic::motion::CallScope motion_owner(
#         cpu,target,sonic::motion::CallScope::observes(cpu.pr)&&sonic::presentation::settings().interpolation);
#     const auto result = dispatch_native(
#         *active_native_context, target, cpu.pr, std::nullopt, false,
#         initial_source);""")
destination.parent.mkdir(parents=True, exist_ok=True)
encoded = source.encode()
if not destination.exists() or destination.read_bytes() != encoded:
    destination.write_bytes(encoded)
print("SONIC_PORT_DISPATCH_READY source_generation_verified=1 retained_aot_unchanged=1 interpolation_instrumentation=0")
