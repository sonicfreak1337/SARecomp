"""Optional visible EC diagnostic entry; standard60 policy and copied saves.

Normal play needs only out/experimental/game.exe. This helper intentionally
copies saves and selects EC; it is not the product's default save namespace.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from datetime import datetime

root=Path(__file__).resolve().parents[1]
busy=subprocess.run(['powershell.exe','-NoProfile','-Command',
    'if (Get-Process game,ninja,clang-cl,lld-link -ErrorAction SilentlyContinue) {exit 1}'],
    creationflags=subprocess.CREATE_NO_WINDOW,capture_output=True)
if busy.returncode:
    raise SystemExit('Another game or compiler is running; left untouched')
run=root/'runs'/('sixty-frame-manual-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
run.mkdir()
saves=run/'user-data'
display=run/'sonic-display.ini'
settings=(root/'out/experimental/sonic-display.ini').read_text(encoding='utf-8-sig')
profile_match=re.search(r'(?m)^active_profile=([^\r\n]+)',settings)
profile=profile_match.group(1) if profile_match else 'default'
if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}',profile): raise SystemExit('Invalid active save profile')
normal_root=Path(os.environ['LOCALAPPDATA'])/'SARecomp'/'experimental'
relative=Path() if profile=='default' else Path('profiles')/profile
source=normal_root/relative/'sonic-adventure-pal-v1003'/'saves'
destination=saves/relative/'sonic-adventure-pal-v1003'/'saves'
destination.mkdir(parents=True)
copied={}
for name in ('sonic-adventure-pal-v1003-vmu.save-c0-s0.ksave',
             'sonic-adventure-pal-v1003-vmu.save-c0-s0.ksave.bak'):
    original=source/name
    if original.is_file():
        before=original.read_bytes()
        (destination/name).write_bytes(before)
        if original.read_bytes()!=before or (destination/name).read_bytes()!=before:
            raise SystemExit('Save changed during isolated copy; game was not launched')
        copied[name]=hashlib.sha256(before).hexdigest()
if not copied: raise SystemExit('Current profile save not found; game was not launched')
# Match the measured renderer/resolution; retain the user's controller mappings,
# camera, language and audio preferences. The normal config is never modified.
for key,value in dict(setup_complete=1,mode='widescreen',width=1280,height=720,
        render_percent=100,renderer='d3d11',window_mode='windowed',presentation_fps=144,
        vsync=0,anisotropy=1).items():
    pattern=rf'(?m)^{re.escape(key)}=[^\r\n]*'
    row=f'{key}={value}'
    settings=re.sub(pattern,row,settings) if re.search(pattern,settings) else settings+'\n'+row+'\n'
display.write_text(settings,encoding='utf-8')
env={k:v for k,v in os.environ.items() if not k.startswith(('KATANA_','SARECOMP_'))}
env.update(KATANA_USER_DATA_ROOT=str(saves),SARECOMP_DISPLAY_CONFIG=str(display),
    KATANA_SONIC_PRIVATE_SCENARIO='emerald-coast',
    KATANA_SONIC_DIAGNOSTIC_MOVIE_SKIP_ONCE='1',SARECOMP_UPDATE_TIMING_TRACE='1',
    KATANA_PORT_FINAL_PROGRESS='1',
    KATANA_NATIVE_PERFORMANCE_TELEMETRY='1',KATANA_NATIVE_GRAPHICS_DIAGNOSTICS_MODE='off')
# No BACKGROUND_TEST, isolated input, input profile, deadline or replay flag.
# The newer contact-family experiment is left off until its live validation.
exe=root/'out/experimental/game.exe'
with exe.open('rb') as stream: sha=hashlib.file_digest(stream,'sha256').hexdigest()
with (run/'stdout.log').open('wb') as out,(run/'stderr.log').open('wb') as err:
    process=subprocess.Popen([str(exe),'--bringup-incomplete-hardware-closure',
        '--content-root',str(root/'.local/baseline/r354/native-content'),
        '--presentation-fps','144'],cwd=exe.parent,env=env,stdout=out,stderr=err,
        stdin=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
report=dict(pid=process.pid,exe=str(exe),exe_sha256=sha,visible=True,manual_input=True,
    renderer='d3d11',width=1280,height=720,output_fps=144,save_copy=str(saves),run=str(run),
    save_source=str(source),copied_save_sha256=copied,
    scope='Standard60 gameplay, EC diagnostic entry, copied saves; original script cadence',
    note='For normal play with your active profile, launch out/experimental/game.exe directly')
(run/'launch.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
