"""Stage the native installer without original disc files or user data."""
from pathlib import Path
import argparse
import shutil
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]

def stage(output, game=None):
    output = output.resolve()
    if not output.is_relative_to(ROOT / 'out') or output == ROOT / 'out':
        raise RuntimeError('Setup staging must use its own directory under out')
    resources = output / 'resources'
    libraries = output / 'lib'
    resources.mkdir(parents=True, exist_ok=True); libraries.mkdir(exist_ok=True)
    def copy(source, destination):
        shutil.copyfile(source, destination)
    copy(ROOT / 'build-linux-setup/sonic-setup', output / 'sonic-setup')
    copy(ROOT / 'build-linux/sdl/libSDL3.so.0.4.16', libraries / 'libSDL3.so.0')
    for name in ['install-files.tsv', 'bootstrap.xor.z', 'install-identity.json']:
        copy(ROOT / 'out/setup-resources' / name, resources / name)
    for name in ['NotoSans-Regular.ttf', 'NotoSans-Bold.ttf']:
        copy(ROOT / '.local/ui-deps' / name, resources / name)
    copy(ROOT / 'assets/setup/hero.png', resources / 'hero.png')
    notice = 'Sonic Adventure: Recompiled\n\n'
    notice += 'This is an independent, non-profit fan project. Not affiliated with, sponsored by, or endorsed by SEGA.\n'
    notice += 'Sonic Adventure and original game content belong to SEGA Corporation and their respective rights holders.\n'
    notice += 'You must provide your own copy of Sonic Adventure.\n\nPort by SoNiCFReaK\nPowered by KatanaRecomp\n\n'
    licenses = {
        'SDL 3.4.16 (built from upstream source)': ROOT / '.local/linux-deps/sdl/SDL3-3.4.16/LICENSE.txt',
        'stb (image decoding, PNG capture and font rasterization)': ROOT / '.local/ui-deps/LICENSE',
        'Noto Sans (SIL Open Font License)': ROOT / '.local/ui-deps/Noto-OFL.txt',
        'LLVM libc++': ROOT / '.local/linux-deps/zig/zig-x86_64-windows-0.16.0/lib/libcxx/LICENSE.TXT',
        'LLVM libc++abi': ROOT / '.local/linux-deps/zig/zig-x86_64-windows-0.16.0/lib/libcxxabi/LICENSE.TXT',
    }
    for title, source in licenses.items():
        notice += '\n' + title + '\n' + '=' * len(title) + '\n' + source.read_text(encoding='utf-8') + '\n'
    (resources / 'THIRD-PARTY-NOTICES.txt').write_text(notice, encoding='utf-8')
    if game:
        with game.open('rb') as stream:
            if stream.read(4) != b'\x7fELF':
                raise RuntimeError('The game must be a native Linux ELF executable')
        copy(game, output / 'game'); (output / 'game').chmod(0o755)
    (output / 'sonic-setup').chmod(0o755)
    files = [p for p in output.rglob('*') if p.is_file()]
    manifest = {p.relative_to(output).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.name != 'package-files.json'}
    (output / 'package-files.json').write_text(json.dumps({'development_setup_only': not bool(game), 'files': manifest}, indent=2) + '\n')
    print('SONIC_LINUX_SETUP_STAGED files=' + str(len(manifest)))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--game', type=Path)
    parser.add_argument('--setup-only', action='store_true')
    args = parser.parse_args()
    if not args.game and not args.setup_only:
        parser.error('Provide the Linux game executable, or explicitly select a development setup-only package')
    stage(args.output, args.game)
