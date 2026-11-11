"""Cross-clock timeline export; durations are never added across processors."""
import json


def merge_intervals(intervals):
    result = []
    for begin, end in sorted(intervals):
        if end < begin:
            raise ValueError('reversed interval')
        if result and begin <= result[-1][1]:
            result[-1] = (result[-1][0], max(end, result[-1][1]))
        else:
            result.append((begin, end))
    return result


def clock_mapping(start, end):
    cpu0 = (start['cpu_before_ns'] + start['cpu_after_ns']) / 2
    cpu1 = (end['cpu_before_ns'] + end['cpu_after_ns']) / 2
    gpu0, gpu1 = start['gpu_ns'], end['gpu_ns']
    if gpu1 <= gpu0 or cpu1 <= cpu0:
        raise ValueError('non-monotonic calibration')
    slope = (cpu1 - cpu0) / (gpu1 - gpu0)
    if not .99 <= slope <= 1.01:
        raise ValueError('device/CPU clocks do not support this alignment')
    uncertainty = max(start['cpu_after_ns'] - start['cpu_before_ns'],
                      end['cpu_after_ns'] - end['cpu_before_ns']) / 2
    return lambda gpu: cpu0 + (gpu - gpu0) * slope, uncertainty


def export_trace(run, destination):
    metadata = json.loads((run / 'run.json').read_text())
    profile = json.loads((run / 'timeline-last120.json').read_text())
    convert, uncertainty = clock_mapping(metadata['gpu_clock_calibration'],
                                         metadata['gpu_clock_calibration_end'])
    frames = [f for f in profile['frames'] if f['mode'] == 'standalone']
    if not frames:
        raise ValueError('no ordinary application frames')
    begin, end = min(f['start_ns'] for f in frames), max(f['end_ns'] for f in frames)
    events = []
    for frame in frames:
        for thread in frame['threads']:
            events.append({'ph': 'M', 'pid': 1, 'tid': thread['id'], 'name': 'thread_name',
                           'args': {'name': thread['name']}})
        for scope in frame['scopes']:
            a, b = max(begin, scope['start_ns']), min(end, scope['end_ns'])
            if b > a:
                events.append({'ph': 'X', 'pid': 1, 'tid': scope['thread'], 'name': scope['name'],
                               'ts': (a - begin) / 1000, 'dur': (b - a) / 1000,
                               'args': {'wait': scope['wait'], 'origin_frame': scope['origin_frame']}})
        for gpu in frame['gpu']['passes']:
            if not gpu['pending'] and gpu.get('gpu_end_ns', 0) > gpu.get('gpu_start_ns', 0):
                a, b = convert(gpu['gpu_start_ns']), convert(gpu['gpu_end_ns'])
                events.append({'ph': 'X', 'pid': 2, 'tid': 1, 'name': gpu['pass'],
                               'ts': (a - begin) / 1000, 'dur': (b - a) / 1000,
                               'args': {'source_frame': gpu['source_frame'],
                                        'submitted_cpu_ns': gpu['submitted_cpu_ns']}})
    events.append({'ph': 'M', 'pid': 2, 'tid': 1, 'name': 'thread_name',
                   'args': {'name': 'GPU command intervals (includes queue gaps; not active utilisation)'}})
    destination.write_text(json.dumps({'traceEvents': events, 'displayTimeUnit': 'ms',
                                      'calibration_half_bracket_ns': uncertainty,
                                      'note': 'CPU and GPU events overlap. Not a sum or photon-latency measurement.'}))
