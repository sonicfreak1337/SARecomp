"""Borrow authenticated pure AOT FPU epochs; preserve admission and bookkeeping."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

EPOCH='                    katana::runtime::HostFpuExecutionEpoch katana_host_fpu_epoch(cpu);'
GUARD='''                if ((cpu.fpscr & (katana::runtime::fpscr_exception_enable_mask | katana::runtime::fpscr_dn_mask)) == katana::runtime::fpscr_dn_mask &&
                    (cpu.fpscr & katana::runtime::fpscr_rounding_mode_mask) <= 1u &&
                    (cpu.fpscr & katana::runtime::fpscr_pr_mask) == 0u) {
'''
TOKEN='\n                    sonic::fpu_region::BinaryRegion sonic_fpu_region(cpu, katana_host_fpu_epoch);'
BINARY=re.compile(r'katana::runtime::fpu_binary\(cpu, katana::runtime::FpuBinaryOperation::(Add|Subtract|Multiply|Divide), (\d+)u, (\d+)u\);')
CALLS={'if','guest_instruction_attempt','flush_release','reload_acquire','complete','fpul',
       'raise_fpu_disabled','raise_illegal_instruction',
       *('katana::runtime::'+name for name in (
        'relocate_code_address_inline','canonical_physical_address_inline','fpu_binary',
        'write_fpu_pair_bits','read_fpu_pair_bits','fpu_multiply_accumulate',
        'fpu_float_from_fpul','fpu_negate','fpu_transform_vector','fpu_absolute',
        'fpu_reciprocal_square_root','fpu_inner_product','fpu_sine_cosine','fpu_square_root'))}

def sha(data):return hashlib.sha256(data).hexdigest()
def write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)

def transform(source):
    pieces=[];position=0;regions=[]
    for match in re.finditer(re.escape(EPOCH),source):
        begin,end=match.start(),source.index('\n                } else {',match.end())
        if source[begin-len(GUARD):begin]!=GUARD:
            # The retained generator also has two precision-polymorphic runs.
            # Keep those byte-for-byte: the hardware helper requires PR=0.
            mixed_guard=GUARD.replace(' <= 1u &&\n                    (cpu.fpscr & katana::runtime::fpscr_pr_mask) == 0u',' <= 1u')
            if source[begin-len(mixed_guard):begin]==mixed_guard:continue
            raise ValueError('Epoch admission changed')
        body=source[match.end():end]
        # Everything inside the admitted branch must remain a closed FPU run.
        # No memory, external service, dispatch or unknown runtime helper.
        calls=set(re.findall(r'\b([a-zA-Z_]\w*(?:::\w+)*)\s*\(',body))
        if calls-CALLS:raise ValueError('Unknown calls inside epoch: '+repr(calls-CALLS))
        if body.count('{')!=body.count('}') or body.count('FPU epoch final instruction metadata')!=1:
            raise ValueError('Malformed FPU region')
        if not re.search(r'cpu\.pending_guest_cycles \+= \d+u;\s*$',body):
            raise ValueError('Missing final epoch accounting')
        operations=Counter()
        def replace(m):
            op,src,dst=m.groups()
            if max(int(src),int(dst))>=16:raise ValueError('Bad floating register')
            operations[op]+=1
            return f'sonic_fpu_region.binary<katana::runtime::FpuBinaryOperation::{op},{src},{dst}>();'
        candidate=BINARY.sub(replace,body)
        if operations:
            reverse=re.sub(r'sonic_fpu_region\.binary<katana::runtime::FpuBinaryOperation::(\w+),(\d+),(\d+)>\(\);',
                lambda m:f'katana::runtime::fpu_binary(cpu, katana::runtime::FpuBinaryOperation::{m[1]}, {m[2]}u, {m[3]}u);',candidate)
            if reverse!=body:raise ValueError('Changes outside binary calls')
            regions.append({'source_offset':begin,'body_sha256':sha(body.encode()),'operations':dict(operations)})
            pieces.append(source[position:match.end()]+TOKEN+candidate)
            position=end
    pieces.append(source[position:])
    result=''.join(pieces)
    reversed_source=result.replace(TOKEN,'')
    reversed_source=re.sub(r'sonic_fpu_region\.binary<katana::runtime::FpuBinaryOperation::(\w+),(\d+),(\d+)>\(\);',
        lambda m:f'katana::runtime::fpu_binary(cpu, katana::runtime::FpuBinaryOperation::{m[1]}, {m[2]}u, {m[3]}u);',reversed_source)
    if reversed_source!=source:raise ValueError('Unexpected change outside pure epoch calls')
    return '#include "sonic_fpu_region.hpp"\n'+result,regions

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root',type=Path,required=True)
    p.add_argument('--destination',type=Path,required=True)
    p.add_argument('--helper',type=Path,required=True)
    p.add_argument('--unit',action='append',default=[])
    a=p.parse_args();root=a.source_root.resolve();destination=a.destination.resolve()
    if root==destination or root in destination.parents or destination in root.parents:raise ValueError('Separate output required')
    manifest=(root/'.katana-generated-artifacts').read_text().splitlines()
    if manifest[0]!='katana-codegen-artifacts-v2' or manifest[1]!='generation\tsha256:'+sha(('\n'.join(manifest[2:])+'\n').encode()):
        raise ValueError('Invalid retained source manifest')
    if len(a.unit)!=len(set(a.unit)):raise ValueError('Duplicate member')
    records={r[0]:r for line in manifest[2:] if len(r:=line.split('\t'))>=3}
    report={'schema':'sarecomp-fpu-regions-v1','manifest_generation':manifest[1],
            'helper_sha256':sha(a.helper.read_bytes()),'units':[]}
    for name in a.unit:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp',name):raise ValueError('Invalid member')
        data=(root/'code'/name).read_bytes()
        if records.get('code/'+name,[])[1:3]!=[str(len(data)),'sha256:'+sha(data)]:raise ValueError('Member identity mismatch')
        transformed,regions=transform(data.decode());output=transformed.encode()
        report['units'].append({'name':name,'source_sha256':sha(data),'output_sha256':sha(output),'regions':regions})
        write(destination/name,output)
    write(destination/'preparation.json',(json.dumps(report,indent=2)+'\n').encode())
    print('SONIC_FPU_REGIONS_READY units='+str(len(report['units']))+' regions='+str(sum(len(u['regions']) for u in report['units']))+
          ' binary_sites='+str(sum(sum(r['operations'].values()) for u in report['units'] for r in u['regions'])))

if __name__=='__main__':main()
