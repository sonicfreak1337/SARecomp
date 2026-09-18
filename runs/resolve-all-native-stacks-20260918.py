import importlib.util,json,re
from pathlib import Path
from bisect import bisect_right
from collections import Counter
root=Path('runs/all-native-profile-gamma-20260918')
spec=importlib.util.spec_from_file_location('r','tools/resolve-linux-perf.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
elf=Path('build-linux/game')
with elf.open('rb') as f:f.seek(16);assert int.from_bytes(f.read(2),'little')==2,'Stack addresses require ET_EXEC'
loads,functions=m.elf(elf,True);starts=sorted({a for a,s in functions});by={a:[] for a in starts}
for (a,sz),ns in functions.items():by[a].append((sz,ns))
def resolve(a):
 if not any(s[3]<=a<s[3]+s[6] for s in loads):return []
 i=bisect_right(starts,a)-1
 return sorted({n for sz,ns in by[starts[i]] if a<starts[i]+sz for n in ns}) if i>=0 else []
def name(ns):return ns[0] if len(ns)==1 else 'ALIASES: '+';'.join(ns)
leaf=Counter();owner=Counter();native=Counter();inclusive=Counter();edges=Counter();samples=[];unknown=0;game_leaves=0
for event in root.joinpath('perf-stacks.txt').read_text().strip().split('\n\n'):
 lines=event.splitlines();frames=[]
 if not lines or 'cpu-clock:u:' not in lines[0]:continue
 for line in lines[1:]:
  match=re.match(r'\s+([0-9a-f]+)\s+.*\((.+)\)$',line)
  if not match:continue
  a=int(match[1],16);dso=Path(match[2]).name;ns=resolve(a) if dso=='game' else []
  frames.append((a,dso,ns))
 if not frames:continue
 sample_owners=set();sample_native=set();sample_symbols=set();callers=[]
 if frames[0][1]=='game':
  game_leaves+=1
  if frames[0][2]:leaf[name(frames[0][2])]+=1
  else:unknown+=1
 for a,dso,ns in frames:
  if dso!='game' or not ns:continue
  sample_symbols.add(name(ns))
  ids=set(re.findall(r'fn_([0-9A-F]{8})_runtime_entry',' '.join(ns)))
  if len(ids)==1:
   key=next(iter(ids));sample_owners.add(key)
   if not callers or callers[-1]!=key:callers.append(key)
  sample_native.update(re.findall(r'_Z[NZ]*N?5sonic\d+([a-z_]+)',' '.join(ns)))
  if any('sonic_native_ninja_model_draw' in n for n in ns):sample_native.add('model_draw')
 for key in sample_owners:owner[key]+=1
 for key in sample_native:native[key]+=1
 for key in sample_symbols:inclusive[key]+=1
 for child,parent in zip(callers,callers[1:]):edges[(parent,child)]+=1
 samples.append({'owners':sorted(sample_owners),'native':sorted(sample_native),'symbols':sorted(sample_symbols)})
reference=json.loads((root/'resolved.json').read_text())
assert game_leaves==reference['game_samples'] and unknown==reference['unresolved']
for row in reference['rows']:
 if row['exact']:assert resolve(int(row['address'],16))==row['symbols']
from_report=Counter()
for row in reference['rows']:
 if row['exact']:from_report[name(row['symbols'])]+=row['samples']
assert leaf==from_report,'Raw stack leaves must reproduce the independently resolved exclusive report'
result={'exe_sha256':reference['exe_sha256'],'total_samples':len(samples),'game_leaf_samples':game_leaves,'unresolved_game_leaves':unknown,'leaf_report_exactly_reproduced':True,'owners_inclusive':dict(owner.most_common()),'native_groups_inclusive':dict(native.most_common()),'functions_inclusive':dict(inclusive.most_common()),'guest_edges':[{'parent':p,'child':c,'samples':n} for (p,c),n in edges.most_common()],'samples':samples}
(root/'stacks-resolved.json').write_text(json.dumps(result,indent=2)+'\n')
print('SAMPLES',len(samples),'GAME',game_leaves,'UNKNOWN',unknown)
for title,counts in [('OWNERS',owner),('NATIVE',native),('FUNCTIONS',inclusive)]:
 print(title)
 for key,n in counts.most_common(35):print(f'{100*n/len(samples):6.2f}%',n,key[:210])
