#!/usr/bin/env python3
"""Summarise one captured application without summing overlapping CPU/GPU work."""
import argparse
import collections
import json
from pathlib import Path
import statistics
from performance.metrics import percentile
from performance.timeline import export_trace


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
    if len(samples) >= 2:
        first = {task['tid']: task for task in samples[0]['tasks']}
        seconds = (samples[-1]['time_ns'] - samples[0]['time_ns']) / 1e9
        for task in samples[-1]['tasks']:
            if task['tid'] in first:
                cpu = (task['cpu_ticks'] - first[task['tid']]['cpu_ticks']) / invocation['clock_ticks']
                thread_cpu.append(dict(name=task['name'], tid=task['tid'],
                                       percent_one_core=100 * cpu / seconds))
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
    result = dict(elapsed_seconds=elapsed, frames=len(frames),
                  frame_summary=json.loads((root / 'summary.json').read_text()),
                  threads=sorted(thread_cpu, key=lambda row: -row['percent_one_core']),
                  cpu_core_equivalents=sum(row['percent_one_core'] for row in thread_cpu) / 100,
                  phases=phase_report,
                  gpu_command_intervals={name: dict(mean_ms=statistics.mean(values),
                                                    p99_ms=percentile(values, .99))
                                         for name, values in gpu.items()},
                  process_rss_peak_bytes=max((frame.get('rss_bytes', 0) for frame in frames), default=0),
                  worker_busy_seconds=metadata['resource_worker_busy_seconds'],
                  note='Scopes nest; GPU intervals overlap CPU work and may include command gaps. '
                       'Neither list is additive. CPU samples exclude threads absent at an endpoint. '
                       'Frame intervals are the actual whole-application throughput measurement.')
    if metadata.get('profile') and metadata.get('gpu_clock_calibration_end'):
        export_trace(run, root / 'cpu-gpu-trace.json')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    args = parser.parse_args()
    print(json.dumps(report(args.capture), indent=2))
