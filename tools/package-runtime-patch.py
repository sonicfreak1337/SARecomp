"""Build a Linux/Deck binary patch for installed v5 applications, without GDI."""
import argparse
import hashlib
import json
import re
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
    parser.add_argument('--diagnostics', choices=('on','off'))
    parser.add_argument('--base', type=Path)
    parser.add_argument('--target', type=Path)
    parser.add_argument('--delta', type=Path)
    parser.add_argument('--patch-id')
    parser.add_argument('--supported-hash', action='append', default=[])
    parser.add_argument('--reference-backup', action='append', default=[])
    args = parser.parse_args()
    stage, output = args.stage.resolve(), args.output.resolve()
    for path in (stage, output):
        if not path.is_relative_to(ROOT/'out') or path == ROOT/'out' or path.exists():
            raise RuntimeError('Use new output files below this project/out')
    stage.mkdir(parents=True)
    base = ROOT/'out/package-staging-v5-steam-deck/game'
    target = ROOT/'out/patch-timing-math-v1/linux/game'
    delta = ROOT/'out/patch-timing-math-v1/linux/from-v5.zst'
    expected_base = '5230777fad5de2a68ef17985b5c53276873a8d51b1b2d64d9eedb67040224f01'
    if args.diagnostics:
        base = target
        target = ROOT/'out/internal-diagnostics-v1/linux/game'
        delta = ROOT/'out/internal-diagnostics-v1/linux/from-native-math-v1.zst'
        expected_base = '7d4fb2b71694dead0ae7f76f105bb975429978a7dce3ce6cad44aace220c6921'
    custom = any((args.base,args.target,args.delta,args.patch_id,args.supported_hash,args.reference_backup))
    if custom:
        if not all((args.base,args.target,args.delta,args.patch_id)) or args.diagnostics:
            raise RuntimeError('Custom performance patches require base, target, delta and patch-id')
        if not re.fullmatch('[a-z0-9][a-z0-9-]{0,47}',args.patch_id):
            raise RuntimeError('Invalid patch id')
        if any(not re.fullmatch('[0-9a-f]{64}',value) for value in args.supported_hash):
            raise RuntimeError('Invalid supported executable hash')
        if any(not re.fullmatch(r'game\.pre-[a-z0-9-]+',value) for value in args.reference_backup):
            raise RuntimeError('Invalid reference backup name')
        base,target,delta=args.base.resolve(),args.target.resolve(),args.delta.resolve()
        if any(not p.is_relative_to(ROOT/'out') for p in (base,target,delta)):
            raise RuntimeError('Patch inputs must be project output files')
        expected_base=digest(base)
    if digest(base) != expected_base:
        raise RuntimeError('The installed v5 base identity changed')
    target_hash = digest(target)
    files = {
        'apply-patch.sh': ROOT/'tools/apply-linux-runtime-patch.sh',
        'game.delta.zst': delta,
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
    if args.diagnostics: supported = [expected_base]
    if custom: supported=list(dict.fromkeys([expected_base]+args.supported_hash))
    metadata = ['SARECOMP-RUNTIME-PATCH-1', f'target\t{target.stat().st_size}\t{target_hash}',
                'base\t'+expected_base, 'delta\t'+digest(stage/'game.delta.zst'),
                'tool\t'+digest(stage/'zstd')]
    metadata += ['supported\t'+value for value in supported]
    if args.diagnostics: metadata.append('diagnostics\t'+args.diagnostics)
    if custom:
        metadata.append('patch-id\t'+args.patch_id)
        metadata += ['reference-backup\t'+name for name in args.reference_backup]
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
    if args.diagnostics:
        (stage/'README.txt').write_text(
            'Sonic Adventure Recompiled - Internal Diagnostics '+args.diagnostics.upper()+'\n\n'
            'Close the game. Run this patch in Desktop Mode, without sudo.\n'
            'Requires the previously installed native-math performance patch.\n'
            'The first use updates the program once; later ON/OFF switches change only an internal policy file.\n'
            'No GDI, reinstallation or game settings changes. Saves and Chao data are preserved.\n'
            'Use your existing Steam/desktop shortcut. The policy applies on the next launch.\n'
            'ON: full runtime diagnostic journals and additional invariant/vertex audits.\n'
            'OFF: no automatic journals, extra vertex audit or periodic diagnostic readback.\n'
            'Memory bounds, module lifetime, executable invalidation and original game timing stay functional.\n'
            'This switch is internal and is not exposed in the Options menu.\n\n'
            'Zstandard 1.5.7, BSD license; https://github.com/facebook/zstd/tree/v1.5.7\n',
            encoding='utf-8',newline='\n')
    if custom:
        (stage/'README.txt').write_text(
            'Sonic Adventure Recompiled - CPU Update 2026-09-16\n\n'
            'Close the game. Run this .run file in Desktop Mode as your normal user.\n'
            'No sudo, GDI, reinstallation or extra packages are required.\n'
            'Updates the previous Native-Math Performance Patch and its Diagnostics update.\n'
            'The Diagnostics update retains the authenticated reference program needed by this delta.\n'
            'Existing Steam and desktop shortcuts keep working. Saves, Chao data and settings are preserved.\n'
            'The current diagnostics ON/OFF preference is preserved. Normal play defaults to OFF.\n'
            'About 1.7 GB of free installation space is needed for verified reconstruction.\n'
            'The previous executable and manifest are retained under the new patch-specific backup name.\n\n'
            'Includes prepared indirect transfers, checked RAM/ALU regions and native memory comparison.\n'
            'Both Original and Recompiled keep native gameplay math. Game timing is unchanged.\n'
            'The new private-register experiment is excluded. Deck performance still needs hardware feedback.\n\n'
            'Zstandard 1.5.7, BSD license; https://github.com/facebook/zstd/tree/v1.5.7\n',
            encoding='utf-8',newline='\n')
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
              'base_sha256': expected_base, 'target_sha256': target_hash,
              'target_bytes': target.stat().st_size, 'archive_sha256': archive_hash}
    if custom: result.update(patch_id=args.patch_id,supported_sha256=supported,reference_backups=args.reference_backup)
    output.with_suffix(output.suffix+'.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
