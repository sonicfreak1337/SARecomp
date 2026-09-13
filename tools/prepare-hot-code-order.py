"""Prepare a private, opt-in link order from multiple resolved CPU profiles.

Only exact function-owner symbols that still exist in the current map qualify.
This changes layout, not generated instructions or the SDK/AOT archives.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', type=Path, action='append', required=True)
    parser.add_argument('--map', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if len(args.profile) < 2:
        parser.error('Use at least two independent scenarios, not one hot route.')
    weights = Counter()
    occurrences = Counter()
    samples = Counter()
    identities = []
    for path in args.profile:
        profile = json.loads(path.read_text(encoding='utf-8'))
        counts = Counter()
        for entry in profile['entries']:
            # Nearest public symbols may be unwind strings or unrelated code.
            # Admit only the owning .pdata function recovered by the resolver.
            primary = (entry.get('runtime_function') or {}).get('primary')
            if not primary:
                continue
            symbols = primary.get('symbols') or []
            if symbols:
                counts[symbols[0]] += entry['count']
        if not counts:
            parser.error(f'{path} has no resolved runtime-function owners')
        total = sum(counts.values())
        for symbol, count in counts.items():
            weights[symbol] += count / total / len(args.profile)
            samples[symbol] += count
            occurrences[symbol] += 1
        identities.append({'path': str(path), 'exe_sha256': profile['exe_sha256'],
                           'profile_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                           'owned_samples': total})
    wanted = {symbol for symbol in weights
              if occurrences[symbol] >= 2 or samples[symbol] >= 5}
    addresses = {}
    code_sections = set()
    with args.map.open(encoding='utf-8', errors='strict') as stream:
        for line in stream:
            fields = line.split(None, 5)
            if len(fields) == 4 and fields[-1] == 'CODE':
                code_sections.add(fields[0].split(':')[0])
            if (len(fields) >= 4 and fields[1] in wanted
                    and fields[0].split(':')[0] in code_sections):
                addresses.setdefault(fields[1], int(fields[2], 16))
    ordered = sorted(addresses, key=lambda symbol: (-weights[symbol], symbol))
    if not ordered:
        parser.error('No sampled code symbols matched the current linker map')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text('\n'.join(ordered) + '\n', encoding='ascii')
    with args.map.open('rb') as map_stream:
        map_sha256 = hashlib.file_digest(map_stream, 'sha256').hexdigest()
    report = {'profiles': identities, 'map': str(args.map),
              'requested_functions': len(wanted), 'matched_functions': len(ordered),
              'missing_symbols': sorted(wanted - addresses.keys()),
              'map_sha256': map_sha256,
              'functions': [{'symbol': s, 'weight': weights[s], 'profiles': occurrences[s],
                             'samples': samples[s], 'previous_va': hex(addresses[s])}
                            for s in ordered]}
    args.output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'SONIC_CODE_ORDER_READY functions={len(ordered)} missing={len(wanted)-len(ordered)}')


if __name__ == '__main__':
    main()
