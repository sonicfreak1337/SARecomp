"""One-time, source-verified migration; never overwrites a saved baseline."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
OLD = Path(r'C:\Users\ultim\Desktop\KatanaRecomp')
BASE = ROOT / '.local/baseline/r354'
PRODUCT = OLD / 'private/ports/r354-complete-batch-20260911a'
PROJECT = OLD / 'private/game-projects/SonicAdventureRecomp'
CORE = OLD / 'work/KatanaRecomp'
BUILD = CORE / 'build-contextual-dirty'
RETAINED = OLD / '.k/w/.katana-port-work-41ab759cf62d/build-clang-cl-lld-performance-release-native-port-ninja'
EXPECTED_EXE = 'cf8ae98cf75675ca4e412a0f97e11c88a2c31340d45f5ae100c700b1370336ec'
EXPECTED_PACK = '3c831c4588e31ef428e785352c0660ac4d766e0dec485406029903a0a00c9fd2'
rows = []
last = time.monotonic()

def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def copy_file(source, target):
    global last
    if target.exists():
        raise RuntimeError(f'refusing existing destination: {target}')
    target.parent.mkdir(parents=True, exist_ok=True)
    # Copy streams, not hardlinks/reparse points or source file attributes.
    h = hashlib.sha256()
    with source.open('rb') as a, target.open('xb') as b:
        for chunk in iter(lambda: a.read(4 * 1024 * 1024), b''):
            h.update(chunk)
            b.write(chunk)
    digest = h.hexdigest()
    if sha(target) != digest:
        raise RuntimeError(f'copy verification failed: {target}')
    rows.append({'path': target.relative_to(ROOT).as_posix(),
                 'bytes': target.stat().st_size, 'sha256': digest})
    if time.monotonic() - last >= 5:
        print(f'SONIC_BASELINE copied={len(rows)} current={target.relative_to(ROOT)}', flush=True)
        last = time.monotonic()

def copy_tree(source, target):
    for parent, directories, files in os.walk(source, followlinks=False):
        directories.sort()
        for name in sorted(files):
            src = Path(parent) / name
            copy_file(src, target / src.relative_to(source))

def main():
    if BASE.exists() or (ROOT / 'baseline/r354.json').exists():
        raise RuntimeError('baseline already exists; verify instead of overwriting')
    if sha(PRODUCT / 'game.exe') != EXPECTED_EXE:
        raise RuntimeError('r354 executable identity mismatch')
    if sha(PRODUCT / 'generated/metadata/native-aot-pack.json') != EXPECTED_PACK:
        raise RuntimeError('r354 pack identity mismatch')
    commit = subprocess.check_output(['git', '-C', str(CORE), 'rev-parse', '178448be'], text=True).strip()
    print('SONIC_BASELINE authenticated r354; independent copy begins', flush=True)
    copy_tree(PRODUCT, BASE / 'product')
    copy_tree(OLD / 'private/native-content/Sonic Adventure PAL v1.003', BASE / 'native-content')
    save_source = Path(os.environ['LOCALAPPDATA']) / 'KatanaRecomp/sonic-adventure-pal-v1003/saves'
    for source in sorted(save_source.glob('*.ksave*')):
        if source.name.endswith('.tmp'):
            raise RuntimeError('save transaction is active')
        copy_file(source, BASE / 'saves/sonic-adventure-pal-v1003/saves' / source.name)
    for name in ('src', 'tools'):
        copy_tree(PROJECT / name, BASE / 'title-source' / name)
    copy_file(PROJECT / 'CMakeLists.txt', BASE / 'title-source/CMakeLists.txt')
    copy_file(RETAINED / 'generated/katana_generated.lib', BASE / 'aot/katana_generated.lib')
    copy_file(RETAINED / 'native-title-adapter/katana_native_title_adapter.lib', BASE / 'aot/katana_native_title_adapter.lib')
    copy_tree(CORE / 'include', BASE / 'sdk/include')
    copy_tree(BUILD / 'generated/include', BASE / 'sdk/generated/include')
    for name in ('katana_native_port_runtime.lib', 'katana_aot_runtime.lib'):
        copy_file(BUILD / name, BASE / 'sdk/lib' / name)
    ffmpeg = OLD / 'deps-cache/ffmpeg-8.1.2-lgpl-shared'
    copy_tree(ffmpeg / 'include', BASE / 'sdk/ffmpeg/include')
    copy_tree(ffmpeg / 'lib', BASE / 'sdk/ffmpeg/lib')
    for source in sorted(PRODUCT.glob('*.dll')):
        copy_file(source, BASE / 'sdk/ffmpeg/bin' / source.name)
    copy_file(OLD / 'private/diagnostics/r354-complete-batch-20260911a/product-verification.json', BASE / 'evidence/product-verification.json')
    archive = BASE / 'katana-source-178448be.zip'
    subprocess.run(['git', '-C', str(CORE), 'archive', '--format=zip', f'--output={archive}', commit], check=True)
    rows.append({'path': archive.relative_to(ROOT).as_posix(), 'bytes': archive.stat().st_size, 'sha256': sha(archive)})
    manifest = {'schema':'sonic-accepted-baseline-v1', 'build':'r354', 'version':'0.49.9',
                'user_accepted':'2026-09-11', 'upstream_commit':commit,
                'source_product':str(PRODUCT), 'exe_sha256':EXPECTED_EXE,
                'pack_sha256':EXPECTED_PACK, 'file_count':len(rows),
                'bytes':sum(r['bytes'] for r in rows), 'files':rows}
    dest = ROOT / 'baseline/r354.json'
    dest.parent.mkdir(parents=True, exist_ok=True)
    with dest.open('x', encoding='utf-8') as f:
        json.dump(manifest, f, indent=2)
        f.write('\n')
    # Product snapshot is read-only; all run output lives outside it.
    for row in rows:
        (ROOT / row['path']).chmod(0o444)
    print(f'SONIC_BASELINE_COMPLETE files={len(rows)} bytes={manifest["bytes"]} manifest={dest}', flush=True)

if __name__ == '__main__':
    main()

