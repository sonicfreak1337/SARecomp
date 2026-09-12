"""Bounded original SUMMARY visit; no physical input, replay, or personal saves."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--tag', required=True)
p.add_argument('--language', choices=('japanese','english','french','spanish','german'), default='english')
p.add_argument('--style', type=int, choices=(1,2,3), default=2)
args = p.parse_args()
if not re.fullmatch(r'[a-z0-9_-]+', args.tag): p.error('Invalid tag')
busy = subprocess.run(['powershell.exe','-NoProfile','-Command',
    'if (Get-Process game,ninja,clang-cl,lld-link -ErrorAction SilentlyContinue) {exit 1}'],
    creationflags=subprocess.CREATE_NO_WINDOW, capture_output=True)
if busy.returncode: raise SystemExit('Another game or compiler is running; left untouched')
run = root/'runs'/args.tag
run.mkdir()
shutil.copytree(root/'.local/baseline/r354/saves', run/'user-data', copy_function=shutil.copyfile)
config = run/'sonic-display.ini'
config.write_text(f'setup_complete=1\nmode=original\nwidth=1280\nheight=720\nrender_percent=100\n'
    f'renderer=vulkan\npresentation_fps=144\nwindow_mode=windowed\ntext_language={args.language}\n'
    f'bind_a=32,0,0,8192\nbind_b=66,0,0,4096\nglyph_style={args.style}\n')
env = {k:v for k,v in os.environ.items() if not k.startswith(('KATANA_', 'SARECOMP_'))}
env.update(KATANA_PORT_BACKGROUND_TEST='1', KATANA_PORT_IGNORE_FOCUS='1',
    KATANA_USER_DATA_ROOT=str(run/'user-data'), SARECOMP_DISPLAY_CONFIG=str(config),
    KATANA_SONIC_PRIVATE_SCENARIO='tutorial-sonic', KATANA_SONIC_DIAGNOSTIC_MOVIE_SKIP_ONCE='1',
    KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS='40000', KATANA_PORT_FINAL_PROGRESS='1',
    KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY=str(run/'frames'),
    KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME='180', KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME='2880',
    KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL='180')
exe = root/'out/experimental/game.exe'
with exe.open('rb') as f: identity=hashlib.file_digest(f,'sha256').hexdigest()
startup=subprocess.STARTUPINFO(); startup.dwFlags=subprocess.STARTF_USESHOWWINDOW; startup.wShowWindow=0
with (run/'stdout.log').open('wb') as out, (run/'stderr.log').open('wb') as err:
    process=subprocess.Popen([str(exe),'--bringup-incomplete-hardware-closure','--content-root',
        str(root/'.local/baseline/r354/native-content')], cwd=exe.parent, env=env,
        stdin=subprocess.DEVNULL, stdout=out, stderr=err, startupinfo=startup,
        creationflags=subprocess.CREATE_NO_WINDOW|subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    print(f'SONIC_TUTORIAL_TEST_STARTED pid={process.pid} hidden=1 muted=1', flush=True)
    try: process.wait(timeout=55)
    finally:
        if process.poll() is None: process.kill(); process.wait()
log=(run/'stderr.log').read_text(errors='replace')
failures=[line for line in log.splitlines() if line.startswith((
    'KATANA_CRASH_CAPSULE ', 'KATANA_NATIVE_PORT_CONTRACT ', 'KATANA_RUNTIME_DISPATCH_ERROR'))]
bound=[line for line in log.splitlines() if line.startswith('SONIC_TUTORIAL_PROMPT bound=1 ')]
queued='id=tutorial-sonic character=0 main=11->18' in log
passed=process.returncode==1 and 'KATANA_SESSION_STOP reason=2 ' in log and queued and bool(bound) and not failures
result=dict(passed=passed, exit_code=process.returncode, exe_sha256=identity, hidden=True, muted=True,
    original_transition=queued, bound=bound, failures=failures, language=args.language, style=args.style)
(run/'result.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
raise SystemExit(0 if passed else 1)
