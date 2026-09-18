"""Insert only seven exact root admissions into authenticated retained units."""
import argparse
import hashlib
import json
import re
from pathlib import Path

UNITS={
 'unit-v8C10CDDC-8C10DA5C-cb14c3e2f790eaff.cpp':('7f4b0d3fb8488f19f5a216bd1af71c4b005b6c934ac43c0f9adea5cc73d05463',(0x8C10CF48,0x8C10CF98,0x8C10CFE8,0x8C10D038)),
 'unit-v8C10DB04-8C10EB80-b3bbbddd2dccf359.cpp':('7dadc0931930f41585f89ba8a559fe6379e3f82cc6758792b034fc1df775d786',(0x8C10E4D0,0x8C10E4EC,0x8C10E5E0)),
}
def sha(data):return hashlib.sha256(data).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for k in ('source-root','output'):p.add_argument('--'+k,type=Path,required=True)
 p.add_argument('--input',type=Path)
 a=p.parse_args()
 if not a.input:
  units=dict(UNITS)
  units['unit-v8C10EC94-8C10FCEC-2baf008af52674a0.cpp']=('813ad53582c9e6b7678647072e849686887a0608cbd7a05fa43d1096d86f418f',(0x8C10EEC4,0x8C10FAD4,0x8C10FAF8))
  digest,entries=units['unit-v8C10DB04-8C10EB80-b3bbbddd2dccf359.cpp'];units['unit-v8C10DB04-8C10EB80-b3bbbddd2dccf359.cpp']=(digest,entries+(0x8C10E6F8,))
  definitions={};prefix=None
  for name,(digest,entries) in units.items():
   data=(a.source_root/'code'/name).read_bytes()
   if sha(data)!=digest:raise ValueError('Original oracle source identity')
   text=data.decode().replace('\r\n','\n')
   if prefix is None:prefix=text[:text.index('BlockExit fn_')].replace('#include "../include/','#include "')
   for entry in entries:
    start=text.index(f'BlockExit fn_{entry:08X}_runtime_entry(CpuState& cpu, BlockExecutionContext& context) {{')
    finish=text.index('    return exit;\n}',start)+len('    return exit;\n}')
    definitions[entry]=text[start:finish]
  out=prefix+'\n'.join(f'BlockExit inverse_original_{entry:08X}(CpuState&,BlockExecutionContext&);' for entry in definitions)+'\n'+'\n'.join(definitions.values())
  for entry in definitions:out=out.replace(f'fn_{entry:08X}_runtime_entry',f'inverse_original_{entry:08X}')
  shard='native-port-dispatch-shard-202785.cpp'; table=(a.source_root/'code'/shard).read_bytes()
  lines=(a.source_root/'.katana-generated-artifacts').read_text().splitlines();records={v[0]:v for line in lines[2:] if len(v:=line.split('\t'))>=3}
  if records.get('code/'+shard,[])[1:3]!=[str(len(table)),'sha256:'+sha(table)]:raise ValueError('Original reference entry table identity')
  entries={int(pc,16):int(owner,16) for pc,owner in re.findall(r'\{0x([A-F0-9]{8})u, &fn_([A-F0-9]{8})_runtime_entry, true, (?:true|false)\}',table.decode()) if int(owner,16) in definitions}
  if not all(entry in entries for entry in definitions):raise ValueError('Missing original root entry')
  out+='\n}\nnamespace sonic::atan_math {bool reference_entry(std::uint32_t pc) noexcept {switch(pc){\n'
  for entry in entries:out+=f'case 0x{entry:08X}u:return true;\n'
  out+='default:return false;}}\nvoid run_retained_reference(katana::runtime::CpuState& cpu) {katana::runtime::BlockExecutionContext context;switch(cpu.pc) {\n'
  for pc,entry in entries.items():out+=f'case 0x{pc:08X}u:katana_port_generated::inverse_original_{entry:08X}(cpu,context);return;\n'
  out+='default:throw std::logic_error("Unreviewed inverse-trig reference target");\n}}}\n'
  a.output.parent.mkdir(parents=True,exist_ok=True);result=out.encode()
  if not a.output.exists() or a.output.read_bytes()!=result:a.output.write_bytes(result)
  a.output.with_suffix('.json').write_text(json.dumps(dict(units={n:d for n,(d,_) in units.items()},owners=[f'{v:08X}' for v in definitions],output_sha256=sha(result)),indent=2)+'\n')
  print('SONIC_INVERSE_TRIG_ORACLE_READY owners=11');return
 name=a.input.name
 digest,entries=UNITS[name]; original=(a.source_root/'code'/name).read_bytes()
 lines=(a.source_root/'.katana-generated-artifacts').read_text().splitlines()
 generation='generation\tsha256:'+sha(('\n'.join(lines[2:])+'\n').encode())
 records={v[0]:v for line in lines[2:] if len(v:=line.split('\t'))>=3}
 if lines[:2]!=['katana-codegen-artifacts-v2',generation] or sha(original)!=digest or records.get('code/'+name,[])[1:3]!=[str(len(original)),'sha256:'+digest]:raise ValueError('Original identity')
 data=a.input.read_bytes()
 if data!=original:
  if a.input.parent.name not in ('region-writes','region-extended-writes'):raise ValueError('Unreviewed active input')
  prior=json.loads((a.input.parent/'preparation.json').read_text()); rows=[v for v in prior['units'] if v['unit']==name]
  if prior['generation']!=generation or prior['mode']!='region' or prior.get('guard_probe',False) or prior.get('extended_regions',False)!=(a.input.parent.name=='region-extended-writes') or len(rows)!=1 or rows[0]['source_sha256']!=digest or rows[0]['output_sha256']!=sha(data):raise ValueError('Active RAM input identity')
 out=data.decode().replace('\r\n','\n')
 for entry in entries:
  declaration=f'BlockExit fn_{entry:08X}_runtime_entry(CpuState& cpu, BlockExecutionContext& context) {{'
  if out.count(declaration)!=1:raise ValueError('Owner entry count')
  start=out.index(declaration); boundary=out.index('    static_cast<void>(services);',start)
  injection=f'''    if (cpu.pc==0x{entry:08X}u && sonic::atan_math::inverse_enabled()) {{
        if (sonic::atan_math::try_execute(cpu,services->immutable_write_guard())) {{
            ++sonic::atan_math::inverse_calls;
            const auto source=sonic::atan_math::inverse_return_site;
            runtime_dispatch_detail::active_exit_source={{source,source&0x1FFFFFFFu}};
            runtime_dispatch_detail::active_exit_kind=katana::runtime::BlockEndKind::Return;
            runtime_dispatch_detail::active_exit_site_class=katana::runtime::DynamicDispatchSiteClass::NotDynamic;
            return;
        }}
        ++sonic::atan_math::inverse_declined;
    }}
'''
  out=out[:boundary]+injection+out[boundary:]
 out='#include "sonic_atan_math.hpp"\n'+out.replace('#include "../include/','#include "')
 a.output.parent.mkdir(parents=True,exist_ok=True); result=out.encode()
 if not a.output.exists() or a.output.read_bytes()!=result:a.output.write_bytes(result)
 a.output.with_suffix('.json').write_text(json.dumps(dict(generation=generation,original_sha256=digest,input_sha256=sha(data),output_sha256=sha(result),entries=[f'{v:08X}' for v in entries]),indent=2)+'\n')
 print(f'SONIC_INVERSE_TRIG_BRIDGE_READY roots={len(entries)}')
if __name__=='__main__':main()
