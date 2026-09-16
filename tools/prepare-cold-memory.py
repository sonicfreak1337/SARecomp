"""Outline only exact generated register-release memory fallback bodies."""
import argparse
import hashlib
import json
from pathlib import Path
import re

READ=re.compile(r'(?P<indent>[ \t]*)const bool katana_reacquire_registers = katana_registers.owns_registers\(\);\n'
    r'(?P=indent)katana_registers.flush_release\(\);\n'
    r'(?P=indent)const auto katana_fallback_value = static_cast<std::uint32_t>\(katana::runtime::guest_read_(?P<width>s8|s16|u32)_at\(cpu, katana_origin, katana_ram_address\)\);\n'
    r'(?P=indent)if \(katana_reacquire_registers\) katana_registers.reload_acquire\(\);\n'
    r'(?P=indent)return katana_fallback_value;')
WRITE=re.compile(r'(?P<indent>[ \t]*)const bool katana_reacquire_registers = katana_registers.owns_registers\(\);\n'
    r'(?P=indent)katana_registers.flush_release\(\);\n'
    r'(?P=indent)katana::runtime::guest_write_(?P<width>u8|u16|u32)_at\(cpu, katana_origin, katana_ram_address,\n'
    r'[ \t]*katana_ram_value, katana_source\);\n'
    r'(?P=indent)if \(katana_reacquire_registers\) katana_registers.reload_acquire\(\);')

def sha(data):return hashlib.sha256(data).hexdigest()
def write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)

def transform(source):
    replacements=[]
    counts={'reads':0,'writes':0}
    for kind,pattern in [('reads',READ),('writes',WRITE)]:
        for m in pattern.finditer(source):
            # Require the original generated helper declaration. The replacement
            # must not accidentally match another owner or instruction body.
            helper='katana_direct_ram_'+('read_'+m['width'] if kind=='reads' else 'write_'+m['width'])
            last=source.rfind('\n    const auto ',0,m.start())+1
            declaration=source[last:source.index('=',last)].strip()
            if declaration!='const auto '+helper:raise ValueError('Unknown fallback owner')
            if kind=='reads':
                new=m['indent']+'return sonic::cold_memory::read_'+m['width']+'(cpu, katana_registers, katana_origin, katana_ram_address);'
            else:
                new=m['indent']+'sonic::cold_memory::write(cpu, katana_registers, katana_origin, katana_ram_address, katana_ram_value, katana_source);'
            replacements.append((m.start(),m.end(),new,m[0]))
            counts[kind]+=1
    if not replacements:raise ValueError('No exact memory fallbacks')
    output=source
    for begin,end,new,old in sorted(replacements,reverse=True):
        if output[begin:end]!=old:raise ValueError('Overlapping replacements')
        output=output[:begin]+new+output[end:]
    return '#include "sonic_cold_memory.hpp"\n'+output,counts

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root',type=Path,required=True)
    p.add_argument('--destination',type=Path,required=True)
    p.add_argument('--unit',action='append',required=True)
    a=p.parse_args()
    root,dest=a.source_root.resolve(),a.destination.resolve()
    if root==dest or root in dest.parents or dest in root.parents:raise ValueError('Outputs overlap retained source')
    lines=(root/'.katana-generated-artifacts').read_text().splitlines()
    if lines[0]!='katana-codegen-artifacts-v2' or lines[1]!='generation\tsha256:'+sha(('\n'.join(lines[2:])+'\n').encode()):
        raise ValueError('Invalid retained source manifest')
    records={parts[0]:parts for line in lines[2:] if len(parts:=line.split('\t'))>=3}
    if len(a.unit)!=len(set(a.unit)):raise ValueError('Duplicate source member')
    report={'schema':'sarecomp-cold-memory-v1','generation':lines[1],'units':[]}
    for unit in a.unit:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp',unit):raise ValueError('Invalid source member')
        data=(root/'code'/unit).read_bytes()
        if records.get('code/'+unit,[])[1:3]!=[str(len(data)),'sha256:'+sha(data)]:raise ValueError('Source identity mismatch')
        output,counts=transform(data.decode())
        encoded=output.encode()
        write(dest/unit,encoded)
        report['units'].append({'unit':unit,'source_sha256':sha(data),'output_sha256':sha(encoded),**counts})
    write(dest/'preparation.json',(json.dumps(report,indent=2)+'\n').encode())
    print('SONIC_COLD_MEMORY_READY units='+str(len(report['units'])))
if __name__=='__main__':main()
