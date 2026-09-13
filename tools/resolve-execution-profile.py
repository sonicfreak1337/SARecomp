"""Resolve private sampled RIPs with the matching retained LLD map, streaming it.

Nearest public symbol is attribution evidence, not a stack/call-count profile.
Folded COMDAT aliases are preserved instead of assigning them to one owner.
"""
import argparse
from bisect import bisect_left
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct

symbol_row=re.compile(r'^\s*0001:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{16})\s+(.*)$')

def resolve(lines, targets):
    targets=sorted(targets);best={};current=None
    def flush(end=None):
        if current is None:return
        start=current['address'];a=bisect_left(targets,start);b=len(targets) if end is None else bisect_left(targets,end)
        for target in targets[a:b]:
            old=best.get(target)
            if old is None or old['address']<start:
                best[target]={**current,'symbols':set(current['symbols']),'objects':set(current['objects'])}
            elif old['address']==start:
                old['symbols'].update(current['symbols']);old['objects'].update(current['objects'])
    for line in lines:
        m=symbol_row.match(line)
        if not m:continue
        symbol,address,owner=m.groups();address=int(address,16)
        if current is not None and current['address']!=address:flush(address if address>current['address'] else None)
        if current is None or current['address']!=address:current={'address':address,'symbols':set(),'objects':set()}
        current['symbols'].add(symbol);current['objects'].add(owner.strip())
    flush();return best

def self_test():
    rows=[' 0001:00000000 a 0000000140001000 a.obj\n',' 0001:00000000 alias 0000000140001000 b.obj\n',
          ' 0001:00000020 next 0000000140001020 c.obj\n',' 0002:00000000 data 0000000140001030 data.obj\n',
          ' 0001:00000010 static 0000000140001010 static.obj\n']
    got=resolve(rows,[0x140000fff,0x140001000,0x140001008,0x140001018,0x140001020,0x140001030])
    assert 0x140000fff not in got
    assert got[0x140001008]['symbols']=={'a','alias'} and got[0x140001008]['objects']=={'a.obj','b.obj'}
    assert got[0x140001018]['symbols']=={'static'} and got[0x140001020]['symbols']=={'next'}
    assert got[0x140001030]['symbols']=={'next'}
    print('EXECUTION_RESOLVER_TEST_OK boundaries aliases unordered_sections data_excluded')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profile',nargs='?',type=Path)
    parser.add_argument('--map',type=Path)
    parser.add_argument('--exe',type=Path)
    parser.add_argument('--self-test',action='store_true')
    args=parser.parse_args()
    if args.self_test:self_test();return
    if not all((args.profile,args.map,args.exe)):parser.error('profile, --map and --exe required')
    profile=json.loads(args.profile.read_text());benchmark=json.loads((args.profile.parent/'result.json').read_text())
    with args.exe.open('rb') as f:
        digest=hashlib.file_digest(f,'sha256').hexdigest()
        f.seek(60);pe=struct.unpack('<I',f.read(4))[0];f.seek(pe)
        if f.read(4)!=b'PE\0\0':raise ValueError('not a PE executable')
        f.seek(pe+8);timestamp=struct.unpack('<I',f.read(4))[0]
        f.seek(pe+24+56);image_size=struct.unpack('<I',f.read(4))[0]
    if digest!=benchmark['exe_sha256']:raise ValueError('executable differs from the benchmark')
    if str(profile['tid']) not in {r.get('execution_thread_id') for r in benchmark['gameplay_samples']}:
        raise ValueError('sampled thread is not the recorded execution thread')
    game=next(m for m in profile['modules'] if m['name'].lower()=='game.exe')
    if game['size']!=image_size:raise ValueError('sampled image size mismatch')
    with args.map.open(errors='replace') as f:
        header=''.join(next(f) for _ in range(8))
    stamp=re.search(r'Timestamp is ([0-9a-fA-F]+)',header);base=re.search(r'Preferred load address is ([0-9a-fA-F]+)',header)
    if not stamp or int(stamp[1],16)!=timestamp or not base:raise ValueError('map/executable timestamp mismatch')
    preferred=int(base[1],16);targets={preferred+r['ip']-game['base']:r['count'] for r in profile['ips'] if game['base']<=r['ip']<game['base']+game['size']}
    stack_targets={preferred+frame['ip']-game['base']-(index!=0) for stack in profile.get('stacks',[]) for index,frame in enumerate(stack['frames'])
        if game['base']<=frame['ip']<game['base']+game['size']}
    with args.map.open(errors='replace') as f:found=resolve(f,set(targets)|stack_targets)
    entries=[];objects=Counter();modules=Counter()
    for row in profile['ips']:
        module=next((m['name'] for m in profile['modules'] if m['base']<=row['ip']<m['base']+m['size']),'<unknown>');modules[module]+=row['count']
    for address,count in targets.items():
        entry=found.get(address);owner='<unresolved>' if entry is None else next(iter(entry['objects'])) if len(entry['objects'])==1 else '<folded multi-object aliases>'
        objects[owner]+=count
        entries.append({'rva':hex(address-preferred),'count':count,'symbol_offset':address-entry['address'] if entry else None,
            'symbols':sorted(entry['symbols']) if entry else [],'objects':sorted(entry['objects']) if entry else []})
    stacks=[]
    for stack in profile.get('stacks',[]):
        frames=[]
        for index,frame in enumerate(stack['frames']):
            module=next((m for m in profile['modules'] if m['base']<=frame['ip']<m['base']+m['size']),None)
            resolved={'ip':frame['ip'],'rva':hex(frame['ip']-module['base']) if module else None,
                'module':module['name'] if module else '<unknown>','export_symbol':frame['symbol']}
            if module==game:
                entry=found.get(preferred+frame['ip']-game['base']-(index!=0))
                resolved.update(symbols=sorted(entry['symbols'])[:8] if entry else [],
                    alias_count=len(entry['symbols']) if entry else 0,objects=sorted(entry['objects']) if entry else [])
            frames.append(resolved)
        stacks.append({'elapsed_ms':stack['elapsed_ms'],'frames':frames})
    output=args.profile.with_name('execution-ip-resolved.json')
    if output.exists():raise ValueError('resolved output must be new')
    result={'schema':'sarecomp-execution-ip-resolved-v1','exe_sha256':digest,'pid':profile['pid'],'tid':profile['tid'],
        'samples':profile['samples'],'elapsed_ms':profile['elapsed_ms'],'suspension_ms':profile['suspension_ms'],
        'max_suspension_ms':profile['max_suspension_ms'],'modules':modules.most_common(),'objects':objects.most_common(),
        'attribution':'nearest public code symbol; aliases retained; not exclusive CPU percentages or call counts',
        'entries':sorted(entries,key=lambda e:e['count'],reverse=True),'stacks':stacks}
    output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('entries','stacks')},indent=2))

if __name__=='__main__':main()
