import json,sys
from pathlib import Path
ref=json.loads(Path('runs/inverse-trig-gamma-emerald-coast-comparison-linux-20260918-f.json').read_text())
a,b=map(lambda p:json.loads(Path(p).read_text()),sys.argv[1:3])
fields=ref['fields']; ends={}
for f in (a['begin_frame'],a['end_frame']):
 x=next(s for s in a['samples'] if int(s['relative_frame'])==f)
 y=next(s for s in b['samples'] if int(s['relative_frame'])==f)
 ends[str(f)]={k:[x.get(k),y.get(k)] for k in fields if x.get(k)!=y.get(k)}
configkeys=['scenario','aspect','phase','gameplay_timing','host_cpu_count','software_raster_threads','begin_frame','end_frame','diagnostics','transfer_plans','ram_regions','telemetry']
config={k:[a.get(k),b.get(k)] for k in configkeys if a.get(k)!=b.get(k)}
metrics=('execution_cpu_ms_per_game_update','process_cpu_ms_per_game_update','new_frames_per_second')
counters={}
for label,data in [('off',a),('on',b)]:
 x=next(s for s in data['samples'] if int(s['relative_frame'])==a['begin_frame'])
 y=next(s for s in data['samples'] if int(s['relative_frame'])==a['end_frame'])
 counters[label]={k:int(y[k])-int(x[k]) for k in y if any(w in k for w in ('render_hierarchy','render_model','rigid_hierarchy','morph_hierarchy','inverse_trig','inverse_memory','collision_world','contact_family','model_pipeline','land_render')) and y[k].isdigit() and x.get(k,'').isdigit()}
r={'schema':'sarecomp-native-land-model-contact-comparison-v1','scenario':a['scenario'],'exe_sha256':[a['exe_sha256'],b['exe_sha256']],'fields':fields,'off':a['measurement'],'on':b['measurement'],'endpoints':ends,'configuration_differences':config,'group_configuration':{label:{k:d.get(k) for k in ('native_movement_contact','native_model_submission','native_land_render')} for label,d in [('off',a),('on',b)]},'percent':{k:100*(b['measurement'][k]/a['measurement'][k]-1) for k in metrics},'window_counters':counters,'matched_work':a['exe_sha256']==b['exe_sha256'] and not a.get('forced_stop',False) and not b.get('forced_stop',False) and not any(ends.values()) and not config and a['completed'] and b['completed'] and a['frame_window_valid'] and b['frame_window_valid'] and a['measurement']['game_updates']==b['measurement']['game_updates']}
r['endpoint_values']={label:{str(f):{k:next(s for s in data['samples'] if int(s['relative_frame'])==f).get(k) for k in fields} for f in (a['begin_frame'],a['end_frame'])} for label,data in [('off',a),('on',b)]}
r['performance_qualified']=r['matched_work']
r['performance_exclusion']=None if r['matched_work'] else 'Selected workload/configuration endpoints differ or a run is incomplete.'
Path(sys.argv[3]).write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:r[k] for k in ('scenario','matched_work','percent','endpoints','configuration_differences')},indent=2))
