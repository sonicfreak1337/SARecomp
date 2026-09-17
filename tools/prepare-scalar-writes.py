"""Fuse qualified scalar AOT RAM writes; retain every original fallback."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import zipfile

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
    p.add_argument('--memory-source', type=Path)
    p.add_argument('--sdk-zip', type=Path)
    p.add_argument('--runtime-source', type=Path, required=True)
    p.add_argument('--source-root', type=Path)
    p.add_argument('--units-file', type=Path)
    p.add_argument('--runtime-only', action='store_true', help='Bind closed native owners without changing guest units')
    p.add_argument('--destination', type=Path, required=True)
    p.add_argument('--mode', choices=('scalar','stack','region'), default='scalar')
    p.add_argument('--guard-probe', action='store_true', help='Private sampled RAM-miss diagnosis, never distribution')
    a = p.parse_args()
    if bool(a.memory_source) == bool(a.sdk_zip): p.error('Select memory source or pinned SDK zip')
    if not a.runtime_only and (not a.source_root or not a.units_file):
        p.error('Guest transformations require source root and unit list')
    out = a.destination.resolve(); root = a.source_root.resolve() if a.source_root else None
    if root and (out == root or out in root.parents or root in out.parents):
        raise ValueError('Prepared sources must be separate')
    if a.sdk_zip:
        with zipfile.ZipFile(a.sdk_zip) as archive: data = archive.read('src/runtime/memory.cpp')
        if digest(data) != MEMORY_SHA: raise ValueError('Pinned memory source identity changed')
        memory = data.decode().replace('\r\n', '\n')
    else: memory = qualified_source(a.memory_source, MEMORY_SHA)
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
    if a.runtime_only:
        report = {'schema':'sarecomp-native-memory-capability-v1', 'guest_units_changed':0,
                  'memory_sha256':MEMORY_SHA, 'runtime_sha256':RUNTIME_SHA}
        write(out/'preparation.json', (json.dumps(report,indent=2)+'\n').encode())
        print('SONIC_NATIVE_MEMORY_CAPABILITY_READY guest_units_changed=0')
        return
    lines = (root/'.katana-generated-artifacts').read_text().splitlines()
    if lines[0] != 'katana-codegen-artifacts-v2' or lines[1] != 'generation\tsha256:'+digest(('\n'.join(lines[2:])+'\n').encode()):
        raise ValueError('Invalid retained source manifest')
    if a.mode in ('stack','region') and lines[1] != 'generation\tsha256:ad51236f53b465bcac54c915df97ecdcb6467f8f06e5b36f85885e19525129f1':
        raise ValueError('Stack admission helpers need review for this generation')
    records = {parts[0]:parts for line in lines[2:] if len(parts:=line.split('\t')) >= 3}
    units = a.units_file.read_text().splitlines()
    if len(units) != len(set(units)): raise ValueError('Duplicate unit')
    stack=None
    if a.mode in ('stack','region'):
        script='prepare-stack-frames.py' if a.mode=='stack' else 'prepare-ram-regions.py'
        spec=importlib.util.spec_from_file_location('memory_regions',Path(__file__).with_name(script))
        stack=importlib.util.module_from_spec(spec);spec.loader.exec_module(stack)
    report = {'schema':'sarecomp-scalar-writes-v1','mode':a.mode,'memory_sha256':MEMORY_SHA,
              'runtime_sha256':RUNTIME_SHA,'generation':lines[1],'units':[]}
    for unit in units:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp',unit): raise ValueError('Invalid unit')
        data = (root/'code'/unit).read_bytes()
        if records.get('code/'+unit,[])[1:3] != [str(len(data)), 'sha256:'+digest(data)]:
            raise ValueError('Guest source identity changed: '+unit)
        frames=[]
        if stack:
            transformed,frames=stack.transform(data.decode())
            count=0
        else:transformed, count = transform(data.decode())
        if a.guard_probe and unit == 'unit-v8C056ED4-8C0585E0-d3674ae50a86c851.cpp':
            # Exact observed PCs, after the original preflight and before any
            # original side effect. Log at most eight misses per site.
            for pc,address,write_access in [('8C057FEA','katana_registers[14] + 4u','write'),
                                             ('8C057FEC','katana_registers[13] + 44u','read')]:
                needle=f'const bool katana_guarded_linear_access_{pc} = katana_direct_ram_can_{write_access}({address}, 4u);'
                diagnostic='''
                    if (!katana_guarded_linear_access_PC) {
                        static unsigned sonic_misses=0;
                        if (sonic_misses < 8u) {
                            ++sonic_misses;
                            const auto fresh=cpu.memory.direct_linear_memory_guard(false);
                            std::fprintf(stderr,"SONIC_RAM_GUARD_MISS pc=PC address=%08x sr=%08x mmu=%08x fpscr=%08x old=%u fresh=%u old_generation=%llu fresh_generation=%llu base=%08x span=%08x watches=%zu sink=%u write_observer_allows=%u\\n",
                                static_cast<unsigned>(ADDRESS),cpu.sr,cpu.mmucr,cpu.fpscr,
                                static_cast<unsigned>(static_cast<bool>(katana_direct_ram)),static_cast<unsigned>(static_cast<bool>(fresh)),
                                static_cast<unsigned long long>(katana_direct_ram.generation),static_cast<unsigned long long>(fresh.generation),
                                fresh.physical_base,fresh.physical_span,cpu.memory.watchpoint_count(),
                                static_cast<unsigned>(static_cast<bool>(cpu.memory.guest_memory_access_sink())),
                                static_cast<unsigned>(cpu.memory.guest_write_observer_allows_prevalidated_linear_writes()));
                        }
                    }'''.replace('PC',pc).replace('ADDRESS',address)
                transformed=once(transformed,needle,needle+diagnostic)
            transformed='#include <cstdio>\n'+transformed
        output = transformed.encode()
        write(out/unit, output)
        report['units'].append({'unit':unit,'source_sha256':digest(data),
                               'output_sha256':digest(output),'write_helpers':count,'frame_sequences':frames})
    report['guard_probe']=a.guard_probe
    write(out/'preparation.json',(json.dumps(report,indent=2)+'\n').encode())
    print('SONIC_SCALAR_WRITES_READY units='+str(len(units))+' helpers='+str(sum(u['write_helpers'] for u in report['units']))+
          ' stack_frames='+str(sum(len(u['frame_sequences']) for u in report['units'])))

if __name__ == '__main__': main()
