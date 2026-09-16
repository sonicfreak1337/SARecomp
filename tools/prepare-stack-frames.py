"""Fuse authenticated, contiguous integer/PR stack instructions; retain resumes."""
import hashlib
import re

HEADER = '#include "sonic_stack_frames.hpp"\n'
START = re.compile(r'(?m)^( +)\{\n\1    // katana-guest 0x([0-9A-F]{8})u\n')
# Entire ordinary-instruction envelopes, whitespace-normalized after binding
# PC, following PC, opcode and the integer register. Delay slots differ and
# cannot match. The retained artifact manifest authenticates the source too.
SHAPES = {
    ('push',False):'3061ef95400e35de73f8354f49775d185354279ade2e68ccf78b7f334b7bb081',
    ('push',True):'cff1df4fdfbe026b5198c320060794866e67c844ae63059c3fff9d7bd01f65a3',
    ('pop',False):'749f2bdff6510c3e1c9af6db5bbd07d87cb1bb38ddedcb55e81a46e649d7088e',
    ('pop',True):'fa7f53db248442e7e13d9611e6d4a04517032169bd1d0d3f1c7ef9267757276a',
}

def transform(source):
    instructions=[]
    for m in START.finditer(source):
        indent,pc=m.groups()
        end=re.compile(r'(?m)^'+indent+r'\}').search(source,m.end())
        if not end:raise ValueError('Unclosed guest instruction')
        block=source[m.start():end.end()]
        # Each ordinary fault path has exactly one original opcode; it is the
        # independent operand binding for the source instruction we recognize.
        opcodes=re.findall(r', 0x([0-9A-F]{8})u\);',block)
        if len(opcodes)!=1:continue
        op=int(opcodes[0],16)
        if op==0x4F22:kind,index='push',16
        elif op==0x4F26:kind,index='pop',16
        elif op&0xFF0F==0x2F06:kind,index='push',(op>>4)&15
        elif op&0xF0FF==0x60F6:kind,index='pop',(op>>8)&15
        else:continue
        if index==15:continue
        following=f'{int(pc,16)+2:08X}'
        normalized='\n'.join(line.strip() for line in block.splitlines())
        normalized=normalized.replace(following,'NEXT').replace(pc,'PC').replace(opcodes[0],'OP')
        if index!=16:normalized=normalized.replace(f'katana_registers[{index}]','REG')
        if hashlib.sha256(normalized.encode()).hexdigest()!=SHAPES[kind,index==16]:continue
        instructions.append({'start':m.start(),'end':end.end(),'indent':indent,'pc':pc,
                             'next':following,'kind':kind,'index':index})
    groups=[]
    for instruction in instructions:
        previous=groups[-1][-1] if groups else None
        contiguous=(previous and previous['kind']==instruction['kind'] and
            previous['next']==instruction['pc'] and previous['indent']==instruction['indent'] and
            source[previous['end']:instruction['start']]==
                '\n'+instruction['indent']+'katana_block_'+instruction['pc']+'_resume:\n')
        if contiguous and len(groups[-1])<16:groups[-1].append(instruction)
        else:groups.append([instruction])
    insertions=[];report=[]
    for group in groups:
        if len(group)<2:continue
        first,last=group[0],group[-1]
        after='katana_block_'+last['next']+'_resume'
        if not source[last['end']:].startswith('\n'+last['indent']+after+':\n'):continue
        kind=first['kind'];indent=first['indent']
        owner=source.rfind('\nBlockExit fn_',0,first['start'])
        if owner<0 or 'bool katana_guest_write_exit_requested = false;' not in source[owner:first['start']]:continue
        indices=','.join(str(i['index'])+'u' for i in group)
        tracker='katana_direct_ram,katana_direct_ram_code_tracker' if kind=='push' else 'katana_direct_ram'
        prefix=(indent+'// sonic-stack-frame '+first['pc']+'\n'+
            indent+'if (!katana_guest_write_exit_requested && '+
                ('katana_direct_ram_writes == nullptr && ' if kind=='push' else '')+'\n'+
            indent+'    sonic::stack_frames::'+kind+'<'+indices+'>(cpu,katana_registers,'+tracker+',\n'+
            indent+'        katana::runtime::relocate_code_address_inline(0x'+last['pc']+'u)))\n'+
            indent+'    goto '+after+';\n')
        insertions.append((first['start'],prefix))
        report.append({'pc':first['pc'],'last_pc':last['pc'],'kind':kind,'registers':[i['index'] for i in group]})
    output=source
    for start,prefix in reversed(insertions):output=output[:start]+prefix+output[start:]
    restored=output
    for _,prefix in insertions:restored=restored.replace(prefix,'',1)
    if restored!=source:raise ValueError('Unrelated code changed')
    return (HEADER+output if report else source),report
