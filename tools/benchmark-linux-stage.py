"""Hidden native Linux gameplay probe; no physical input or installed saves."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time


def measurement(samples):
    """Exclude startup and the first sample; never equate presents with new draws."""
    if len(samples) < 2:
        return None
    first, last = samples[0], samples[-1]
    frames = int(last['drawn_frames']) - int(first['drawn_frames'])
    elapsed = int(last['elapsed_ms']) - int(first['elapsed_ms'])
    if frames <= 0 or elapsed <= 0:
        return None
    result = {'new_frames': frames, 'elapsed_ms': elapsed,
              'new_frames_per_second': frames * 1000 / elapsed}
    for name in ('execution', 'process'):
        if all(s.get(name + '_cpu_valid') == '1' for s in (first, last)):
            ticks = int(last[name + '_cpu_100ns']) - int(first[name + '_cpu_100ns'])
            if ticks >= 0:
                result[name + '_cpu_ms_per_frame'] = ticks / 10000 / frames
    return result


p = argparse.ArgumentParser()
p.add_argument('--exe', type=Path, required=True)
p.add_argument('--content', type=Path, required=True)
p.add_argument('--lib', type=Path, required=True)
p.add_argument('--run', type=Path, required=True)
p.add_argument('--scenario', default='emerald-coast')
p.add_argument('--aspect', choices=('original','deck'), default='original',
               help='Deck uses 16:10 culling at a reduced VM test resolution')
p.add_argument('--descriptor-cache', choices=('on','off'), default='on')
p.add_argument('--state-cache', choices=('on','off'), default='on')
p.add_argument('--shared-corners', choices=('on','off'), default='on')
p.add_argument('--verify-corners', action='store_true', help='Rebuild every reused vertex; diagnostic, not a throughput comparison')
p.add_argument('--profile', action='store_true', help='Read-only perf sampling; diagnostic, not a throughput comparison')
p.add_argument('--callgraph', action='store_true', help='With --profile, sample caller chains at 99 Hz')
a = p.parse_args()
if a.callgraph and not a.profile:
    p.error('--callgraph requires --profile')
if not re.fullmatch('[a-z0-9-]+', a.scenario):
    p.error('Invalid scenario')
exe = a.exe.resolve(strict=True)
with exe.open('rb') as stream:
    exe_sha256 = hashlib.file_digest(stream, 'sha256').hexdigest()
content = a.content.resolve(strict=True)
library = a.lib.resolve(strict=True)
run = a.run.resolve()
run.mkdir(parents=True, exist_ok=False)
(run/'user-data').mkdir()
display = run/'sonic-display.ini'
viewport = ('mode=widescreen\nwidth=800\nheight=500\n' if a.aspect=='deck'
            else 'mode=original\nwidth=640\nheight=480\n')
display.write_text('setup_complete=1\n'+viewport+
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
    'SARECOMP_MESH_SHARED_CORNERS':'1' if a.shared_corners=='on' else '0',
    'SARECOMP_INDEXED_CORNERS_VERIFY':'1' if a.verify_corners else '0',
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
                    perf_command=['sudo', '-n', 'perf', 'record', '-e', 'cpu-clock:u',
                                  '-F', '99' if a.callgraph else '199', '-t', str(tid),
                                  '-o', str(run/'perf.data')]
                    if a.callgraph:
                        perf_command+=['--call-graph','fp']
                    perf = subprocess.Popen(perf_command+['--','sleep','30'],
                                            stdout=perf_log, stderr=perf_log)
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
with exe.open('rb') as stream:
    if hashlib.file_digest(stream, 'sha256').hexdigest() != exe_sha256:
        raise RuntimeError('The profiled executable changed during the run')
# These ELFs have no build ID. Check identity before resolving sampled PCs,
# not just after writing a potentially misidentified report.
if perf and perf.returncode == 0:
    with (run/'perf-report.txt').open('w') as report:
        subprocess.run(['sudo', '-n', 'perf', 'report', '--stdio', '--no-children',
                        '--percent-limit', '0.5', '--sort', 'symbol', '-i', str(run/'perf.data')],
                       stdout=report, stderr=subprocess.STDOUT, check=False)
    with (run/'perf-symbols.txt').open('w') as report:
        subprocess.run(['sudo', '-n', 'perf', 'report', '--stdio', '--no-children',
                        '--call-graph','none','--show-nr-samples', '--no-demangle', '--percent-limit', '0',
                        '--field-separator=|', '--sort', 'symbol,dso', '-i', str(run/'perf.data')],
                       stdout=report, stderr=subprocess.STDOUT, check=True)
    if a.callgraph:
        with (run/'perf-families.txt').open('w') as report:
            subprocess.run(['sudo','-n','perf','report','--stdio','--children',
                            '--call-graph','none','--show-nr-samples','--no-demangle',
                            '--percent-limit','0.5','--field-separator=|','--sort','symbol,dso',
                            '-i',str(run/'perf.data')],stdout=report,stderr=subprocess.STDOUT,check=True)
with exe.open('rb') as stream:
    if hashlib.file_digest(stream, 'sha256').hexdigest() != exe_sha256:
        raise RuntimeError('The profiled executable changed during the run')
text = log_path.read_text(errors='replace')
frontiers = re.findall(r'^KATANA_RUNTIME_STOP_FRONTIER (.+)$', text, re.MULTILINE)
stop_reason = json.loads(frontiers[-1]).get('stop_reason') if frontiers else None
# NativePortStopReason::HostDeadline is 2; a completed probe alone must not
# hide a later failure during shutdown.
expected_stop = game.returncode == 1 and stop_reason == 2 and not forced
result = {'exit_code':game.returncode, 'forced_stop':forced, 'profile':a.profile, 'callgraph':a.callgraph,
          'exe':str(exe), 'exe_sha256':exe_sha256, 'scenario':a.scenario, 'aspect':a.aspect,
          'descriptor_cache':a.descriptor_cache,
          'state_cache':a.state_cache,
          'shared_corners':a.shared_corners, 'verify_corners':a.verify_corners,
          'measurement':measurement(samples),
          'wall_seconds':time.monotonic()-start, 'samples':samples,
          'completed':'SONIC_NATIVE_SCENARIO_GAMEPLAY_COMPLETE ' in text,
          'expected_stop':expected_stop, 'stop_reason':stop_reason,
          'perf_exit_code':perf.returncode if perf else None,
          'configuration':display.read_text()}
(run/'summary.json').write_text(json.dumps(result, indent=2)+'\n')
print('SONIC_LINUX_PROBE_FINISHED ' + json.dumps({k:v for k,v in result.items()
      if k not in ('samples','configuration')}), flush=True)
raise SystemExit(0 if result['completed'] and expected_stop else 1)
