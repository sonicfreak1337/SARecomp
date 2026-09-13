"""Extend the authenticated module universe using the SDK's canonical v1 encoding."""
import hashlib
import re
import struct
from pathlib import Path

OLD_UNIVERSE = 'sha256:95f3e1ba1f78748550bfdd86ee126db5c78d323ae88457e87b92c50545bb7dbf'
OLD_PACK = 'sha256:a9ea91e8d6f02a6e1c22514d893465727b2379696eb3ddbb95e3bd7eb652b05d'
OLD_COVERAGE = 'sha256:222735ea74494bce6ff348f892c6e7cedb05b83bc90070ed39fe60fca981b4e1'

def canonical(modules):
    h = hashlib.sha256()
    def number(value): h.update(struct.pack('<Q',value))
    def string(value):
        encoded=value.encode('utf-8');number(len(encoded));h.update(encoded)
    string('katana-native-port-loaded-aot-module-universe-v1')
    number(len(modules))
    for start,size,identity,bindings,blocks in sorted(modules,key=lambda m:m[:3]):
        number(start);number(size);string(identity)
        number(len(bindings))
        for transform,source,offset,encoded_size,runtime in sorted(bindings):
            number(transform);string(source);number(offset);number(encoded_size);number(runtime)
        number(len(blocks))
        for offset,size,identity in sorted(blocks):
            number(offset);number(size);string(identity)
    return 'sha256:'+h.hexdigest()

def authenticated_modules(generated):
    generated=Path(generated)
    manifest=(generated/'.katana-generated-artifacts').read_text().splitlines()
    if manifest[0]!='katana-codegen-artifacts-v2' or manifest[1]!='generation\tsha256:'+hashlib.sha256(
            ('\n'.join(manifest[2:])+'\n').encode()).hexdigest():
        raise RuntimeError('Unrecognized module generation')
    records={row.split('\t')[0]:row.split('\t')[1:] for row in manifest[2:]}
    modules=[]
    for path in sorted((generated/'code').glob('native-port-loaded-aot-shard-*.cpp')):
        data=path.read_bytes();record=records[path.relative_to(generated).as_posix()]
        if int(record[0])!=len(data) or record[1]!='sha256:'+hashlib.sha256(data).hexdigest():
            raise RuntimeError('Unauthenticated module shard')
        source=data.decode()
        arrays={int(i):(int(count),body) for count,i,body in re.findall(
            r'NativePortLoadedAotBlockIdentityView, (\d+)u> native_loaded_aot_blocks_(\d+)\{\{(.*?)\}\};',
            source,re.S)}
        bindings={int(i):(int(count),body) for count,i,body in re.findall(
            r'NativePortLoadedAotSourceBindingView, (\d+)u> native_loaded_aot_sources_(\d+)\{\{(.*?)\}\};',
            source,re.S)}
        for start,size,identity,si,bi in re.findall(
            r'modules.push_back\(\{0x([0-9A-F]+)u, (\d+)u, "(sha256:[0-9a-f]+)", .*?\(native_loaded_aot_sources_(\d+)\), .*?\(native_loaded_aot_blocks_(\d+)\)\}\);',source):
            count,body=arrays[int(bi)]
            blocks=[(int(a),int(b),c) for a,b,c in re.findall(r'\{(\d+)u, (\d+)u, "(sha256:[0-9a-f]+)"\}',body)]
            if len(blocks)!=count:raise RuntimeError('Module block parse mismatch')
            count,body=bindings[int(si)]
            sources=[({'Identity':0,'SegaPrs':1}[a],b,int(c),int(d),int(e,16)) for a,b,c,d,e in re.findall(
                r'NativePortLoadedAotSourceTransform::(\w+), "(sha256:[0-9a-f]+)", (\d+)ull, (\d+)u, 0x([0-9A-F]+)u',body)]
            if len(sources)!=count:raise RuntimeError('Module binding parse mismatch')
            modules.append((int(start,16),int(size),identity,sources,blocks))
    if canonical(modules)!=OLD_UNIVERSE:
        raise RuntimeError('Retained module universe mismatch')
    return modules

def extend(generated, additions, modules=None):
    if modules is None:
        modules=authenticated_modules(generated)
    target=[m for m in modules if m[0]==0x82980000]
    if len(target)!=1:raise RuntimeError('MINICART module ambiguity')
    blocks=target[0][4]
    if len(blocks)!=9286 or set(a for a,_,_ in blocks)&set(a for a,_,_ in additions):
        raise RuntimeError('MINICART supplement block overlap')
    blocks.extend(additions)
    new_universe=canonical(modules)
    def derived(kind,old):
        return 'sha256:'+hashlib.sha256((kind+'\n'+old+'\n'+new_universe+'\n').encode()).hexdigest()
    return {OLD_UNIVERSE:new_universe,
            OLD_PACK:derived('sonic-minicart-aot-supplement-v1',OLD_PACK),
            OLD_COVERAGE:derived('sonic-minicart-empty-coverage-v1',OLD_COVERAGE)}
