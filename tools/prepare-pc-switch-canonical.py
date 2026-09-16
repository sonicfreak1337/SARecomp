"""Fold exact P1/P2 case pairs in copied AOT sources; preserve all PC outcomes."""
import argparse
import hashlib
import json
from pathlib import Path
import re

EXPRESSION = 'katana::runtime::unrelocate_code_address_inline(cpu.pc)'
START = re.compile(r'(?m)^(?P<indent>[ \t]*)switch \('+re.escape(EXPRESSION)+r'\) \{\n')
PAIR = re.compile(r'(?P<first>[ \t]*case 0x(?P<a>[0-9A-F]{8})u:\n)'
                  r'(?P<second>[ \t]*case 0x(?P<b>[0-9A-F]{8})u:\n)'
                  r'(?P<action>[ \t]*(?:goto katana_block_[0-9A-F]+(?:_resume)?|break);\n)')
DEFAULT = re.compile(r'[ \t]*default:\n[ \t]*throw std::runtime_error\('
                     r'"PC liegt ausserhalb (?:der generierten Funktion|des generierten IR-Blocks)"\);\n')
MASK = 0xDFFFFFFF

def sha(data):
    return hashlib.sha256(data).hexdigest()

def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)

def transform(source):
    edits = []
    report = {'switches': 0, 'original_labels': 0, 'canonical_labels': 0}
    for start in START.finditer(source):
        end = re.compile(r'(?m)^'+re.escape(start['indent'])+r'\}').search(source, start.end())
        if not end:
            raise ValueError('Unclosed PC switch')
        body = source[start.end():end.start()]
        pairs = list(PAIR.finditer(body))
        remainder = PAIR.sub('', body)
        if not pairs or not DEFAULT.fullmatch(remainder):
            raise ValueError('Unknown PC-switch grammar')
        seen, reduced, replacements = {}, {}, []
        for pair in pairs:
            a, b = int(pair['a'],16), int(pair['b'],16)
            action = pair['action'].strip()
            if a ^ b != 0x20000000 or a & MASK != b or a >> 24 != 0xAC:
                raise ValueError('Not an exact ordered P2/P1 pair')
            if a in seen or b in seen or b in reduced:
                raise ValueError('Duplicate PC label')
            seen[a] = seen[b] = action
            reduced[b] = action
            # f(x)=x&MASK has exactly two 32-bit preimages for each retained
            # label. Reconstructing all of them proves the default domain as
            # well as every accepted address; no sampled-address assumption.
            replacements.append((start.end()+pair.start('first'),
                                 start.end()+pair.end('first'), '', pair['first']))
        reconstructed = {value: action for pc, action in reduced.items()
                         for value in (pc, pc ^ 0x20000000)}
        if reconstructed != seen:
            raise ValueError('Canonicalization changes a PC outcome')
        expression_begin = source.index(EXPRESSION, start.start(), start.end())
        replacements.append((expression_begin, expression_begin+len(EXPRESSION),
                             EXPRESSION+' & 0xDFFFFFFFu', EXPRESSION))
        edits.extend(replacements)
        report['switches'] += 1
        report['original_labels'] += len(seen)
        report['canonical_labels'] += len(reduced)
    if report['switches'] != source.count('switch ('+EXPRESSION+')'):
        raise ValueError('An original PC switch was not qualified')
    if not edits:
        raise ValueError('No qualified PC switches')
    output = source
    for begin, end, replacement, original in sorted(edits, reverse=True):
        if output[begin:end] != original:
            raise ValueError('Overlapping or stale source replacement')
        output = output[:begin]+replacement+output[end:]
    return output, report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--destination', type=Path, required=True)
    p.add_argument('--unit', action='append', required=True)
    a=p.parse_args()
    root, destination=a.source_root.resolve(),a.destination.resolve()
    if root==destination or root in destination.parents or destination in root.parents:
        raise ValueError('Outputs must remain separate from retained source')
    lines=(root/'.katana-generated-artifacts').read_text().splitlines()
    if lines[0]!='katana-codegen-artifacts-v2' or lines[1]!='generation\tsha256:'+sha(('\n'.join(lines[2:])+'\n').encode()):
        raise ValueError('Invalid retained source manifest')
    records={parts[0]:parts for line in lines[2:] if len(parts:=line.split('\t'))>=3}
    if len(a.unit)!=len(set(a.unit)):
        raise ValueError('Duplicate source member')
    report={'schema':'sarecomp-pc-switch-canonical-v1','generation':lines[1], 'units':[]}
    for unit in a.unit:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp',unit):
            raise ValueError('Invalid source member')
        data=(root/'code'/unit).read_bytes()
        if records.get('code/'+unit,[])[1:3]!=[str(len(data)),'sha256:'+sha(data)]:
            raise ValueError('Source identity mismatch: '+unit)
        transformed,counts=transform(data.decode())
        output=transformed.encode()
        write(destination/unit,output)
        report['units'].append({'unit':unit,'source_sha256':sha(data),'output_sha256':sha(output),**counts})
    write(destination/'preparation.json',(json.dumps(report,indent=2)+'\n').encode())
    print('SONIC_PC_SWITCH_CANONICAL_READY units='+str(len(report['units']))+
          ' switches='+str(sum(u['switches'] for u in report['units'])))

if __name__=='__main__':
    main()
