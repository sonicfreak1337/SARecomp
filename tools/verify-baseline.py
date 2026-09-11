import argparse
import hashlib
import json
from pathlib import Path
import time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--quick', action='store_true', help='Check executable and AOT metadata only')
args = parser.parse_args()
portable = (root/'.local/baseline/r354-restored.json').exists()
manifest = json.loads((root / ('baseline/development-bundle.json' if portable else 'baseline/r354.json')).read_text(encoding='utf-8'))
checked = 0
last = time.monotonic()
for row in manifest['files']:
    if args.quick and row['path'] not in (
        '.local/baseline/r354/product/game.exe',
        '.local/baseline/r354/product/generated/metadata/native-aot-pack.json'):
        continue
    path = (root / row['path']).resolve()
    allowed = [root/'.local/baseline/r354']
    if portable: allowed.append(root/'.local/toolchain')
    if not any(path.is_relative_to(folder.resolve()) for folder in allowed):
        raise SystemExit('Invalid baseline manifest path')
    if path.stat().st_size != row['bytes']:
        raise SystemExit(f'Baseline size mismatch: {path}')
    with path.open('rb') as f:
        digest = hashlib.file_digest(f, 'sha256').hexdigest()
    if digest != row['sha256']:
        raise SystemExit(f'Baseline hash mismatch: {path}')
    checked += 1
    if time.monotonic() - last >= 5:
        print(f'SONIC_BASELINE_VERIFY checked={checked}', flush=True)
        last = time.monotonic()
print(f'SONIC_BASELINE_VERIFIED build=r354 files={checked} mode={"quick" if args.quick else "full"}')
