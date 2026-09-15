"""Build a Linux/Deck binary patch for installed v5 applications, without GDI."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--stage', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    stage, output = args.stage.resolve(), args.output.resolve()
    for path in (stage, output):
        if not path.is_relative_to(ROOT/'out') or path == ROOT/'out' or path.exists():
            raise RuntimeError('Use new output files below this project/out')
    stage.mkdir(parents=True)
    base = ROOT/'out/package-staging-v5-steam-deck/game'
    target = ROOT/'out/patch-timing-math-v1/linux/game'
    if digest(base) != '5230777fad5de2a68ef17985b5c53276873a8d51b1b2d64d9eedb67040224f01':
        raise RuntimeError('The installed v5 base identity changed')
    target_hash = digest(target)
    files = {
        'apply-patch.sh': ROOT/'tools/apply-linux-runtime-patch.sh',
        'game.delta.zst': ROOT/'out/patch-timing-math-v1/linux/from-v5.zst',
        'zstd': ROOT/'build-patch-zstd/programs/zstd-frugal',
        'ZSTD-LICENSE.txt': ROOT/'.local/patch-deps/zstd-1.5.7/LICENSE',
    }
    for name, source in files.items():
        if name.endswith('.sh'):
            (stage/name).write_text(source.read_text(encoding='utf-8'), encoding='utf-8', newline='\n')
        else:
            shutil.copyfile(source, stage/name)
    supported = [
        '3ad1aa3a85904e36eee7907a610b14d39142dda8d26a2b692f12130f53361576',
        '8cac5cca4c6ca6e2f341228b98ed3410aa213440d996c0da5f9d7b39f444392d',
        digest(base),
    ]
    metadata = ['SARECOMP-RUNTIME-PATCH-1', f'target\t{target.stat().st_size}\t{target_hash}',
                'base\t'+supported[-1], 'delta\t'+digest(stage/'game.delta.zst'),
                'tool\t'+digest(stage/'zstd')]
    metadata += ['supported\t'+value for value in supported]
    (stage/'patch.tsv').write_text('\n'.join(metadata)+'\n', encoding='ascii', newline='\n')
    (stage/'README.txt').write_text(
        'Sonic Adventure Recompiled - v5 Performance Patch\n\n'
        'Close the game. Run the .run file in Desktop Mode as your normal user.\n'
        'No sudo, GDI, game reinstallation or extra packages are required.\n'
        'Your v5 installation supplies the reference program for this binary delta.\n'
        'Existing Steam and desktop launch paths are updated, including retained v2-v4 paths.\n'
        'Saves, Chao data, original content and settings are not accessed.\n'
        'Program and manifest backups are retained beside each updated executable.\n\n'
        'Zstandard 1.5.7 is included under its BSD license.\n'
        'Source: https://github.com/facebook/zstd/tree/v1.5.7\n'
        'Decoder: zstd-frugal, glibc 2.31, static Zstandard library, no optional codecs.\n',
        encoding='utf-8', newline='\n')
    archive = stage.parent/(stage.name+'.tar')
    if archive.exists():
        raise RuntimeError('Patch archive already exists')
    with tarfile.open(archive, 'w') as tar:
        for path in sorted(stage.iterdir()):
            info = tar.gettarinfo(str(path), arcname=path.name)
            info.uid = info.gid = info.mtime = 0
            info.uname = info.gname = ''
            info.mode = 0o755 if path.name in ('zstd', 'apply-patch.sh') else 0o644
            with path.open('rb') as stream:
                tar.addfile(info, stream)
    archive_hash = digest(archive)
    wrapper = '''#!/bin/sh
set -eu
command -v bash >/dev/null || { echo 'Bash is required.' >&2; exit 1; }
patch_temp=$(mktemp -d "${TMPDIR:-/tmp}/sarecomp-patch.XXXXXXXX")
cleanup() {
    case "$patch_temp" in */sarecomp-patch.????????) rm -rf -- "$patch_temp" ;; esac
}
trap cleanup EXIT
trap 'exit 130' HUP INT TERM
tail -n +__LINE__ "$0" > "$patch_temp/payload.tar"
actual=$(sha256sum "$patch_temp/payload.tar" | cut -d ' ' -f 1)
[ "$actual" = "__HASH__" ] || { echo 'The patch download is damaged.' >&2; exit 1; }
tar -xf "$patch_temp/payload.tar" -C "$patch_temp"
bash "$patch_temp/apply-patch.sh" "$patch_temp" "$@"
exit $?
'''
    wrapper = wrapper.replace('__HASH__', archive_hash)
    wrapper = wrapper.replace('__LINE__', str(len(wrapper.splitlines())+1))
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open('xb') as out:
        out.write(wrapper.encode('ascii'))
        with archive.open('rb') as stream:
            shutil.copyfileobj(stream, out)
    result = {'file': str(output), 'bytes': output.stat().st_size, 'sha256': digest(output),
              'base_sha256': supported[-1], 'target_sha256': target_hash,
              'target_bytes': target.stat().st_size, 'archive_sha256': archive_hash}
    output.with_suffix(output.suffix+'.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
