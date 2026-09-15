"""Source-authenticated, optional AOT statistics; timing/fault state stays exact."""
import argparse
import hashlib
import json
from pathlib import Path
import re

RUNTIME_SHA = '74661d49e5556055b6fd7d05e42d89c7fc4f8aed9167b21012f204de752c52a3'
REPLACEMENTS = {
    'katana::runtime::ExplicitGuestInstructionAttempt': 'sonic::statistics::InstructionAttempt',
    **{'katana::runtime::direct_linear_guard_read_u'+str(n):
       'sonic::statistics::read_u'+str(n) for n in (8,16,32)},
}

def sha(data): return hashlib.sha256(data).hexdigest()

def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes()!=data: path.write_bytes(data)

def helper(header):
    if sha(header)!=RUNTIME_SHA: raise ValueError('Pinned instruction-attempt header changed')
    source=header.decode()
    begin=source.index('class ExplicitGuestInstructionAttempt final {')
    end=source.index('\nclass GuestInstructionAttempt final {', begin)
    body=source[begin:end].strip().replace('ExplicitGuestInstructionAttempt','InstructionAttempt')
    changes={
        '++cpu_.attempted_guest_instructions;':'if (record_) ++cpu_.attempted_guest_instructions;',
        'if (!completed_ &&':'if (record_ && !completed_ &&',
        'bool completed_ = false;':'bool completed_ = false;\n    const bool record_ = SONIC_COLLECT_AOT_STATISTICS;',
    }
    for before,after in changes.items():
        if body.count(before)!=1: raise ValueError('Instruction-attempt boundary changed')
        body=body.replace(before,after)
    return ('''// Generated from the pinned runtime.hpp; do not edit.
#pragma once
#include "katana/runtime/runtime.hpp"
#include "sonic_internal_diagnostics.hpp"
#include <bit>
#include <cstring>
#if defined(SARECOMP_AOT_STATISTICS_OMIT)
#define SONIC_COLLECT_AOT_STATISTICS false
#else
#define SONIC_COLLECT_AOT_STATISTICS sonic::diagnostics::runtime_checks_enabled()
#endif
namespace sonic::statistics {
using namespace katana::runtime;
// Only observational attempt/retire totals are optional. Preserve active PC,
// physical fault origin, exact pending cycles and exception-generation rules.
'''+body+'''

template<class T>
inline bool read_without_statistics(const DirectLinearMemoryGuard& guard,
                                    std::uint32_t address, T& value) noexcept {
    std::uint32_t offset=0;
    if (!direct_linear_guard_offset(guard,address,sizeof(T),offset)) return false;
    if constexpr (std::endian::native==std::endian::little) {
        std::memcpy(&value,guard.read_bytes+offset,sizeof(T));
    } else {
        value=0;
        for (unsigned i=0;i<sizeof(T);++i) value|=T(guard.read_bytes[offset+i])<<(i*8);
    }
    return true;
}
'''+''.join('''
inline bool read_u%(n)s(const DirectLinearMemoryGuard& guard,
                       std::uint32_t address, std::uint%(n)s_t& value) noexcept {
    if (SONIC_COLLECT_AOT_STATISTICS)
        return katana::runtime::direct_linear_guard_read_u%(n)s(guard,address,value);
    return read_without_statistics(guard,address,value);
}
'''%{'n':n} for n in (8,16,32))+'''
} // namespace sonic::statistics
#undef SONIC_COLLECT_AOT_STATISTICS
''').encode()

def transform(source):
    counts={}
    candidate=source
    for old,new in REPLACEMENTS.items():
        # Never replace the prefix of the separate grouped-read template.
        pattern=re.compile(re.escape(old)+r'\b')
        candidate,count=pattern.subn(new,candidate)
        counts[old.rsplit('::',1)[-1]]=count
    reversed_source=candidate
    for old,new in REPLACEMENTS.items(): reversed_source=reversed_source.replace(new,old)
    if reversed_source!=source: raise ValueError('Unexpected change outside policy helpers')
    if not counts['ExplicitGuestInstructionAttempt']: raise ValueError('No instruction attempts in member')
    return '#include "sonic_aot_statistics.hpp"\n'+candidate,counts

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root',type=Path,required=True)
    p.add_argument('--destination',type=Path,required=True)
    p.add_argument('--runtime-header',type=Path,required=True)
    p.add_argument('--mode',choices=('runtime','omit'),default='runtime')
    p.add_argument('--unit',action='append',default=[])
    a=p.parse_args()
    root,destination=a.source_root.resolve(),a.destination.resolve()
    if root==destination or root in destination.parents or destination in root.parents:
        raise ValueError('Output must be separate from retained inputs')
    manifest=(root/'.katana-generated-artifacts').read_text().splitlines()
    if manifest[0]!='katana-codegen-artifacts-v2' or manifest[1]!='generation\tsha256:'+sha(('\n'.join(manifest[2:])+'\n').encode()):
        raise ValueError('Invalid retained source manifest')
    if len(a.unit)!=len(set(a.unit)): raise ValueError('Duplicate member')
    generated_helper=helper(a.runtime_header.read_bytes())
    report={'schema':'sarecomp-aot-statistics-v1','mode':a.mode,
            'compile_definitions':['SARECOMP_AOT_STATISTICS_OMIT=1'] if a.mode=='omit' else [],
            'manifest_generation':manifest[1],
            'runtime_sha256':RUNTIME_SHA,'helper_sha256':sha(generated_helper),'units':[]}
    records={r[0]:r for line in manifest[2:] if len(r:=line.split('\t'))>=3}
    for name in a.unit:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp',name): raise ValueError('Invalid member')
        data=(root/'code'/name).read_bytes()
        if records.get('code/'+name,[])[1:3]!=[str(len(data)),'sha256:'+sha(data)]: raise ValueError('Member identity mismatch')
        transformed,counts=transform(data.decode())
        output=transformed.encode()
        report['units'].append({'name':name,'source_sha256':sha(data),'output_sha256':sha(output),'counts':counts})
        write(destination/name,output)
    write(destination/'sonic_aot_statistics.hpp',generated_helper)
    write(destination/'preparation.json',(json.dumps(report,indent=2)+'\n').encode())
    print('SONIC_AOT_STATISTICS_READY units='+str(len(report['units']))+' attempts='+
          str(sum(u['counts']['ExplicitGuestInstructionAttempt'] for u in report['units'])))

if __name__=='__main__': main()
