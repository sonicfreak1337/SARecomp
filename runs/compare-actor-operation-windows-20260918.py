import hashlib,json,sys
from pathlib import Path
base=Path('runs')
fields=['main_state','scene','game_ticks','player_state_readable','player_x_bits','player_y_bits','player_z_bits','hud_timer_ticks','active_video_hz','release_slots','logical_delta','native_gameplay_math','native_leaf_math','animation_hierarchy_native_calls','animation_hierarchy_original_calls','palette_native_calls']
a=json.loads((base/'actor-operation-windows-off-20260918/result.json').read_text())
b=json.loads((base/'actor-operation-windows-on-20260918/result.json').read_text())
ends={}
for f in (5,25):
 x=next(s for s in a['gameplay_samples'] if int(s['relative_frame'])==f)
 y=next(s for s in b['gameplay_samples'] if int(s['relative_frame'])==f)
 ends[str(f)]={k:[x.get(k),y.get(k)] for k in fields if x.get(k)!=y.get(k)}
counts={}
for label,data in [('off',a),('on',b)]:
 x,y=[next(s for s in data['gameplay_samples'] if int(s['relative_frame'])==f) for f in (5,25)]
 counts[label]={k:int(y[k])-int(x[k]) for k in y if any(w in k for w in ('render_hierarchy','render_model','contact_family','model_pipeline','land_render','object_contact','camera_operation','actor_operation','actor_state')) and y[k].isdigit() and x.get(k,'').isdigit()}
paths=[base/f'actor-operation-windows-{v}-20260918/frames/frame-270.bmp' for v in ('off','on')]
sha=[hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
r={'scenario':a['scenario'],'exe_sha256':[a['exe_sha256'],b['exe_sha256']],'fields':fields,'endpoints':ends,'window_counters':counts,'image_sha256':sha,'image_byte_equal':paths[0].read_bytes()==paths[1].read_bytes(),'completed':[a['completed'],b['completed']],'performance_qualified':False,'performance_exclusion':'Exact image captures and readback were enabled.'}
r['group_configuration']={label:{k:data[k] for k in ('native_movement_contact','native_model_submission','native_land_render','native_object_contact','native_camera_operation','native_actor_operation')} for label,data in [('off',a),('on',b)]}
r['matched_work']=not any(ends.values()) and all(r['completed']) and a['frame_window_valid'] and b['frame_window_valid']
(base/'actor-operation-gameplay-comparison-windows-20260918.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({k:v for k,v in r.items() if k!='window_counters'},indent=2))
