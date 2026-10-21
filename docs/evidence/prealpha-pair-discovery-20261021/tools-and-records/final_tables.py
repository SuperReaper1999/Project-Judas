#!/usr/bin/env python3
"""Offline matched normal-run tables, strict wall windows and exact digest prefixes.

No builds/workloads. Sampled/profile-on/capture runs are labelled diagnostic and
excluded from direct performance comparisons. Original raw files stay untouched.
"""
import argparse
import collections
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics

FIELDS=('trajectory_digest','bodies','awake','sleeping','candidates','contacts','impact_events',
        'impact_queries','impact_iterations','impact_peak','max_linear_speed','max_angular_speed','nonfinite_fields')


def read(path):
    with path.open() as stream:return list(csv.DictReader(stream))


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def stats(values):
    if not values:return {'n':0}
    a=sorted(values);p=(len(a)-1)*.95;lo=math.floor(p);hi=min(len(a)-1,lo+1)
    return {'n':len(a),'mean':statistics.mean(a),'median':statistics.median(a),
            'p95':a[lo]+(a[hi]-a[lo])*(p-lo),'worst':a[-1]}


def unique_rows(rows,key):
    out={}
    for row in rows:
        value=int(row[key])
        if value in out:raise RuntimeError('Duplicate '+key+' identity')
        out[value]=row
    return out


def load_run(label,root):
    folder=root/'live' if (root/'live/steps.csv').is_file() else root
    receipt=json.loads((root/'AS_RUN.json').read_text())
    steps,frames,begins=(read(folder/name) for name in ('steps.csv','frames.csv','begins.csv'))
    stepindex=unique_rows(steps,'step');frameindex=unique_rows(frames,'frame');beginindex=unique_rows(begins,'frame')
    if sorted(stepindex)!=list(range(1,len(steps)+1)):raise RuntimeError('Nonsequential fixed steps in '+label)
    byframe=collections.defaultdict(list)
    for row in steps:
        for field in FIELDS:
            if field not in row:raise RuntimeError('Missing physical/work field: '+field)
        byframe[int(row['frame'])].append(row)
    # Both explicit receipt flags and observed files/actual arguments classify
    # diagnostic runs. Missing metadata is not silently called profile-off.
    sampler=folder/'sampling/metadata.json'
    sampled=bool(receipt.get('sample'))
    if sampler.is_file():sampled|=bool(json.loads(sampler.read_text()).get('started'))
    argv=receipt.get('argv',[])
    profile=receipt.get('engine_profiler')
    if len(argv)>=12:profile=bool(profile) or argv[-3]!='0'
    stage=(folder/'stage-frames.jsonl').is_file() or bool(receipt.get('stage_capture'))
    normal=not sampled and profile is False and not stage and all(receipt.get(key) is True for key in ['source_inputs_unchanged','binary_unchanged','core_unchanged','fixture_unchanged','native_nvidia_confirmed'])
    associations=[]
    for frame_number,frame in sorted(frameindex.items()):
        own=byframe[frame_number]
        if int(frame['fixed_steps'])!=len(own):raise RuntimeError('Frame/step count mismatch '+label+'/'+str(frame_number))
        if frame_number not in beginindex:raise RuntimeError('Missing begin marker '+label+'/'+str(frame_number))
        following=beginindex.get(frame_number+1)
        start=float(beginindex[frame_number]['wall_ms'])
        end=float(following['wall_ms']) if following else None
        duration=end-start if end is not None else None
        recorded_interval=float(following['interval_ms']) if following else None
        # Same steady-clock sample is used for both markers; floating output
        # precision can lose <1 microsecond on subtraction of long offsets.
        if duration is not None and abs(duration-recorded_interval)>.002:
            raise RuntimeError('Begin marker interval mismatch '+label+'/'+str(frame_number))
        fixed=sum(float(row['fixed_ms']) for row in own)
        associations.append({'frame':frame_number,'begin_wall_ms':start,'next_begin_wall_ms':end,
            'interval_ms':duration,'reported_next_interval_ms':recorded_interval,
            'end_before_swap_wall_ms':float(frame['wall_ms']),
            'observed_fixed_steps':len(own),'first_step':int(own[0]['step']) if own else None,
            'last_step':int(own[-1]['step']) if own else None,'summed_fixed_ms':fixed,
            'summed_rigid_physics_ms':sum(float(row['total_physics_ms']) for row in own),
            'work_before_swap_ms':float(frame['work_before_swap_ms']),
            'pre_swap_nonfixed_ms':float(frame['work_before_swap_ms'])-fixed})
    stop=max(float(row['wall_ms']) for row in frames)
    wall=[]
    for lo,hi in ((0.,min(10000.,stop)),(10000.,min(20000.,stop)),(20000.,stop)):
        if hi<=lo:continue
        completed=[row for row in associations if row['next_begin_wall_ms'] is not None and
                   lo<=row['next_begin_wall_ms']<hi]
        contributing=[row for row in associations if row['next_begin_wall_ms'] is not None and
                      max(lo,row['begin_wall_ms'])<min(hi,row['next_begin_wall_ms'])]
        observed=[row for row in steps if lo<=float(row['wall_ms'])<hi]
        clipped=sum(min(hi,row['next_begin_wall_ms'])-max(lo,row['begin_wall_ms']) for row in contributing)
        fractional=sum((min(hi,row['next_begin_wall_ms'])-max(lo,row['begin_wall_ms']))/row['interval_ms']
                       for row in contributing if row['interval_ms']>0)
        item={'window_start_ms':lo,'window_end_ms':hi,'window_duration_ms':hi-lo,
            'fully_measured_marker_intervals_completed':len(completed),
            'measured_marker_interval_coverage_ms':clipped,
            'fractional_interval_equivalents_by_exact_wall_overlap':fractional,
            'completed_marker_interval_ms':stats([row['interval_ms'] for row in completed]),
            'frames_with_eight_fixed_steps':sum(row['observed_fixed_steps']==8 for row in completed),
            'mean_observed_steps_per_completed_interval':statistics.mean(row['observed_fixed_steps'] for row in completed) if completed else None,
            'fixed_steps_observed_completed_in_window':len(observed),
            'fixed_ms_by_observation_completion':sum(float(row['fixed_ms']) for row in observed),
            'fixed_ms_of_steps_observed_completed':stats([float(row['fixed_ms']) for row in observed]),
            'impact_queries_by_observation_completion':sum(int(row['impact_queries']) for row in observed),
            'impact_events_by_observation_completion':sum(int(row['impact_events']) for row in observed),
            'pre_swap_nonfixed_ms_for_completed_intervals':stats([row['pre_swap_nonfixed_ms'] for row in completed])}
        if receipt.get('mode')=='live35':
            item['completed_marker_intervals_per_wall_second']=len(completed)/(hi-lo)*1000
            item['fractional_marker_interval_rate_per_second']=fractional/(hi-lo)*1000
        wall.append(item)
    windows={}
    for lo,hi in ((1,300),(301,600),(601,900),(901,1800),(1,len(steps))):
        selected=[row for row in steps if lo<=int(row['step'])<=hi]
        if selected:
            windows[f'{lo}-{hi}']={'fixed_ms':stats([float(row['fixed_ms']) for row in selected]),
                'rigid_physics_ms':stats([float(row['total_physics_ms']) for row in selected]),
                'impact_queries':sum(int(row['impact_queries']) for row in selected),
                'impact_events':sum(int(row['impact_events']) for row in selected)}
    quality_path=folder/'quality.json'
    summary={'label':label,'root':str(root),'mode':receipt.get('mode'),
        'normal_performance_eligible':normal,'diagnostic_classification':{'sampled':sampled,'profile_enabled':profile,'stage_capture':stage},
        'steps':len(steps),'frames':len(frames),'stop_before_swap_wall_ms':stop,
        'body_counts':sorted({int(row['bodies']) for row in steps}),
        'nonfinite_sum':sum(int(row['nonfinite_fields']) for row in steps),
        'fixture_sha256':receipt.get('fixture_sha256'),'binary_sha256':receipt.get('binary_sha256'),
        'core_sha256':receipt.get('core_sha256'),'child_exit':receipt.get('exit'),
        'native_nvidia_confirmed':receipt.get('native_nvidia_confirmed'),
        'workload_settings':argv[3:] if len(argv)>=4 else None,
        'input_hashes':{name:sha(folder/name) for name in ('steps.csv','frames.csv','begins.csv')},
        'as_run_sha256':sha(root/'AS_RUN.json'),'fixed_windows':windows,'strict_wall_windows':wall,
        'camera':receipt.get('camera'),'quality':json.loads(quality_path.read_text()) if quality_path.is_file() else None}
    return summary,steps,associations


def compare(left,right):
    a,arows,_=left;b,brows,_=right;n=min(len(arows),len(brows));failures={}
    for field in FIELDS:
        indices=[int(x['step']) for x,y in zip(arows[:n],brows[:n]) if x[field]!=y[field]]
        if indices:failures[field]={'count':len(indices),'first_steps':indices[:20]}
    same_fixture=a['fixture_sha256']==b['fixture_sha256'] and a['fixture_sha256'] is not None
    same_settings=a['workload_settings']==b['workload_settings'] and a['workload_settings'] is not None and a.get('camera')==b.get('camera') and a.get('camera') is not None
    eligible=a['normal_performance_eligible'] and b['normal_performance_eligible'] and same_fixture and same_settings
    return {'left':a['label'],'right':b['label'],'common_steps':n,'left_steps':len(arows),'right_steps':len(brows),
        'fields':list(FIELDS),'all_common_prefix_physical_and_work_fields_exact':n>0 and not failures,
        'mismatches':failures,'fixture_match':same_fixture,'workload_settings_match':same_settings,
        'normal_direct_timing_comparison_eligible':eligible,
        'reason_if_ineligible':None if eligible else 'Diagnostic capture/sampling, mismatched workload settings/fixture, or missing provenance; keep timing results separate.'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',action='append',required=True,metavar='LABEL=PATH')
    parser.add_argument('--compare',action='append',default=[],metavar='LEFT=RIGHT')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise SystemExit('Fresh table output required')
    runs={}
    for item in args.run:
        label,separator,path=item.partition('=')
        if not separator or not label or label in runs or not all(c.isalnum() or c in '-_' for c in label):raise RuntimeError('Unique safe LABEL=PATH required')
        runs[label]=load_run(label,Path(path))
    comparisons=[]
    for item in args.compare:
        left,separator,right=item.partition('=')
        if not separator or left not in runs or right not in runs:raise RuntimeError('Unknown comparison label')
        comparisons.append(compare(runs[left],runs[right]))
    report={'schema':'judas.normal-performance-and-prefix-tables.v1','runs':{name:value[0] for name,value in runs.items()},
        'comparisons':comparisons,'limitations':[
            'Sampled, profile-on and stage-capture results are explicitly diagnostic and excluded from direct normal performance comparisons.',
            'Strict windows are [0,10s),[10s,20s),[20s,final before-swap observation). Marker intervals are intersected exactly with each wall window; no post10s catch-all replaces these windows.',
            'Marker interval belongs to the preceding frame. All fixed times are summed from that exact frame identity, never copied from last-step-only frame fields.',
            'Marker rates are benchmark start-to-start cadence, not GPU presentation timestamps; synthetic mode rates are intentionally omitted.',
            'steps.csv wall_ms is the post-digest completed-observation time. Fixed durations crossing a wall-window boundary cannot be exactly split from ordinary logs; completion-attributed durations are labelled accordingly.',
            'Fractional interval-equivalent rates describe exact window overlap, not an integer frame-present count. Last frame has no next start marker; measured coverage remains explicit.',
            'Eight steps indicates catch-up maximum, not proof of discarded backlog; profiler cap/discard fields are the authority.',
            'A digest compares actual published pose, velocities and sleep for every live body. Common-prefix equality does not certify unobserved post-prefix behaviour or universal physics correctness.']}
    args.output.mkdir(parents=True)
    for name,(_,_,rows) in runs.items():
        with (args.output/(name+'-frame-association.csv')).open('w') as stream:
            writer=csv.DictWriter(stream,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    (args.output/'TABLES.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'output':str(args.output),'runs':{name:{'steps':value[0]['steps'],
        'normal_performance_eligible':value[0]['normal_performance_eligible']} for name,value in runs.items()},'comparisons':comparisons},indent=2))


if __name__=='__main__':main()
