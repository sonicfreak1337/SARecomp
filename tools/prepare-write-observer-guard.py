"""Cache observer permission with the same generation-checked AOT RAM guard."""
import argparse
import hashlib
import json
from pathlib import Path
import re

CAPTURE = 'cpu.memory.direct_linear_memory_guard(false)'
QUERY = 'cpu.memory.guest_write_observer_allows_prevalidated_linear_writes()'
NEW_CAPTURE = 'sonic::write_observer::capture(cpu.memory)'
NEW_QUERY = 'katana_direct_ram.permits_observed_writes'
FUNCTION = re.compile(r'BlockExit fn_[0-9A-F]+_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{')
MEMORY_HEADER_SHA = '8e194176d954262a97110f1e2e3bc8ca6d32c44d33e52c965b7eb45607b7b640'
MEMORY_SOURCE_SHA = '56806312c7d8fcdd5d5d33c0678397757ba0c52830566333f872cc8ba63db6f5'

def sha(data): return hashlib.sha256(data).hexdigest()

def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data: path.write_bytes(data)

def transform(source):
    starts = [m.start() for m in FUNCTION.finditer(source)] + [len(source)]
    if len(starts) < 2: raise ValueError('No runtime owner definitions')
    result = source[:starts[0]]
    counts = {'functions': 0, 'queries': 0, 'captures': 0}
    for begin, end in zip(starts, starts[1:]):
        body = source[begin:end]
        queries = body.count(QUERY)
        if queries:
            # Only the generator's read-guard declaration and recaptures may
            # change. No temporary proof may escape or skip guard resolution.
            captures = len(re.findall(r'(?:auto )?katana_direct_ram = '+re.escape(CAPTURE)+r';', body))
            if captures == 0 or captures != body.count(CAPTURE):
                raise ValueError('Unknown direct guard capture')
            can_write = re.compile(r'if \(!'+re.escape(QUERY)+r'\)\s*return false;\s*std::uint32_t katana_direct_address = 0u;\s*return katana_direct_ram_resolve\(')
            resolved = re.compile(re.escape(QUERY)+r' &&\s*katana_direct_ram_resolve\(')
            if len(can_write.findall(body)) != 1 or len(resolved.findall(body)) + 1 != queries:
                raise ValueError('Observer query does not immediately guard address resolution')
            body = body.replace(CAPTURE, NEW_CAPTURE).replace(QUERY, NEW_QUERY)
            counts['functions'] += 1
            counts['queries'] += queries
            counts['captures'] += captures
        result += body
    if result.replace(NEW_CAPTURE, CAPTURE).replace(NEW_QUERY, QUERY) != source:
        raise ValueError('Transformation changed unrelated code')
    return '#include "sonic_write_observer_guard.hpp"\n'+result, counts

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--destination', type=Path, required=True)
    p.add_argument('--memory-header', type=Path, required=True)
    p.add_argument('--memory-source', type=Path, required=True)
    p.add_argument('--helper', type=Path, required=True)
    p.add_argument('--unit', action='append', required=True)
    a = p.parse_args()
    root, destination = a.source_root.resolve(), a.destination.resolve()
    if root == destination or root in destination.parents or destination in root.parents:
        raise ValueError('Outputs must remain separate from retained source')
    if sha(a.memory_header.read_bytes()) != MEMORY_HEADER_SHA or sha(a.memory_source.read_bytes()) != MEMORY_SOURCE_SHA:
        raise ValueError('Memory generation contract changed; review before recaching observer permission')
    lines = (root/'.katana-generated-artifacts').read_text().splitlines()
    if lines[0] != 'katana-codegen-artifacts-v2' or lines[1] != 'generation\tsha256:'+sha(('\n'.join(lines[2:])+'\n').encode()):
        raise ValueError('Invalid retained source manifest')
    records = {parts[0]: parts for line in lines[2:] if len(parts := line.split('\t')) >= 3}
    if len(a.unit) != len(set(a.unit)): raise ValueError('Duplicate source member')
    report = {'schema': 'sarecomp-write-observer-guard-v1', 'generation': lines[1],
              'memory_header_sha256': MEMORY_HEADER_SHA, 'memory_source_sha256': MEMORY_SOURCE_SHA,
              'helper_sha256': sha(a.helper.read_bytes()), 'units': []}
    for unit in a.unit:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp', unit):
            raise ValueError('Invalid source member')
        data = (root/'code'/unit).read_bytes()
        if records.get('code/'+unit, [])[1:3] != [str(len(data)), 'sha256:'+sha(data)]:
            raise ValueError('Source identity mismatch: '+unit)
        transformed, counts = transform(data.decode())
        output = transformed.encode()
        write(destination/unit, output)
        report['units'].append({'unit':unit, 'source_sha256':sha(data), 'output_sha256':sha(output), **counts})
    write(destination/'preparation.json', (json.dumps(report, indent=2)+'\n').encode())
    print('SONIC_WRITE_OBSERVER_GUARD_READY units='+str(len(report['units']))+
          ' queries='+str(sum(u['queries'] for u in report['units'])))

if __name__ == '__main__': main()
