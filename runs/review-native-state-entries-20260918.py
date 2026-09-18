from pathlib import Path
import sys,re,json,struct,hashlib,importlib.util
sys.path.insert(0,str(Path('tools').resolve()))
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
contact=module('state_review_contact','tools/prepare-movement-contact.py'); a=contact.render
bridge=module('state_review_bridge','tools/prepare-render-hierarchy-bridge.py')
bridge.author=a; a.POLYMORPHIC_EPOCHS.add(0x8C0FC3F0)
ram=Path('.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin').read_bytes()
assert hashlib.sha256(ram).hexdigest()==a.shared.RAM_SHA
source=Path('.local/working-product/generated')
lines=(source/'.katana-generated-artifacts').read_text().splitlines()
generation='generation\tsha256:'+hashlib.sha256(('\n'.join(lines[2:])+'\n').encode()).hexdigest()
assert lines[:2]==['katana-codegen-artifacts-v2',generation]
records={v[0]:v for line in lines[2:] if len(v:=line.split('\t'))>=3}
rows=json.loads(Path('runs/native-scope-review-20260918/actor-state-closure.json').read_text())
out=Path('runs/native-state-transfers-20260918');results=[]
for row in rows:
 e=row['entry']
 if not 0x8C0FBD00<=e<0x8C0FE500:continue
 data=(source/'code'/row['unit']).read_bytes();sha=hashlib.sha256(data).hexdigest()
 assert sha==row['unit_sha'] and records['code/'+row['unit']][1:3]==[str(len(data)),'sha256:'+sha]
 text=data.decode().replace('\r\n','\n');needle=f'BlockExit fn_{e:08X}_runtime_entry(CpuState& cpu, BlockExecutionContext& context) {{'
 start=text.index(needle);end=text.index('    return exit;\n}',start)+len('    return exit;\n}');body=text[start:end]
 pcs={int(x,16) for x in re.findall(r'// katana-guest 0x([0-9A-F]{8})u',body)}
 direct={int(x,16) for x in re.findall(r'fn_([0-9A-F]{8})_runtime_entry',body)}-{e}
 branch_pcs=[p for p in pcs if struct.unpack_from('<H',ram,p-a.shared.BASE)[0]&0xF0FF==0x0023]
 assert len(branch_pcs)<=1
 transfers={}
 for pc in pcs:
  op=struct.unpack_from('<H',ram,pc-a.shared.BASE)[0]
  if op>>12==0xA:
   target=pc+4+2*a.shared.signed(op&4095,12)
   if target not in pcs:
    assert target in direct,'Unbound static branch entry'
    transfers[pc]=(target,)
  if pc in branch_pcs:
   targets=sorted(set(int(x,16) for x in re.findall(r'katana_exact_guarded_target_matches\(jump_target, 0x([0-9A-F]{8})u\)',body)))
   assert targets and all(t in direct for t in targets)
   assert pc==0x8C0FDCA6
   table=struct.unpack_from('<13h',ram,0x8C0FDCB4-a.shared.BASE)
   assert sorted({pc+4+x for x in table})==targets,'Original 13-state jump table mismatch'
   transfers[pc]=tuple(targets)
 a.TRANSFERS[e]=transfers;a.EPOCHS[e]=tuple(map(tuple,row['epochs']))
 record={k:row[k] for k in ('entry','begin','end','unit','unit_sha','epochs')}
 record['transfers']={f'{p:08X}':[f'{v:08X}' for v in ts] for p,ts in sorted(transfers.items())}
 try:
  ins,delays,calls=a.inspect(ram,e,row['begin'],row['end'])
  bridge.verify_original_epochs(body,e)
  emitted=a.emit_body(ram,e,ins)
  _,routes=bridge.local_resumes(contact.module('state_contact_resume','prepare-movement-contact-bridge.py').readonly_resume_routers(body,runtime_only=True),ram,('state',e,row['begin'],row['end']))
  record.update(instructions=len(ins),delays=len(delays),calls=calls,local_resumes=routes,authored=True)
  (out/f'actor-{e:08X}.inc').write_text(emitted,newline='\n')
 except Exception as ex:record.update(authored=False,remaining=str(ex))
 results.append(record)
report={'schema':'sarecomp-actor-scope-review-v1','generation':generation,'ram_sha256':a.shared.RAM_SHA,'runtime_integrated':False,'rows':results}
(out/'actor-scope.json').write_text(json.dumps(report,indent=2)+'\n')
print('ACTOR_SCOPE owners',len(results),'authored',sum(r['authored'] for r in results),'transfers',sum(len(r['transfers']) for r in results))
print('REMAINING',[(f"{r['entry']:08X}",r['remaining']) for r in results if not r['authored']])
