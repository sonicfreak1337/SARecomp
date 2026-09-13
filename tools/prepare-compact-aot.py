"""Copy eight SHA-bound hot AOT units unchanged; audit exclusive native ownership.

CONTROL and COMPACT differ only in compiler options owned by CMake. OFF does
not invoke this tool. The profile is an IP sample witness, not a speedup claim.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

PROFILE_SHA = '41cc7ec7235fee434c51e9ec85a52a31d5bd13d1fbf9316d5bebbee5fe2c8c8c'
EXCLUDED = 'unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp'
# Rank, sample count and unchanged retained source identity. Ties sort by name.
UNITS = {
    'unit-v8C029400-8C029B00-9bed0201322da5d9.cpp':
        (63, 'd99c58370239f5009140005a238795a5dd6a0e434ba9ffe068857deaa556b9d1'),
    'unit-v8C10EC94-8C10FCEC-2baf008af52674a0.cpp':
        (33, '813ad53582c9e6b7678647072e849686887a0608cbd7a05fa43d1096d86f418f'),
    'unit-v8C033122-8C0342E0-7edcb8468a2b4534.cpp':
        (27, '446bbb70e5155bd2b1b02444d511b7a02478293061a248ec76a856eb89597189'),
    'unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp':
        (23, 'c34ed098e7625b5a432263ebf286ae486cb86b534dad0cdb97d4991f2253e45a'),
    'unit-v8C056ED4-8C0585E0-d3674ae50a86c851.cpp':
        (23, '06075cb56dc09dc42d835118d62ef6eb4ea24cf25490961aba5cf8c73aeec427'),
    'unit-v8C09036C-8C09168E-46c0db15f975e3ef.cpp':
        (22, 'b4a86704a31fcf69d3482c8b703720cdd7f068439d5fe1e08c76c6353756465f'),
    'unit-v8C0273A2-8C028EC2-3c3ba866ec5c4265.cpp':
        (20, '9b1f8bbff7ae1464c4a1b6dcb8380307c582dde17d7ef0069c8c9865f98ff2cd'),
    'unit-v8C051E56-8C053338-ce4429c39e6969de.cpp':
        (20, 'bd3ac13951986ce9098d8b3384a409e8bcbbdc31cf39ec9f25efdc644a0262c0'),
}
ENTRY = re.compile(r'(?m)^BlockExit (fn_[0-9A-F]+_runtime_entry)'
                   r'\(CpuState& cpu, BlockExecutionContext& context\) \{')
MEMBER = re.compile(r'(unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp)\.obj$')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def public_entries(data):
    entries = ENTRY.findall(data.decode('utf-8'))
    if not entries or len(entries) != len(set(entries)):
        raise ValueError('Missing or duplicate public AOT entry definitions')
    return entries


def profile_selection(path):
    raw = path.read_bytes()
    if sha(raw) != PROFILE_SHA:
        raise ValueError('Bound execution profile changed')
    profile = json.loads(raw)
    if profile['schema'] != 'sarecomp-execution-ip-resolved-v1':
        raise ValueError('Unexpected execution profile schema')
    counts = Counter()
    for entry in profile['entries']:
        owners = entry.get('objects', [])
        # Never credit a folded multi-object alias to an individual unit.
        if len(owners) != 1:
            continue
        match = MEMBER.search(owners[0])
        if match and match[1] != EXCLUDED:
            counts[match[1]] += entry['count']
    selected = sorted(counts.items(), key=lambda item: (-item[1], item[0]))[:8]
    if selected != [(name, row[0]) for name, row in UNITS.items()]:
        raise ValueError('Eight hottest unambiguous AOT units changed')
    return profile


def prepare(args):
    source_root = args.source_root.resolve(strict=True)
    destination = args.destination.resolve()
    project = Path(__file__).resolve().parents[1]
    # This recipe may only emit into a separate Sonic build-* directory.
    relative = destination.relative_to(project)
    if (not relative.parts or not relative.parts[0].startswith('build-') or
            source_root == destination or source_root in destination.parents or
            destination in source_root.parents):
        raise ValueError('Output must be inside a separate Sonic build-* directory')
    profile = profile_selection(args.profile)
    manifest_raw = (source_root / '.katana-generated-artifacts').read_bytes()
    manifest = manifest_raw.decode('utf-8').splitlines()
    if (len(manifest) < 3 or manifest[0] != 'katana-codegen-artifacts-v2' or
            manifest[1] != 'generation\tsha256:' +
            sha(('\n'.join(manifest[2:]) + '\n').encode())):
        raise ValueError('Invalid retained generated-source manifest')
    records = [line.split('\t') for line in manifest[2:]]
    units, outputs, all_entries = [], {}, set()
    for name, (samples, expected_sha) in UNITS.items():
        data = (source_root / 'code' / name).read_bytes()
        rows = [row for row in records if row[0] == 'code/' + name]
        if (sha(data) != expected_sha or len(rows) != 1 or
                rows[0][1:3] != [str(len(data)), 'sha256:' + expected_sha]):
            raise ValueError('Selected source/manifest identity mismatch: ' + name)
        entries = public_entries(data)
        if all_entries.intersection(entries):
            raise ValueError('Public entry overlaps another selected unit')
        all_entries.update(entries)
        outputs[name] = data
        units.append({'name': name, 'samples': samples, 'bytes': len(data),
                      'source_sha256': expected_sha, 'output_sha256': expected_sha,
                      'entries': entries})
    report = {
        'schema': 'sarecomp-compact-aot-v1', 'mode': args.mode,
        'archive_owner': 'sonic_compact_aot_' + args.mode,
        'profile_sha256': PROFILE_SHA, 'profile_exe_sha256': profile['exe_sha256'],
        'profile_total_samples': profile['samples'],
        'selection': 'top eight single-object AOT owners; inverse excluded; ties by name',
        'excluded_unit': EXCLUDED, 'selected_samples': sum(x['samples'] for x in units),
        'source_root': str(source_root), 'source_manifest_sha256': sha(manifest_raw),
        'source_manifest_generation': manifest[1], 'source_modification': 'none',
        'compile_contract': {
            'optimization': ['/O1', '/Ob1'] if args.mode == 'compact' else ['/O2', '/Ob2'],
            'floating_point': '/fp:strict', 'lto': False},
        'units': units, 'public_entries': len(all_entries),
    }
    outputs['preparation.json'] = (json.dumps(report, indent=2) + '\n').encode()
    # Check every destination before the first write; never write through links.
    for name in outputs:
        target = destination / name
        if target.is_symlink() or (target.exists() and
                (not target.is_file() or target.stat().st_nlink > 1)):
            raise ValueError('Refusing linked or nonregular output: ' + str(target))
    destination.mkdir(parents=True, exist_ok=True)
    for name, data in outputs.items():
        target = destination / name
        if not target.exists() or target.read_bytes() != data:
            target.write_bytes(data)
    print(f'SONIC_COMPACT_AOT_READY mode={args.mode} units=8 '
          f'entries={len(all_entries)} samples={report["selected_samples"]} source_changes=0')


def audit(args):
    report = json.loads(args.report.read_bytes())
    mode = report['mode']
    if (report['schema'] != 'sarecomp-compact-aot-v1' or mode not in ('control', 'compact') or
            report['archive_owner'] != 'sonic_compact_aot_' + mode or
            report['profile_sha256'] != PROFILE_SHA or
            [unit['name'] for unit in report['units']] != list(UNITS)):
        raise ValueError('Unexpected compact AOT provenance')
    expected = {}
    for unit in report['units']:
        name = unit['name']
        data = (args.report.parent / name).read_bytes()
        expected_sha = UNITS[name][1]
        if (sha(data) != expected_sha or unit['source_sha256'] != expected_sha or
                unit['output_sha256'] != expected_sha or
                public_entries(data) != unit['entries']):
            raise ValueError('Prepared source/provenance changed: ' + name)
        for entry in unit['entries']:
            if entry in expected:
                raise ValueError('Duplicate audited entry')
            expected[entry] = report['archive_owner'] + ':' + name.lower() + '.obj'
    seen = {entry: [] for entry in expected}
    pattern = re.compile(r'^\s*[0-9A-Fa-f]+:[0-9A-Fa-f]+\s+'
                         r'\?(fn_[0-9A-F]+_runtime_entry)@katana_port_generated@@')
    members = tuple(name.lower() + '.obj' for name in UNITS)
    with args.map.open(errors='strict') as stream:
        for line in stream:
            lower = line.lower()
            if ('katana_generated:' in lower or '.lto.katana_generated.' in lower):
                if any(member in lower for member in members):
                    raise ValueError('Original selected AOT member still linked: ' + line.strip())
            match = pattern.match(line)
            if match and match[1] in seen:
                seen[match[1]].append(line.split()[-1].lower())
    for entry, owners in seen.items():
        if owners != [expected[entry]]:
            raise ValueError(f'Compact AOT owner mismatch: {entry} owners={owners}')
    print(f'SONIC_COMPACT_AOT_LINK_PASS mode={mode} units=8 entries={len(seen)} '
          'original_selected_members=0')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('prepare')
    for name in ('source-root', 'destination', 'profile'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--mode', choices=('control', 'compact'), required=True)
    p.set_defaults(run=prepare)
    a = commands.add_parser('audit')
    a.add_argument('--map', type=Path, required=True)
    a.add_argument('--report', type=Path, required=True)
    a.set_defaults(run=audit)
    args = parser.parse_args()
    args.run(args)


if __name__ == '__main__':
    main()
