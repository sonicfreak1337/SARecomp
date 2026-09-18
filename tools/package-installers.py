"""Authoring-only package builder. End users run a ready native installer."""
from pathlib import Path
import argparse
import lzma
import hashlib
import json
import os
import re
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]

# Never repackage withdrawn programs through an old --reuse-stage directory.
# These shipped binaries tie native gameplay math to Recompiled timing.
WITHDRAWN_GAME_SHA256 = frozenset({
    '0e6fdeca6de6eee7cbfff73b7792515776860a188dab7e5cdab0b43f89a4ab8f',
    '5230777fad5de2a68ef17985b5c53276873a8d51b1b2d64d9eedb67040224f01',
    '8cac5cca4c6ca6e2f341228b98ed3410aa213440d996c0da5f9d7b39f444392d',
    '3ad1aa3a85904e36eee7907a610b14d39142dda8d26a2b692f12130f53361576',
})


def linux_compression_filters():
    # Verified on the complete stripped game, not just a sample: ~6.5 MB less
    # than the previous 4 MiB dictionary, with the same decoded bytes/time.
    # The consumer needs ~32 MiB dictionary storage; authoring remains preset 4.
    return [{'id': lzma.FILTER_X86},
            {'id': lzma.FILTER_LZMA2, 'preset': 4, 'dict_size': 32 * 1024 * 1024}]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def owned(path):
    path = path.resolve()
    if not path.is_relative_to(ROOT / 'out') or path == ROOT / 'out':
        raise RuntimeError('Use a dedicated output below this project/out')
    return path


def media_runtime(windows):
    platform='windows' if windows else 'linux'
    root=ROOT/'.local/lean-ffmpeg'/('runtime-'+platform)
    info=json.loads((root/'sonic-build.json').read_text(encoding='utf-8'))
    names=(('avcodec-62.dll','avformat-62.dll','avutil-60.dll','swresample-6.dll','swscale-9.dll')
           if windows else
           ('libavcodec.so.62','libavformat.so.62','libavutil.so.60','libswresample.so.6','libswscale.so.9'))
    if info['platform']!=platform or info['revision']!='5a03dfa0f607ee6156a59bdad3987cd3b858ee5d' or set(info['files'])!=set(names):
        raise RuntimeError('Build the pinned lean FFmpeg runtime with tools/build-lean-ffmpeg.py')
    for name in names:
        file=root/name;record=info['files'][name]
        if file.stat().st_size!=record['bytes'] or digest(file)!=record['sha256']:
            raise RuntimeError('Lean media runtime changed: '+name)
    return root,names


def stage(edition, destination, game=None, version=None):
    if version is None and (ROOT / 'VERSION').is_file():
        version = (ROOT / 'VERSION').read_text(encoding='utf-8').strip()
    if version is not None and not re.fullmatch(r'\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?', version):
        raise RuntimeError('A valid release version is required')
    destination = owned(destination)
    if destination.exists():
        raise RuntimeError('Use a fresh staging directory; existing files are preserved')
    destination.mkdir(parents=True)
    def copy(source, relative):
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    windows = edition == 'windows'
    media,media_names=media_runtime(windows)
    for name in media_names:
        copy(media/name, name if windows else 'lib/'+name)
    for name in ('LICENSE.txt','sonic-build.json','sonic-configure.patch'):
        copy(media/name, 'resources/FFmpeg-'+name)
    copy(ROOT/'tools/build-lean-ffmpeg.py','resources/FFmpeg-build-recipe.py')
    if windows:
        build = ROOT / 'out/windows-installer-build'
        for name in ('game.exe', 'sonic-config.exe', 'SDL3.dll'):
            copy(game if name == 'game.exe' and game is not None else build / name, name)
        copy(ROOT / 'build-windows-setup/sonic-setup.exe', 'sonic-setup.exe')
        redist = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC'
        versions = sorted(redist.glob('*/x64/Microsoft.VC143.CRT'), key=lambda p: p.parts[-3])
        if not versions:
            raise RuntimeError('Visual C++ redistributable files were not found')
        # Imports were checked on the game, setup, SDL and FFmpeg DLLs.
        for name in ('msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll'):
            copy(versions[-1] / name, name)
    else:
        for name, source in {
            'game': game if game is not None else ROOT / 'build-linux/game',
            'sonic-startup-ui': ROOT / 'build-linux/sonic-startup-ui',
            'sonic-setup': ROOT / 'build-linux-setup/sonic-setup',
            'lib/libSDL3.so.0': ROOT / 'build-linux/sdl/libSDL3.so.0.4.16',
        }.items():
            copy(source, name)
        with (destination / 'game').open('rb') as stream:
            if stream.read(4) != b'\x7fELF':
                raise RuntimeError('Linux requires a native ELF game')
    for name in ('install-files.tsv', 'bootstrap.xor.z', 'install-identity.json'):
        copy(ROOT / 'out/setup-resources' / name, 'resources/' + name)
    fonts = ['NotoSans-Regular.ttf', 'NotoSans-Bold.ttf']
    if not windows:
        fonts.append('NotoSansCJKjp-Regular.otf')
    for name in fonts:
        copy(ROOT / '.local/ui-deps' / name, 'resources/' + name)
    copy(ROOT / 'assets/setup/hero.png', 'resources/hero.png')
    copy(ROOT / 'assets/ui/options-background.png', 'assets/ui/options-background.png')
    for extension in (('png', 'ico') if windows else ('png',)):
        name = 'sonic-adventure-recompiled.' + extension
        copy(ROOT / 'assets/icons' / name, 'resources/' + name)
    (destination / 'resources/edition.txt').write_text(edition + '\n', encoding='utf-8', newline='\n')
    notice = ('Sonic Adventure Recompiled\n\nThis is an independent, non-profit fan project. '
              'Not affiliated with, sponsored by, or endorsed by SEGA.\nSonic Adventure and original '
              'game content belong to SEGA Corporation and their respective rights holders.\n'
              'You must provide your own copy of Sonic Adventure.\n\nPort by SoNiCFReaK\nPowered by KatanaRecomp\n')
    licenses = {
        'SDL 3.4.16': ROOT / '.local/linux-deps/sdl/SDL3-3.4.16/LICENSE.txt',
        'stb': ROOT / '.local/ui-deps/LICENSE',
        'Noto Sans': ROOT / '.local/ui-deps/Noto-OFL.txt',
        'volk': ROOT / 'third_party/volk/LICENSE.md',
        'Vulkan-Headers': ROOT / 'third_party/vulkan-headers/LICENSE.md',
        'Vulkan-Headers MIT': ROOT / 'third_party/vulkan-headers/LICENSES/MIT.txt',
        'Vulkan-Headers Apache 2.0': ROOT / 'third_party/vulkan-headers/LICENSES/Apache-2.0.txt',
    }
    if not windows:
        licenses.update({
            'Noto Sans CJK': ROOT / '.local/ui-deps/Noto-CJK-OFL.txt',
            'LLVM libc++': ROOT / '.local/linux-deps/zig/zig-x86_64-windows-0.16.0/lib/libcxx/LICENSE.TXT',
            'LLVM libc++abi': ROOT / '.local/linux-deps/zig/zig-x86_64-windows-0.16.0/lib/libcxxabi/LICENSE.TXT',
        })
    for title, source in licenses.items():
        notice += '\n' + title + '\n' + '=' * len(title) + '\n' + source.read_text(encoding='utf-8') + '\n'
    ffmpeg_revision = '5a03dfa0f607ee6156a59bdad3987cd3b858ee5d'
    notice += ('\nFFmpeg: dynamically linked LGPL 2.1-or-later build containing SA1 media readers.\n'
               f'Corresponding upstream source: https://github.com/FFmpeg/FFmpeg/tree/{ffmpeg_revision}\n'
               'Exact source archive URL and SHA-256, configuration and toolchain are in FFmpeg-sonic-build.json.\n'
               'The build recipe and the small configure compatibility patch are included beside this notice.\n'
               'Libraries can be replaced with ABI-compatible LGPL builds. See the included FFmpeg license.\n')
    (destination / 'resources/THIRD-PARTY-NOTICES.txt').write_text(notice, encoding='utf-8', newline='\n')
    (destination / 'README.txt').write_text(
        'Sonic Adventure Recompiled\n\nRun sonic-setup to install using your Sonic Adventure PAL GDI and all tracks.\n'
        'Setup performs verification and extraction only; no Katana export or compilation is needed.\n'
        'A desktop and applications-menu shortcut named Sonic Adventure Recompiled is created.\n'
        'Game content, application versions and personal saves use separate folders. Reinstallation preserves saves.\n'
        'Steam Deck: install in Desktop Mode, then add the installed game shortcut as a non-Steam game. '
        'The game uses native 1280x800 fullscreen in handheld mode.\n'
        'Linux: x86_64, glibc 2.31 or newer, X11/XWayland and a Vulkan graphics driver are required.\n', encoding='utf-8', newline='\n')
    if not windows:
        # Strip only the staged copies. Runtime sections and unwind tables remain;
        # development symbols stay in build-linux for voluntary crash diagnosis.
        objcopy = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Tools/Llvm/x64/bin/llvm-objcopy.exe'
        for relative in ('game', 'sonic-setup', 'sonic-startup-ui'):
            file = destination / relative
            temporary = file.with_suffix('.stripped')
            before = file.stat().st_size
            subprocess.run([str(objcopy), '--strip-all', str(file), str(temporary)], check=True)
            temporary.replace(file)
            print(f'SONIC_PACKAGE_STRIPPED file={relative} removed_bytes={before-file.stat().st_size}', flush=True)
    if version is not None:
        executable = destination / ('game.exe' if windows else 'game')
        release = {'version': version, 'edition': edition,
                   'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                   'runtime_sha256': digest(executable)}
        (destination / 'resources/release.json').write_text(json.dumps(release, indent=2) + '\n', encoding='utf-8')
    files = sorted(p for p in destination.rglob('*') if p.is_file())
    records = []
    for file in files:
        if file.suffix.lower() in ('.gdi', '.raw', '.ksave') or file.name.startswith('track'):
            raise RuntimeError('Original media or user data cannot enter a package')
        records.append((file.relative_to(destination).as_posix(), file.stat().st_size, digest(file)))
    (destination / 'resources/payload-files.tsv').write_text(
        'SARECOMP-PAYLOAD-1\n' + ''.join(f'{name}\t{size}\t{sha}\n' for name, size, sha in records), encoding='utf-8', newline='\n')
    print(f'SONIC_SETUP_STAGED edition={edition} files={len(records)} bytes={sum(r[1] for r in records)}', flush=True)
    return destination


LINUX_HEADER = r'''#!/bin/sh
set -eu
payload_line=$(awk '/^__SARECOMP_PAYLOAD__$/ {print NR+1; exit}' "$0")
test -n "$payload_line" || exit 1
temporary=$(mktemp -d "${TMPDIR:-/tmp}/sarecomp-setup.XXXXXXXX")
cleanup() { case "$temporary" in "${TMPDIR:-/tmp}"/sarecomp-setup.*) rm -rf -- "$temporary";; esac; }
trap cleanup EXIT HUP INT TERM
echo "Preparing Sonic Adventure Recompiled setup..."
tail -n +"$payload_line" "$0" | tar -xJ -C "$temporary"
cd "$temporary"
./sonic-setup "$@"
exit $?
__SARECOMP_PAYLOAD__
'''


def package(edition, staging, output):
    output = owned(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    manifest = staging / 'resources/payload-files.tsv'
    lines = manifest.read_text(encoding='utf-8').splitlines()
    if not lines or lines[0] != 'SARECOMP-PAYLOAD-1':
        raise RuntimeError('Payload manifest is missing')
    names = {'resources/payload-files.tsv'}
    for row in lines[1:]:
        name, size, sha = row.split('\t')
        if name in ('game', 'game.exe') and sha in WITHDRAWN_GAME_SHA256:
            raise RuntimeError('Withdrawn game: Original timing disables native math. Rebuild the corrected runtime before packaging.')
        path = staging / name
        if not path.resolve().is_relative_to(staging) or path.is_symlink() or path.stat().st_size != int(size) or digest(path) != sha:
            raise RuntimeError('Staged payload changed: ' + name)
        names.add(name)
    if names != {p.relative_to(staging).as_posix() for p in staging.rglob('*') if p.is_file()}:
        raise RuntimeError('Unlisted files in package staging')
    print(f'SONIC_PACKAGE_AUDIT_OK files={len(names)} originals=0 saves=0 debug_files=0', flush=True)
    if edition != 'windows':
        with output.open('wb') as stream:
            stream.write(LINUX_HEADER.encode())
            filters = linux_compression_filters()
            with lzma.LZMAFile(stream, mode='wb', format=lzma.FORMAT_XZ, filters=filters) as compressed:
                with tarfile.open(fileobj=compressed, mode='w|', format=tarfile.PAX_FORMAT) as archive:
                    for file in sorted(p for p in staging.rglob('*') if p.is_file()):
                        name = file.relative_to(staging).as_posix()
                        item = archive.gettarinfo(file, name)
                        item.uid = item.gid = item.mtime = 0
                        item.uname = item.gname = ''
                        item.mode = 0o755 if name in ('game', 'sonic-setup', 'sonic-startup-ui') else 0o644
                        with file.open('rb') as data:
                            archive.addfile(item, data)
        output.chmod(0o755)
    else:
        nsis = ROOT / '.local/installer-tools/nsis-3.12/makensis.exe'
        if not nsis.is_file():
            nsis = ROOT / '.local/installer-tools/nsis-3.12/NSIS/makensis.exe'
        if not nsis.is_file():
            raise RuntimeError('Prepare the pinned NSIS 3.12 portable tool first')
        script = staging.parent / (staging.name + '.nsi')
        def quoted(path):
            value = str(path)
            if '"' in value or '$' in value or '\n' in value:
                raise RuntimeError('Unsupported package authoring path')
            return '"' + value + '"'
        script.write_text('''Unicode true
RequestExecutionLevel user
SilentInstall silent
CRCCheck force
SetCompressor /SOLID lzma
SetCompressorDictSize 16
Name "Sonic Adventure Recompiled"
!include "FileFunc.nsh"
Var Arguments
Section
InitPluginsDir
SetOutPath "$PLUGINSDIR\\payload"
''' + 'File /r ' + quoted(staging / '*') + '\n'
            '${GetParameters} $Arguments\nExecWait \'"$PLUGINSDIR\\payload\\sonic-setup.exe" $Arguments\' $0\nSetErrorLevel $0\nSectionEnd\n'
            + 'Icon ' + quoted(ROOT / 'assets/icons/sonic-adventure-recompiled.ico') + '\n'
            + 'OutFile ' + quoted(output) + '\n', encoding='utf-8-sig')
        subprocess.run([str(nsis), '/V2', str(script)], check=True)
    compression = ({'format': 'xz', 'codec': 'lzma2', 'x86_filter': True, 'preset': 4, 'dictionary_mib': 32}
                   if edition != 'windows' else
                   {'format': 'nsis', 'codec': 'lzma', 'solid': True, 'dictionary_mib': 16})
    report = {'edition': edition, 'file': output.name, 'bytes': output.stat().st_size,
              'sha256': digest(output), 'compression': compression}
    output.with_suffix(output.suffix + '.json').write_text(json.dumps(report, indent=2) + '\n')
    print('SONIC_INSTALLER_READY ' + json.dumps(report), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--edition', choices=('linux', 'steam-deck', 'windows'), required=True)
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reuse-stage', action='store_true')
    parser.add_argument('--game', type=Path, help='Verified current runtime instead of the default build location')
    parser.add_argument('--version', help='Release version; defaults to the repository VERSION file')
    args = parser.parse_args()
    if args.reuse_stage and (args.game is not None or args.version is not None):
        parser.error('--game/--version require fresh staging')
    package(args.edition, owned(args.stage) if args.reuse_stage else stage(args.edition, args.stage, args.game, args.version), args.output)
