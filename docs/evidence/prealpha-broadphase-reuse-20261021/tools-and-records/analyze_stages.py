#!/usr/bin/env python3
"""Offline existing-profiler analysis; no engine/build/process under test."""
import argparse
import bisect
import collections
import csv
import hashlib
import json
from pathlib import Path


def read(path):
    with path.open() as stream:return list(csv.DictReader(stream))


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--start',type=int,default=500)
    parser.add_argument('--end',type=int,default=900)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise SystemExit('Fresh analysis output required')
    folder=args.run/'live';step_rows=read(folder/'steps.csv')
    all_byframe=collections.defaultdict(list)
    for row in step_rows:all_byframe[int(row['frame'])].append(row)
    chosen=[row for row in step_rows if args.start<=int(row['step'])<=args.end]
    chosen_ids={int(row['step']) for row in chosen}
    chosen_frames={int(row['frame']) for row in chosen}
    frames={};intervals=[]
    for line in (folder/'stage-frames.jsonl').open():
        frame=json.loads(line);number=frame['benchmark_frame']
        if number in frames:raise RuntimeError('Duplicate published benchmark frame')
        frames[number]=frame
    for number in sorted(chosen_frames):
        frame=frames[number];steps=all_byframe[number]
        if len(frame['fixed_steps'])!=len(steps):raise RuntimeError('Step/snapshot count mismatch in '+str(number))
        for public,physical in zip(frame['fixed_steps'],steps):
            if int(physical['step']) in chosen_ids:intervals.append((public['start_ns'],public['end_ns']))
    intervals.sort();starts=[a for a,b in intervals]

    def overlap(a,b):
        total=0;index=max(0,bisect.bisect_right(starts,a)-1)
        while index<len(intervals) and intervals[index][0]<b:
            lo,hi=intervals[index];total+=max(0,min(hi,b)-max(lo,a));index+=1
        return total

    def contained(a,b):
        index=bisect.bisect_right(starts,a)-1
        return index>=0 and intervals[index][0]<=a and b<=intervals[index][1]

    scope_totals=collections.defaultdict(lambda:{'count':0,'inclusive_ns':0,'reported_exclusive_ns':0,
        'elapsed_inside_selected_fixed_intervals_ns':0,'fully_inside_fixed_reported_exclusive_ns':0,
        'fully_inside_fixed_count':0,'cross_origin_count':0})
    parent_totals=collections.defaultdict(lambda:{'count':0,'inclusive_ns':0,'reported_exclusive_ns':0})
    rows=[];counters=collections.defaultdict(list);diagnostics=collections.defaultdict(int)
    worker_names={};cross=0
    for number in sorted(chosen_frames):
        frame=frames[number]
        for key,value in frame['diagnostics'].items():diagnostics[key]=max(diagnostics[key],value)
        worker_names.update({thread['id']:thread['name'] for thread in frame['threads']})
        names={scope['node']:scope['name'] for scope in frame['scopes']}
        for scope in frame['scopes']:
            role='main' if scope['thread']==frame['main_thread'] else 'worker'
            key=(role,scope['name'],bool(scope['wait']));target=scope_totals[key]
            lo,hi=scope['start_ns'],scope['end_ns'];inside=contained(lo,hi)
            target['count']+=1;target['inclusive_ns']+=hi-lo
            target['reported_exclusive_ns']+=scope['exclusive_ns']
            target['elapsed_inside_selected_fixed_intervals_ns']+=overlap(lo,hi)
            if inside:
                target['fully_inside_fixed_reported_exclusive_ns']+=scope['exclusive_ns']
                target['fully_inside_fixed_count']+=1
            if scope['origin_frame']!=frame['profile_frame']:target['cross_origin_count']+=1;cross+=1
            if scope['name']=='Physics candidate generation':
                parent=names.get(scope['parent'],'<unpublished parent node '+str(scope['parent'])+'>')
                x=parent_totals[parent];x['count']+=1;x['inclusive_ns']+=hi-lo;x['reported_exclusive_ns']+=scope['exclusive_ns']
            rows.append({'benchmark_frame':number,'profile_frame':frame['profile_frame'],
                'role':role,'thread_name':worker_names.get(scope['thread'],'<unnamed>'),
                'fully_inside_selected_fixed_interval':inside,
                'elapsed_inside_selected_fixed_intervals_ns':overlap(lo,hi),**scope})
        for counter in frame['counters']:
            counters[(counter['name'],counter['mode'])].append(counter['value'])
    n=len(chosen)
    summary=[]
    for (role,name,wait),values in scope_totals.items():
        summary.append({'role':role,'name':name,'wait':wait,**values,
                        'inclusive_elapsed_ms_per_selected_step':values['inclusive_ns']/1e6/n,
                        'reported_exclusive_elapsed_ms_per_selected_step':values['reported_exclusive_ns']/1e6/n,
                        'elapsed_inside_selected_fixed_intervals_ms_per_step':values['elapsed_inside_selected_fixed_intervals_ns']/1e6/n})
    summary.sort(key=lambda row:-row['inclusive_ns'])
    report={'schema':'judas.existing-profiler-stage-attribution.v1','selected_step_range':[args.start,args.end],
        'steps':n,'frames':len(chosen_frames),'source':str(folder),
        'input_sha256':{name:sha(folder/name) for name in ('steps.csv','stage-frames.jsonl','stage-metadata.json')},
        'selected_fixed_elapsed_ms':sum(hi-lo for lo,hi in intervals)/1e6,
        'diagnostics_max':dict(diagnostics),'incomplete_frames':sum(frames[x]['incomplete'] for x in chosen_frames),
        'cross_origin_scope_records':cross,'threads':worker_names,'scopes':summary,
        'candidate_generation_by_parent':{name:{**value,'elapsed_ms_per_step':value['inclusive_ns']/1e6/n}
                                          for name,value in parent_totals.items()},
        'counters':[{ 'name':key[0],'mode':key[1],'frames':len(values),'mean':sum(values)/len(values),
                     'minimum':min(values),'maximum':max(values),
                     'sum_only_valid_for_mode0':sum(values) if key[1]==0 else None}
                    for key,values in sorted(counters.items())],
        'limitations':['Scope times are elapsed, not OS-thread CPU. Inclusive scopes and worker lanes overlap and must not be summed into a main-thread percentage.',
            'Worker queue wait scopes can start in old frames and end here; full recorded wait spans are retained separately from interval intersections, never labelled CPU.',
            'Reported exclusive time is valid for complete recorded scopes; partial overlaps do not permit proportional clipping of exclusive time. Fully-contained scope counts/values are therefore explicit.',
            'Fixed interval intersections exclude between-step rendering but cannot recover missing uninstrumented child CPU or overlapping asynchronous child scopes from other publications.',
            'Counter mode0 is frame-sum,1 latest,2 maximum. Latest/max observations must not be added and presented as totals.',
            'This diagnostic capture perturbs frame cadence. Compare normal profiler-off repeats for performance; raw scope magnitudes describe this capture only.',
            'Open-scope count and all drop/incomplete counters are preserved; ongoing worker waits are not silently called complete work.']}
    args.output.mkdir(parents=True)
    (args.output/'SUMMARY.json').write_text(json.dumps(report,indent=2)+'\n')
    with (args.output/'scope_rows.csv').open('w') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    print(json.dumps({'output':str(args.output),'steps':n,'candidate_generation_by_parent':report['candidate_generation_by_parent'],
                      'main_physics_scopes':[row for row in summary if row['role']=='main' and ('Physics' in row['name'] or row['name']=='Rigid physics')],
                      'diagnostics':report['diagnostics_max']},indent=2))


if __name__=='__main__':main()
