"""Developer packaging step. End-user setup only extracts, patches and verifies."""
from pathlib import Path
import argparse
import hashlib
import json
import zlib

ROOT = Path(__file__).resolve().parents[1]

def sha(data):
    return hashlib.sha256(data).hexdigest()

def prepare(source, destination):
    source, destination = source.resolve(), destination.resolve()
    if destination == source or destination.is_relative_to(source):
        raise RuntimeError('Installer output must be separate from its baseline input')
    if not destination.is_relative_to(ROOT / 'out'):
        raise RuntimeError('Installer resources must be generated under this project out directory')
    destination.mkdir(parents=True, exist_ok=True)
    boot = (source / 'boot.bin').read_bytes()
    ip = (source / 'ip.bin').read_bytes()
    target = (source / 'postpal-main-ram.bin').read_bytes()
    expected = {
        'boot.bin': 'b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af',
        'ip.bin': '0b257318c1273095d4236ce2df84e2d673a89c8a7a0059b763f181bc8a793265',
        'postpal-main-ram.bin': '3704eb66597dc1a3c2e66d48fd050e0924c97ff2b956774aa4e2700319c0c80a'
    }
    for name, content in [('boot.bin', boot), ('ip.bin', ip), ('postpal-main-ram.bin', target)]:
        if sha(content) != expected[name]:
            raise RuntimeError('Unrecognized baseline input: ' + name)
    if len(target) != 16 * 1024 * 1024 or len(ip) != 32768:
        raise RuntimeError('Unexpected bootstrap layout')
    base = bytearray(len(target))
    base[0x10000:0x10000 + len(boot)] = boot
    base[0x8000:0x8000 + len(ip)] = ip
    delta = bytes(a ^ b for a, b in zip(base, target))
    packed = zlib.compress(delta, 9)
    restored = bytes(a ^ b for a, b in zip(base, zlib.decompress(packed)))
    if restored != target:
        raise RuntimeError('Bootstrap reconstruction failed')
    (destination / 'bootstrap.xor.z').write_bytes(packed)
    rows = [('boot.bin', len(boot), sha(boot), '1ST_READ.BIN'),
            ('ip.bin', len(ip), sha(ip), '@IP'),
            ('postpal-main-ram.bin', len(target), sha(target), '@RAM')]
    for path in sorted((source / 'SONICAD').rglob('*')):
        if path.is_symlink():
            raise RuntimeError('Baseline assets must be independent files')
        if path.is_file():
            relative = path.relative_to(source).as_posix()
            data = path.read_bytes()
            rows.append((relative, len(data), sha(data), relative))
    manifest = ('SARECOMP-INSTALL-1\n' + ''.join('\t'.join(map(str, row)) + '\n' for row in rows)).encode()
    (destination / 'install-files.tsv').write_bytes(manifest)
    identity = {'release': 'Sonic Adventure PAL v1.003 / MK-5100050',
                'file_count': len(rows), 'content_bytes': sum(row[1] for row in rows),
                'recipe_sha256': sha(manifest), 'bootstrap_delta_sha256': sha(packed),
                'bootstrap_delta_bytes': len(packed), 'ram_sha256': expected['postpal-main-ram.bin']}
    (destination / 'install-identity.json').write_text(json.dumps(identity, indent=2) + '\n')
    header = '#pragma once\nnamespace sonic::setup {\n'
    header += 'inline constexpr const char* install_recipe_sha256="' + sha(manifest) + '";\n'
    header += 'inline constexpr const char* bootstrap_delta_sha256="' + sha(packed) + '";\n}\n'
    (destination / 'install_identity.hpp').write_text(header)
    print(json.dumps(identity))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.baseline, args.output)
