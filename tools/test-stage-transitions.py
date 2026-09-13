"""Hidden, state-driven level-switch/pause-quit checks; no OS input or replay."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--tag', required=True)
parser.add_argument('--mode', choices=('level-switch', 'pause-quit', 'quicksave'), required=True)
parser.add_argument('--renderer', choices=('d3d11', 'vulkan'), default='d3d11')
parser.add_argument('--fixture', action='store_true')
args = parser.parse_args()
if not re.fullmatch(r'[a-zA-Z0-9_-]+', args.tag): parser.error('Invalid tag')
busy = subprocess.run(['powershell.exe', '-NoProfile', '-Command',
    'if (Get-Process game,ninja,clang-cl,lld-link -ErrorAction SilentlyContinue) {exit 1}'],
    creationflags=subprocess.CREATE_NO_WINDOW, capture_output=True)
if busy.returncode: raise SystemExit('Another game/compiler is running; left untouched')
run = root/'runs'/args.tag
run.mkdir()
data = run/'user-data'
# Never open personal files for writing. Use the current default profile by copy.
source = Path(os.environ['LOCALAPPDATA'])/'SARecomp/experimental/sonic-adventure-pal-v1003/saves'
destination = data/'sonic-adventure-pal-v1003/saves'
destination.mkdir(parents=True)
before = {}
for item in source.glob('*.ksave*'):
    if not item.is_file(): continue
    payload = item.read_bytes()
    before[str(item)] = hashlib.sha256(payload).hexdigest()
    (destination/item.name).write_bytes(payload)
if not before: raise SystemExit('No current default-profile save found')
config = run/'sonic-display.ini'
config.write_text('setup_complete=1\nmode=widescreen\nwidth=1280\nheight=720\n'
    f'render_percent=100\nrenderer={args.renderer}\nwindow_mode=windowed\n'
    'presentation_fps=144\nvsync=0\nanisotropy=1\nkeyboard_enabled=0\n')
env = {k:v for k,v in os.environ.items() if not k.startswith(('KATANA_', 'SARECOMP_'))}
env.update(KATANA_PORT_BACKGROUND_TEST='1', KATANA_PORT_IGNORE_FOCUS='1',
    KATANA_USER_DATA_ROOT=str(data), SARECOMP_DISPLAY_CONFIG=str(config),
    KATANA_PORT_FINAL_PROGRESS='1', KATANA_NATIVE_PERFORMANCE_TELEMETRY='1',
    KATANA_NATIVE_GRAPHICS_DIAGNOSTICS_MODE='off', KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS='90000',
    KATANA_SONIC_PRIVATE_SCENARIO='emerald-coast', SARECOMP_BENCHMARK_ISOLATED_INPUT='1',
    KATANA_SONIC_DIAGNOSTIC_MOVIE_SKIP_ONCE='1', SARECOMP_UPDATE_TIMING_TRACE='1',
    SARECOMP_HIDDEN_TRANSITION_TEST=args.mode)
if args.mode == 'quicksave':
    # Existing producer-owned F5/F9 probe: save at600, load at780 and960.
    # Its counter is host-monotonic and is not rewound by the loads.
    env.update(KATANA_NATIVE_DEVELOPMENT_STATE_PROBE='roundtrip',
        KATANA_SONIC_GAMEPLAY_PROBE='1', KATANA_SONIC_GAMEPLAY_INPUT_PROFILE='3')
if args.fixture:
    env.update(KATANA_SONIC_GAMEPLAY_PROBE='1', KATANA_SONIC_GAMEPLAY_INPUT_PROFILE='3',
        SARECOMP_SIXTY_FRAME_FIXTURE='1', SARECOMP_SIXTY_FRAME_INTERACTIVE='1',
        SARECOMP_NATIVE_PALETTE_LIGHTING='1', SARECOMP_NATIVE_VERTEX_NORMALS='1',
        SARECOMP_NATIVE_MATRIX_STACK='1', SARECOMP_NATIVE_COLLISION_MATH='1',
        SARECOMP_NATIVE_MATRIX_INVERSE='1', SARECOMP_NATIVE_TRIANGLE_CONTACTS='1',
        SARECOMP_NATIVE_ATAN_MATH='1')
exe = root/'out/experimental/game.exe'
with exe.open('rb') as stream: identity = hashlib.file_digest(stream,'sha256').hexdigest()
startup = subprocess.STARTUPINFO()
startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
forced = False
started = time.monotonic()
with (run/'stdout.log').open('wb') as out, (run/'stderr.log').open('wb') as err:
    process = subprocess.Popen([str(exe), '--bringup-incomplete-hardware-closure', '--content-root',
        str(root/'.local/baseline/r354/native-content'), '--presentation-fps', '144'],
        cwd=exe.parent, env=env, stdout=out, stderr=err, stdin=subprocess.DEVNULL,
        startupinfo=startup, creationflags=subprocess.CREATE_NO_WINDOW|subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    print(f'SONIC_TRANSITION_STARTED pid={process.pid} mode={args.mode} hidden=1 muted=1', flush=True)
    try:
        process.wait(timeout=105)
    except subprocess.TimeoutExpired:
        forced = True
        process.kill()
        process.wait()
    finally:
        if process.poll() is None: process.kill(); process.wait()
log = (run/'stderr.log').read_text(errors='replace')+'\n'+(run/'stdout.log').read_text(errors='replace')
steps = [line for line in log.splitlines() if line.startswith((
    'SONIC_HIDDEN_TRANSITION_TEST ', 'SONIC_NATIVE_DEVELOPMENT_STATE ', 'SONIC_GAMEPLAY_CADENCE '))]
failures = [line for line in log.splitlines() if line.startswith((
    'SONIC_NATIVE_CALLBACK_FAILURE ', 'SONIC_NATIVE_AOT_SERVICE_FAILURE ',
    'SONIC_NATIVE_OVERLAY_TASK_GRAPH_FAILURE ', 'KATANA_CRASH_CAPSULE ',
    'KATANA_NATIVE_PORT_CONTRACT ', 'KATANA_RUNTIME_DISPATCH_ERROR'))]
unchanged = all(Path(path).is_file() and hashlib.sha256(Path(path).read_bytes()).hexdigest()==sha
    for path,sha in before.items())
stops = re.findall(r'^KATANA_SESSION_STOP reason=(\d+) ', log, re.MULTILINE)
controlled_stop = bool(stops) and stops[-1]=='2' and process.returncode==1
roundtrip = [s for s in steps if s.startswith('SONIC_NATIVE_DEVELOPMENT_STATE ')]
if args.mode == 'quicksave':
    failures.extend(s for s in roundtrip if ' failure=' in s)
    operations = [re.search(r'operation=(\w+)', s)[1] for s in roundtrip]
    digests = [re.search(r'ram_digest=(\d+)', s)[1] for s in roundtrip if 'ram_digest=' in s]
    exercised = operations==['save','load','load'] and len(digests)==3 and len(set(digests))==1
else:
    exercised = any(' step=passed ' in s for s in steps)
standard_active = any('standard60=1' in s or 'SONIC_GAMEPLAY_CADENCE active=1 default=1' in s for s in steps)
report = dict(schema='sarecomp-hidden-transition-v1', mode=args.mode, renderer=args.renderer,
    standard_path=not args.fixture, exe_sha256=identity, exit_code=process.returncode, forced=forced, hidden=True, muted=True,
    replay=False, personal_saves_unchanged=unchanged, steps=steps, failures=failures,
    elapsed_seconds=time.monotonic()-started,
    controlled_stop=controlled_stop,
    passed=(args.fixture or standard_active) and controlled_stop and not forced and unchanged and not failures and exercised)
(run/'result.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2), flush=True)
raise SystemExit(0 if report['passed'] else 1)
