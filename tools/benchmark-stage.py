"""One hidden, muted stage benchmark; separate saves and no GPU readback."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--tag', required=True)
parser.add_argument('--scenario', default='emerald-coast')
parser.add_argument('--width', type=int, default=3182)
parser.add_argument('--height', type=int, default=1332)
parser.add_argument('--render-percent', type=int, default=100)
parser.add_argument('--timing', action='store_true')
parser.add_argument('--exe', default='out/experimental/game.exe')
parser.add_argument('--renderer', choices=('d3d11','vulkan'), default='d3d11')
args = parser.parse_args()
if not re.fullmatch(r'[a-zA-Z0-9_-]+', args.tag): parser.error('Invalid tag')
if not re.fullmatch(r'[a-z0-9-]+', args.scenario): parser.error('Invalid scenario')
busy = subprocess.run(['powershell.exe','-NoProfile','-Command',
    "if (Get-Process game,ninja,clang-cl,lld-link -ErrorAction SilentlyContinue) {exit 1}"],
    creationflags=subprocess.CREATE_NO_WINDOW, capture_output=True)
if busy.returncode: raise SystemExit('Another game or compiler is running; left untouched')
run = root/'runs'/args.tag
run.mkdir()
saves = run/'user-data'
shutil.copytree(root/'.local/baseline/r354/saves', saves, copy_function=shutil.copyfile)
display = run/'sonic-display.ini'
display.write_text(f'mode=widescreen\nwidth={args.width}\nheight={args.height}\nrender_percent={args.render_percent}\nrenderer={args.renderer}\n')
env = {k:v for k,v in os.environ.items() if not k.startswith(('KATANA_', 'SARECOMP_'))}
env.update({
    'KATANA_PORT_BACKGROUND_TEST':'1', 'KATANA_PORT_IGNORE_FOCUS':'1',
    'KATANA_USER_DATA_ROOT':str(saves), 'KATANA_PORT_FINAL_PROGRESS':'1',
    'KATANA_NATIVE_PERFORMANCE_TELEMETRY':'1', 'KATANA_NATIVE_GRAPHICS_DIAGNOSTICS_MODE':'off',
    'KATANA_SONIC_PRIVATE_SCENARIO':args.scenario, 'KATANA_SONIC_GAMEPLAY_PROBE':'1',
    'KATANA_SONIC_GAMEPLAY_INPUT_PROFILE':'3', 'KATANA_SONIC_DIAGNOSTIC_MOVIE_SKIP_ONCE':'1',
    'KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS':'100000', 'SARECOMP_DISPLAY_CONFIG':str(display),
})
if args.timing: env['KATANA_SONIC_DIAGNOSTIC_TIMING']='1'
exe = (root/args.exe).resolve(strict=True)
# Reference executables use the exact same frozen DLLs and installed assets.
env['PATH'] = str(root/'out/experimental') + os.pathsep + env.get('PATH', '')
with exe.open('rb') as f: exe_sha = hashlib.file_digest(f,'sha256').hexdigest()
kernel = c.WinDLL('kernel32', use_last_error=True)
kernel.GetProcessTimes.argtypes = [w.HANDLE]+[c.POINTER(w.FILETIME)]*4
kernel.GetProcessTimes.restype = w.BOOL
def cpu_ms(process):
    times = [w.FILETIME() for _ in range(4)]
    if not kernel.GetProcessTimes(w.HANDLE(int(process._handle)), *map(c.byref,times)):
        raise c.WinError(c.get_last_error())
    return sum((t.dwHighDateTime<<32)|t.dwLowDateTime for t in times[2:])/10000
def rows(text, prefix):
    return [dict(re.findall(r'(\w+)=([^ ]+)',line)) for line in text.splitlines() if line.startswith(prefix)]
samples=[]
started=time.monotonic()
forced=False
with (run/'stdout.log').open('wb') as out, (run/'stderr.log').open('wb') as err:
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(exe),'--bringup-incomplete-hardware-closure','--content-root',
        str(root/'.local/baseline/r354/native-content'),'--presentation-fps','144'],
        cwd=root/'out/experimental',env=env,stdout=out,stderr=err,stdin=subprocess.DEVNULL,
        startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW|subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    print(f'SONIC_BENCHMARK_STARTED pid={process.pid} tag={args.tag} hidden=1 muted=1',flush=True)
    try:
        last_frame=-1
        while process.poll() is None:
            time.sleep(0.5)
            elapsed=time.monotonic()-started
            reports=rows((run/'stderr.log').read_text(errors='replace'),'SONIC_NATIVE_SCENARIO_GAMEPLAY_SAMPLE ')
            if reports and int(reports[-1]['frame'])!=last_frame:
                row=reports[-1]
                last_frame=int(row['frame'])
                samples.append({'frame':last_frame,'elapsed_ms':int(row['elapsed_ms']),
                    'monotonic_ns':int(row['monotonic_ns']),'observer_ms':elapsed*1000,
                    'cpu_ms':cpu_ms(process)})
            if elapsed>110:
                process.kill(); forced=True; break
        process.wait()
    finally:
        if process.poll() is None: process.kill(); process.wait()
stderr=(run/'stderr.log').read_text(errors='replace')
stdout=(run/'stdout.log').read_text(errors='replace')
gameplay=rows(stderr,'SONIC_NATIVE_SCENARIO_GAMEPLAY_SAMPLE ')
steady=[r for r in gameplay if int(r['elapsed_ms'])>=10000]
result={'schema':'sarecomp-stage-performance-v1','exe_sha256':exe_sha,**vars(args),
    'exit_code':process.returncode,'forced':forced,'wall_ms':(time.monotonic()-started)*1000,
    'hidden':True,'muted':True,'captures':False,'input_profile':3,'cpu_samples':samples,
    'gameplay_samples':gameplay,'completed':'SONIC_NATIVE_SCENARIO_GAMEPLAY_COMPLETE ' in stderr,
    'failures':[line for line in (stderr+'\n'+stdout).splitlines() if line.startswith((
        'KATANA_CRASH_CAPSULE ', 'KATANA_NATIVE_PORT_CONTRACT ', 'KATANA_RUNTIME_DISPATCH_ERROR'))]}
if len(steady)>1:
    a,b=steady[0],steady[-1]
    seconds=(int(b['monotonic_ns'])-int(a['monotonic_ns']))/1e9
    result.update(sim_fps=(int(b['frame'])-int(a['frame']))/seconds,
        presentation_fps=(int(b['presentations'])-int(a['presentations']))/seconds)
cpu=[r for r in samples if r['elapsed_ms']>=10000]
if len(cpu)>1:
    a,b=cpu[0],cpu[-1]
    result.update(cpu_ms_per_frame=(b['cpu_ms']-a['cpu_ms'])/(b['frame']-a['frame']),
        cpu_core_equivalents=(b['cpu_ms']-a['cpu_ms'])/(b['observer_ms']-a['observer_ms']))
for line in stdout.splitlines():
    if line.startswith('KATANA_NATIVE_PERFORMANCE_SNAPSHOT '):
        result['telemetry']=json.loads(line.partition(' ')[2])
frontiers = [json.loads(line.partition(' ')[2]) for line in stderr.splitlines()
    if line.startswith('KATANA_RUNTIME_STOP_FRONTIER ')]
result['stop_reason'] = frontiers[-1]['stop_reason'] if frontiers else None
# This probe requests a graceful deadline at 60 seconds of gameplay. Exit 1
# alone is also used for real runtime faults, so require the matching frontier.
result['passed'] = (result['completed'] and process.returncode == 1
    and result['stop_reason'] == 2 and not result['failures'] and not forced
    and len(steady) > 1 and len(cpu) > 1)
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('cpu_samples','gameplay_samples','telemetry')},indent=2))
raise SystemExit(0 if result['passed'] else 1)
