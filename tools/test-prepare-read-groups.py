"""Check actual retained group qualification and exact fallback preservation."""
import importlib.util
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('read_groups', root/'tools/prepare-ram-read-groups.py')
recipe=importlib.util.module_from_spec(spec)
spec.loader.exec_module(recipe)
source=(root/'.local/working-product/generated/code/unit-v8C029400-8C029B00-9bed0201322da5d9.cpp').read_text()
output,groups=recipe.transform(source)
assert len(groups)==43 and sum(map(len,groups))==101
restored=output.removeprefix('#include "sonic_read_group.hpp"\n')
for group in reversed(groups):
    before,after=recipe.fast_group(group)
    begin,end=group[0]['begin'],group[-1]['end']
    original=source[begin:end]
    replacement=before+original+after
    assert replacement in restored
    restored=restored.replace(replacement,original,1)
assert restored==source

group=groups[0]
begin,end=group[0]['begin'],group[-1]['end']
second=group[1]['begin']
for inserted in ('services->consume_guest_cycles(1u, 0u);\n',
                 'katana_registers[0] += 4u;\n',
                 'katana_block_00000000_resume:\n'):
    changed=source[:second]+inserted+source[second:]
    admitted=recipe.groups(changed)
    assert not any(g[0]['pc']==group[0]['pc'] and g[-1]['pc']==group[-1]['pc'] for g in admitted)

# A valid same-PC occurrence elsewhere must not authorize a changed envelope.
mutant=source[begin:end].replace('guest_instruction_attempt.complete();',
                                'cpu.mmucr = 1u; guest_instruction_attempt.complete();',1)
admitted=recipe.groups(source+'\n'+mutant)
assert not any(g[0]['begin']>=len(source) for g in admitted)

# Pointer-dependent reads must use the staged result, never the old real GPR.
synthetic=[
    dict(pc='8C000100',indent='    ',address='katana_registers[3]',destination=4),
    dict(pc='8C000102',indent='    ',address='katana_registers[4]',destination=3),
    dict(pc='8C000104',indent='    ',address='katana_registers[0] + katana_registers[3]',destination=5,kind='scalar-fmov')]
before,after=recipe.fast_group(synthetic)
assert 'sonic_group.read(sonic_group_value_0, sonic_group_value_1)' in before
assert 'sonic_group.read(katana_registers[0] + sonic_group_value_1, sonic_group_value_2)' in before
assert before.index('sonic_group.commit(3u)')<before.index('katana_registers[4] =')
assert '0x8C000104u' in before and after.strip()=='sonic_read_group_8C000100_end: ;'
print('SONIC_READ_GROUP_QUALIFICATION_OK groups=43 instructions=101 original_fallback_exact=1 rejected_gaps=3 duplicate_pc_isolated=1 dependent_reads=1')
