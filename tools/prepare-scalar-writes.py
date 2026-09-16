"""Fuse qualified scalar AOT RAM writes; retain every original fallback."""
import argparse
import hashlib
import json
from pathlib import Path
import re

MEMORY_SHA = '56806312c7d8fcdd5d5d33c0678397757ba0c52830566333f872cc8ba63db6f5'
RUNTIME_SHA = '50b7ce3809dd6c20ef190ed792616209fa41752b1f0074460875f73c6ee2c5f9'
HEADER = '#include "sonic_scalar_write_view.hpp"\n'
CAPTURE = '    auto katana_direct_ram = cpu.memory.direct_linear_memory_guard(false);\n'
VIEW = '    const sonic::scalar_writes::View katana_scalar_writes(cpu.memory, katana_direct_ram_code_tracker);\n'
QUERY = 'if (!cpu.memory.guest_write_observer_allows_prevalidated_linear_writes())'
NEW_QUERY = 'if (!katana_scalar_writes.available() && !cpu.memory.guest_write_observer_allows_prevalidated_linear_writes())'
WRITE = re.compile(r'(    const auto katana_direct_ram_write_u(?:8|16|32) =\n.*?const bool katana_allow_direct_write\) \{\n)', re.S)
OWNER = re.compile(r'(?m)^BlockExit fn_[0-9A-F]+_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{\n')
PREFIX = '''        std::uint32_t katana_scalar_address = 0u;
        if (katana_batch == nullptr && katana_allow_direct_write &&
            katana_direct_ram_translate(katana_ram_address, katana_scalar_address) &&
            katana_scalar_writes.try_write(katana_scalar_address, katana_ram_value))
            return;
'''

def digest(data): return hashlib.sha256(data).hexdigest()

def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data: path.write_bytes(data)

def qualified_source(path, expected):
    data = path.read_bytes()
    if digest(data) != expected: raise ValueError('Source identity changed: '+str(path))
    return data.decode().replace('\r\n', '\n')

def once(source, old, new):
    if source.count(old) != 1: raise ValueError('Runtime boundary changed: '+old[:100])
    return source.replace(old, new)

def transform_owner(source):
    # The source is already authenticated by the retained artifact manifest.
    # A mechanically reversible prefix leaves instruction origins, translation,
    # cycles, register handling, executable exits and all fallbacks untouched.
    matches = list(WRITE.finditer(source))
    if not matches: return source, 0
    for match in matches:
        text = match[0]
        if text.count('const auto katana_direct_ram_write_') != 1 or 'Memory::DirectLinearWriteBatch* const katana_batch' not in text:
            raise ValueError('Unrecognized scalar write declaration')
    if source.count(CAPTURE) != 1 or source.count(QUERY) != 1:
        raise ValueError('Write owner must have one standard capture/admission')
    result = source.replace(CAPTURE, CAPTURE+VIEW).replace(QUERY, NEW_QUERY)
    result, count = WRITE.subn(lambda m: m[0]+PREFIX, result)
    if count != len(matches): raise ValueError('Write site count changed')
    restored = result.replace(PREFIX, '').replace(CAPTURE+VIEW, CAPTURE).replace(NEW_QUERY, QUERY)
    if restored != source: raise ValueError('Unrelated guest code changed')
    return result, count


def transform(source):
    # Read-only functions in the same unit must not pay for a write capability.
    # Each transformed helper is bound to its own authenticated function scope.
    owners = list(OWNER.finditer(source))
    output = source
    helpers = 0
    for i in range(len(owners)-1, -1, -1):
        start = owners[i].start()
        end = owners[i+1].start() if i+1 < len(owners) else len(source)
        body, count = transform_owner(source[start:end])
        output = output[:start]+body+output[end:]
        helpers += count
    if helpers != len(list(WRITE.finditer(source))):
        raise ValueError('Scalar write helper outside a qualified owner')
    return (HEADER+output if helpers else source), helpers

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--memory-source', type=Path, required=True)
    p.add_argument('--runtime-source', type=Path, required=True)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--units-file', type=Path, required=True)
    p.add_argument('--destination', type=Path, required=True)
    a = p.parse_args()
    out = a.destination.resolve(); root = a.source_root.resolve()
    if out == root or out in root.parents or root in out.parents:
        raise ValueError('Prepared sources must be separate')
    memory = qualified_source(a.memory_source, MEMORY_SHA)
    memory = once(memory, '    if (write && guest_write_observer_) return {};',
        '    if (write && guest_write_observer_ &&\n'
        '        !sonic::scalar_writes::capture_admitted(this, guest_write_observer_generation_)) return {};')
    runtime = qualified_source(a.runtime_source, RUNTIME_SHA)
    runtime = once(runtime, '    write_observer_generation_ =\n        context.cpu->memory.guest_write_observer_generation();',
        '    write_observer_generation_ =\n        context.cpu->memory.guest_write_observer_generation();\n'
        '    sonic::scalar_writes::bind(context.cpu->memory, immutable_guard, write_observer_generation_);')
    runtime = once(runtime, 'NativePortAotServices::~NativePortAotServices() noexcept {\n',
        'NativePortAotServices::~NativePortAotServices() noexcept {\n'
        '    if (context_ && context_->cpu) sonic::scalar_writes::unbind(&context_->cpu->memory, immutable_guard_);\n')
    write(out/'memory.cpp', (HEADER+memory).encode())
    write(out/'native_port_runtime.cpp', (HEADER+runtime).encode())
    lines = (root/'.katana-generated-artifacts').read_text().splitlines()
    if lines[0] != 'katana-codegen-artifacts-v2' or lines[1] != 'generation\tsha256:'+digest(('\n'.join(lines[2:])+'\n').encode()):
        raise ValueError('Invalid retained source manifest')
    records = {parts[0]:parts for line in lines[2:] if len(parts:=line.split('\t')) >= 3}
    units = a.units_file.read_text().splitlines()
    if len(units) != len(set(units)): raise ValueError('Duplicate unit')
    report = {'schema':'sarecomp-scalar-writes-v1','memory_sha256':MEMORY_SHA,
              'runtime_sha256':RUNTIME_SHA,'generation':lines[1],'units':[]}
    for unit in units:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp',unit): raise ValueError('Invalid unit')
        data = (root/'code'/unit).read_bytes()
        if records.get('code/'+unit,[])[1:3] != [str(len(data)), 'sha256:'+digest(data)]:
            raise ValueError('Guest source identity changed: '+unit)
        transformed, count = transform(data.decode())
        output = transformed.encode()
        write(out/unit, output)
        report['units'].append({'unit':unit,'source_sha256':digest(data),
                               'output_sha256':digest(output),'write_helpers':count})
    write(out/'preparation.json',(json.dumps(report,indent=2)+'\n').encode())
    print('SONIC_SCALAR_WRITES_READY units='+str(len(units))+' helpers='+str(sum(u['write_helpers'] for u in report['units'])))

if __name__ == '__main__': main()
