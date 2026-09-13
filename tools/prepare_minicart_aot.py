"""Bind only the resumable entries actually emitted by the sealed native backend."""
import hashlib
from pathlib import Path
import re
import sys
import json
from minicart_universe import extend

def write_changed(path,text):
    if not path.exists() or path.read_text(encoding='utf-8')!=text:
        path.write_text(text,encoding='utf-8')

directory = Path(sys.argv[1])
data = (directory/'minicart.bin').read_bytes()
identity = '2d3ec72d9f62a0ec626155f822a77bac7209999aa89f825b3082a69cf85db2a1'
if len(data) != 1434052 or hashlib.sha256(data).hexdigest() != identity:
    raise RuntimeError('MINICART supplement source identity mismatch')
source = (directory/'minicart-aot.cpp').read_text()
definitions = list(re.finditer(r'BlockExit fn_([0-9A-F]{8})_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{', source))
if [int(match[1],16) for match in definitions] != [0x8298A81E,0x8298AA60]:
    raise RuntimeError('MINICART native entry definitions changed')
windows = [tuple(int(v,16) for v in line.split()) for line in (directory/'windows.tsv').read_text().splitlines()]
entries = {}
for index, match in enumerate(definitions):
    owner = int(match[1],16)
    body = source[match.end():definitions[index+1].start() if index+1<len(definitions) else len(source)]
    cases = {int(value,16) for value in re.findall(r'case 0x(8298[0-9A-F]{4})u:',body)}
    if owner not in cases:
        raise RuntimeError('Missing root resume label')
    for entry in sorted(cases):
        candidates = [(start,end) for function,start,end in windows if function==owner and start<=entry<end]
        if len(candidates)!=1 or entry in entries:
            raise RuntimeError(f'Ambiguous original block for {entry:08X}')
        start,end = candidates[0]
        offset,size = entry-0x82980000,end-entry
        if entry&1 or size<2 or size&1 or offset+size>len(data):
            raise RuntimeError('Invalid original block byte window')
        entries[entry] = (owner,offset,size,hashlib.sha256(data[offset:offset+size]).hexdigest())
lines = ['// Generated from authenticated MINICART bytes by the sealed r354 backend.',
         '#pragma once', '#include <algorithm>', '#include <array>', '#include <stdexcept>',
         'namespace katana_port_generated {']
for match in definitions:
    lines += [f'katana::runtime::BlockExit fn_{match[1]}_runtime_entry(katana::runtime::CpuState&, katana::runtime::BlockExecutionContext&);']
lines += ['namespace runtime_dispatch_detail {',
          f'inline constexpr std::array<NativePortDispatchEntry,{2*len(entries)}> sonic_minicart_entries{{{{']
for alias in [0,0x20000000]:
    for entry,(owner,*_) in sorted(entries.items()):
        lines += [f'    {{0x{entry|alias:08X}u,&fn_{owner:08X}_runtime_entry,false,false}},']
lines += ['}};',f'inline constexpr std::array<katana::runtime::NativePortLoadedAotBlockIdentityView,{len(entries)}> sonic_minicart_identities{{{{']
for owner,offset,size,digest in entries.values():
    lines += [f'    {{{offset}u,{size}u,"sha256:{digest}"}},']
lines += ['}};', '''
inline void append_sonic_minicart_entries(std::vector<NativePortDispatchEntry>& entries) {
    for(const auto& entry:sonic_minicart_entries) {
        const auto found=std::lower_bound(entries.begin(),entries.end(),entry.address,
            [](const auto& item,const auto value){return item.address<value;});
        if(found!=entries.end() && found->address==entry.address)
            throw std::runtime_error("MINICART supplement overlaps retained AOT");
    }
    entries.insert(entries.end(),sonic_minicart_entries.begin(),sonic_minicart_entries.end());
    std::inplace_merge(entries.begin(),entries.end()-sonic_minicart_entries.size(),entries.end(),
        [](const auto& a,const auto& b){return a.address<b.address;});
}
inline void extend_sonic_minicart_identities(std::vector<katana::runtime::NativePortLoadedAotModuleView>& modules) {
    const auto found=std::find_if(modules.begin(),modules.end(),[](const auto& module){return module.source_start==0x82980000u;});
    if(found==modules.end() || found->byte_size!=1434052u ||
       found->sha256!="sha256:2d3ec72d9f62a0ec626155f822a77bac7209999aa89f825b3082a69cf85db2a1" ||
       found->block_identities.size()!=9286u)
        throw std::runtime_error("MINICART retained module identity changed");
    static const auto blocks=[&]{
        std::vector<katana::runtime::NativePortLoadedAotBlockIdentityView> result(found->block_identities.begin(),found->block_identities.end());
        result.insert(result.end(),sonic_minicart_identities.begin(),sonic_minicart_identities.end());
        std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.source_offset<b.source_offset;});
        if(std::adjacent_find(result.begin(),result.end(),[](const auto& a,const auto& b){return a.source_offset==b.source_offset;})!=result.end())
            throw std::runtime_error("MINICART supplemental block identity collision");
        return result;
    }();
    found->block_identities=blocks;
}
} // runtime_dispatch_detail
} // katana_port_generated
''']
write_changed(directory/'minicart-bindings.hpp','\n'.join(lines))
identities=extend(Path(sys.argv[2]),[(offset,size,'sha256:'+digest) for _,offset,size,digest in entries.values()])
write_changed(directory/'minicart-identities.json',json.dumps(identities,indent=2)+'\n')
print(f'SONIC_MINICART_BINDINGS_READY functions=2 block_identities={len(entries)} dispatch_entries={2*len(entries)}')
