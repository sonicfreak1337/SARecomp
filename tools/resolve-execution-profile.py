"""Resolve private sampled RIPs with the matching retained LLD map, streaming it.

Nearest public symbol is attribution evidence, not a stack/call-count profile.
Folded COMDAT aliases are preserved instead of assigning them to one owner.
"""
import argparse
from bisect import bisect_left, bisect_right
from collections import Counter
import hashlib
import io
import json
from pathlib import Path
import re
import struct

symbol_row=re.compile(r'^\s*0001:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{16})\s+(.*)$')


class PeRuntimeFunctions:
    """Bounded, seek-only AMD64 PE exception-directory reader (never loads .text)."""

    def __init__(self, stream):
        self.stream=stream
        stream.seek(0,2);self.file_size=stream.tell()
        dos=self.read_at(0,64)
        if dos[:2]!=b'MZ':raise ValueError('not a DOS/PE executable')
        pe=struct.unpack_from('<I',dos,60)[0]
        header=self.read_at(pe,24)
        if pe<64 or header[:4]!=b'PE\0\0':raise ValueError('invalid PE signature/offset')
        machine,nsections,self.timestamp=struct.unpack_from('<HHI',header,4)
        optional_size=struct.unpack_from('<H',header,20)[0]
        if machine!=0x8664 or not 1<=nsections<=96:raise ValueError('expected AMD64 PE with valid section count')
        optional=self.read_at(pe+24,optional_size)
        if len(optional)<112 or struct.unpack_from('<H',optional)[0]!=0x20b:raise ValueError('expected PE32+ optional header')
        self.image_base=struct.unpack_from('<Q',optional,24)[0]
        self.image_size,self.header_size=struct.unpack_from('<II',optional,56)
        directories=struct.unpack_from('<I',optional,108)[0]
        if (not self.image_size or not 0<self.header_size<=min(self.file_size,self.image_size)
                or directories>(optional_size-112)//8):raise ValueError('invalid PE image/header/directory bounds')
        table_end=pe+24+optional_size+nsections*40
        if table_end>self.header_size:raise ValueError('section table exceeds headers')
        self.sections=[]
        for index in range(nsections):
            row=self.read_at(pe+24+optional_size+index*40,40)
            virtual_size,va,raw_size,raw=struct.unpack_from('<IIII',row,8)
            span=max(virtual_size,raw_size)
            if not span or va<self.header_size or va+span>self.image_size:raise ValueError('invalid section RVA bounds')
            if raw_size and (raw<self.header_size or raw+raw_size>self.file_size):raise ValueError('invalid section file bounds')
            self.sections.append((va,va+span,raw,raw_size))
        self.sections.sort()
        if any(a[1]>b[0] for a,b in zip(self.sections,self.sections[1:])):raise ValueError('overlapping section RVAs')
        raw_spans=sorted((raw,raw+size) for _,_,raw,size in self.sections if size)
        if any(a[1]>b[0] for a,b in zip(raw_spans,raw_spans[1:])):raise ValueError('overlapping section file spans')
        self.functions=[];self.cache={}
        if directories>3:
            rva,size=struct.unpack_from('<II',optional,112+3*8)
            if bool(rva)!=bool(size) or size%12:raise ValueError('invalid exception-directory size/address')
            if size:
                offset=self.rva_offset(rva,size)
                for index in range(size//12):
                    function=struct.unpack('<III',self.read_at(offset+index*12,12))
                    self.validate_function(function)
                    if self.functions and self.functions[-1][1]>function[0]:raise ValueError('unsorted/overlapping runtime functions')
                    self.functions.append(function)
        self.starts=[f[0] for f in self.functions]

    def read_at(self, offset, size):
        if offset<0 or size<0 or offset>self.file_size or size>self.file_size-offset:
            raise ValueError('PE file read out of bounds')
        self.stream.seek(offset);data=self.stream.read(size)
        if len(data)!=size:raise ValueError('truncated PE read')
        return data

    def rva_offset(self, rva, size):
        if rva<0 or size<=0 or rva>=self.image_size or size>self.image_size-rva:
            raise ValueError('PE RVA read out of image bounds')
        if rva<self.header_size:
            if size>self.header_size-rva:raise ValueError('PE RVA read crosses headers')
            return rva
        for start,end,raw,raw_size in self.sections:
            if start<=rva<end:
                delta=rva-start
                if size>raw_size-delta:raise ValueError('PE RVA read is not fully file backed')
                return raw+delta
        raise ValueError('PE RVA read outside sections')

    def read_rva(self, rva, size):
        return self.read_at(self.rva_offset(rva,size),size)

    def validate_function(self, function):
        begin,end,unwind=function
        if not 0<begin<end<=self.image_size or not unwind or unwind%4:
            raise ValueError('invalid runtime-function bounds/unwind alignment')
        self.rva_offset(begin,end-begin)
        self.rva_offset(unwind,4)

    def lookup(self, rva):
        index=bisect_right(self.starts,rva)-1
        if index<0 or rva>=self.functions[index][1]:return None
        initial=self.functions[index]
        if initial in self.cache:return self.cache[initial]
        chain=[];seen=set();function=initial
        while True:
            if function in seen or len(chain)>=64:raise ValueError('runtime-function unwind chain cycle/depth limit')
            seen.add(function);self.validate_function(function)
            begin,end,unwind=function
            version_flags,prolog,codes,frame=struct.unpack('<BBBB',self.read_rva(unwind,4))
            version=version_flags&7;flags=version_flags>>3
            if version not in (1,2) or flags&~7 or (flags&4 and flags&3):
                raise ValueError('unsupported/invalid x64 unwind version or flags')
            # UNWIND_CODE entries are two bytes; the optional tail is DWORD aligned.
            tail=4+2*((codes+1)&~1)
            self.read_rva(unwind,tail)
            chain.append({'begin_rva':hex(begin),'end_rva':hex(end),'unwind_rva':hex(unwind),
                          'unwind_version':version,'unwind_flags':flags})
            if flags&4:
                function=struct.unpack('<III',self.read_rva(unwind+tail,12))
                continue
            if flags&3:
                handler=struct.unpack('<I',self.read_rva(unwind+tail,4))[0]
                if not handler:raise ValueError('null unwind handler RVA')
                self.rva_offset(handler,1)
            result={'containing':chain[0],'primary':chain[-1],'chain':chain,'chain_depth':len(chain)-1}
            self.cache[initial]=result
            return result


def runtime_attribution(function, found, preferred):
    if function is None:return None
    result=dict(function)
    for name in ('containing','primary'):
        record=dict(function[name]);address=preferred+int(record['begin_rva'],16);entry=found.get(address)
        record.update(symbol_offset=address-entry['address'] if entry else None,
                      symbol_match=('exact_address' if entry['address']==address else 'nearest_preceding') if entry else 'unresolved',
                      symbols=sorted(entry['symbols']) if entry else [],objects=sorted(entry['objects']) if entry else [])
        result[name]=record
    return result

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
    # Three file-backed sections, one primary function and a chained secondary
    # range. An odd unwind-code count exercises the DWORD tail padding.
    fixture=bytearray(0x800);fixture[:2]=b'MZ';struct.pack_into('<I',fixture,60,0x80)
    fixture[0x80:0x84]=b'PE\0\0';struct.pack_into('<HHI',fixture,0x84,0x8664,3,123)
    struct.pack_into('<H',fixture,0x94,240);optional=0x98
    struct.pack_into('<H',fixture,optional,0x20b);struct.pack_into('<Q',fixture,optional+24,0x140000000)
    struct.pack_into('<II',fixture,optional+56,0x4000,0x200)
    struct.pack_into('<I',fixture,optional+108,16);struct.pack_into('<II',fixture,optional+136,0x2000,24)
    for index,(va,raw) in enumerate(((0x1000,0x200),(0x2000,0x400),(0x3000,0x600))):
        struct.pack_into('<IIII',fixture,optional+240+index*40+8,0x200,va,0x200,raw)
    primary=(0x1000,0x1040,0x3000);secondary=(0x1040,0x1080,0x3010)
    struct.pack_into('<III',fixture,0x400,*primary);struct.pack_into('<III',fixture,0x40c,*secondary)
    fixture[0x600]=1;fixture[0x610]=1|(4<<3);fixture[0x612]=1
    struct.pack_into('<III',fixture,0x618,*primary)
    pe=PeRuntimeFunctions(io.BytesIO(fixture))
    assert pe.lookup(0xfff) is None and pe.lookup(0x1080) is None
    assert pe.lookup(0x1000)['chain_depth']==0 and pe.lookup(0x103f)['primary']['begin_rva']=='0x1000'
    chained=pe.lookup(0x1040)
    assert chained['containing']['end_rva']=='0x1080' and chained['primary']['begin_rva']=='0x1000' and chained['chain_depth']==1
    mapped=runtime_attribution(chained,resolve(rows,[0x140001000,0x140001040]),0x140000000)
    assert mapped['primary']['symbols']==['a','alias'] and mapped['primary']['symbol_match']=='exact_address'
    assert mapped['containing']['symbol_match']=='nearest_preceding'
    def rejects(data, rva=0x1040):
        try:PeRuntimeFunctions(io.BytesIO(data)).lookup(rva)
        except ValueError:return
        raise AssertionError('malformed PE accepted')
    bad=bytearray(fixture);struct.pack_into('<III',bad,0x618,*secondary);rejects(bad)
    bad=bytearray(fixture);struct.pack_into('<I',bad,0x414,0x4000);rejects(bad)
    bad=bytearray(fixture);struct.pack_into('<I',bad,optional+140,23);rejects(bad)
    bad=bytearray(fixture);struct.pack_into('<I',bad,0x40c,0x1030);rejects(bad)
    bad=bytearray(fixture);bad[0x610]=1|(5<<3);rejects(bad)
    bad=bytearray(fixture);struct.pack_into('<I',bad,0x414,0x31fc);bad[0x7fc]=1|(4<<3);rejects(bad)
    rejects(fixture[:-1])
    # A legitimate unwind handler is validated, not classified as sampled EH work.
    bad=bytearray(fixture);bad[0x610]=1|(1<<3);struct.pack_into('<I',bad,0x618,0x1000)
    assert PeRuntimeFunctions(io.BytesIO(bad)).lookup(0x1040)['chain_depth']==0
    print('EXECUTION_RESOLVER_TEST_OK boundaries aliases unordered_sections data_excluded pdata_ranges chain_padding chain_cycle malformed_bounds handler')

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
    game=next(m for m in profile['modules'] if m['name'].lower()=='game.exe')
    with args.exe.open('rb') as f:
        digest=hashlib.file_digest(f,'sha256').hexdigest()
        if digest!=benchmark['exe_sha256']:raise ValueError('executable differs from the benchmark')
        pe=PeRuntimeFunctions(f);timestamp=pe.timestamp;image_size=pe.image_size
        if game['size']!=image_size:raise ValueError('sampled image size mismatch')
        rvas={r['ip']-game['base'] for r in profile['ips'] if game['base']<=r['ip']<game['base']+game['size']}
        rvas.update(frame['ip']-game['base']-(index!=0) for stack in profile.get('stacks',[]) for index,frame in enumerate(stack['frames'])
            if game['base']<=frame['ip']<game['base']+game['size'])
        runtime_functions={rva:pe.lookup(rva) for rva in rvas}
    if str(profile['tid']) not in {r.get('execution_thread_id') for r in benchmark['gameplay_samples']}:
        raise ValueError('sampled thread is not the recorded execution thread')
    with args.map.open(errors='replace') as f:
        header=''.join(next(f) for _ in range(8))
    stamp=re.search(r'Timestamp is ([0-9a-fA-F]+)',header);base=re.search(r'Preferred load address is ([0-9a-fA-F]+)',header)
    if not stamp or int(stamp[1],16)!=timestamp or not base:raise ValueError('map/executable timestamp mismatch')
    preferred=int(base[1],16);targets={preferred+r['ip']-game['base']:r['count'] for r in profile['ips'] if game['base']<=r['ip']<game['base']+game['size']}
    if preferred!=pe.image_base:raise ValueError('map/executable preferred image base mismatch')
    stack_targets={preferred+frame['ip']-game['base']-(index!=0) for stack in profile.get('stacks',[]) for index,frame in enumerate(stack['frames'])
        if game['base']<=frame['ip']<game['base']+game['size']}
    begins={preferred+int(function[name]['begin_rva'],16) for function in runtime_functions.values() if function for name in ('containing','primary')}
    with args.map.open(errors='replace') as f:found=resolve(f,set(targets)|stack_targets|begins)
    entries=[];objects=Counter();modules=Counter()
    for row in profile['ips']:
        module=next((m['name'] for m in profile['modules'] if m['base']<=row['ip']<m['base']+m['size']),'<unknown>');modules[module]+=row['count']
    for address,count in targets.items():
        entry=found.get(address);owner='<unresolved>' if entry is None else next(iter(entry['objects'])) if len(entry['objects'])==1 else '<folded multi-object aliases>'
        objects[owner]+=count
        entries.append({'rva':hex(address-preferred),'count':count,'symbol_offset':address-entry['address'] if entry else None,
            'symbols':sorted(entry['symbols']) if entry else [],'objects':sorted(entry['objects']) if entry else [],
            'runtime_function':runtime_attribution(runtime_functions[address-preferred],found,preferred)})
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
                    alias_count=len(entry['symbols']) if entry else 0,objects=sorted(entry['objects']) if entry else [],
                    runtime_function=runtime_attribution(runtime_functions[frame['ip']-game['base']-(index!=0)],found,preferred))
            frames.append(resolved)
        stacks.append({'elapsed_ms':stack['elapsed_ms'],'frames':frames})
    output=args.profile.with_name('execution-ip-resolved.json')
    if output.exists():raise ValueError('resolved output must be new')
    result={'schema':'sarecomp-execution-ip-resolved-v1','exe_sha256':digest,'pid':profile['pid'],'tid':profile['tid'],
        'samples':profile['samples'],'elapsed_ms':profile['elapsed_ms'],'suspension_ms':profile['suspension_ms'],
        'max_suspension_ms':profile['max_suspension_ms'],'modules':modules.most_common(),'objects':objects.most_common(),
        'attribution':'nearest public code symbol; aliases retained; not exclusive CPU percentages or call counts',
        'runtime_function_attribution':'SHA-verified AMD64 PE exception-directory ranges; primary follows UNW_FLAG_CHAININFO; begin symbols use the same map scan with exact/nearest match explicit; aliases and EH labels do not prove exclusive source ownership or exception cost; null means no containing table entry; non-top stack return addresses use IP-1',
        'entries':sorted(entries,key=lambda e:e['count'],reverse=True),'stacks':stacks}
    output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('entries','stacks')},indent=2))

if __name__=='__main__':main()
