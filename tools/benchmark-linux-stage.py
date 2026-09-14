"""Hidden native Linux gameplay probe; no physical input or installed saves."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('--exe', type=Path, required=True)
p.add_argument('--content', type=Path, required=True)
p.add_argument('--lib', type=Path, required=True)
p.add_argument('--run', type=Path, required=True)
p.add_argument('--scenario', default='emerald-coast')
p.add_argument('--descriptor-cache', choices=('on','off'), default='on')
p.add_argument('--state-cache', choices=('on','off'), default='on')
p.add_argument('--profile', action='store_true', help='Read-only perf sampling; diagnostic, not a throughput comparison')
a = p.parse_args()
if not re.fullmatch('[a-z0-9-]+', a.scenario):
    p.error('Invalid scenario')
exe = a.exe.resolve(strict=True)
content = a.content.resolve(strict=True)
library = a.lib.resolve(strict=True)
run = a.run.resolve()
run.mkdir(parents=True, exist_ok=False)
(run/'user-data').mkdir()
display = run/'sonic-display.ini'
display.write_text('setup_complete=1\nmode=original\nwidth=640\nheight=480\n'
                   'render_percent=50\nrenderer=vulkan\nwindow_mode=windowed\n'
                   'vsync=2\ngameplay_timing=1\n')
env = {k:v for k,v in os.environ.items() if not k.startswith(('KATANA_', 'SARECOMP_'))}
env.update({
    'LD_LIBRARY_PATH': str(library), 'SDL_AUDIODRIVER':'dummy',
    'KATANA_PORT_BACKGROUND_TEST':'1', 'SARECOMP_PROBE_WAIT_FOR_GAMEPLAY':'1',
    'KATANA_PORT_IGNORE_FOCUS':'1', 'KATANA_USER_DATA_ROOT':str(run/'user-data'),
    'SARECOMP_DISPLAY_CONFIG':str(display), 'SARECOMP_BENCHMARK_ISOLATED_INPUT':'1',
    'SARECOMP_VULKAN_DESCRIPTOR_CACHE':'1' if a.descriptor_cache=='on' else '0',
    'SARECOMP_VULKAN_STATE_CACHE':'1' if a.state_cache=='on' else '0',
    'KATANA_PORT_FINAL_PROGRESS':'1', 'KATANA_NATIVE_PERFORMANCE_TELEMETRY':'1',
    'KATANA_NATIVE_GRAPHICS_DIAGNOSTICS_MODE':'off',
    'KATANA_SONIC_PRIVATE_SCENARIO':a.scenario, 'KATANA_SONIC_GAMEPLAY_PROBE':'1',
    'KATANA_SONIC_GAMEPLAY_INPUT_PROFILE':'3', 'KATANA_SONIC_DIAGNOSTIC_MOVIE_SKIP_ONCE':'1',
    # TCG takes much longer to load than hardware. The separate gameplay
    # clock still starts only after a rendered frame from the selected stage.
    'KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS':'1200000',
})
log_path = run/'game.log'
perf = None
perf_log = None
perf_attempted = False
forced = False
start = time.monotonic()
samples = []
with log_path.open('w') as log:
    game = subprocess.Popen([str(exe), '--bringup-incomplete-hardware-closure',
                             '--content-root', str(content)], cwd=exe.parent,
                            env=env, stdout=log, stderr=log, stdin=subprocess.DEVNULL)
    print(f'SONIC_LINUX_PROBE_STARTED pid={game.pid} profile={int(a.profile)}', flush=True)
    try:
        last_count = 0
        while game.poll() is None:
            time.sleep(1)
            text = log_path.read_text(errors='replace')
            samples = [dict(re.findall(r'(\w+)=([^ ]+)', line))
                       for line in text.splitlines()
                       if line.startswith('SONIC_NATIVE_SCENARIO_GAMEPLAY_SAMPLE ')]
            if len(samples) > last_count:
                last_count = len(samples)
                print('SONIC_LINUX_PROBE_SAMPLE elapsed_ms=' + samples[-1]['elapsed_ms'] +
                      ' drawn_frames=' + samples[-1]['drawn_frames'], flush=True)
            if a.profile and samples and not perf_attempted:
                perf_attempted = True
                tid = int(samples[-1]['execution_thread_id'])
                # Verify that the sampled thread belongs to this owned game.
                if tid and Path(f'/proc/{game.pid}/task/{tid}').is_dir():
                    perf_log = (run/'perf-command.log').open('w')
                    perf = subprocess.Popen(['sudo', '-n', 'perf', 'record', '-e', 'cpu-clock:u',
                                             '-F', '199', '-t', str(tid), '-o', str(run/'perf.data'),
                                             '--', 'sleep', '30'], stdout=perf_log, stderr=perf_log)
            if time.monotonic() - start > 1230:
                forced = True
                game.terminate()
                try: game.wait(timeout=10)
                except subprocess.TimeoutExpired: game.kill()
                break
        game.wait()
    finally:
        if game.poll() is None:
            game.terminate()
            try: game.wait(timeout=10)
            except subprocess.TimeoutExpired: game.kill(); game.wait()
        if perf:
            perf.wait(timeout=40)
        if perf_log:
            perf_log.close()
if perf and perf.returncode == 0:
    with (run/'perf-report.txt').open('w') as report:
        subprocess.run(['sudo', '-n', 'perf', 'report', '--stdio', '--no-children',
                        '--percent-limit', '0.5', '--sort', 'symbol', '-i', str(run/'perf.data')],
                       stdout=report, stderr=subprocess.STDOUT, check=False)
text = log_path.read_text(errors='replace')
frontiers = re.findall(r'^KATANA_RUNTIME_STOP_FRONTIER (.+)$', text, re.MULTILINE)
stop_reason = json.loads(frontiers[-1]).get('stop_reason') if frontiers else None
# NativePortStopReason::HostDeadline is 2; a completed probe alone must not
# hide a later failure during shutdown.
expected_stop = game.returncode == 1 and stop_reason == 2 and not forced
result = {'exit_code':game.returncode, 'forced_stop':forced, 'profile':a.profile,
          'descriptor_cache':a.descriptor_cache,
          'state_cache':a.state_cache,
          'wall_seconds':time.monotonic()-start, 'samples':samples,
          'completed':'SONIC_NATIVE_SCENARIO_GAMEPLAY_COMPLETE ' in text,
          'expected_stop':expected_stop, 'stop_reason':stop_reason,
          'perf_exit_code':perf.returncode if perf else None,
          'configuration':display.read_text()}
(run/'summary.json').write_text(json.dumps(result, indent=2)+'\n')
print('SONIC_LINUX_PROBE_FINISHED ' + json.dumps({k:v for k,v in result.items()
      if k not in ('samples','configuration')}), flush=True)
raise SystemExit(0 if result['completed'] and expected_stop else 1)
