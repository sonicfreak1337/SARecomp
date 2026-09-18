"""Verify the installed update and normal-start policy, without FPS claims."""
from pathlib import Path
import hashlib,json,re

root=Path(__file__).resolve().parents[1]
def read(name):return json.loads((root/name).read_text())
def digest(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

meta=read('out/native-gameplay-update-20260918/validation.run.json')
install=read('runs/native-gameplay-installation-20260918.json')
assert install['passed'] and install['all_three_supported_bases']
assert install['same_program_inode'] and install['policy_preserved']
assert install['saves_chao_settings_preserved'] and install['idempotent']
assert install['diagnostic_switches_passed'] and install['original_installations_unchanged']
assert install['package_sha256']==meta['sha256']
group_flags=('movement_contact','model_submission','land_render','object_contact','camera_operation','actor_operation','player_operation')
counter_names=('contact_family_calls','object_contact_calls','camera_operation_calls','land_render_calls','actor_operation_calls','player_operation_calls','render_model_calls')
zero_names=('contact_family_resumes','world_resumes','render_hierarchy_resumes','render_model_revocations')
records=[]
for platform in ('windows','linux'):
    for mode in ('original','recompiled'):
        relative=(f'runs/native-gameplay-release-windows-{mode}-20260918/result.json' if platform=='windows' else f'runs/native-gameplay-installed-{mode}-20260918/summary.json')
        d=read(relative)
        assert d['completed'] and d['frame_window_valid'] and d['exit_code']==1 and d['stop_reason']==2
        assert not d.get('forced',d.get('forced_stop',True))
        assert all(d['native_'+key]=='installed' for key in group_flags)
        if platform=='windows':
            assert d['passed'] and d['hidden'] and d['muted'] and d['isolated_input_confirmed']
        else:
            assert d['expected_stop'] and d['diagnostics']=='installed' and d['exe_sha256']==meta['target_sha256']
            assert '/native-gameplay-install-20260918/' in d['exe']
        rows=d['gameplay_samples'] if platform=='windows' else d['samples']
        rows=[r for r in rows if 5<=int(r['relative_frame'])<=25]
        first,last=rows[0],rows[-1]
        assert int(first['relative_frame'])==5 and int(last['relative_frame'])==25
        cadence=('50','2','2') if mode=='original' else ('60','1','1')
        assert all(tuple(r[k] for k in ('active_video_hz','release_slots','logical_delta'))==cadence for r in rows)
        # Nested model submissions borrow the render operation; the standalone
        # model-root counter is not incremented on that shared path.
        counts={k:int(last[k])-int(first[k]) for k in counter_names+zero_names+('model_pipeline_root_operations',)}
        assert all(counts[k]>0 for k in counter_names),counts
        assert all(counts[k]==0 for k in zero_names),counts
        records.append({'platform':platform,'mode':mode,'evidence':relative,'exe_sha256':d['exe_sha256'],'cadence':cadence,'normal_start_groups':True,'counts':counts})
programs=[]
for name in ('build-linux/game','out/native-gameplay-update-20260918/game','out/model-submission-windows-20260918/game.exe'):
    path=root/name
    programs.append({'file':name,'bytes':path.stat().st_size,'sha256':digest(path)})
assert programs[1]['sha256']==meta['target_sha256']
assert all(r['exe_sha256']==programs[2]['sha256'] for r in records if r['platform']=='windows')
component_block=re.search(r'set\(components\s+(.*?)\)',(root/'cmake/AdapterIdentity.cmake').read_text(),re.S).group(1)
components=[(root/'src'/name).resolve() for name in component_block.split()]
source_files=[{'path':p.relative_to(root).as_posix(),'sha256':digest(p)} for p in components]
source_identity=hashlib.sha256(':'.join(p['sha256'] for p in source_files).encode()).hexdigest()
for build in ('build-linux','build-performance'):
    assert 'sha256:'+source_identity in (root/build/'generated/native_provider_identity.hpp').read_text()
report={'passed':True,'purpose':'installed functional and normal-start policy checks; not a performance comparison','patch_sha256':meta['sha256'],'programs':programs,'runs':records,'adapter_source_identity':source_identity,'source_files':source_files}
(root/'runs/native-gameplay-release-verification-20260918.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':True,'modes_checked':len(records),'patch_sha256':meta['sha256']}))
