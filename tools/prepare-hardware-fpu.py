"""Source-bound, opt-in hardware scalar arithmetic with unchanged fallback."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

spec=importlib.util.spec_from_file_location('fpu_recipe',Path(__file__).with_name('prepare-fpu-runtime.py'))
recipe=importlib.util.module_from_spec(spec)
spec.loader.exec_module(recipe)


def generate(sdk, helper):
    outputs=recipe.generate(sdk)
    base=outputs['fpu.cpp'].decode()
    before='    bool accepted = false;\n    switch (operation) {'
    after=('    bool accepted = sonic_try_hardware_single(n, m, operation, rounding, result);\n'
           '    if (!accepted) switch (operation) {')
    if base.count(before)!=1:
        raise RuntimeError('Original fast-helper boundary changed')
    anchor='namespace {\n// This replaces helper work only, never instruction admission.'
    insertion='namespace {\n#include "sonic_hardware_fpu.inc"\n// This replaces helper work only, never instruction admission.'
    if base.count(anchor)!=1:
        raise RuntimeError('Original helper scope changed')
    candidate=base.replace(before,after,1).replace(anchor,insertion,1)
    if candidate.replace(insertion,anchor,1).replace(after,before,1)!=base:
        raise RuntimeError('Unexpected hardware FPU substitution')
    outputs['fpu.cpp']=candidate.encode()
    report={'schema':'sarecomp-hardware-fpu-v1','base_source_sha256':recipe.SOURCE_SHA,
            'base_prepared_sha256':recipe.sha(base.encode()),
            'helper_sha256':recipe.sha(helper.read_bytes()),
            'outputs':{name:{'bytes':len(data),'sha256':recipe.sha(data)}
                       for name,data in outputs.items() if name!='provenance.json'},
            'fallback':'all prior integer/exception behavior unchanged',
            'default':'off','scope':'scalar Add/Subtract/Multiply/Divide normal or zero; interior results only'}
    outputs['provenance.json']=(json.dumps(report,indent=2)+'\n').encode()
    return outputs


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk',type=Path,required=True)
    p.add_argument('--helper',type=Path,required=True)
    p.add_argument('--output-dir',type=Path,required=True)
    args=p.parse_args()
    root=Path(__file__).resolve().parents[1]
    destination=args.output_dir.resolve()
    relative=destination.relative_to(root)
    if not relative.parts or not relative.parts[0].startswith('build-'):
        raise RuntimeError('Output must be inside a separate project build-* directory')
    outputs=generate(args.sdk,args.helper)
    for name in outputs:
        target=destination/name
        if target.is_symlink() or (target.exists() and (not target.is_file() or target.stat().st_nlink>1)):
            raise RuntimeError('Refusing linked or nonregular output')
    destination.mkdir(parents=True,exist_ok=True)
    for name,data in outputs.items():
        target=destination/name
        if not target.exists() or target.read_bytes()!=data:
            target.write_bytes(data)
    print('SONIC_HARDWARE_FPU_PREPARED original_fallback=UNCHANGED default=OFF')


if __name__=='__main__':main()
