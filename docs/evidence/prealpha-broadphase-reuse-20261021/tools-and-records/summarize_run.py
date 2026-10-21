#!/usr/bin/env python3
"""Offline fixed/frame association for the ordinary stress driver; no workloads."""
import argparse
import collections
import csv
import json
import math
from pathlib import Path
import statistics


def read(path):
    with path.open() as stream:return list(csv.DictReader(stream))


def stats(values):
    if not values:return {'n':0}
    a=sorted(values);p=(len(a)-1)*.95;lo=math.floor(p);hi=min(len(a)-1,lo+1)
    return {'n':len(a),'mean':statistics.mean(a),'median':statistics.median(a),
            'p95':a[lo]+(a[hi]-a[lo])*(p-lo),'worst':a[-1]}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory',type=Path,help='Run root containing live/ or CSV files directly')
    p.add_argument('--output',type=Path)
    p.add_argument('--start',type=int,default=1)
    p.add_argument('--end',type=int,default=10000000)
    p.add_argument('--late-wall-ms',type=float,default=10000)
    a=p.parse_args();folder=a.directory/'live' if (a.directory/'live/steps.csv').is_file() else a.directory
    steps,frames=read(folder/'steps.csv'),read(folder/'frames.csv')
    selected=[row for row in steps if a.start<=int(row['step'])<=a.end]
    byframe=collections.defaultdict(list)
    for row in steps:byframe[int(row['frame'])].append(row)
    frameindex={int(row['frame']):row for row in frames}
    associations=[]
    for row in frames:
        number=int(row['frame']);fixed=byframe[number]
        next_frame=frameindex.get(number+1)
        physics=sum(float(step['fixed_ms']) for step in fixed)
        narrow_physics=sum(float(step['total_physics_ms']) for step in fixed)
        associations.append({'frame':number,'fixed_steps_field':int(row['fixed_steps']),
            'actual_observed_fixed_steps':len(fixed),'step_first':int(fixed[0]['step']) if fixed else None,
            'step_last':int(fixed[-1]['step']) if fixed else None,'summed_fixed_ms':physics,
            'summed_rigid_physics_ms':narrow_physics,'work_before_swap_ms':float(row['work_before_swap_ms']),
            'pre_swap_non_fixed_ms':float(row['work_before_swap_ms'])-physics,
            'next_start_interval_ms':float(next_frame['interval_ms']) if next_frame else None,
            'frame_end_wall_ms':float(row['wall_ms'])})
    late=[row for row in associations if row['frame_end_wall_ms']>a.late_wall_ms and row['next_start_interval_ms'] is not None]
    windows={}
    for lo,hi in ((1,300),(301,600),(601,900),(901,1800),(1801,10000000),(a.start,a.end)):
        chunk=[row for row in steps if lo<=int(row['step'])<=hi]
        if chunk:
            windows[f'{lo}-{hi}']={'fixed_ms':stats([float(row['fixed_ms']) for row in chunk]),
                'rigid_physics_ms':stats([float(row['total_physics_ms']) for row in chunk]),
                'impact_events':sum(int(row['impact_events']) for row in chunk),
                'impact_queries':sum(int(row['impact_queries']) for row in chunk),
                'nonfinite_sum':sum(int(row['nonfinite_fields']) for row in chunk)}
    receipt=a.directory/'AS_RUN.json'
    context=json.loads(receipt.read_text()) if receipt.is_file() else {}
    report={'schema':'judas.fixed-frame-association.v1','source_directory':str(folder),
        'mode':context.get('mode','unknown'),'steps':len(steps),'frames':len(frames),'windows':windows,
        'selected_fixed_ms':stats([float(row['fixed_ms']) for row in selected]),
        'body_counts':sorted(set(int(row['bodies']) for row in steps)),
        'frame_step_count_mismatches':sum(row['fixed_steps_field']!=row['actual_observed_fixed_steps'] for row in associations),
        'late_frames':len(late),'late_mean_fixed_steps':statistics.mean(row['actual_observed_fixed_steps'] for row in late) if late else None,
        'late_start_interval_ms':stats([row['next_start_interval_ms'] for row in late]),
        'late_work_before_swap_ms':stats([row['work_before_swap_ms'] for row in late]),
        'late_summed_fixed_ms':stats([row['summed_fixed_ms'] for row in late]),
        'late_pre_swap_non_fixed_ms':stats([row['pre_swap_non_fixed_ms'] for row in late]),
        'late_frames_with_8_fixed_steps':sum(row['actual_observed_fixed_steps']==8 for row in late),
        'associations':associations,
        'limitations':['interval_ms on frame n measures time since the start marker in frame n-1; it belongs to the preceding work interval, not current frame n fixed steps.',
            'Use summed per-step fixed_ms/total_physics_ms; the frame CSV last-fixed/stat fields are only the final fixed step, never the sum.',
            'pre_swap_non_fixed_ms includes game/presentation, CSV/digest observer and harness overhead; it is not a pure renderer measurement.',
            'next-start interval includes swap/outer services and the next beforeFrame work before its marker. Optional snapshot capture perturbs this interval.',
            'Only real live35 cadence may support FPS interpretation; synthetic900/1800 frame intervals are throughput, not FPS.',
            'Eight fixed steps signals the configured catch-up maximum, not necessarily nonzero discarded backlog; exact cap/discard fields require profiler snapshots.']}
    if context.get('mode')=='live35' and late:
        report['late_fps_from_mean_start_interval']=1000/statistics.mean(row['next_start_interval_ms'] for row in late)
    if a.output:
        if a.output.exists():raise SystemExit('Refuse overwrite existing summary')
        a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({key:value for key,value in report.items() if key!='associations'},indent=2))


if __name__=='__main__':main()
