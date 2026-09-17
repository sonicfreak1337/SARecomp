"""Inventory retained instruction shapes excluded from native RAM prefixes."""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import re


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--preparation', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    spec = importlib.util.spec_from_file_location('regions', Path(__file__).with_name('prepare-ram-regions.py'))
    regions = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(regions)
    prepared = json.loads(args.preparation.read_text())
    totals, unsupported, examples = Counter(), Counter(), {}
    for unit in prepared['units']:
        data = (args.source_root / 'code' / unit['unit']).read_bytes()
        if hashlib.sha256(data).hexdigest() != unit['source_sha256']:
            raise ValueError('Retained source changed: ' + unit['unit'])
        source = data.decode()
        totals['units'] += 1
        totals['existing_regions'] += len(unit['frame_sequences'])
        totals['existing_region_instructions'] += sum(x['instructions'] for x in unit['frame_sequences'])
        for match in regions.START.finditer(source):
            end = source.find('\n' + match[1] + '}', match.end())
            if end < 0:
                raise ValueError('Unclosed instruction')
            block = source[match.start():end + 2 + len(match[1])]
            totals['ordinary_instruction_blocks'] += 1
            if regions.classify(block):
                totals['classified_blocks'] += 1
                continue
            origin = re.search(r'const katana::runtime::GuestInstructionOrigin guest_origin\{[^\n]+\};\n', block)
            finish = block.find('guest_instruction_attempt.complete();')
            body = block[origin.end():finish].strip() if origin and finish > origin.end() else block
            body = re.sub(r'//[^\n]*', '', body)
            body = re.sub(r'0x[0-9A-Fa-f]+u|\b\d+u\b', 'N', body)
            body = re.sub(r'katana_registers\[\d+\]', 'REG', body)
            body = re.sub(r'cpu\.fr\[\d+\]', 'FR', body)
            body = ' '.join(body.split())
            unsupported[body] += 1
            examples.setdefault(body, {'unit': unit['unit'], 'pc': match[2], 'source': block})
    result = {'scope': 'static retained source shapes, not dynamic coverage or FPS',
              'totals': dict(totals), 'unsupported': [
                  {'count': count, 'shape': body, **examples[body]}
                  for body, count in unsupported.most_common()]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'totals': dict(totals), 'top': [
        {'count': count, 'shape': body[:320], 'pc': examples[body]['pc']}
        for body, count in unsupported.most_common(16)]}, indent=2))


if __name__ == '__main__':
    main()
