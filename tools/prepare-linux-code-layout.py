"""Prepare an optional ELF section order from two actual Linux game profiles.

Only rearranges existing code sections; no AOT export, function replacement,
KEEP, fixed addresses, data ordering or compiler-option changes. Libraries
whose functions share .text move as complete existing sections.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def symbol_sections(objdump, build, inputs, wanted):
    found=defaultdict(set)
    header=re.compile(r'^(.+?):\s+file format elf64-x86-64$')
    safe=re.compile(r'[A-Za-z0-9_.$/+():-]+')
    for source in inputs:
        owner=None
        with tempfile.TemporaryFile(mode='w+') as errors:
            process=subprocess.Popen([str(objdump),'--syms',source],cwd=build,
                stdout=subprocess.PIPE,stderr=errors,text=True,encoding='utf-8')
            try:
                for line in process.stdout:
                    match=header.match(line.strip())
                    if match:
                        owner=match[1]
                        continue
                    fields=line.split()
                    if len(fields)<6 or fields[-1] not in wanted or 'F' not in fields[1:-3]:
                        continue
                    section=fields[-3]
                    if not owner or not (section=='.text' or section.startswith('.text.')):
                        continue
                    if not safe.fullmatch(section) or not safe.fullmatch(owner):
                        raise ValueError('Unexpected ELF input section name')
                    # Named function sections may be COMDAT copies in multiple
                    # inputs. Their normal linker selection remains unchanged.
                    if section!='.text':
                        selector=f'*("{section}")'
                    else:
                        archive=re.fullmatch(r'(.+\.a)\(([^()]+)\)',owner)
                        file=(archive[1]+':'+archive[2]) if archive else owner
                        selector=f'"{file}"(.text)'
                    found[fields[-1]].add(selector)
                process.wait()
            finally:
                if process.poll() is None:
                    process.kill();process.wait()
                process.stdout.close()
            if process.returncode:
                errors.seek(0)
                raise RuntimeError(errors.read(8000))
    return found


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',type=Path,action='append',required=True,
                        help='Completed summary.json with adjacent perf-symbols.txt')
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--reference-elf',type=Path,required=True)
    parser.add_argument('--objdump',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if len(args.profile)<2:
        parser.error('Use at least two distinct gameplay scenarios')
    reference_sha=digest(args.reference_elf)
    weights=Counter();samples=Counter();occurrences=Counter();profiles=[];scenarios=set()
    for path in args.profile:
        info=json.loads(path.read_text())
        if (info.get('exe_sha256')!=reference_sha or not info.get('completed') or
                not info.get('expected_stop') or info.get('perf_exit_code')!=0):
            parser.error('Profile must complete on the exact reference executable: '+str(path))
        scenarios.add(info['scenario'])
        counts=Counter();filename=Path(info['exe']).name
        symbols=path.with_name('perf-symbols.txt')
        for line in symbols.read_text().splitlines():
            if not line.strip() or line.startswith('#'):
                continue
            fields=[x.strip() for x in line.split('|')]
            # perf may append an IPC column even for a cpu-clock event.
            if len(fields)<4 or fields[3]!=filename:
                continue
            symbol=fields[2].removeprefix('[.] ')
            if re.fullmatch(r'[A-Za-z_][A-Za-z0-9_.$]*',symbol) and fields[1].isdigit():
                counts[symbol]+=int(fields[1])
        total=sum(counts.values())
        if not total:
            parser.error('No game-owned symbol samples in '+str(symbols))
        for symbol,count in counts.items():
            weights[symbol]+=count/total/len(args.profile)
            samples[symbol]+=count;occurrences[symbol]+=1
        profiles.append({'scenario':info['scenario'],'exe_sha256':reference_sha,
                         'symbols_sha256':digest(symbols),'owned_samples':total})
    if len(scenarios)<2:
        parser.error('Profiles must cover distinct gameplay scenarios')
    wanted={s for s in weights if occurrences[s]>1 or samples[s]>=5}
    inputs=['CMakeFiles/game.dir/launcher/main.cpp.o',
            'CMakeFiles/game.dir/generated/motion-dispatch/native-port-dispatch.cpp.o',
            'CMakeFiles/game.dir/generated/minicart/minicart-aot.cpp.o']
    inputs+=['libsonic_linux_'+s+'.a' for s in
             ('guest','title','services','graphics','vulkan','startup','movie','platform','audio','aot_runtime')]
    build=args.build.resolve(strict=True)
    found=symbol_sections(args.objdump.resolve(strict=True),build,inputs,wanted)
    groups=Counter();matched=[];ambiguous=[]
    for symbol in sorted(wanted):
        choices=found.get(symbol,set())
        if len(choices)!=1:
            if choices:ambiguous.append(symbol)
            continue
        selector=next(iter(choices));groups[selector]+=weights[symbol]
        matched.append({'symbol':symbol,'samples':samples[symbol],
                        'weight':weights[symbol],'selector':selector})
    if not groups:
        parser.error('No unambiguous sampled code sections found')
    order=sorted(groups,key=lambda s:(-groups[s],s))
    script=('/* Generated by prepare-linux-code-layout.py; code placement only. */\n'
            'SECTIONS {\n  .text.sarecomp_hot : {\n'+
            ''.join('    '+s+'\n' for s in order)+
            '  }\n} INSERT BEFORE .text;\n')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(script,encoding='ascii',newline='\n')
    report={'profiles':profiles,'reference_sha256':reference_sha,
            'inputs':{s:digest(build/s) for s in inputs},'script_sha256':digest(args.output),
            'sections':len(order),'requested_symbols':len(wanted),'functions':matched,
            'ambiguous':ambiguous,'missing':sorted(wanted-found.keys())}
    args.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
    print('SONIC_LINUX_CODE_LAYOUT_READY sections='+str(len(order))+
          ' functions='+str(len(matched))+' missing='+str(len(report['missing']))+
          ' ambiguous='+str(len(ambiguous)))


if __name__=='__main__':
    main()
