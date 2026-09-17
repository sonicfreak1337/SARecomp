"""Install a packaged Deck test build in a fresh, isolated Linux user namespace."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--installer', type=Path, required=True)
    p.add_argument('--installer-sha', required=True)
    p.add_argument('--game-sha', required=True)
    p.add_argument('--gdi', type=Path, required=True)
    p.add_argument('--run', type=Path, required=True)
    a = p.parse_args()
    assert os.name == 'posix' and os.getuid() != 0
    assert sha(a.installer) == a.installer_sha
    a.run.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('KATANA_', 'SARECOMP_'))}
    env.update(KATANA_PORT_BACKGROUND_TEST='1', KATANA_USER_DATA_ROOT=str(a.run/'state'), SDL_AUDIODRIVER='dummy')
    start = time.monotonic()
    with (a.run/'install.log').open('w') as log:
        process = subprocess.run(['sh', str(a.installer), '--full-install-check', str(a.gdi)], env=env,
                                 stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, timeout=1800)
    text = (a.run/'install.log').read_text()
    assert process.returncode == 0 and 'SONIC_FULL_INSTALL_OK files=2070 ' in text, text[-1000:]
    games = list((a.run/'SARecomp-app').glob('*/game'))
    assert len(games) == 1
    game = games[0]
    app = game.parent
    assert sha(game) == a.game_sha
    manifest = app/'resources/payload-files.tsv'
    assert app.name == '1.0-candidate-'+sha(manifest)[:16]
    payload_count = 0
    for row in manifest.read_text().splitlines()[1:]:
        name, size, digest = row.split('\t')
        path = app/name
        assert path.resolve().is_relative_to(app.resolve()) and not path.is_symlink()
        assert path.stat().st_size == int(size) and sha(path) == digest, name
        payload_count += 1
    assert not (app/'.sarecomp-diagnostics').exists()
    content = Path((a.run/'state-content/katana-content-root.txt').read_text().strip())
    assert content.resolve().is_relative_to((a.run/'state-content').resolve())
    original_count = 0
    for row in (app/'resources/install-files.tsv').read_text().splitlines()[1:]:
        name, size, digest, _ = row.split('\t')
        path = content/name
        assert path.resolve().is_relative_to(content.resolve())
        assert path.stat().st_size == int(size) and sha(path) == digest, name
        original_count += 1
    assert original_count == 2070
    settings = dict(row.split('=', 1) for row in (a.run/'state/sonic-display.ini').read_text().splitlines() if '=' in row)
    for key, value in {'gameplay_timing': '0', 'width': '1280', 'height': '800', 'renderer': 'vulkan', 'window_mode': 'fullscreen'}.items():
        assert settings[key] == value, (key, settings)
    env.update(LD_LIBRARY_PATH=str(app/'lib'), KATANA_USER_DATA_ROOT=str(a.run/'defaults-check'))
    with (a.run/'defaults.log').open('w') as log:
        process = subprocess.run([str(app/'sonic-setup'), '--defaults-check', 'steam-deck'], env=env,
                                 stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, timeout=180)
    assert process.returncode == 0 and 'SONIC_SETUP_DEFAULTS_OK' in (a.run/'defaults.log').read_text()
    report = dict(installer=str(a.installer), installer_sha256=a.installer_sha, installed_game=str(game),
                  game_sha256=a.game_sha, content=str(content), uid=os.getuid(), payload_files_verified=payload_count,
                  original_files_verified=original_count, settings=settings, diagnostics_default='off',
                  settings_preservation_check=True, full_installation=True, seconds=time.monotonic()-start)
    (a.run/'result.json').write_text(json.dumps(report, indent=2)+'\n')
    print('SONIC_DECK_TEST_INSTALLER_OK '+json.dumps(report), flush=True)


if __name__ == '__main__':
    main()
