"""Hidden native Linux gameplay probe; no physical input or installed saves."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

def sample_rows(text):
    result=[]
    for line in text.splitlines():
        if not line.startswith('SONIC_NATIVE_SCENARIO_GAMEPLAY_SAMPLE '): continue
        fields=re.findall(r'(\w+)=([^ ]+)',line)
        row=dict(fields)
        if len(fields)!=len(row) or any(not row.get(key,'').isdigit() for key in
                ('frame','relative_frame','elapsed_ms','monotonic_ns','drawn_frames','game_ticks','final')):
            continue
        result.append(row)
    return result


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
    # Original's authored overload recovery may run different numbers of game
    # updates in equally long image windows. Expose that work count alongside
    # frame cost instead of attributing fewer updates to faster execution.
    if 'game_ticks' in first and 'game_ticks' in last:
        updates = (int(last['game_ticks']) - int(first['game_ticks'])) & 0xFFFFFFFF
        if 0 < updates <= frames * 8:
            result['game_updates'] = updates
            result['game_updates_per_frame'] = updates / frames
            for name in ('execution', 'process'):
                value = result.get(name + '_cpu_ms_per_frame')
                if value is not None:
                    result[name + '_cpu_ms_per_game_update'] = value * frames / updates
    return result


p = argparse.ArgumentParser()
p.add_argument('--exe', type=Path, required=True)
p.add_argument('--content', type=Path, required=True)
p.add_argument('--lib', type=Path, required=True)
p.add_argument('--run', type=Path, required=True)
p.add_argument('--scenario', default='emerald-coast',
    choices=('emerald-coast','gamma-emerald-coast','sonic-windy-valley','amy-hot-shelter','sonic-chaos-4',
             'knuckles-sky-deck','knuckles-lost-world'),
    help='Reviewed performance scenarios; IDs match the private stage table exactly')
p.add_argument('--gameplay-timing', choices=('original','recompiled'), default='recompiled')
p.add_argument('--gameplay-math', choices=('native','retained'), default='native')
p.add_argument('--native-animation', choices=('off','on','installed'), default='installed',
               help='Same-executable comparison of the complete native animation hierarchy')
p.add_argument('--native-pose', choices=('off','on','installed'), default='installed',
               help='Same-executable comparison of the complete native pose mixer')
p.add_argument('--native-model-packets', choices=('off','on','verify'), default='off')
p.add_argument('--native-model-vertex-stream', choices=('off','on'), default='off')
p.add_argument('--native-projection-batch', choices=('off','on','verify','installed'), default='installed')
p.add_argument('--native-matrix-bulk', choices=('off','on'), default='off')
p.add_argument('--ram-prepared-access', choices=('off','on'), default='off')
p.add_argument('--native-collision-memory', choices=('off','on'), default='off')
p.add_argument('--native-collision-closure', choices=('off','on'), default='off')
p.add_argument('--native-model-pipeline', choices=('off','on','installed'), default='installed')
p.add_argument('--native-object-activation', choices=('off','on','installed'), default='installed')
p.add_argument('--native-movement', choices=('off','on'), default='off')
p.add_argument('--native-movement-contact', choices=('off','on'), default='off')
p.add_argument('--native-model-submission', choices=('off','on'), default='off')
p.add_argument('--native-land-render', choices=('off','on'), default='off')
p.add_argument('--native-collision-world', choices=('off','on','installed'), default='installed')
p.add_argument('--native-world-sdk', choices=('off','on','installed'), default='installed')
p.add_argument('--native-render-hierarchy', choices=('off','on','installed'), default='installed')
p.add_argument('--native-render-local-proofs', choices=('off','on','installed'), default='installed')
p.add_argument('--native-render-root-proofs', choices=('off','on','installed'), default='installed')
p.add_argument('--native-inverse-trig', choices=('off','on','installed'), default='installed')
p.add_argument('--native-inverse-memory', choices=('off','on','installed'), default='installed')
p.add_argument('--native-rigid-hierarchy', choices=('off','on','installed'), default='installed')
p.add_argument('--native-morph-hierarchy', choices=('off','on','installed'), default='installed')
p.add_argument('--native-closed-memory', choices=('off','on','installed'), default='installed')
p.add_argument('--native-render-context', choices=('off','on','installed'), default='installed')
p.add_argument('--native-palette-batch', choices=('off','on','installed'), default='installed')
p.add_argument('--collision-scope', choices=('gameplay','all','installed'), default='installed')
p.add_argument('--async-audio-status', choices=('off','on','installed'), default='installed')
p.add_argument('--sound-metadata', choices=('off','on','verify'), default='off')
p.add_argument('--deferred-midi-notes', choices=('off','on'), default='off')
p.add_argument('--math-scope', choices=('gameplay','all'), default='all',
               help='Compare the former scene gate against independently admitted native leaves')
p.add_argument('--phase', choices=('gameplay','stage-entry'), default='gameplay',
               help='Stage-entry includes the ordinary introductory camera before gameplay')
p.add_argument('--diagnostics', choices=('on','off','installed'), default='off')
p.add_argument('--transfer-plans', choices=('original','cached','verify','installed'), default='original')
p.add_argument('--scalar-writes', choices=('original','fused'), default='original',
               help='Private native RAM store experiment; defaults to the retained path')
p.add_argument('--stack-frames', choices=('original','fused'), default='original',
               help='Private complete stack sequence experiment')
p.add_argument('--fpu-register-cache', choices=('original','retained'), default='original',
               help='Private nontrapping FPU integer-register retention')
p.add_argument('--ram-regions', choices=('original','fused','installed'), default='original',
               help='Private mixed RAM/ALU prefix execution')
p.add_argument('--telemetry', choices=('on','off'), default='off',
               help='Per-provider timers; off keeps release-like execution cost')
p.add_argument('--begin-frame', type=int, default=0, help='Optional exact warmup boundary; requires --end-frame')
p.add_argument('--end-frame', type=int, default=0, help='Stop the isolated probe after this many title boundaries')
p.add_argument('--aspect', choices=('original','deck'), default='original',
               help='Deck uses 16:10 culling at a reduced VM test resolution')
p.add_argument('--descriptor-cache', choices=('on','off'), default='on')
p.add_argument('--state-cache', choices=('on','off'), default='on')
p.add_argument('--shared-corners', choices=('on','off'), default='on')
p.add_argument('--verify-corners', action='store_true', help='Rebuild every reused vertex; diagnostic, not a throughput comparison')
p.add_argument('--profile', action='store_true', help='Read-only perf sampling; diagnostic, not a throughput comparison')
p.add_argument('--callgraph', action='store_true', help='With --profile, sample caller chains at 99 Hz')
a = p.parse_args()
if not (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')):
    p.error('A virtual display is required; run this hidden probe with xvfb-run -a')
if (a.begin_frame or a.end_frame) and not 0 < a.begin_frame < a.end_frame <= 10000:
    p.error('Fixed window requires 0 < begin-frame < end-frame <= 10000')
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
                   'vsync=2\ngameplay_timing='+str(int(a.gameplay_timing=='recompiled'))+'\n')
env = {k:v for k,v in os.environ.items() if not k.startswith(('KATANA_', 'SARECOMP_'))}
env.update({
    'SARECOMP_NATIVE_POSE_BLEND':'1' if a.native_pose=='on' else '0',
    'SARECOMP_NATIVE_COLLISION_ALL_SCENES':'1' if a.collision_scope=='all' else '0',
    'SARECOMP_INTERNAL_DIAGNOSTICS':'1' if a.diagnostics=='on' else '0',
    'SARECOMP_PREPARED_TRANSFERS':'0' if a.transfer_plans=='original' else '1',
    'SARECOMP_PREPARED_TRANSFERS_VERIFY':'1' if a.transfer_plans=='verify' else '0',
    'SARECOMP_SCALAR_WRITES':'1' if a.scalar_writes=='fused' else '0',
    'SARECOMP_STACK_FRAMES':'1' if a.stack_frames=='fused' else '0',
    'SARECOMP_FPU_REGISTER_CACHE':'1' if a.fpu_register_cache=='retained' else '0',
    'SARECOMP_RAM_REGIONS':'1' if a.ram_regions=='fused' else '0',
    'LD_LIBRARY_PATH': str(library), 'SDL_AUDIODRIVER':'dummy',
    'KATANA_PORT_BACKGROUND_TEST':'1', 'SARECOMP_PROBE_WAIT_FOR_GAMEPLAY':str(int(a.phase=='gameplay')),
    'KATANA_PORT_IGNORE_FOCUS':'1', 'KATANA_USER_DATA_ROOT':str(run/'user-data'),
    'SARECOMP_DISPLAY_CONFIG':str(display), 'SARECOMP_BENCHMARK_ISOLATED_INPUT':'1',
    'SARECOMP_VULKAN_DESCRIPTOR_CACHE':'1' if a.descriptor_cache=='on' else '0',
    'SARECOMP_VULKAN_STATE_CACHE':'1' if a.state_cache=='on' else '0',
    'SARECOMP_MESH_SHARED_CORNERS':'1' if a.shared_corners=='on' else '0',
    'SARECOMP_INDEXED_CORNERS_VERIFY':'1' if a.verify_corners else '0',
    'SARECOMP_GAMEPLAY_MATH_RETAINED':'1' if a.gameplay_math=='retained' else '0',
    'SARECOMP_NATIVE_MATH_GAMEPLAY_ONLY':str(int(a.math_scope=='gameplay')),
    'SARECOMP_NATIVE_ANIMATION_HIERARCHY':str(int(a.native_animation=='on')),
    'SARECOMP_NATIVE_MODEL_PACKETS':str(int(a.native_model_packets!='off' or a.native_model_vertex_stream=='on')),
    'SARECOMP_NATIVE_MODEL_PACKETS_VERIFY':str(int(a.native_model_packets=='verify')),
    'SARECOMP_NATIVE_MODEL_VERTEX_STREAM':str(int(a.native_model_vertex_stream=='on')),
    'SARECOMP_NATIVE_PROJECTION_BATCH':str(int(a.native_projection_batch!='off')),
    'SARECOMP_NATIVE_PROJECTION_BATCH_VERIFY':str(int(a.native_projection_batch=='verify')),
    'SARECOMP_RAM_PREPARED_ACCESS':str(int(a.ram_prepared_access=='on')),
    'SARECOMP_NATIVE_MATRIX_BULK':str(int(a.native_matrix_bulk=='on')),
    'SARECOMP_NATIVE_CLOSED_MEMORY':str(int(a.native_closed_memory=='on')),
    'SARECOMP_NATIVE_COLLISION_MEMORY':str(int(a.native_collision_memory=='on')),
    'SARECOMP_NATIVE_COLLISION_CLOSURE':str(int(a.native_collision_closure=='on')),
    'SARECOMP_NATIVE_MODEL_PIPELINE':str(int(a.native_model_pipeline=='on')),
    'SARECOMP_NATIVE_OBJECT_ACTIVATION':str(int(a.native_object_activation=='on')),
    'SARECOMP_NATIVE_MOVEMENT':str(int(a.native_movement=='on')),
    'SARECOMP_NATIVE_MOVEMENT_CONTACT':str(int(a.native_movement_contact=='on')),
    'SARECOMP_NATIVE_MODEL_SUBMISSION':str(int(a.native_model_submission=='on')),
    'SARECOMP_NATIVE_LAND_RENDER':str(int(a.native_land_render=='on')),
    'SARECOMP_NATIVE_COLLISION_WORLD':str(int(a.native_collision_world=='on')),
    'SARECOMP_NATIVE_WORLD_SDK':str(int(a.native_world_sdk=='on')),
    'SARECOMP_NATIVE_RENDER_HIERARCHY':str(int(a.native_render_hierarchy=='on')),
    'SARECOMP_NATIVE_RENDER_LOCAL_PROOFS':str(int(a.native_render_local_proofs=='on')),
    'SARECOMP_NATIVE_RENDER_ROOT_PROOFS':str(int(a.native_render_root_proofs=='on')),
    'SARECOMP_NATIVE_INVERSE_TRIG':str(int(a.native_inverse_trig=='on')),
    'SARECOMP_NATIVE_INVERSE_MEMORY':str(int(a.native_inverse_memory=='on')),
    'SARECOMP_NATIVE_RIGID_HIERARCHY':str(int(a.native_rigid_hierarchy=='on')),
    'SARECOMP_NATIVE_MORPH_HIERARCHY':str(int(a.native_morph_hierarchy=='on')),
    'SARECOMP_NATIVE_RENDER_CONTEXT':str(int(a.native_render_context=='on')),
    'SARECOMP_NATIVE_PALETTE_BATCH':str(int(a.native_palette_batch=='on')),
    'SARECOMP_ASYNC_AUDIO_STATUS':str(int(a.async_audio_status=='on')),
    'SARECOMP_SOUND_METADATA_CACHE':str(int(a.sound_metadata!='off')),
    'SARECOMP_SOUND_METADATA_VERIFY':str(int(a.sound_metadata=='verify')),
    'SARECOMP_DEFERRED_MIDI_NOTES':str(int(a.deferred_midi_notes=='on')),
    'KATANA_PORT_FINAL_PROGRESS':'1',
    'KATANA_NATIVE_PERFORMANCE_TELEMETRY':'1' if a.telemetry=='on' else '0',
    'KATANA_NATIVE_GRAPHICS_DIAGNOSTICS_MODE':'off',
    'KATANA_SONIC_PRIVATE_SCENARIO':a.scenario, 'KATANA_SONIC_GAMEPLAY_PROBE':'1',
    'KATANA_SONIC_GAMEPLAY_INPUT_PROFILE':'3', 'KATANA_SONIC_DIAGNOSTIC_MOVIE_SKIP_ONCE':'1',
    # TCG takes much longer to load than hardware. The separate gameplay
    # clock still starts only after a rendered frame from the selected stage.
    'KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS':'1200000',
})
log_path = run/'game.log'
for name, selection in (
    ('SARECOMP_NATIVE_ANIMATION_HIERARCHY', a.native_animation),
    ('SARECOMP_NATIVE_POSE_BLEND', a.native_pose),
    ('SARECOMP_NATIVE_OBJECT_ACTIVATION', a.native_object_activation),
    ('SARECOMP_NATIVE_COLLISION_WORLD', a.native_collision_world),
    ('SARECOMP_NATIVE_WORLD_SDK', a.native_world_sdk),
    ('SARECOMP_NATIVE_RENDER_HIERARCHY', a.native_render_hierarchy),
    ('SARECOMP_NATIVE_RENDER_LOCAL_PROOFS', a.native_render_local_proofs),
    ('SARECOMP_NATIVE_RENDER_ROOT_PROOFS', a.native_render_root_proofs),
    ('SARECOMP_NATIVE_INVERSE_TRIG', a.native_inverse_trig),
    ('SARECOMP_NATIVE_INVERSE_MEMORY', a.native_inverse_memory),
    ('SARECOMP_NATIVE_RIGID_HIERARCHY', a.native_rigid_hierarchy),
    ('SARECOMP_NATIVE_MORPH_HIERARCHY', a.native_morph_hierarchy),
    ('SARECOMP_NATIVE_MODEL_PIPELINE', a.native_model_pipeline),
    ('SARECOMP_NATIVE_PROJECTION_BATCH', a.native_projection_batch),
    ('SARECOMP_NATIVE_CLOSED_MEMORY', a.native_closed_memory),
    ('SARECOMP_NATIVE_RENDER_CONTEXT', a.native_render_context),
    ('SARECOMP_NATIVE_PALETTE_BATCH', a.native_palette_batch),
    ('SARECOMP_NATIVE_COLLISION_ALL_SCENES', a.collision_scope),
    ('SARECOMP_ASYNC_AUDIO_STATUS', a.async_audio_status),
):
    if selection == 'installed': env.pop(name, None)
if a.diagnostics=='installed':env.pop('SARECOMP_INTERNAL_DIAGNOSTICS',None)
if a.transfer_plans=='installed':env.pop('SARECOMP_PREPARED_TRANSFERS',None)
if a.ram_regions=='installed':env.pop('SARECOMP_RAM_REGIONS',None)
perf = None
perf_log = None
if a.end_frame: env.update(SARECOMP_PROBE_BEGIN_FRAME=str(a.begin_frame), SARECOMP_PROBE_END_FRAME=str(a.end_frame))
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
            samples = sample_rows(text)
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
samples = sample_rows(text)
stop_reason = json.loads(frontiers[-1]).get('stop_reason') if frontiers else None
# NativePortStopReason::HostDeadline is 2; a completed probe alone must not
# hide a later failure during shutdown.
expected_stop = game.returncode == 1 and stop_reason == 2 and not forced
transfer_checks = re.findall(r'^SONIC_PREPARED_TRANSFERS verified=(\d+) misses=(\d+)$', text, re.MULTILINE)
transfer_verification = ({'verified':int(transfer_checks[-1][0]), 'misses':int(transfer_checks[-1][1])}
                         if transfer_checks else None)
transfer_verification_valid = (a.transfer_plans != 'verify' or
                               bool(transfer_verification and transfer_verification['verified'] > 0))
measured_samples = samples
frame_window_valid = True
if a.end_frame:
    measured_samples=[s for s in samples if a.begin_frame <= int(s['relative_frame']) <= a.end_frame]
    frame_window_valid=(len(measured_samples)>1 and int(measured_samples[0]['relative_frame'])==a.begin_frame
        and int(measured_samples[-1]['relative_frame'])==a.end_frame
        and all(int(s.get('frame_window_begin','0'))==a.begin_frame
                and int(s.get('frame_window_end','0'))==a.end_frame for s in measured_samples))
result = {'exit_code':game.returncode, 'forced_stop':forced, 'profile':a.profile, 'callgraph':a.callgraph,
          'host_cpu_count':os.cpu_count(), 'software_raster_threads':env.get('LP_NUM_THREADS'),
          'exe':str(exe), 'exe_sha256':exe_sha256, 'scenario':a.scenario, 'aspect':a.aspect,
          'gameplay_timing':a.gameplay_timing, 'gameplay_math':a.gameplay_math,
          'math_scope':a.math_scope, 'phase':a.phase,
          'native_animation':a.native_animation,
          'native_model_packets':a.native_model_packets,
          'native_model_vertex_stream':a.native_model_vertex_stream,
          'native_projection_batch':a.native_projection_batch,
          'ram_prepared_access':a.ram_prepared_access,
          'native_matrix_bulk':a.native_matrix_bulk,
          'native_closed_memory':a.native_closed_memory,
          'native_collision_memory':a.native_collision_memory,
          'native_collision_closure':a.native_collision_closure,
          'native_model_pipeline':a.native_model_pipeline,
          'native_movement':a.native_movement,
          'native_movement_contact':a.native_movement_contact,
          'native_model_submission':a.native_model_submission,
          'native_land_render':a.native_land_render,
          'native_collision_world':a.native_collision_world,
          'native_world_sdk':a.native_world_sdk,
          'native_render_hierarchy':a.native_render_hierarchy,
          'native_render_local_proofs':a.native_render_local_proofs,
          'native_render_root_proofs':a.native_render_root_proofs,
          'native_inverse_trig':a.native_inverse_trig,
          'native_inverse_memory':a.native_inverse_memory,
          'native_rigid_hierarchy':a.native_rigid_hierarchy,
          'native_morph_hierarchy':a.native_morph_hierarchy,
          'native_object_activation':a.native_object_activation,
          'native_render_context':a.native_render_context,
          'native_palette_batch':a.native_palette_batch,
          'collision_scope':a.collision_scope,
          'async_audio_status':a.async_audio_status,
          'sound_metadata':a.sound_metadata,'deferred_midi_notes':a.deferred_midi_notes,
          'native_pose':a.native_pose,
          'diagnostics':a.diagnostics,
          'transfer_plans':a.transfer_plans,
          'scalar_writes':a.scalar_writes,
          'stack_frames':a.stack_frames,
          'fpu_register_cache':a.fpu_register_cache,
          'ram_regions':a.ram_regions,
          'telemetry':a.telemetry, 'transfer_verification':transfer_verification,
          'transfer_verification_valid':transfer_verification_valid,
          'descriptor_cache':a.descriptor_cache,
          'state_cache':a.state_cache,
          'shared_corners':a.shared_corners, 'verify_corners':a.verify_corners,
          'begin_frame':a.begin_frame, 'end_frame':a.end_frame, 'frame_window_valid':frame_window_valid,
          'measurement':measurement(measured_samples),
          'wall_seconds':time.monotonic()-start, 'samples':samples,
          'completed':'SONIC_NATIVE_SCENARIO_GAMEPLAY_COMPLETE ' in text,
          'expected_stop':expected_stop, 'stop_reason':stop_reason,
          'perf_exit_code':perf.returncode if perf else None,
          'configuration':display.read_text()}
(run/'summary.json').write_text(json.dumps(result, indent=2)+'\n')
print('SONIC_LINUX_PROBE_FINISHED ' + json.dumps({k:v for k,v in result.items()
      if k not in ('samples','configuration')}), flush=True)
raise SystemExit(0 if result['completed'] and expected_stop and frame_window_valid and transfer_verification_valid else 1)
