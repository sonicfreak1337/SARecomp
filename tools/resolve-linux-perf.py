"""Resolve file-offset or exact-symbol perf rows against the bound Linux ELF.

Sampling a stripped distribution does not require uploading a second huge ELF.
Only bounded ELF function symbols count as exact; never attribute nearest symbols.
"""
import argparse
from bisect import bisect_right
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct


def digest(path, offset=0, count=None):
    h=hashlib.sha256()
    with path.open('rb') as f:
        f.seek(offset)
        while count is None or count:
            data=f.read(min(count,1024*1024) if count is not None else 1024*1024)
            if not data:break
            h.update(data)
            if count is not None:count-=len(data)
    if count:raise ValueError('truncated ELF')
    return h.hexdigest()


def elf(path, symbols=False):
    with path.open('rb') as f:
        h=f.read(64)
        if h[:6]!=b'\x7fELF\x02\x01':raise ValueError('expected ELF64 little endian')
        po,so=struct.unpack_from('<QQ',h,32)
        pe,pn,se,sn=struct.unpack_from('<HHHH',h,54)
        if pe!=56 or se!=64 or not 0<pn<100 or not 0<sn<1000:raise ValueError('invalid ELF tables')
        f.seek(po);segments=[struct.unpack('<IIQQQQQQ',f.read(pe)) for _ in range(pn)]
        f.seek(so);sections=[struct.unpack('<IIQQQQIIQQ',f.read(se)) for _ in range(sn)]
        functions={}
        if symbols:
            for section in sections:
                if section[1]!=2:continue
                strings=sections[section[6]];f.seek(strings[4]);names=f.read(strings[5])
                f.seek(section[4]);table=f.read(section[5])
                if section[9]!=24:raise ValueError('invalid symbol size')
                for row in range(0,len(table),24):
                    name,info,_,index,value,size=struct.unpack_from('<IBBHQQ',table,row)
                    if info&15!=2 or not index or not size:continue
                    functions.setdefault((value,size),[]).append(names[name:names.index(0,name)].decode())
    return [s for s in segments if s[0]==1 and s[1]&1],functions


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf',type=Path,required=True)
    p.add_argument('--stripped',type=Path,required=True)
    p.add_argument('--run',type=Path,required=True)
    a=p.parse_args();summary=json.loads((a.run/'summary.json').read_text())
    if digest(a.stripped)!=summary['exe_sha256']:raise ValueError('sampled binary SHA mismatch')
    loads,functions=elf(a.elf,True);stripped_loads,_=elf(a.stripped)
    if loads!=stripped_loads:raise ValueError('executable segment layout mismatch')
    for segment in loads:
        if digest(a.elf,segment[2],segment[5])!=digest(a.stripped,segment[2],segment[5]):
            raise ValueError('executable segment bytes mismatch')
    starts=sorted({address for address,_ in functions});by_start={x:[] for x in starts}
    for (address,size),names in functions.items():by_start[address].append((size,sorted(names)))
    named={}
    for key,names in functions.items():
        for name in names:named.setdefault(name,set()).add(key)
    resolved=[];owners=Counter();helpers=Counter();unresolved=0;total=0
    for row in (a.run/'perf-symbols.txt').read_text().splitlines():
        fields=[f.strip() for f in row.split('|')]
        if len(fields)<4 or fields[3]!='game':continue
        count=int(fields[1]);total+=count
        m=re.fullmatch(r'\[.\] 0x([0-9a-fA-F]+)',fields[2])
        if m:
            offset=int(m[1],16)
            segments=[s for s in loads if s[2]<=offset<s[2]+s[5]]
            if len(segments)!=1:raise ValueError('sample outside executable segment')
            address=offset+segments[0][3]-segments[0][2]
        else:
            # perf can read the retained symbol table directly without a
            # build ID. Accept only an exact, unique STT_FUNC identity;
            # this row is a symbol aggregate, not an exact sampled PC.
            symbol=fields[2].removeprefix('[.] ')
            candidates=named.get(symbol,set()) if fields[2].startswith('[.] ') else set()
            if not candidates:
                # Synthesized PLT names need not have an ELF function symbol.
                # Keep their sample count explicitly unresolved.
                unresolved+=count
                resolved.append({'file_offset':None,'address':None,
                    'address_kind':'unresolved_symbol','reported_symbol':symbol,
                    'samples':count,'symbols':[],'exact':False})
                continue
            if len(candidates)!=1:raise ValueError('ambiguous perf symbol '+symbol)
            address,_=next(iter(candidates));offset=None
            if not any(s[3]<=address<s[3]+s[6] for s in loads):raise ValueError('symbol outside executable segment')
        i=bisect_right(starts,address)-1
        matches=[(size,names) for size,names in by_start[starts[i]] if address<starts[i]+size] if i>=0 else []
        names=sorted({n for _,ns in matches for n in ns})
        row={'file_offset':hex(offset) if offset is not None else None,
             'address':hex(address),'address_kind':'sample' if m else 'symbol_start',
             'samples':count,'symbols':names,'exact':bool(matches)}
        resolved.append(row)
        if not names:unresolved+=count;continue
        guest=set(re.findall(r'fn_([0-9A-F]{8})_runtime_entry',' '.join(names)))
        if len(guest)==1:owners[next(iter(guest))]+=count
        elif len(names)==1:helpers[names[0]]+=count
        else:helpers['ALIASES: '+';'.join(names)]+=count
    result={'exe_sha256':summary['exe_sha256'],'game_samples':total,'unresolved':unresolved,
            'guest_samples':sum(owners.values()),'owners':dict(owners.most_common()),
            'helpers':dict(helpers.most_common()),'rows':resolved}
    (a.run/'resolved.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ('exe_sha256','game_samples','unresolved','guest_samples')}))
    print('OWNERS',owners.most_common(25));print('HELPERS',helpers.most_common(25))


if __name__=='__main__':main()
