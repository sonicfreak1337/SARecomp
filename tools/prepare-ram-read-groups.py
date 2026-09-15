"""Opt-in Linux read groups; original AOT and every fallback stay intact."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

spec = importlib.util.spec_from_file_location('reads', Path(__file__).with_name('prepare-ram-read-aot.py'))
reads = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reads)


def groups(source):
    # Reuse the exact complete envelope and helper qualification, not a looser
    # second decoder. The transformed per-load output is deliberately discarded.
    _, sites = reads.transform(source, 'preloaded', True)
    accepted = {(site['pc'], site['source_line']): site for site in sites}
    spans = []
    for matcher in (reads.START, reads.FPU_START):
        for start in matcher.finditer(source):
            pc = start['pc']
            key = (pc, source.count('\n', 0, start.start())+1)
            if key not in accepted:
                continue
            end = re.compile(r'(?m)^'+re.escape(start['indent'])+r'\}').search(source, start.end())
            site = dict(accepted[key], begin=start.start(), end=end.end(),
                        indent=start['indent'], address=start['address'])
            site['destination'] = (int(site['opcode'], 16) >> 8) & 15
            spans.append(site)
    spans.sort(key=lambda site: site['begin'])
    result, current = [], []
    for site in spans:
        if current:
            previous = current[-1]
            gap = source[previous['end']:site['begin']]
            adjacent = (int(site['pc'], 16) == int(previous['pc'], 16)+2 and
                        site['indent'] == previous['indent'] and
                        re.fullmatch(r'\s*(?:katana_block_'+site['pc']+r'_resume:\s*)?', gap))
            if not adjacent or len(current) == 16:
                if len(current) > 1:
                    result.append(current)
                current = []
        current.append(site)
    if len(current) > 1:
        result.append(current)
    return result


def fast_group(group):
    first, last = group[0], group[-1]
    indent = first['indent']
    end_label = 'sonic_read_group_'+first['pc']+'_end'
    contains_fmov = any(site.get('kind') == 'scalar-fmov' for site in group)
    lines = ['{', '    sonic::memory::ReadGroup32 sonic_group(cpu, katana_direct_ram, '+str(contains_fmov).lower()+');']
    local_gpr, writes, conditions = {}, [], []
    for index, site in enumerate(group):
        value = f'sonic_group_value_{index}'
        lines.append('    std::uint32_t '+value+' = 0u;')
        address = re.sub(r'katana_registers\[(\d+)\]',
                         lambda m: local_gpr.get(int(m[1]), m[0]), site['address'])
        conditions.append(f'sonic_group.read({address}, {value})')
        destination = site['destination']
        if site.get('kind') == 'scalar-fmov':
            writes.append(f'cpu.fr[{destination}] = {value};')
        else:
            writes.append(f'katana_registers[{destination}] = {value};')
            local_gpr[destination] = value
    conditions.append(f'sonic_group.commit({len(group)}u)')
    lines.append('    if ('+(' &&\n'+indent+'        ').join(conditions)+') {')
    lines.extend('        '+write for write in writes)
    lines.append('        sonic::memory::complete_read_group(cpu,')
    lines.append(f'            katana::runtime::relocate_code_address_inline(0x{last["pc"]}u), {len(group)}u);')
    lines.extend(['        goto '+end_label+';', '    }', '}'])
    return '\n'.join(indent+line for line in lines)+'\n', '\n'+indent+end_label+': ;'


def transform(source):
    selected = groups(source)
    output = source
    for group in reversed(selected):
        before, after = fast_group(group)
        begin, end = group[0]['begin'], group[-1]['end']
        output = output[:begin]+before+output[begin:end]+after+output[end:]
    return '#include "sonic_read_group.hpp"\n'+output, selected


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--destination', type=Path, required=True)
    p.add_argument('--helper', type=Path, required=True)
    p.add_argument('--unit', action='append', required=True)
    args = p.parse_args()
    root, destination = args.source_root.resolve(), args.destination.resolve()
    if root == destination or root in destination.parents or destination in root.parents:
        raise RuntimeError('Read-group output must be separate from retained sources')
    manifest = (root/'.katana-generated-artifacts').read_text().splitlines()
    if manifest[0] != 'katana-codegen-artifacts-v2' or manifest[1] != 'generation\tsha256:'+reads.sha(('\n'.join(manifest[2:])+'\n').encode()):
        raise RuntimeError('Invalid generated artifact manifest')
    if len(args.unit) != len(set(args.unit)):
        raise RuntimeError('Duplicate selected AOT unit')
    report = {'schema':'sarecomp-read-groups-v1', 'manifest_generation':manifest[1],
              'helper_sha256':reads.sha(args.helper.read_bytes()), 'units':[]}
    for name in args.unit:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp', name):
            raise RuntimeError('Invalid selected AOT unit')
        data = (root/'code'/name).read_bytes()
        records = [line.split('\t') for line in manifest[2:] if line.split('\t')[0] == 'code/'+name]
        if len(records) != 1 or records[0][1:3] != [str(len(data)), 'sha256:'+reads.sha(data)]:
            raise RuntimeError('Selected AOT source identity mismatch: '+name)
        output, selected = transform(data.decode())
        encoded = output.encode()
        report['units'].append({'name':name, 'source_sha256':reads.sha(data), 'output_sha256':reads.sha(encoded),
                                'groups':selected, 'instructions':sum(map(len,selected))})
        reads.write_if_changed(destination/name, encoded)
    if not any(unit['groups'] for unit in report['units']):
        raise RuntimeError('No read group qualified')
    reads.write_if_changed(destination/'preparation.json', (json.dumps(report, indent=2)+'\n').encode())
    print('SONIC_READ_GROUPS_READY units='+str(len(report['units']))+' groups='+
          str(sum(len(unit['groups']) for unit in report['units']))+' instructions='+
          str(sum(unit['instructions'] for unit in report['units'])))


if __name__ == '__main__':
    main()
