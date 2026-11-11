#!/usr/bin/env python3
"""Summarise one captured application without summing overlapping CPU/GPU work."""
import argparse
import collections
import csv
import json
from pathlib import Path
import statistics
from performance.metrics import percentile
from performance.timeline import export_trace, frame_completion


def report(root):
    run = root / 'application'
    metadata = json.loads((run / 'run.json').read_text())
    invocation = json.loads((root / 'invocation.json').read_text())
    frames = [json.loads(line) for line in (run / 'frames.jsonl').open()]
    begin, end = metadata['sample_start_steady_ns'], metadata['sample_end_steady_ns']
    elapsed = (end - begin) / 1e9
    samples = [json.loads(line) for line in (root / 'os.jsonl').open()]
    samples = [row for row in samples if begin <= row['time_ns'] <= end]
    thread_cpu = []
    other_core_equivalents = None
    if len(samples) >= 2:
        first = {task['tid']: task for task in samples[0]['tasks']}
        seconds = (samples[-1]['time_ns'] - samples[0]['time_ns']) / 1e9
        for task in samples[-1]['tasks']:
            if task['tid'] in first:
                cpu = (task['cpu_ticks'] - first[task['tid']]['cpu_ticks']) / invocation['clock_ticks']
                thread_cpu.append(dict(name=task['name'], tid=task['tid'],
                                       percent_one_core=100 * cpu / seconds))
        def busy_ticks(row):
            fields = [int(value) for value in row['system_cpu'][0].split()[1:]]
            return sum(fields[:8]) - fields[3] - fields[4]
        host_cores = (busy_ticks(samples[-1]) - busy_ticks(samples[0])) / invocation['clock_ticks'] / seconds
        other_core_equivalents = max(0, host_cores - sum(row['percent_one_core'] for row in thread_cpu) / 100)
    phases = collections.defaultdict(list)
    for frame in frames:
        for name, ms in frame.get('phases_ms', {}).items():
            phases[name].append(ms)
    phase_report = {name: dict(mean_ms_per_rendered_frame=sum(values) / len(frames),
                               p99_when_present_ms=percentile(values, .99))
                    for name, values in phases.items()}
    gpu = collections.defaultdict(list)
    for line in (run / 'gpu.jsonl').open():
        for item in json.loads(line)['passes']:
            if not item['pending']:
                gpu[item['name']].append(item['ms'])
    high_water, resident = [], []
    for sample in samples:
        for line in sample['status'].splitlines():
            if line.startswith('VmHWM:'):
                high_water.append(int(line.split()[1]) * 1024)
            elif line.startswith('VmRSS:'):
                resident.append(int(line.split()[1]) * 1024)
    telemetry = collections.defaultdict(list)
    with (root / 'gpu.csv').open() as source:
        for raw in csv.DictReader(source):
            for key, value in raw.items():
                name = key.strip()
                if name not in ('utilization.gpu [%]', 'utilization.memory [%]',
                                'memory.used [MiB]', 'clocks.current.graphics [MHz]', 'power.draw [W]'):
                    continue
                try:
                    telemetry[name].append(float(value.strip().split()[0]))
                except (ValueError, IndexError):
                    pass
    pool_elapsed = metadata['resource_workers_elapsed_seconds']
    pool_capacity = metadata['resource_workers'] * pool_elapsed
    result = dict(elapsed_seconds=elapsed, frames=len(frames),
                  frame_summary=json.loads((root / 'summary.json').read_text()),
                  threads=sorted(thread_cpu, key=lambda row: -row['percent_one_core']),
                  cpu_core_equivalents=sum(row['percent_one_core'] for row in thread_cpu) / 100,
                  estimated_other_host_core_equivalents=other_core_equivalents,
                  phases=phase_report,
                  gpu_command_intervals={name: dict(mean_ms=statistics.mean(values),
                                                    p99_ms=percentile(values, .99))
                                         for name, values in gpu.items()},
                  process_rss_peak_bytes=max(resident + [frame['rss_bytes'] for frame in frames
                                                         if frame.get('rss_bytes')], default=None),
                  process_lifetime_high_water_bytes=max(high_water, default=None),
                  device_telemetry_including_warmup={name: dict(mean=statistics.mean(values),
                                                       p95=percentile(values, .95), maximum=max(values))
                                                   for name, values in telemetry.items()},
                  device_telemetry_scope='Whole device including desktop, startup, warmup and shutdown; not per-process steady-state attribution',
                  worker_pool_capacity_seconds=pool_capacity,
                  worker_pool_busy_fraction=(metadata['resource_worker_busy_seconds'] / pool_capacity
                                             if pool_capacity else None),
                  worker_busy_seconds=metadata['resource_worker_busy_seconds'],
                  note='Scopes nest; GPU intervals overlap CPU work and may include command gaps. '
                       'Neither list is additive. CPU samples exclude threads absent at an endpoint. '
                       'Frame intervals are the actual whole-application throughput measurement.')
    if metadata.get('profile'):
        timeline = json.loads((run / 'timeline-last120.json').read_text())
        # The profiler also records loading and shutdown. Neither belongs in a
        # steady-state gameplay hitch table, even when it is a retained spike.
        spikes = [frame for frame in timeline['spikes']
                  if begin <= frame['start_ns'] and frame['end_ns'] <= end]
        result['retained_gameplay_spikes'] = []
        for frame in sorted(spikes, key=lambda item: item['end_ns'] - item['start_ns'], reverse=True):
            scopes = [scope for scope in frame['scopes'] if scope['thread'] == frame['main_thread']]
            scopes.sort(key=lambda item: item['end_ns'] - item['start_ns'], reverse=True)
            result['retained_gameplay_spikes'].append(dict(
                frame=frame['id'], outer_ms=(frame['end_ns'] - frame['start_ns']) / 1e6,
                fixed=frame['fixed'], counters=frame['counters'],
                largest_scopes=[dict(name=scope['name'], ms=(scope['end_ns'] - scope['start_ns']) / 1e6)
                                for scope in scopes[:12]]))
    if metadata.get('profile') and metadata.get('gpu_clock_calibration_end'):
        export_trace(run, root / 'cpu-gpu-trace.json')
        try:
            result['same_frame_completion'] = frame_completion(
                frames, [json.loads(line) for line in (run / 'gpu.jsonl').open()], metadata)
        except ValueError as error:
            result['same_frame_completion'] = dict(available=False, reason=str(error))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    args = parser.parse_args()
    print(json.dumps(report(args.capture), indent=2))
