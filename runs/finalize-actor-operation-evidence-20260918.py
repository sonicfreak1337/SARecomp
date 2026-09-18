from pathlib import Path
import json,hashlib
files=[
'runs/actor-operation-executables-20260918.json',
'runs/actor-operation-allocated-sections-20260918.json',
'runs/actor-operation-unchanged-generated-20260918.json',
'runs/actor-operation-gameplay-comparison-windows-20260918.json',
'runs/actor-operation-gamma-comparison-linux-20260918.json',
'runs/build-actor-operation-windows-game-20260918.log',
'runs/build-actor-operation-linux-game-20260918.log',
'runs/test-actor-operation-windows-20260918.log',
'runs/test-actor-operation-linux-20260918.log',
'runs/test-actor-render-regression-windows-20260918.log',
'runs/compare-actor-operation-windows-20260918.py',
'runs/compare-actor-operation-linux-20260918.py',
'runs/inspect-player-local-dispatch-20260918.py',
'runs/player-local-dispatch-20260918.json']
for side in ['off','on']:
 files.extend(f'runs/actor-operation-windows-{side}-20260918/{name}' for name in ['result.json','stderr.log'])
 files.extend(f'runs/actor-operation-gamma-linux-{side}-20260918/{name}' for name in ['summary.json','game.log','sonic-display.ini'])
for scene in ['lost','sky']:
 d=f'runs/actor-operation-{scene}-linux-on-20260918'
 s=json.loads(Path(d,'summary.json').read_text())
 assert s['completed'] and s['expected_stop'] and not s['forced_stop'] and s['frame_window_valid']
 files.extend(f'{d}/{name}' for name in ['summary.json','game.log','sonic-display.ini'])
for name in files:
 p=Path(name);s=p.read_text(encoding='utf-8-sig')
 p.write_bytes(('\n'.join(x.rstrip() for x in s.splitlines()).rstrip()+'\n').encode())
source=['src/sonic_render_hierarchy.hpp','src/sonic_render_hierarchy.cpp','src/native_title_adapter.cpp','tools/prepare-render-hierarchy.py','tools/prepare-render-hierarchy-bridge.py','tools/actor-operation-owners.json','tools/test_actor_operation.cpp','tools/test_render_hierarchy_aot.cpp','cmake/SonicRenderHierarchy.cmake','cmake/SonicRenderHierarchyBridge.cmake','cmake/SonicMovementContact.cmake','cmake/AdapterIdentity.cmake','tools/benchmark-stage.py','tools/benchmark-linux-stage.py']
manifest={n:{'bytes':Path(n).stat().st_size,'sha256':hashlib.sha256(Path(n).read_bytes()).hexdigest()} for n in source+files}
Path('runs/actor-operation-evidence-manifest-20260918.json').write_text(json.dumps(manifest,indent=2)+'\n',newline='\n')
files.extend(['runs/actor-operation-evidence-manifest-20260918.json','runs/finalize-actor-operation-evidence-20260918.py'])
Path('runs/actor-operation-evidence-files-20260918.txt').write_text('\n'.join(files)+'\n',newline='\n')
print('ACTOR_EVIDENCE_READY',len(files),'files',sum(Path(p).stat().st_size for p in files),'bytes')
