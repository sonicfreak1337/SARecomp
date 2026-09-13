"""Source-bound elimination of a forwarding overload; retained arithmetic stays intact."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile

UNIT = 'unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp'
SOURCE_SHA = '79ecc1ebbdf3e06c517d5edcc42850c08eb50c7dfe6fe4359c59b0e626472558'
CALL = re.compile(r'(?m)^( +)(katana::runtime::fpu_binary\(cpu, katana::runtime::FpuBinaryOperation::(Add|Subtract|Multiply|Divide), (\d+)u, (\d+)u)\);$')
WRAPPER = '''void fpu_binary(CpuState& cpu,
                const FpuBinaryOperation operation,
                const std::uint8_t source,
                const std::uint8_t destination) noexcept {
    static_cast<void>(fpu_binary(cpu, operation, source, destination, std::nullopt));
}'''

def sha(data): return hashlib.sha256(data).hexdigest()

def prepare(args):
    source_root=args.source_root.resolve(); destination=args.destination.resolve()
    if source_root==destination or source_root in destination.parents or destination in source_root.parents:
        raise ValueError('Experiment output must be separate from retained input')
    data=(source_root/'code'/UNIT).read_bytes()
    if sha(data)!=SOURCE_SHA: raise ValueError('Selected AOT unit changed')
    manifest=(source_root/'.katana-generated-artifacts').read_text().splitlines()
    if manifest[:1]!=['katana-codegen-artifacts-v2'] or manifest[1]!='generation\tsha256:'+sha(('\n'.join(manifest[2:])+'\n').encode()):
        raise ValueError('Invalid retained source manifest')
    records=[line.split('\t') for line in manifest[2:] if line.split('\t')[0]=='code/'+UNIT]
    if len(records)!=1 or records[0][1:3]!=[str(len(data)),'sha256:'+SOURCE_SHA]:
        raise ValueError('Retained manifest differs from source')
    with zipfile.ZipFile(args.sdk) as sdk: fpu=sdk.read('src/runtime/fpu.cpp').decode()
    if fpu.count(WRAPPER)!=1: raise ValueError('SDK forwarding overload changed')
    source=data.decode(); matches=list(CALL.finditer(source))
    counts={op:sum(m[3]==op for m in matches) for op in ('Add','Subtract','Multiply','Divide')}
    if counts!={'Add':67,'Subtract':82,'Multiply':274,'Divide':2}:
        raise ValueError('Selected call family changed')
    entries=re.findall(r'(?m)^BlockExit (fn_[0-9A-F]+_runtime_entry)\(CpuState& cpu, BlockExecutionContext& context\) \{',source)
    if not entries or len(entries)!=len(set(entries)): raise ValueError('Entry definitions changed')
    candidate=CALL.sub(lambda m:m[1]+'static_cast<void>('+m[2]+', std::nullopt));',source)
    # Every replacement is the exact body of the original forwarding overload.
    # Reverse only those call lines to prove all guards/epochs/accounting stay.
    reverse=re.sub(r'(?m)^( +)static_cast<void>\((katana::runtime::fpu_binary\(cpu, katana::runtime::FpuBinaryOperation::(?:Add|Subtract|Multiply|Divide), \d+u, \d+u), std::nullopt\)\);$',r'\1\2);',candidate)
    if reverse!=source: raise ValueError('Changes escaped the selected calls')
    output=data if args.mode=='control' else candidate.encode()
    report={'schema':'sarecomp-fpu-calls-v1','mode':args.mode,'unit':UNIT,
        'source_sha256':SOURCE_SHA,'output_sha256':sha(output),'sdk_fpu_sha256':sha(fpu.encode()),
        'forwarding_calls':len(matches),'operations':counts,'entries':entries,
        'source_lines':[source.count('\n',0,m.start())+1 for m in matches]}
    destination.mkdir(parents=True,exist_ok=True)
    for name,value in ((UNIT,output),('preparation.json',(json.dumps(report,indent=2)+'\n').encode())):
        path=destination/name
        if not path.exists() or path.read_bytes()!=value:path.write_bytes(value)
    print(f'SONIC_FPU_CALLS_READY mode={args.mode} forwarding_calls={len(matches)} entries={len(entries)} arithmetic=retained')

def audit(args):
    report=json.loads(args.report.read_text()); entries={e:[] for e in report['entries']}
    pattern=re.compile(r'^\s*[0-9A-Fa-f]+:[0-9A-Fa-f]+\s+\?(fn_[0-9A-F]+_runtime_entry)@katana_port_generated@@')
    member=report['unit'].lower()+'.obj'
    with args.map.open() as stream:
        for line in stream:
            if member in line.lower() and ('katana_generated:' in line.lower() or '.lto.katana_generated.' in line.lower()):
                raise ValueError('Original selected member still linked')
            match=pattern.match(line)
            if match and match[1] in entries:entries[match[1]].append(line.split()[-1].lower())
    if any(owners!=['sonic_fpu_calls:'+member] for owners in entries.values()):
        raise ValueError('Selected entry did not bind exclusively to experimental member')
    print(f'SONIC_FPU_CALLS_LINK_PASS mode={report["mode"]} entries={len(entries)} original_selected_members=0')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); commands=parser.add_subparsers(dest='command',required=True)
    p=commands.add_parser('prepare')
    for name in ('source-root','destination','sdk'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--mode',choices=('control','direct'),required=True);p.set_defaults(run=prepare)
    a=commands.add_parser('audit');a.add_argument('--map',type=Path,required=True);a.add_argument('--report',type=Path,required=True);a.set_defaults(run=audit)
    args=parser.parse_args();args.run(args)
