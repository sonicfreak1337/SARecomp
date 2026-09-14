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
parser.add_argument('--update-timing', action='store_true', help='Private read-only original update/timer trace; diagnostic timing')
parser.add_argument('--render-completion', action='store_true', help='Private native guest-render completion experiment')
parser.add_argument('--sixty-frame-fixture', action='store_true', help='Private60-Hz/single-step experiment; not a product setting')
parser.add_argument('--sixty-frame-matrix', action='store_true', help='Explicit hidden Sonic StageLoader validation beyond EC')
parser.add_argument('--native-palette', action='store_true', help='Private exact native palette-lighting leaf experiment')
parser.add_argument('--native-vertex-normals', action='store_true', help='Private exact native vertex-normal leaf experiment')
parser.add_argument('--native-matrix-stack', action='store_true', help='Private exact native matrix push/pop experiment')
parser.add_argument('--matrix-write-batch', action='store_true', help='Private SDK matrix store batching; requires native matrix stack')
parser.add_argument('--native-collision-math', action='store_true', help='Private exact native collision vector math experiment')
parser.add_argument('--native-matrix-inverse', action='store_true', help='Private complete native matrix inverse/determinant family')
parser.add_argument('--native-triangle-contacts', action='store_true', help='Private complete native triangle contact owner')
parser.add_argument('--collision-candidates', choices=('native','retained'), default='native', help='Matched complete TOUCH-POLY owner comparison')
parser.add_argument('--motion-sampling', choices=('native','retained'), default='native', help='Matched complete motion/keyframe and SRT owner comparison')
parser.add_argument('--mesh-plan', choices=('cached','retained','verify'), default='cached', help='Matched authored topology/UV source-plan cache')
parser.add_argument('--native-atan-math', action='store_true', help='Private complete native atan/quotient/polynomial/scale family')
parser.add_argument('--matrix-vectors', choices=('native','retained'), default='native', help='Matched native SDK matrix-vector family comparison')
parser.add_argument('--indexed-corners', choices=('on','off'), default='off', help='Transient authored-corner reuse experiment')
parser.add_argument('--verify-indexed-corners', action='store_true', help='Compare every reused corner with the original game vertex builder')
parser.add_argument('--original-math-families', action='store_true', help='Compare against retained atan/contact owners')
parser.add_argument('--dispatch-memo', choices=('on','off'), default='on')
parser.add_argument('--dispatch-stats', action='store_true')
parser.add_argument('--profile-ms', type=int, default=0, help='Private execution-thread IP sample duration, 1000..30000; perturbs timing')
parser.add_argument('--profile-stacks', action='store_true', help='Up to 32 bounded Windows stack traces outside the game module; diagnostic only')
parser.add_argument('--trace-exceptions', action='store_true', help='Trace host exceptions in the owned game; diagnostic, not a timing comparison')
parser.add_argument('--winmm-order', choices=('position-first','capabilities-first'), default='capabilities-first')
parser.add_argument('--hardware-input', choices=('fallback','isolated'), default='fallback',
    help='Isolated skips physical devices but retains the same forward probe and normal remapping')
parser.add_argument('--exe', default='out/experimental/game.exe')
parser.add_argument('--renderer', choices=('d3d11','vulkan'), default='d3d11')
parser.add_argument('--vulkan-offscreen', action='store_true', help='Explicit hidden GPU test without monitor presentation; not display FPS')
parser.add_argument('--vulkan-descriptor-cache', choices=('on','off'), default='on')
parser.add_argument('--vulkan-state-cache', choices=('on','off'), default='on')
parser.add_argument('--gameplay-timing', choices=('original','recompiled'), default='recompiled')
# Render interpolation was withdrawn; benchmark the original frame stream.
parser.add_argument('--vsync', type=int, choices=(1,2), default=2)
parser.add_argument('--anisotropy', type=int, choices=(1,), default=1, help='Retired product control; original filtering only')
args = parser.parse_args()
if args.vulkan_offscreen and args.renderer!='vulkan': parser.error('Offscreen mode requires Vulkan')
if not re.fullmatch(r'[a-zA-Z0-9_-]+', args.tag): parser.error('Invalid tag')
if not re.fullmatch(r'[a-z0-9-]+', args.scenario): parser.error('Invalid scenario')
if args.profile_ms and not 1000 <= args.profile_ms <= 30000: parser.error('Profile duration must be 1000..30000 ms')
if args.profile_stacks and not args.profile_ms: parser.error('--profile-stacks requires --profile-ms')
if args.sixty_frame_fixture and args.scenario!='emerald-coast' and not args.sixty_frame_matrix: parser.error('Additional stages require --sixty-frame-matrix')
if args.sixty_frame_matrix and not args.sixty_frame_fixture: parser.error('Matrix requires --sixty-frame-fixture')
if args.native_palette and not args.sixty_frame_fixture: parser.error('Native palette experiment requires the private60-frame fixture')
if args.native_vertex_normals and not args.sixty_frame_fixture: parser.error('Native vertex-normal experiment requires the private60-frame fixture')
if args.native_matrix_stack and not args.sixty_frame_fixture: parser.error('Native matrix-stack experiment requires the private60-frame fixture')
if args.matrix_write_batch and not args.native_matrix_stack: parser.error('Matrix store batching requires --native-matrix-stack')
if args.native_collision_math and not args.sixty_frame_fixture: parser.error('Native collision math requires the private60-frame fixture')
if args.native_matrix_inverse and not args.sixty_frame_fixture: parser.error('Native inverse requires the private60-frame fixture')
if (args.native_triangle_contacts or args.native_atan_math) and args.gameplay_timing!='recompiled': parser.error('Private native math experiments require Recompiled timing')
if args.original_math_families and (args.native_triangle_contacts or args.native_atan_math): parser.error('Native and retained family overrides conflict')
sampler_exe = root/'build-performance/sonic_execution_sampler.exe'
if args.profile_ms and not sampler_exe.is_file(): parser.error('Build sonic_execution_sampler first')
tracer_exe=root/'build-performance/sonic_exception_tracer.exe'
if args.trace_exceptions and not tracer_exe.is_file(): parser.error('Build sonic_exception_tracer first')
busy = subprocess.run(['powershell.exe','-NoProfile','-Command',
    "if (Get-Process game,ninja,clang-cl,lld-link -ErrorAction SilentlyContinue) {exit 1}"],
    creationflags=subprocess.CREATE_NO_WINDOW, capture_output=True)
if busy.returncode: raise SystemExit('Another game or compiler is running; left untouched')
run = root/'runs'/args.tag
run.mkdir()
saves = run/'user-data'
shutil.copytree(root/'.local/baseline/r354/saves', saves, copy_function=shutil.copyfile)
display = run/'sonic-display.ini'
display.write_text(f'setup_complete=1\nmode=widescreen\nwidth={args.width}\nheight={args.height}\nrender_percent={args.render_percent}\nrenderer={args.renderer}\nvsync={args.vsync}\nanisotropy={args.anisotropy}\ngameplay_timing={int(args.gameplay_timing=="recompiled")}\n')
env = {k:v for k,v in os.environ.items() if not k.startswith(('KATANA_', 'SARECOMP_'))}
env['SARECOMP_NATIVE_MATRIX_VECTORS']='1' if args.matrix_vectors=='native' else '0'
env['SARECOMP_NATIVE_COLLISION_CANDIDATES']='1' if args.collision_candidates=='native' else '0'
env['SARECOMP_NATIVE_MOTION_SAMPLING']='1' if args.motion_sampling=='native' else '0'
env['SARECOMP_MESH_SOURCE_PLAN']='0' if args.mesh_plan=='retained' else '1'
env['SARECOMP_MESH_SOURCE_PLAN_VERIFY']='1' if args.mesh_plan=='verify' else '0'
env['SARECOMP_INDEXED_CORNERS']='1' if args.indexed_corners=='on' else '0'
env['SARECOMP_INDEXED_CORNERS_VERIFY']='1' if args.verify_indexed_corners else '0'
env.update({
    'KATANA_PORT_BACKGROUND_TEST':'1', 'KATANA_PORT_IGNORE_FOCUS':'1',
    'KATANA_USER_DATA_ROOT':str(saves), 'KATANA_PORT_FINAL_PROGRESS':'1',
    'KATANA_NATIVE_PERFORMANCE_TELEMETRY':'1', 'KATANA_NATIVE_GRAPHICS_DIAGNOSTICS_MODE':'off',
    'KATANA_SONIC_PRIVATE_SCENARIO':args.scenario, 'KATANA_SONIC_GAMEPLAY_PROBE':'1',
    'KATANA_SONIC_GAMEPLAY_INPUT_PROFILE':'3', 'KATANA_SONIC_DIAGNOSTIC_MOVIE_SKIP_ONCE':'1',
    'KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS':'100000', 'SARECOMP_DISPLAY_CONFIG':str(display),
})
env['SARECOMP_VULKAN_DESCRIPTOR_CACHE']='1' if args.vulkan_descriptor_cache=='on' else '0'
env['SARECOMP_VULKAN_STATE_CACHE']='1' if args.vulkan_state_cache=='on' else '0'
if args.vulkan_offscreen: env['SARECOMP_VULKAN_OFFSCREEN_TEST']='1'
if args.timing: env['KATANA_SONIC_DIAGNOSTIC_TIMING']='1'
if args.update_timing: env['SARECOMP_UPDATE_TIMING_TRACE']='1'
if args.render_completion: env['SARECOMP_RENDER_COMPLETION_EXPERIMENT']='1'
if args.sixty_frame_fixture: env['SARECOMP_SIXTY_FRAME_FIXTURE']='1'
if args.sixty_frame_matrix: env['SARECOMP_SIXTY_FRAME_MATRIX']='1'
if args.native_palette: env['SARECOMP_NATIVE_PALETTE_LIGHTING']='1'
if args.native_vertex_normals: env['SARECOMP_NATIVE_VERTEX_NORMALS']='1'
if args.native_matrix_stack: env['SARECOMP_NATIVE_MATRIX_STACK']='1'
if args.matrix_write_batch: env['SARECOMP_NATIVE_MATRIX_WRITE_BATCH']='1'
if args.native_collision_math: env['SARECOMP_NATIVE_COLLISION_MATH']='1'
if args.native_matrix_inverse: env['SARECOMP_NATIVE_MATRIX_INVERSE']='1'
if args.native_triangle_contacts: env['SARECOMP_NATIVE_TRIANGLE_CONTACTS']='1'
if args.native_atan_math: env['SARECOMP_NATIVE_ATAN_MATH']='1'
if args.original_math_families:
    env['SARECOMP_NATIVE_ATAN_MATH']='0'
    env['SARECOMP_NATIVE_TRIANGLE_CONTACTS']='0'
if args.dispatch_memo=='off': env['SARECOMP_DISPATCH_MEMO_DISABLE']='1'
if args.dispatch_stats: env['SARECOMP_DISPATCH_MEMO_STATS']='1'
if args.winmm_order=='position-first': env['SARECOMP_WINMM_POSITION_FIRST']='1'
if args.hardware_input=='isolated': env['SARECOMP_BENCHMARK_ISOLATED_INPUT']='1'
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
profiler=None
tracer=None
with (run/'stdout.log').open('wb') as out, (run/'stderr.log').open('wb') as err:
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    process = subprocess.Popen([str(exe),'--bringup-incomplete-hardware-closure','--content-root',
        str(root/'.local/baseline/r354/native-content')],
        cwd=root/'out/experimental',env=env,stdout=out,stderr=err,stdin=subprocess.DEVNULL,
        startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW|subprocess.BELOW_NORMAL_PRIORITY_CLASS)
    print(f'SONIC_BENCHMARK_STARTED pid={process.pid} tag={args.tag} hidden=1 muted=1',flush=True)
    try:
        if args.trace_exceptions:
            tracer=subprocess.Popen([str(tracer_exe),str(process.pid),str(exe)],stdout=out,stderr=err,stdin=subprocess.DEVNULL,
                startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW|subprocess.BELOW_NORMAL_PRIORITY_CLASS)
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
                if args.profile_ms and profiler is None and int(row['elapsed_ms'])>=10000 and row.get('execution_cpu_valid')=='1':
                    profiler=subprocess.Popen([str(sampler_exe),str(process.pid),row['execution_thread_id'],
                        str(args.profile_ms),str(run/'execution-ip.json')]+(['--stacks'] if args.profile_stacks else []),stdout=out,stderr=err,stdin=subprocess.DEVNULL,
                        startupinfo=startup,creationflags=subprocess.CREATE_NO_WINDOW|subprocess.BELOW_NORMAL_PRIORITY_CLASS)
            if elapsed>110:
                process.kill(); forced=True; break
        process.wait()
    finally:
        if process.poll() is None: process.kill(); process.wait()
        # The owned game must be stopped before forcibly retiring a failed
        # sampler: terminating a sampler during SuspendThread could strand it.
        if profiler is not None:
            try: profiler.wait(timeout=5)
            except subprocess.TimeoutExpired: profiler.kill();profiler.wait()
        if tracer is not None:
            try: tracer.wait(timeout=5)
            except subprocess.TimeoutExpired: tracer.kill();tracer.wait()
stderr=(run/'stderr.log').read_text(errors='replace')
stdout=(run/'stdout.log').read_text(errors='replace')
gameplay=rows(stderr,'SONIC_NATIVE_SCENARIO_GAMEPLAY_SAMPLE ')
steady=[r for r in gameplay if int(r['elapsed_ms'])>=10000]
result={'schema':'sarecomp-stage-performance-v3','exe_sha256':exe_sha,**vars(args),
    'monitor_presentation':'offscreen-test' if args.vulkan_offscreen else 'window-surface',
    'exit_code':process.returncode,'forced':forced,'wall_ms':(time.monotonic()-started)*1000,
    'hidden':True,'muted':True,'captures':False,'input_profile':3,'cpu_samples':samples,
    'profile_instrumented':bool(args.profile_ms),'profiler_exit_code':profiler.returncode if profiler else None,
    'gameplay_samples':gameplay,'completed':'SONIC_NATIVE_SCENARIO_GAMEPLAY_COMPLETE ' in stderr,
    'failures':[line for line in (stderr+'\n'+stdout).splitlines() if line.startswith((
        'KATANA_CRASH_CAPSULE ', 'KATANA_NATIVE_PORT_CONTRACT ', 'KATANA_RUNTIME_DISPATCH_ERROR'))]}
completion=rows(stderr,'SONIC_NATIVE_SCENARIO_GAMEPLAY_COMPLETE ')
if completion:
    last=completion[-1]
    result['frame_samples']=int(last.get('frame_samples','0'))
    result['frame_samples_truncated']=last.get('frame_samples_truncated','1')!='0'
    for key in ('p95_frame_ns','p99_frame_ns','max_frame_ns'):
        if key in last: result[key.replace('_ns','_ms')]=int(last[key])/1e6
if len(steady)>1:
    a,b=steady[0],steady[-1]
    seconds=(int(b['monotonic_ns'])-int(a['monotonic_ns']))/1e9
    # A title boundary can repeat an existing image. Neither this counter nor
    # a new draw proves an additional gameplay/simulation update.
    result.update(title_boundary_fps=(int(b['frame'])-int(a['frame']))/seconds,
        new_draw_fps=(int(b['drawn_frames'])-int(a['drawn_frames']))/seconds,
        presentation_fps=(int(b['presentations'])-int(a['presentations']))/seconds)
    result['cadence_witnesses']=[{key:row[key] for key in ('active_video_hz','release_slots','logical_delta')}
        for row in steady if row.get('cadence_readable')=='1']
    result['clock_provenance']=[dict(zip(('clock_owner','clock_bound_frame','tv_mode_word'),value))
        for value in sorted({tuple(row[key] for key in ('clock_owner','clock_bound_frame','tv_mode_word'))
            for row in steady if row.get('tv_mode_readable')=='1'})]
    # In-process sample endpoints align CPU work with the actual title
    # boundary. Keep the older external process samples for old binaries.
    clocks=[row for row in steady if row.get('execution_cpu_valid')=='1']
    result['execution_thread_continuous']=len(clocks)==len(steady) and len({row['execution_thread_id'] for row in clocks})==1
    if result['execution_thread_continuous']:
        first,last=clocks[0],clocks[-1]
        boundaries=int(last['frame'])-int(first['frame'])
        wall_ms=(int(last['monotonic_ns'])-int(first['monotonic_ns']))/1e6
        thread_ms=(int(last['execution_cpu_100ns'])-int(first['execution_cpu_100ns']))/10000
        if boundaries>0 and wall_ms>0 and thread_ms>=0:
            result.update(execution_thread_cpu_ms_per_title_boundary=thread_ms/boundaries,
                execution_thread_core_equivalents=thread_ms/wall_ms)
            if all(row.get('process_cpu_valid')=='1' for row in clocks):
                process_ms=(int(last['process_cpu_100ns'])-int(first['process_cpu_100ns']))/10000
                if process_ms>=0:result.update(aligned_process_cpu_ms_per_title_boundary=process_ms/boundaries,
                    aligned_process_core_equivalents=process_ms/wall_ms)
            if all(row.get('execution_cycles_valid')=='1' for row in clocks):
                cycles=int(last['execution_cycles'])-int(first['execution_cycles'])
                if cycles>=0:result['execution_cycles_per_title_boundary']=cycles/boundaries
cpu=[r for r in samples if r['elapsed_ms']>=10000]
if len(cpu)>1:
    a,b=cpu[0],cpu[-1]
    result.update(cpu_ms_per_title_boundary=(b['cpu_ms']-a['cpu_ms'])/(b['frame']-a['frame']),
        cpu_core_equivalents=(b['cpu_ms']-a['cpu_ms'])/(b['observer_ms']-a['observer_ms']))
for line in stdout.splitlines():
    if line.startswith('KATANA_NATIVE_PERFORMANCE_SNAPSHOT '):
        result['telemetry']=json.loads(line.partition(' ')[2])
frontiers = [json.loads(line.partition(' ')[2]) for line in stderr.splitlines()
    if line.startswith('KATANA_RUNTIME_STOP_FRONTIER ')]
result['stop_reason'] = frontiers[-1]['stop_reason'] if frontiers else None
profile_path=run/'execution-ip.json'
profile=json.loads(profile_path.read_text()) if args.profile_ms and profile_path.is_file() else None
result['profile_passed'] = not args.profile_ms or (profiler is not None and profiler.returncode==0
    and profile is not None and profile['samples']>0 and profile['errors']==0)
result['exception_trace_exit_code']=tracer.returncode if tracer else None
result['exception_trace_passed']=not args.trace_exceptions or (tracer is not None and tracer.returncode==0
    and f'SONIC_EXCEPTION_TRACE_ATTACHED pid={process.pid}' in stdout)
result['isolated_input_confirmed'] = 'SONIC_INPUT_PROBE isolated_hardware=1 profile=3 remapping=normal' in stderr
if args.update_timing:
    (run/'update-timing.json').write_text(json.dumps({
        'schema':'sarecomp-original-update-timing-v1',
        'events':rows(stderr, 'SONIC_UPDATE_TIMING_EVENT '),
        'end':rows(stderr, 'SONIC_UPDATE_TIMING_END ')},indent=2)+'\n')
    result['update_timing_passed'] = bool(steady) and all(
        r.get('update_timing')=='1' and r.get('update_unreadable')=='0' for r in steady
    ) and int(steady[-1].get('update_tasks','0')) > 0 and 'SONIC_UPDATE_TIMING_END ' in stderr
    if args.sixty_frame_fixture:
        result['update_timing_passed'] &= all(row.get('sixty_frame_fixture')=='1'
            and row.get('logical_delta')=='1' and row.get('release_slots')=='1'
            and row.get('active_video_hz')=='60' and row.get('tv_mode_word')=='0'
            for row in steady)
    elif steady:
        result['update_timing_passed'] &= int(steady[-1].get('update_elapsed','0')) > 0
    if len(steady)>1:
        a,b=steady[0],steady[-1]
        seconds=(int(b['monotonic_ns'])-int(a['monotonic_ns']))/1e9
        result['task_traversals_per_second']=(int(b['update_tasks'])-int(a['update_tasks']))/seconds
        result['task_traversals_per_title_boundary']=(int(b['update_tasks'])-int(a['update_tasks']))/(int(b['frame'])-int(a['frame']))
        elapsed_calls=int(b['update_elapsed'])-int(a['update_elapsed'])
        result['elapsed_extra_fraction']=(int(b['update_elapsed_extra'])-int(a['update_elapsed_extra']))/elapsed_calls if elapsed_calls else None
        if all(row.get('player_state_readable')=='1' for row in steady):
            result['game_timer_ticks_per_second']=(int(b['game_ticks'])-int(a['game_ticks']))/seconds
            # Death/respawn resets the visible stage timer. Never report its
            # net first/last difference as simulation speed across that reset.
            timer_intervals=[(int(right['hud_timer_ticks'])-int(left['hud_timer_ticks']),
                (int(right['monotonic_ns'])-int(left['monotonic_ns']))/1e9)
                for left,right in zip(steady,steady[1:])]
            result['hud_timer_reset_intervals']=sum(ticks<0 for ticks,_ in timer_intervals)
            valid_timer_intervals=[(ticks,dt) for ticks,dt in timer_intervals if ticks>=0 and dt>0]
            result['hud_timer_ticks_per_second']=(
                sum(ticks for ticks,_ in valid_timer_intervals)/sum(dt for _,dt in valid_timer_intervals)
                if valid_timer_intervals else None)
            result['hud_timer_rate_excludes_reset_intervals']=bool(result['hud_timer_reset_intervals'])
if args.render_completion:
    def valid_completions(row):
        submitted=int(row.get('guest_render_submitted','0'))
        completed=int(row.get('guest_render_completed','0'))
        dispatched=int(row.get('guest_render_dispatched','0'))
        return (row.get('render_completion_experiment')=='1' and
            submitted >= completed >= dispatched > 0 and submitted-dispatched <= 2 and
            0 <= int(row.get('guest_render_counter','-1')) <= 12500000 and
            row.get('periodic_callbacks')=='0')
    result['render_completion_passed']=bool(steady) and all(map(valid_completions,steady))
    if len(steady)>1:
        a,b=steady[0],steady[-1]
        result['render_notifications_per_second']=(int(b['guest_render_dispatched'])-int(a['guest_render_dispatched']))/((int(b['monotonic_ns'])-int(a['monotonic_ns']))/1e9)
# This probe requests a graceful deadline at 60 seconds of gameplay. Exit 1
# alone is also used for real runtime faults, so require the matching frontier.
result['passed'] = (result['completed'] and process.returncode == 1
    and result['stop_reason'] == 2 and not result['failures'] and not forced
    and len(steady) > 1 and len(cpu) > 1 and result['profile_passed'] and result['exception_trace_passed']
    and (not args.update_timing or result['update_timing_passed'])
    and (not args.render_completion or result['render_completion_passed'])
    and result['isolated_input_confirmed'] == (args.hardware_input=='isolated'))
if args.native_palette:
    result['palette_native_calls']=int(steady[-1].get('palette_native_calls','0')) if steady else 0
    result['palette_original_calls']=int(steady[-1].get('palette_original_calls','0')) if steady else 0
    result['palette_native_executed']=result['palette_native_calls']>0
    result['passed'] &= result['palette_native_executed']
if args.native_vertex_normals:
    result['vertex_normals_native_calls']=int(steady[-1].get('vertex_normals_native_calls','0')) if steady else 0
    result['vertex_normals_original_calls']=int(steady[-1].get('vertex_normals_original_calls','0')) if steady else 0
    result['vertex_normals_native_executed']=result['vertex_normals_native_calls']>0
    result['passed'] &= result['vertex_normals_native_executed']
if args.native_matrix_stack:
    for kind in ('push','pop'):
        for path in ('native','original'):
            key=f'matrix_{kind}_{path}_calls'
            result[key]=int(steady[-1].get(key,'0')) if steady else 0
    result['matrix_stack_native_executed']=all(result[f'matrix_{kind}_native_calls']>0 for kind in ('push','pop'))
    result['passed'] &= result['matrix_stack_native_executed']
    result['matrix_staged_groups']=int(steady[-1].get('matrix_staged_groups','0')) if steady else 0
    if args.matrix_write_batch:
        # A staged/flush group can still use SDK scalar replay. This witnesses
        # execution of the experiment, not fast-path admission or a speedup.
        result['matrix_batch_staging_executed']=result['matrix_staged_groups']>0
        result['passed'] &= result['matrix_batch_staging_executed']
if args.native_collision_math:
    for kind in ('cross','length','normalize'):
        for path in ('native','original'):
            key=f'collision_{kind}_{path}_calls'
            result[key]=int(steady[-1].get(key,'0')) if steady else 0
    result['collision_native_executed']=all(result[f'collision_{kind}_native_calls']>0 for kind in ('cross','length','normalize'))
    result['passed'] &= result['collision_native_executed']
if args.native_matrix_inverse:
    for kind in ('inverse','determinant'):
        for path in ('native','original'):
            key=f'matrix_{kind}_{path}_calls'
            result[key]=int(steady[-1].get(key,'0')) if steady else 0
    # The inverse owns its fixed determinant call; it need not reach the
    # separate public determinant wrapper for that nested work.
    result['matrix_inverse_native_executed']=result['matrix_inverse_native_calls']>0
    result['passed'] &= result['matrix_inverse_native_executed']
if args.native_atan_math:
    for kind in ('atan','atan_quotient','atan_polynomial','atan_scale'):
        for path in ('native','original'):
            key=f'{kind}_{path}_calls'
            result[key]=int(steady[-1].get(key,'0')) if steady else 0
    result['atan_native_executed']=result['atan_native_calls']>0
    result['passed'] &= result['atan_native_executed']
result['matrix_vectors']=args.matrix_vectors
for kind in ('point','direction','store','translation'):
    for path in ('native','original'):
        key=f'matrix_vector_{kind}_{path}_calls'
        result[key]=int(steady[-1].get(key,'0')) if steady else 0
if args.native_triangle_contacts:
    for path in ('native','original'):
        key=f'triangle_contacts_{path}_calls'
        result[key]=int(steady[-1].get(key,'0')) if steady else 0
    result['triangle_contacts_native_executed']=result['triangle_contacts_native_calls']>0
    result['passed'] &= result['triangle_contacts_native_executed']
if args.sixty_frame_fixture:
    # Scenario/contract PASS is deliberately separate from reaching the user's
    # performance goal. A correctly configured but CPU-limited run is not 60fps.
    result['sixty_frame_target_passed'] = result['passed'] and args.update_timing and all(
        59.5 <= result.get(key,0) <= 60.5 for key in
        ('new_draw_fps','task_traversals_per_second','game_timer_ticks_per_second')) \
        and (args.vsync==1 or 59.5 <= result.get('presentation_fps',0) <= 60.5) \
        and result.get('frame_samples',0)>0 and not result.get('frame_samples_truncated',True) \
        and 0 < result.get('p95_frame_ms',0) <= 17.5
if args.vulkan_offscreen:
    result['offscreen_mode_active']='SONIC_VULKAN_OFFSCREEN_TEST active=1' in stderr
    result['passed'] &= result['offscreen_mode_active']
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('cpu_samples','gameplay_samples','telemetry')},indent=2))
raise SystemExit(0 if result['passed'] else 1)
