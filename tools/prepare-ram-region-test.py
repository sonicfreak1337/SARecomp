"""Compile an actual retained 83-instruction region and its transformed twin."""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import re

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,required=True)
p.add_argument('--mixed-source',type=Path,required=True)
p.add_argument('--vector-source',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
data=a.source.read_bytes()
# Identity authenticated independently of the transformation under test.
manifest=(a.source.parent.parent/'.katana-generated-artifacts').read_text().splitlines()
record=[l.split('\t') for l in manifest if l.startswith('code/'+a.source.name+'\t')]
if len(record)!=1 or record[0][1:3]!=[str(len(data)),'sha256:'+hashlib.sha256(data).hexdigest()]:
    raise ValueError('Original test witness changed')
spec=importlib.util.spec_from_file_location('regions',Path(__file__).with_name('prepare-ram-regions.py'))
regions=importlib.util.module_from_spec(spec);spec.loader.exec_module(regions)
source=data.decode(); changed,report=regions.transform(source)
if not any(g['pc']=='8C0CC090' and g['instructions']==83 for g in report):
    raise ValueError('Required mixed witness did not qualify')
helpers=source[source.index('    const auto katana_direct_ram_translate ='):source.index('    if (katana::runtime::unrelocate_code_address_inline(cpu.pc) ==')]
start=re.search(r'(?m)^ +\{\n +// katana-guest 0x8C0CC090u',source).start()
end=source.index('                katana_block_8C0CC136_resume:',start)
original=source[start:end]
# Extract the same surrounding instruction interval, including the actual
# prefix and original miss labels produced by the real generator.
label=re.search(r'sonic_ram_8C0CC090_(\d+)_0:',changed)
begin=changed.rfind('                {\n                    if (sonic::scalar_writes',0,label.start())
finish=changed.index('                katana_block_8C0CC136_resume:',label.start())
candidate=changed[begin:finish]
result='''namespace runtime_dispatch_detail {
struct Address { std::uint32_t v,p; };
inline Address active_exit_source{};
inline BlockEndKind active_exit_kind{};
inline DynamicDispatchSiteClass active_exit_site_class{};
}
static bool katana_commit_post_instruction_safepoint(CpuState& c,Services& s,std::uint32_t pc) {
    return finalize_guest_block(c,s,1024u,pc,0u,false,false,false).interrupt.has_value();
}
'''
for name,body in [('retained_region',original),('transformed_region',candidate)]:
    result+=f'void {name}(Owned& o,const DirectLinearMemoryGuard& entry,std::uint32_t resume) {{\n'
    result+='''    auto& cpu=o.f.cpu;
    auto* services=&o.f.services;
    NativeAotRegisterFile<0xFFFFu,0x3Fu> katana_registers(cpu);
    bool katana_guest_write_exit_requested=false;
    const auto* katana_direct_ram_code_tracker=&o.guard;
    auto katana_direct_ram=entry;
    Memory::DirectLinearWriteBatch* const katana_direct_ram_writes=nullptr;
'''+helpers+'''
    if(resume==0x8C0CC098u)goto katana_block_8C0CC098_resume;
    if(resume==0x8C0CC0A2u)goto katana_block_8C0CC0A2_resume;
'''+body+'\n}\n'

# A second retained witness crosses two real FMUL instructions, with RAM
# loads/stores on both sides. Keep its original legality/exception envelopes.
mixed_data=a.mixed_source.read_bytes()
record=[l.split('\t') for l in manifest if l.startswith('code/'+a.mixed_source.name+'\t')]
if len(record)!=1 or record[0][1:3]!=[str(len(mixed_data)),'sha256:'+hashlib.sha256(mixed_data).hexdigest()]:
    raise ValueError('Original arithmetic witness changed')
mixed=mixed_data.decode(); changed,report=regions.transform(mixed)
if not any(g['pc']=='8C036F66' and g['instructions']==35 and g['arithmetic']==2 for g in report):
    raise ValueError('Required arithmetic witness did not qualify')
start=re.search(r'(?m)^ +\{\n +// katana-guest 0x8C036F66u',mixed).start()
last=re.search(r'(?m)^([ ]+)\{\n +// katana-guest 0x8C036FAAu',mixed)
end=mixed.index('\n'+last[1]+'}',last.end())+2+len(last[1])
original=mixed[start:end]
label=re.search(r'sonic_ram_8C036F66_(\d+)_0:',changed)
begin=changed.rfind('                {\n                    if (sonic::scalar_writes',0,label.start())
end_label=f'sonic_ram_8C036F66_{label[1]}_end: ;'
finish=changed.index(end_label,label.start())+len(end_label)
candidate=changed[begin:finish]
mixed_helpers=mixed[mixed.index('    const auto katana_direct_ram_translate ='):mixed.index('    if (katana::runtime::unrelocate_code_address_inline(cpu.pc) ==')]
for name,body in [('retained_mixed_region',original),('transformed_mixed_region',candidate)]:
    result+=f'void {name}(Owned& o,const DirectLinearMemoryGuard& entry,std::uint32_t resume) {{\n'
    result+='''    auto& cpu=o.f.cpu;
    auto* services=&o.f.services;
    NativeAotRegisterFile<0xFFFFu,0x3Fu> katana_registers(cpu);
    bool katana_guest_write_exit_requested=false;
    const auto* katana_direct_ram_code_tracker=&o.guard;
    auto katana_direct_ram=entry;
    Memory::DirectLinearWriteBatch* const katana_direct_ram_writes=nullptr;
'''+mixed_helpers+'''
    if(resume==0x8C036F94u)goto katana_block_8C036F94_resume;
    if(resume==0x8C036F98u)goto katana_block_8C036F98_resume;
'''+body+'\n}\n'
for path,stem,first,last,size in (
    (a.mixed_source,'gbr_region',0x8C037350,0x8C03736E,16),
    (a.vector_source,'vector_region',0x8C03358C,0x8C03359C,9)):
    data=path.read_bytes()
    record=[l.split('\t') for l in manifest if l.startswith('code/'+path.name+'\t')]
    if len(record)!=1 or record[0][1:3]!=[str(len(data)),'sha256:'+hashlib.sha256(data).hexdigest()]:
        raise ValueError('Original vector witness changed')
    source=data.decode();changed,report=regions.transform(source)
    if not any(g['pc']==f'{first:08X}' and g['instructions']==size for g in report):
        raise ValueError('Required vector witness did not qualify')
    nodes=[n for atom in regions.instructions(source) for n in atom if first<=n['pc']<=last]
    original=source[nodes[0]['start']:nodes[-1]['end']]
    label=re.search(f'sonic_ram_{first:08X}_(\\d+)_0:',changed)
    begin=changed.rfind(nodes[0]['indent']+'{\n'+nodes[0]['indent']+'    if (sonic::scalar_writes',0,label.start())
    end_label=f'sonic_ram_{first:08X}_{label[1]}_end: ;'
    finish=changed.index(end_label,label.start())+len(end_label)
    candidate=changed[begin:finish]
    helpers=source[source.index('    const auto katana_direct_ram_translate ='):source.index('    if (katana::runtime::unrelocate_code_address_inline(cpu.pc) ==')]
    for prefix,body in [('retained_',original),('transformed_',candidate)]:
        result+=f'void {prefix}{stem}(Owned& o,const DirectLinearMemoryGuard& entry) {{\n'
        result+='''    auto& cpu=o.f.cpu;
    auto* services=&o.f.services;
    NativeAotRegisterFile<0xFFFFu,0x3Fu> katana_registers(cpu);
    bool katana_guest_write_exit_requested=false;
    const auto* katana_direct_ram_code_tracker=&o.guard;
    auto katana_direct_ram=entry;
    Memory::DirectLinearWriteBatch* const katana_direct_ram_writes=nullptr;
'''+helpers+body+'\n}\n'

a.output.parent.mkdir(parents=True,exist_ok=True)
if not a.output.exists() or a.output.read_text()!=result:a.output.write_text(result)
print('SONIC_RAM_REGION_TEST_WITNESS_READY instructions=143 memory=104 arithmetic=3')
