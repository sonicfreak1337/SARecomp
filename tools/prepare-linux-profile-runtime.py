"""Build the LLVM 21 profile writer for private Linux training binaries only."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tarfile

ARCHIVE_SHA='b065a2686674d931f2696517965260b1c92ffd18a492efb847fafb54457d690f'
def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--archive',type=Path,required=True)
    p.add_argument('--zig',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();zig=a.zig.resolve()
    if sha(a.archive)!=ARCHIVE_SHA:raise ValueError('LLVM compiler-rt source identity mismatch')
    version=subprocess.check_output([str(zig),'cc','--version'],text=True).splitlines()[0]
    if version!='clang version 21.1.0':raise ValueError('Unqualified profile compiler: '+version)
    out.mkdir(parents=True,exist_ok=True)
    source=out/'compiler-rt-21.1.0.src'
    with tarfile.open(a.archive) as archive:
        archive.extractall(out,filter='data')
    cmake=(source/'lib/profile/CMakeLists.txt').read_text()
    names=re.search(r'set\(PROFILE_SOURCES\s+(.*?)\s+\)',cmake,re.S)[1].split()
    if len(names)!=19 or len(set(names))!=19:raise ValueError('Unexpected profile source list')
    objects=out/'objects';objects.mkdir(exist_ok=True)
    flags=['-target','x86_64-linux-gnu.2.31','-O2','-g0','-fPIC','-fvisibility=hidden',
           '-ffunction-sections','-fdata-sections','-Wno-pedantic',
           '-DCOMPILER_RT_HAS_ATOMICS=1','-DCOMPILER_RT_HAS_FCNTL_LCK=1',
           '-DCOMPILER_RT_HAS_FLOCK=1','-DCOMPILER_RT_HAS_UNAME=1',
           '-I'+str(source/'lib'),'-I'+str(source/'include')]
    paths=[]
    for name in names:
        if not re.fullmatch(r'\w+\.(c|cpp)',name):raise ValueError('Invalid source member')
        path=source/'lib/profile'/name;obj=objects/(name+'.o')
        language='c++' if name.endswith('.cpp') else 'cc'
        extra=['-nostdinc++','-fno-exceptions','-fno-rtti'] if language=='c++' else []
        subprocess.run([str(zig),language,*flags,*extra,'-c',str(path),'-o',str(obj)],check=True)
        paths.append(obj)
    temporary=out/'libclang_rt.profile-x86_64.next.a'
    if temporary.exists():temporary.unlink()
    subprocess.run([str(zig),'ar','rcs',str(temporary),*(str(p) for p in paths)],check=True)
    library=out/'libclang_rt.profile-x86_64.a';temporary.replace(library)
    report={'schema':'sarecomp-linux-profile-runtime-v1','archive_sha256':ARCHIVE_SHA,
            'compiler':version,'compiler_sha256':sha(zig),'target':'x86_64-linux-gnu.2.31',
            'flags':flags,'sources':names,'library':str(library),'library_sha256':sha(library),
            'library_bytes':library.stat().st_size,'distribution':False}
    (out/'provenance.json').write_text(json.dumps(report,indent=2)+'\n')
    print('SONIC_LINUX_PROFILE_RUNTIME_READY bytes='+str(library.stat().st_size)+' sha256='+report['library_sha256'])

if __name__=='__main__':main()
