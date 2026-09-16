"""Expose proven constant initialization of generated dispatch TLS declarations."""
import argparse
import hashlib
import json
from pathlib import Path
import re

DISPATCH_SHA='646a4a150152ecc97759d38d181208dea18d177de98461316c55654117440a67'
DECLARATIONS=(
    'extern thread_local PlatformServices* active_services;',
    'extern thread_local katana::runtime::BlockAddress active_exit_source;',
    'extern thread_local katana::runtime::BlockEndKind active_exit_kind;',
    'extern thread_local katana::runtime::DynamicDispatchSiteClass active_exit_site_class;',
    'extern thread_local bool tail_dispatch_completed;',
)
DEFINITIONS=(
    'thread_local katana::runtime::NativePortAotServices* active_services = nullptr;',
    'thread_local katana::runtime::BlockAddress active_exit_source{};',
    'thread_local katana::runtime::BlockEndKind active_exit_kind =\n    katana::runtime::BlockEndKind::Fallthrough;',
    'thread_local katana::runtime::DynamicDispatchSiteClass active_exit_site_class =\n    katana::runtime::DynamicDispatchSiteClass::NotDynamic;',
    'thread_local bool tail_dispatch_completed = false;',
)
def sha(data):return hashlib.sha256(data).hexdigest()
def write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
def transform(source):
    for declaration in DECLARATIONS:
        if source.count(declaration)!=1:raise ValueError('Unknown TLS declaration multiplicity')
    result=source
    for declaration in DECLARATIONS:result=result.replace(declaration,declaration.replace('extern thread_local','extern constinit thread_local'))
    if result.replace('extern constinit thread_local','extern thread_local')!=source:
        raise ValueError('Changed unrelated source')
    return result
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root',type=Path,required=True)
    p.add_argument('--destination',type=Path,required=True)
    p.add_argument('--runtime-dispatch',type=Path,required=True)
    p.add_argument('--unit',action='append',required=True)
    a=p.parse_args()
    root,dest=a.source_root.resolve(),a.destination.resolve()
    if root==dest or root in dest.parents or dest in root.parents:raise ValueError('Outputs overlap retained source')
    lines=(root/'.katana-generated-artifacts').read_text().splitlines()
    if lines[0]!='katana-codegen-artifacts-v2' or lines[1]!='generation\tsha256:'+sha(('\n'.join(lines[2:])+'\n').encode()):
        raise ValueError('Invalid retained source manifest')
    records={parts[0]:parts for line in lines[2:] if len(parts:=line.split('\t'))>=3}
    dispatch=(root/'code/native-port-dispatch.cpp').read_bytes()
    if sha(dispatch)!=DISPATCH_SHA:raise ValueError('TLS definition source changed')
    runtime_dispatch=a.runtime_dispatch.read_text()
    for definition in DEFINITIONS:
        if dispatch.decode().count(definition)!=1 or runtime_dispatch.count(definition)!=1:
            raise ValueError('Unknown original or effective TLS definition')
    # C++ requires constinit on the initializing declaration too. The actual
    # dispatch definition TU must be replaced, not merely checked in a fixture.
    effective_output=runtime_dispatch
    for definition in DEFINITIONS:
        effective_output=effective_output.replace(definition,'constinit '+definition)
    if effective_output.replace('constinit thread_local','thread_local')!=runtime_dispatch:
        raise ValueError('Changed unrelated effective dispatch source')
    write(dest/'native-port-dispatch.cpp',effective_output.encode())
    # Independently compile the exact real definitions with constinit. A changed
    # constructor/default initializer must not silently weaken this premise.
    proof='#include "katana/runtime/native_port_aot_runtime.hpp"\nnamespace sonic_tls_definition_proof {\n'
    proof+='\n'.join('constinit '+definition for definition in DEFINITIONS)+'\n}\n'
    write(dest/'definition-proof.cpp',proof.encode())
    if len(a.unit)!=len(set(a.unit)):raise ValueError('Duplicate source member')
    report={'schema':'sarecomp-constinit-dispatch-v1','generation':lines[1],
            'dispatch_source_sha256':DISPATCH_SHA,
            'runtime_dispatch_sha256':sha(a.runtime_dispatch.read_bytes()),
            'runtime_dispatch_output_sha256':sha(effective_output.encode()),
            'definition_proof_sha256':sha(proof.encode()),'units':[]}
    for unit in a.unit:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp',unit):raise ValueError('Invalid source member')
        data=(root/'code'/unit).read_bytes()
        if records.get('code/'+unit,[])[1:3]!=[str(len(data)),'sha256:'+sha(data)]:raise ValueError('Source identity mismatch')
        output=transform(data.decode()).encode()
        write(dest/unit,output)
        report['units'].append({'unit':unit,'source_sha256':sha(data),'output_sha256':sha(output),'declarations':len(DECLARATIONS)})
    write(dest/'preparation.json',(json.dumps(report,indent=2)+'\n').encode())
    print('SONIC_CONSTINIT_DISPATCH_READY units='+str(len(report['units'])))
if __name__=='__main__':main()
