"""Retain owned integer registers across authenticated, nontrapping FPU calls."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

GENERATION='sha256:ad51236f53b465bcac54c915df97ecdcb6467f8f06e5b36f85885e19525129f1'
RUNTIME_SHA='1b5ced39e89c7a2ca2b223218538a8dcde933d2374234e2dca4ce6f212ef31eb'
STATE_SHA='9282825fed1e356fa08ded9c2359a08a85145a31dff46fc8a6dba1646466dc62'
HEADER='#include "sonic_fpu_register_cache.hpp"\n'
START=re.compile(r'(?m)^( +)\{\n\1    // katana-guest 0x([0-9A-F]{8})u\n\1    katana_registers.flush_release\(\);')
SHAPES={
 '32d0a8a1e4d691b68ce95522d9903113c0d38f4587aa29f831b2967b82b1c3ba',
 '03aafeae769853789a583b03af9f8497b28bf4cf7bdc66da948c4cc5373ad04e',
 '899bbdb70e14a7421233938476922535819f1f0568a66f54f11081a531af31ca',
 '3faa80f32e602776d3831f65dbd34f46d98becf4f3e731dc5fdec4c676226679',
 'a5c4403373dbf33046c273e8ae952a17ba09808e54ee1144d1927ec7deb0fd93',
 'af5d156afc1aeef5e55cbafdd3f1ceb7be0fc1c8e6f8a3a3b13a8988db65b402',
 '22e81245d7fa747169e6f77fb66a6745be477f54e6912d6becbccd486b1254e7',
 'b01679c604e19f38d42b42ea93fce61d3545a3d3cca72e817ddcb8d6ae8b2529',
 'ff9b14823118a12cd1c59ca00c6829edd8d297a9f5fdce39a9f27105ec363663',
}

def sha(data):return hashlib.sha256(data).hexdigest()
def write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)

def transform(source):
    edits=[];sites=[]
    for m in START.finditer(source):
        indent,pc=m.groups()
        end=re.compile(r'(?m)^'+re.escape(indent)+r'\}').search(source,m.end())
        if not end:raise ValueError('Unclosed FPU envelope')
        body=source[m.start():end.end()]
        names=re.findall(r'katana::runtime::(fpu_\w+)\(',body)
        if len(names)!=1 or names[0] not in ('fpu_binary','fpu_square_root','fpu_reciprocal_square_root'):continue
        if body.count('katana_registers.reload_acquire();')!=1:continue
        normalized='\n'.join(line.strip() for line in body.splitlines())
        normalized=re.sub(r'0x[0-9A-F]{8}u','PC',normalized)
        normalized=re.sub(r'FpuBinaryOperation::(?:Add|Subtract|Multiply|Divide)','FpuBinaryOperation::OP',normalized)
        normalized=re.sub(r'(?<=[, ])(?:[0-9]+)u','REG',normalized)
        if sha(normalized.encode()) not in SHAPES:raise ValueError('Unreviewed FPU envelope at '+pc)
        prefix=(indent+'    const bool sonic_keep_fpu_registers =\n'+
                indent+'        sonic::fpu_register_cache::enabled() && katana_registers.owns_registers() &&\n'+
                indent+'        sonic::fpu_register_cache::admitted(cpu);\n')
        old=indent+'    katana_registers.flush_release();'
        new=prefix+indent+'    if (!sonic_keep_fpu_registers) katana_registers.flush_release();'
        output=body.replace(old,new,1).replace('katana_registers.reload_acquire();',
            'if (!sonic_keep_fpu_registers) katana_registers.reload_acquire();',1)
        reverse=output.replace(new,old,1).replace('if (!sonic_keep_fpu_registers) katana_registers.reload_acquire();',
            'katana_registers.reload_acquire();',1)
        if reverse!=body:raise ValueError('Changed an operation or boundary')
        edits.append((m.start(),end.end(),output))
        sites.append({'pc':pc,'operation':names[0],'envelope_sha256':sha(body.encode())})
    result=source
    for begin,end,output in reversed(edits):result=result[:begin]+output+result[end:]
    return (HEADER+result if edits else result),sites

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root',type=Path,required=True)
    p.add_argument('--destination',type=Path,required=True)
    p.add_argument('--runtime',type=Path,required=True)
    p.add_argument('--state-header',type=Path,required=True)
    p.add_argument('--units-file',type=Path,required=True)
    a=p.parse_args();root=a.source_root.resolve();dest=a.destination.resolve()
    if root==dest or root in dest.parents or dest in root.parents:raise ValueError('Separate output required')
    lines=(root/'.katana-generated-artifacts').read_text().splitlines()
    if lines[:2]!=['katana-codegen-artifacts-v2','generation\t'+GENERATION] or GENERATION!='sha256:'+sha(('\n'.join(lines[2:])+'\n').encode()):
        raise ValueError('Unreviewed source generation')
    if sha(a.runtime.read_bytes())!=RUNTIME_SHA or sha(a.state_header.read_bytes())!=STATE_SHA:
        raise ValueError('FPU/register-cache contract changed')
    records={r[0]:r for line in lines[2:] if len(r:=line.split('\t'))>=3}
    units=a.units_file.read_text().splitlines()
    if len(units)!=len(set(units)):raise ValueError('Duplicate unit')
    report={'schema':'sarecomp-fpu-register-cache-v1','generation':GENERATION,
        'runtime_sha256':RUNTIME_SHA,'register_state_sha256':STATE_SHA,'units':[]}
    totals=Counter()
    for unit in units:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp',unit):raise ValueError('Invalid unit')
        data=(root/'code'/unit).read_bytes()
        if records.get('code/'+unit,[])[1:3]!=[str(len(data)),'sha256:'+sha(data)]:raise ValueError('Source changed')
        output,sites=transform(data.decode());payload=output.encode()
        totals.update(s['operation'] for s in sites)
        report['units'].append({'unit':unit,'source_sha256':sha(data),'output_sha256':sha(payload),'sites':sites})
        write(dest/unit,payload)
    report['totals']=dict(totals)
    write(dest/'preparation.json',(json.dumps(report,indent=2)+'\n').encode())
    print('SONIC_FPU_REGISTER_CACHE_READY units='+str(len(units))+' sites='+str(sum(totals.values())))
if __name__=='__main__':main()
