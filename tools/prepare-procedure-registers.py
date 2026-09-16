"""Prepare a reviewed connected procedure closure; keep the public AOT ABI."""
import argparse
import hashlib
import json
from pathlib import Path
import re

# Whole original bodies are pinned to the access/boundary audit, in addition
# to the artifact manifest. A newly generated body requires an explicit review.
BODIES = {
    '8C057B00': '61216dd422e5f672dcbb90786b3dd58d293b4aac20794a0b63bf35cefbcdfc5a',
    '8C040784': 'd2ba66cf1788aa110bad46e1c7a8783810ef55bc222980c6320be88d2147b349',
    '8C040942': 'adab290568ca58182238831e2a2f39c987a11895a282b93ad6c8d86e3f113028',
    '8C0417C8': '68da4ac5973bbb6e33b7ecb4e4066f2ba9fb90b9da2d3601cc57dd49e2c80b6b',
    '8C041A2E': '6ec9f6113ebc3483016884ac267390755bc0c4af8bb56112dd368a2d6e46d019',
    '8C055C8E': '151864ef49313365f40d1e08db877f7a7360b861a4fd23aad91d8f7b4977d28a',
}
OWNER = re.compile(r'(?m)^BlockExit fn_([0-9A-F]+)_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{\n')
REGISTERS = re.compile(r'katana::runtime::NativeAotRegisterFile<0x[0-9A-F]+u, 0x[0-9A-F]+u> katana_registers\(cpu\);')
RAW = re.compile(r'\bcpu\.(r\[(\d+)\]|t\b|pr\b|gbr\b|mach\b|macl\b|fpul\b)')
EXPECTED = {'8C057B00': (7, 2), '8C040784': (1, 0), '8C040942': (1, 0),
            '8C0417C8': (0, 0), '8C041A2E': (2, 0), '8C055C8E': (0, 0)}
PARAMS = 'CpuState& cpu, BlockExecutionContext& context'
PRIVATE_PARAMS = PARAMS + ', sonic::procedure_registers::Bank<>& bank'

def digest(data): return hashlib.sha256(data).hexdigest()

def once(text, old, new):
    if text.count(old) != 1: raise ValueError('Procedure boundary changed: '+old[:100])
    return text.replace(old, new)

def owners(text):
    matches = list(OWNER.finditer(text))
    return [(m[1], m.start(), matches[i+1].start() if i+1<len(matches) else len(text))
            for i,m in enumerate(matches)]

def transform_owner(body, name):
    original_body = body
    if sorted(set(re.findall(r'cpu\.(\w+)\(', body))) not in ([], ['privileged_mode_inline']):
        raise ValueError('Unreviewed CpuState method in '+name)
    # Compare writes CPU T without a release, and the next statement explicitly
    # imports its result. All other raw fields must follow current ownership.
    compare = re.compile(r'(katana::runtime::fpu_compare_greater\(cpu, \d+u, \d+u\);\n\s*)katana_registers.t\(\) = cpu.t;')
    body, comparisons = compare.subn(r'\1katana_registers.t() = SONIC_PUBLIC_T_RESULT;', body)
    raw_count = len(list(RAW.finditer(body)))
    body = RAW.sub(lambda m: 'bank.raw_r('+m[2]+')' if m[2] else 'bank.raw_'+m[1]+'()', body)
    body = body.replace('SONIC_PUBLIC_T_RESULT', 'cpu.t')
    body, count = REGISTERS.subn('sonic::procedure_registers::Frame<> katana_registers(bank);', body)
    if count != 1: raise ValueError('Expected one complete register owner: '+name)
    # Match the complete reviewed front half of each direct-call envelope.
    # Existing exception, continuation PC, memory-generation and depth guards
    # remain verbatim. Unknown/public callees retain their original flush.
    private_calls = 0
    for target in BODIES:
        common = '''                    katana::runtime::NativeAotCallDepthGuard native_call_depth;
                    if (native_call_depth) {
                        const auto exception_generation_before_native_call = cpu.exception_generation;
'''
        fixed = ('katana_registers.flush_release();\n'
                 '                if (services != nullptr && services->can_chain_executable_block(cpu.pc)) {\n'+common+
                 f'                        static_cast<void>(fn_{target}_runtime_entry(cpu, context));')
        selected = ('katana_registers.flush_release();\n'
                    f'                if (katana::runtime::unrelocate_code_address_inline(call_target) == 0x{target}u && services != nullptr && services->can_chain_executable_block(cpu.pc)) {{\n'+common+
                    '                        consume_native_bringup_direct_aot_dispatch(call_target);\n'+
                    f'                        static_cast<void>(fn_{target}_runtime_entry(cpu, context));')
        for old in (fixed, selected):
            n = body.count(old)
            new = old.replace('katana_registers.flush_release();', 'katana_registers.suspend_for_private_call();').replace(
                f'fn_{target}_runtime_entry(cpu, context)', f'fn_{target}_private(cpu, context, bank)')
            body = body.replace(old, new)
            private_calls += n
    if (private_calls, comparisons) != EXPECTED[name]:
        raise ValueError(f'Closure/compare shape changed: {name} {(private_calls,comparisons)}')
    # The public wrapper owns the bank. Private direct returns keep it live;
    # the original non-direct epilogue publishes before scheduler/BlockExit.
    body = once(body, '    context.scheduler_cycle = services->scheduler_cycle();',
                '    bank.publish_release();\n    context.scheduler_cycle = services->scheduler_cycle();')
    signature = f'BlockExit fn_{name}_runtime_entry({PARAMS}) {{\n'
    wrapper = signature + ('    sonic::procedure_registers::Bank bank(cpu);\n'+
                           f'    return fn_{name}_private(cpu, context, bank);\n}}\n\n')
    body = once(body, signature, f'BlockExit fn_{name}_private({PRIVATE_PARAMS}) {{\n')
    # Leaves reached through public dispatch retain their small original mask;
    # only private callers use the shared-bank version. No root copy penalty.
    public = wrapper if private_calls else original_body
    return public+body, {'owner':name,'private_calls':private_calls,'raw_accesses':raw_count,
                         'public_compare_results':comparisons}

def write(path, data):
    if path.is_symlink() or (path.exists() and path.stat().st_nlink != 1):
        raise ValueError('Linked output: '+str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes()!=data: path.write_bytes(data)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root',type=Path,required=True)
    p.add_argument('--units-file',type=Path,required=True)
    p.add_argument('--destination',type=Path,required=True)
    a=p.parse_args(); root=a.source_root.resolve(); out=a.destination.resolve()
    repo=Path(__file__).resolve().parents[1]
    if not out.relative_to(repo).parts[0].startswith('build-'):
        raise ValueError('Prepared output must be build-local')
    lines=(root/'.katana-generated-artifacts').read_text().splitlines()
    generation='generation\tsha256:'+digest(('\n'.join(lines[2:])+'\n').encode())
    if lines[:2]!=['katana-codegen-artifacts-v2',generation]:raise ValueError('Retained manifest changed')
    records={row[0]:row for line in lines[2:] if len(row:=line.split('\t'))>=3}
    report={'generation':generation,'units':[],'owners':[]}
    seen=set()
    for line in a.units_file.read_text().splitlines():
        path=Path(line); original=(root/'code'/path.name).read_bytes(); data=path.read_bytes()
        if path.resolve().parent==out:raise ValueError('Input/output overlap')
        original_hash=digest(original)
        if records.get('code/'+path.name,[])[1:3]!=[str(len(original)),'sha256:'+original_hash]:
            raise ValueError('Retained unit changed: '+path.name)
        if path.resolve().parent==root/'code':
            if data!=original:raise ValueError('Original changed')
        elif path.parent.name=='region-writes':
            previous=json.loads((path.parent/'preparation.json').read_text())
            entry=[x for x in previous['units'] if x['unit']==path.name]
            if (previous['mode']!='region' or previous.get('guard_probe',False) or
                previous['generation']!=generation or len(entry)!=1 or
                entry[0]['source_sha256']!=original_hash or entry[0]['output_sha256']!=digest(data)):
                raise ValueError('RAM-region provenance changed: '+path.name)
        else:raise ValueError('Unreviewed preparation: '+str(path))
        old=original.decode().replace('\r\n','\n')
        for name,begin,end in owners(old):
            if name in BODIES and digest(old[begin:end].encode())!=BODIES[name]:
                raise ValueError('Audited procedure changed: '+name)
        text=data.decode().replace('\r\n','\n')
        for name,begin,end in reversed(owners(text)):
            if name not in BODIES:continue
            if name in seen:raise ValueError('Duplicate procedure: '+name)
            changed,row=transform_owner(text[begin:end],name)
            report['owners'].append(row);seen.add(name)
            text=text[:begin]+changed+text[end:]
        declarations='\n'.join(f'BlockExit fn_{name}_private({PRIVATE_PARAMS});' for name in sorted(BODIES))
        marker='using BlockExit = katana::runtime::BlockExit;'
        text=once(text,marker,marker+'\n'+declarations)
        data_out=('#include "sonic_procedure_registers.hpp"\n'+text).encode()
        write(out/path.name,data_out)
        report['units'].append({'unit':path.name,'input_sha256':digest(data),
                                'original_sha256':original_hash,'output_sha256':digest(data_out)})
    if seen!=set(BODIES):raise ValueError('Incomplete private closure: '+str(set(BODIES)-seen))
    write(out/'preparation.json',(json.dumps(report,indent=2)+'\n').encode())
    print('SONIC_PRIVATE_PROCEDURES_READY owners='+str(len(seen))+' direct_sites='+str(sum(x['private_calls'] for x in report['owners'])))

if __name__=='__main__':main()
