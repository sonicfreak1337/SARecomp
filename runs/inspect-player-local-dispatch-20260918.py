from pathlib import Path
import hashlib,json,re,struct
base=0x8C000000;entry=0x8C0CBD40;branch=0x8C0CBF44;table=0x8C0CBF50
root=Path('.local/working-product/generated');unit=root/'code/unit-v8C0CBD40-8C0CCFDC-8bb83195ded15666.cpp'
raw=unit.read_bytes();digest=hashlib.sha256(raw).hexdigest();manifest=(root/'.katana-generated-artifacts').read_text().splitlines()
records={v[0]:v for line in manifest[2:] if len(v:=line.split('\t'))>=3}
assert records['code/'+unit.name][1:3]==[str(len(raw)),'sha256:'+digest]
text=raw.decode().replace('\r\n','\n');start=text.index(f'BlockExit fn_{entry:08X}_runtime_entry(CpuState& cpu, BlockExecutionContext& context) {{');end=text.index('    return exit;\n}',start)+len('    return exit;\n}');body=text[start:end]
pc={int(v,16) for v in re.findall(r'// katana-guest 0x([0-9A-F]{8})u',body)}
blocks={int(v,16) for v in re.findall(r'\bkatana_block_([0-9A-F]{8}):',body)}
ram=Path('.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin').read_bytes();assert hashlib.sha256(ram).hexdigest()=='b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c'
after=body[body.index('// katana-guest 0x8C0CBF44u'):];dispatch=after[after.index('switch ('):after.index('case 0x')]
exact={int(v,16) for v in re.findall(r'katana_exact_guarded_target_matches\(jump_target, 0x([0-9A-F]{8})u\)',dispatch)}
targets=[branch+4+v for v in struct.unpack_from('<56h',ram,table-base)]
assert set(targets)==exact and set(targets)<=pc and set(targets)<=blocks
assert struct.unpack_from('<H',ram,0x8C0CBF30-base)[0]==0xE138
jmps=[p for p in sorted(pc) if struct.unpack_from('<H',ram,p-base)[0]&0xF0FF==0x402B]
report=dict(schema='sarecomp-player-local-dispatch-v1',generation=manifest[1],unit=unit.name,unit_sha256=digest,entry=f'{entry:08X}',annotated_instructions=len(pc),table=f'{table:08X}',branch=f'{branch:08X}',states=56,unique_targets=len(exact),targets=[f'{t:08X}' for t in targets],all_targets_original_local_blocks=True,jmp_sites=[f'{p:08X}' for p in jmps],default_target='8C0CCE72',default_jmp_site='8C0CBF38',direct_children=sorted(set(re.findall(r'fn_([0-9A-F]{8})_runtime_entry',body))-{f'{entry:08X}'}),runtime_integrated=False,requirement='BRAF and local JMP must branch within this original owner without a call frame or invented global function. Current state-transfer implementation handles transfers between existing owners only.')
Path('runs/player-local-dispatch-20260918.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['entry','annotated_instructions','states','unique_targets','jmp_sites','all_targets_original_local_blocks']}))
