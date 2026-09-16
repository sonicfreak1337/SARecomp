"""Bind a real partial-register AOT body to the private-ABI component test."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
data=a.source.read_bytes()
lines=(a.source.parent.parent/'.katana-generated-artifacts').read_text().splitlines()
digest=lambda x:hashlib.sha256(x).hexdigest()
if lines[:2]!=['katana-codegen-artifacts-v2','generation\tsha256:'+digest(('\n'.join(lines[2:])+'\n').encode())]:
    raise ValueError('Unqualified retained manifest')
records=[line.split('\t') for line in lines if line.startswith('code/'+a.source.name+'\t')]
if len(records)!=1 or records[0][1:3]!=[str(len(data)),'sha256:'+digest(data)]:
    raise ValueError('Retained AOT body identity changed')
text=data.decode()
begin=text.index('BlockExit fn_8C055C8E_runtime_entry(CpuState& cpu, BlockExecutionContext& context) {')
end=text.index('\nBlockExit fn_8C055C9A_runtime_entry',begin)
owner=text[begin:end]
registers='katana::runtime::NativeAotRegisterFile<0x00000018u, 0x00000000u> katana_registers(cpu);'
begin=owner.index(registers)
end=owner.index('    }();',begin)
body=owner[begin:end].replace('runtime_dispatch_detail::','procedure_fixture_detail::')
# This witness deliberately mixes cached R3/R4 with raw R0/R5 and PR.
# Do not pretend a class substitution alone handles that generated contract.
spec=importlib.util.spec_from_file_location('procedure_preparation',Path(__file__).with_name('prepare-procedure-registers.py'))
preparation=importlib.util.module_from_spec(spec);spec.loader.exec_module(preparation)
changed,report=preparation.transform_owner(owner,'8C055C8E')
private_owner=changed[changed.index('BlockExit fn_8C055C8E_private('):]
begin=private_owner.index('sonic::procedure_registers::Frame<> katana_registers(bank);')
end=private_owner.index('    }();',begin)
private=private_owner[begin:end].replace('runtime_dispatch_detail::','procedure_fixture_detail::').replace('Frame<>','Frame<Count>')
source='''// Entire original inner body, including branches, delay slot and accounting.
namespace procedure_fixture_detail {
inline BlockAddress active_exit_source{};
inline BlockEndKind active_exit_kind{};
inline DynamicDispatchSiteClass active_exit_site_class{};
}
'''
source+='static void retained_angle(CpuState& cpu) {\n'+body+'}\n'
source+='template<bool Count> static void private_angle(CpuState& cpu, Bank<Count>& bank) {\n'+private+'}\n'
out=a.output.resolve()
root=Path(__file__).resolve().parents[1]
relative=out.relative_to(root)
if not relative.parts[0].startswith('build-'):raise ValueError('Test output must be build-local')
if out.is_symlink() or (out.exists() and out.stat().st_nlink!=1):raise ValueError('Linked test output')
out.parent.mkdir(parents=True,exist_ok=True)
encoded=source.encode()
if not out.exists() or out.read_bytes()!=encoded:out.write_bytes(encoded)
print(json.dumps({'owner':'8C055C8E','source_sha256':digest(data),
                  'body_sha256':digest(body.encode()),'output_sha256':digest(encoded),
                  'original_register_mask':'0x18','raw_register_rewrites':report['raw_accesses'],
                  'game_preparer_used':True,'game_integration':False}))
