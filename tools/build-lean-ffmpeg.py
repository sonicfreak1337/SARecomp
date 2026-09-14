"""Build only SA1's media readers, out of tree. No game rebuild or packaging.

Windows host: Git Bash + project-local Make/NASM and the pinned Zig compiler.
Keep x86-64 runtime SIMD dispatch and the Linux glibc 2.31 compatibility floor.
The full dependency and already delivered installers remain untouched.
"""
import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / '.local/lean-ffmpeg'
REVISION = '5a03dfa0f607ee6156a59bdad3987cd3b858ee5d'
ARCHIVES = (
    (f'https://codeload.github.com/FFmpeg/FFmpeg/tar.gz/{REVISION}',
     'ffmpeg.tar.gz', '5de5245447bafc6734be2e6151becfc389550863d6bcef31b1146a21a69ac49a', 'source'),
    ('https://repo.msys2.org/msys/x86_64/make-4.4.1-2-x86_64.pkg.tar.zst',
     'make.tar.zst', '2408af61717dae87b00c855b132769a125c708907fc94a46bb16dae076113e5c', 'make'),
    ('https://www.nasm.us/pub/nasm/releasebuilds/3.01/win64/nasm-3.01-win64.zip',
     'nasm.zip', 'e0ba5157007abc7b1a65118a96657a961ddf55f7e3f632ee035366dfce039ca4', 'nasm'),
)


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def prepare():
    BASE.mkdir(parents=True, exist_ok=True)
    for url, name, expected, folder in ARCHIVES:
        archive = BASE / name
        if not archive.exists():
            temporary = archive.with_suffix('.download')
            urllib.request.urlretrieve(url, temporary)
            if sha(temporary) != expected:
                raise RuntimeError('Dependency digest mismatch: ' + name)
            temporary.replace(archive)
        if sha(archive) != expected:
            raise RuntimeError('Dependency digest mismatch: ' + name)
        destination = BASE / folder
        if not destination.exists():
            if name.endswith('.zip'):
                with zipfile.ZipFile(archive) as a:
                    a.extractall(destination)
            else:
                with tarfile.open(archive) as a:
                    a.extractall(destination, filter='data')


def stage_runtime(platform):
    install=BASE/('install-'+platform)
    info=json.loads((install/'sonic-build.json').read_text())
    if info['revision']!=REVISION or info['platform']!=platform or \
            '--enable-demuxer=adx,mpegps,mpegvideo' not in info['configure']:
        raise RuntimeError('Build the complete pinned media dependency first')
    names=(('avcodec-62.dll','avformat-62.dll','avutil-60.dll','swresample-6.dll','swscale-9.dll')
           if platform=='windows' else
           ('libavcodec.so.62','libavformat.so.62','libavutil.so.60','libswresample.so.6','libswscale.so.9'))
    objcopy=Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/llvm-objcopy.exe')
    runtime=BASE/('runtime-'+platform);runtime.mkdir(exist_ok=True)
    info['files']={}
    for name in names:
        source=install/('bin' if platform=='windows' else 'lib')/name
        temporary=runtime/(name+'.stripped')
        subprocess.run([str(objcopy),'--strip-all',str(source),str(temporary)],check=True)
        temporary.replace(runtime/name)
        info['files'][name]={'bytes':(runtime/name).stat().st_size,'sha256':sha(runtime/name)}
    for name in ('LICENSE.txt','sonic-configure.patch'):
        shutil.copy2(install/name,runtime/name)
    (runtime/'sonic-build.json').write_text(json.dumps(info,indent=2)+'\n')
    print('SONIC_LEAN_MEDIA_RUNTIME '+json.dumps({'platform':platform,
        'bytes':sum(x['bytes'] for x in info['files'].values()),'directory':str(runtime)}),flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--platform', choices=('linux', 'windows'), default='linux')
    p.add_argument('--jobs', type=int, default=3)
    p.add_argument('--stage-only', action='store_true', help='Strip previously built libraries, without recompiling')
    a = p.parse_args()
    if not 1 <= a.jobs <= 16:
        p.error('jobs must be 1..16')
    if a.stage_only:
        stage_runtime(a.platform)
        return
    prepare()
    bash = Path('C:/Program Files/Git/bin/bash.exe')
    zig = ROOT / '.local/linux-deps/zig/zig-x86_64-windows-0.16.0/zig.exe'
    if not bash.is_file() or not zig.is_file():
        raise RuntimeError('Git Bash and the pinned project-local Zig 0.16 are required')
    build = BASE / ('build-' + a.platform)
    build.mkdir(exist_ok=True)
    # FFmpeg explicitly supports a relative src/ tree, avoiding GNU Make's
    # whitespace limitation in the user's "Sonic Adventure Recompiled" path.
    if not (build / 'src').exists():
        shutil.copytree(BASE / f'source/FFmpeg-{REVISION}', build / 'src')
    # A glibc 2.31 symbol table still contains obsolete sysctl, while modern
    # glibc headers omit sys/sysctl.h. Probe both, as for sysctlbyname upstream.
    original = BASE / f'source/FFmpeg-{REVISION}/configure'
    text = original.read_text()
    if text.count('check_func  sysctl\n') != 1:
        raise RuntimeError('Pinned sysctl configure probe changed')
    configured = text.replace('check_func  sysctl\n', 'check_func_headers sys/sysctl.h sysctl\n')
    if a.platform == 'windows':
        old = 'disabled debug && add_ldexeflags -Wl,--pic-executable,-e,mainCRTStartup'
        if configured.count(old) != 1:
            raise RuntimeError('Pinned GNU PE workaround changed')
        # LLVM's COFF linker retains relocations with dynamicbase already;
        # this GNU ld executable workaround is unsupported by Zig's driver.
        configured = configured.replace(old, ': # LLVM COFF retains ASLR relocations')
    if (build/'src/configure').read_text() != configured:
        (build/'src/configure').write_text(configured, newline='\n')
    install = BASE / ('install-' + a.platform)
    target = 'x86_64-linux-gnu.2.31' if a.platform == 'linux' else 'x86_64-windows-gnu'
    for tool, command in (('cc', f'cc -target {target} -mcpu=baseline'),
                          ('hostcc', 'cc -target x86_64-windows-gnu -mcpu=baseline'),
                          ('ar', 'ar'), ('ranlib', 'ranlib'), ('dlltool', 'dlltool')):
        (build / ('zig-' + tool)).write_text(
            '#!/bin/sh\nexec "$SONIC_MEDIA_ZIG" ' + command + ' "$@"\n', newline='\n')
    env = dict(os.environ)
    env['SONIC_MEDIA_ZIG'] = zig.as_posix()
    env['SONIC_MEDIA_NM'] = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/llvm-nm.exe'
    (build/'llvm-nm').write_text('#!/bin/sh\nexec "$SONIC_MEDIA_NM" "$@"\n', newline='\n')
    # Git's MSYS runtime is already used by the shell. No machine installation.
    env['PATH'] = os.pathsep.join((str(BASE/'make/usr/bin'), str(BASE/'nasm/nasm-3.01'),
                                  'C:/Program Files/Git/usr/bin', env['PATH']))
    configure = ['src/configure',
        '--prefix=../install-' + a.platform, '--target-os=' + ('linux' if a.platform == 'linux' else 'mingw32'),
        '--arch=x86_64', '--cpu=generic', '--enable-cross-compile',
        '--cc=./zig-cc', '--host-cc=./zig-hostcc', '--ar=./zig-ar', '--ranlib=./zig-ranlib', '--nm=./llvm-nm',
        '--disable-everything', '--disable-autodetect', '--disable-programs', '--disable-doc',
        '--disable-network', '--disable-avdevice', '--disable-avfilter',
        '--disable-debug', '--disable-stripping', '--disable-static', '--enable-shared',
        '--enable-pic', '--enable-runtime-cpudetect', '--disable-gpl', '--disable-nonfree',
        '--disable-version3', '--enable-decoder=adpcm_adx,mpeg1video,mpeg2video',
        # MPEG-PS probes its elementary video stream through the MPEG-video
        # demuxer before selecting MPEG-1 versus MPEG-2. Both belong together.
        '--enable-demuxer=adx,mpegps,mpegvideo', '--enable-parser=adx,mpegvideo', '--enable-protocol=file']
    signature = json.dumps({'configure':configure, 'source_configure_sha256':sha(build/'src/configure')})
    stamp = build / 'sonic-configure.json'
    if not stamp.exists() or stamp.read_text() != signature:
        subprocess.run([str(bash), configure[0], *configure[1:]], cwd=build, env=env, check=True)
        stamp.write_text(signature)
    make_variables=' DLLTOOL=./zig-dlltool' if a.platform=='windows' else ''
    subprocess.run([str(bash), '-c', 'make -j ' + str(a.jobs) + make_variables], cwd=build, env=env, check=True)
    if a.platform=='windows':
        # Recover an interrupted install whose DLLs are current but whose
        # optional MSVC import libraries have not yet been produced.
        for component,major in (('avcodec',62),('avformat',62),('avutil',60),('swresample',6),('swscale',9)):
            directory=build/('lib'+component);library=directory/(component+'.lib')
            if not library.exists():
                subprocess.run([str(zig),'dlltool','-m','i386:x86-64','-d',str(directory/f'{component}-{major}.def'),
                    '-l',str(library),'-D',f'{component}-{major}.dll'],check=True)
    subprocess.run([str(bash), '-c', 'make install' + make_variables], cwd=build, env=env, check=True)
    shutil.copy2(BASE / f'source/FFmpeg-{REVISION}/COPYING.LGPLv2.1', install/'LICENSE.txt')
    (install/'sonic-configure.patch').write_text(''.join(difflib.unified_diff(
        text.splitlines(keepends=True), configured.splitlines(keepends=True),
        fromfile='a/configure', tofile='b/configure')), newline='\n')
    (install/'sonic-build.json').write_text(json.dumps({
        'revision':REVISION, 'platform':a.platform, 'target':target,
        'configure':configure, 'make_variables':make_variables, 'archives':ARCHIVES,
    }, indent=2)+'\n')
    print('SONIC_LEAN_MEDIA_BUILT ' + str(install), flush=True)
    stage_runtime(a.platform)


if __name__ == '__main__':
    main()
