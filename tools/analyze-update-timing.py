"""Reconcile a saved private trace with the SHA-bound original loop contract."""
import argparse
import collections
import json
from pathlib import Path
import statistics
import struct

parser = argparse.ArgumentParser()
parser.add_argument('run', type=Path)
args = parser.parse_args()
result = json.loads((args.run/'result.json').read_text())
trace = json.loads((args.run/'update-timing.json').read_text())
assert result['update_timing_passed'], 'Not a complete timing witness'
events = [{k:int(v) for k,v in row.items()} for row in trace['events']]
assert len(trace['end']) == 1 and int(trace['end'][0]['unreadable']) == 0
assert [row['sequence'] for row in events] == list(range(len(events)))
assert all(a['monotonic_ns'] <= b['monotonic_ns'] for a,b in zip(events,events[1:]))
tv = {int(row['tv_mode_word']) for row in result['gameplay_samples']}
assert len(tv) == 1 and next(iter(tv)) in (0,1), 'Mixed/unknown TV-mode domain'
pal = next(iter(tv)) == 1
floating = lambda value: struct.unpack('<f',struct.pack('<I',value))[0]
groups = []
current = []
for row in events:
    current.append(row)
    if row['event'] == 4:
        groups.append(current)
        current = []
histogram = collections.Counter()
elapsed_ms = []
previous_phase = None
for group in groups:
    first = group[0]
    assert first['event'] == 0 and first['iteration'] == 0
    alternate = first['caller'] == 0x8C04E9A0
    assert alternate or first['caller'] == 0x8C04EA94
    delta,phase = first['delta'],first['phase']
    assert delta > 0
    if previous_phase is not None:
        assert phase == previous_phase, 'PAL phase discontinuity'
    index = iteration = extra = 0
    while True:
        for event,pr in ((0,0x8C04E73C),(1,0x8C04E782)):
            row = group[index]; index += 1
            assert (row['event'],row['pr'],row['caller'],row['iteration'],row['phase'],row['frame']) == (
                event,pr,first['caller'],iteration,phase,first['frame'])
            assert row['delta'] == delta
        if pal and iteration == 0:
            phase = (phase+1)&255
            if delta == 2 and phase == 3:
                extra += 1
            elif (phase if phase < 128 else phase-256) >= 5:
                extra += 1
                phase = 0
        if iteration == 1:
            row = group[index]; index += 1
            threshold = 1900.0 if alternate else 1850.0
            assert (row['event'],row['pr'],row['iteration'],row['phase']) == (
                2,0x8C04E9DE if alternate else 0x8C04EB18,1,phase)
            assert floating(row['threshold']) == threshold
            value = floating(row['value'])
            elapsed_ms.append(value/60.0)
            extra += not (threshold > value)
        iteration += 1
        if iteration >= delta+extra:
            break
        row = group[index]; index += 1
        assert (row['event'],row['pr'],row['iteration'],row['phase'],row['value']) == (
            3,0x8C04E9FE if alternate else 0x8C04EB38,iteration,phase,delta+extra-iteration)
    boundary = group[index]; index += 1
    assert index == len(group) and boundary['event'] == 4
    assert boundary['iteration'] == iteration and boundary['phase'] == phase
    assert boundary['frame'] == first['frame']+1, 'Title boundary did not follow these task passes'
    previous_phase = phase
    histogram[iteration] += 1
steady = [row for row in result['gameplay_samples'] if int(row['elapsed_ms']) >= 10000]
a,b = steady[0],steady[-1]
delta_counter = lambda key: int(b[key])-int(a[key])
assert delta_counter('update_tasks') == delta_counter('update_tails')
assert delta_counter('update_tasks') == delta_counter('update_main_tasks')+delta_counter('update_alternate_tasks')
report = {
    'schema':'sarecomp-update-contract-analysis-v1',
    'exe_sha256':result['exe_sha256'],
    'original_boot_sha256':'b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af',
    'complete_trace_wrappers':len(groups),
    'passes_histogram':dict(sorted(histogram.items())),
    'unvalidated_final_partial_events':len(current),
    'buffer_records_dropped':int(trace['end'][0]['dropped']),
    'retail_sequence_phase_threshold_waits_passed':True,
    'steady_boundaries':delta_counter('frame'),
    'steady_task_traversals':delta_counter('update_tasks'),
    'steady_setup_calls':delta_counter('update_timer_setups'),
    'steady_host_periodic_callbacks':delta_counter('periodic_callbacks'),
    'elapsed_ms_min_median_max':[min(elapsed_ms),statistics.median(elapsed_ms),max(elapsed_ms)],
    **{key:result[key] for key in ('new_draw_fps','presentation_fps',
       'task_traversals_per_second','task_traversals_per_title_boundary','elapsed_extra_fraction')},
}
(args.run/'update-contract-analysis.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
