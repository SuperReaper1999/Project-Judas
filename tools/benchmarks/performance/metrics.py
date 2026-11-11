"""Whole-frame budgets. Never add nested CPU scopes or CPU/GPU durations."""
import math
import statistics


def simulation_loss(rows, profiled=True):
    """Keep unobserved historical clock loss unknown, not a zero-loss pass."""
    discarded = sum(row['discarded_seconds'] for row in rows) if profiled else None
    complete = profiled and bool(rows) and all(
        row.get('incoming_delta_clipped_seconds') is not None for row in rows)
    clipped = sum(row['incoming_delta_clipped_seconds'] for row in rows) if complete else None
    return {'physics_discarded_seconds': discarded,
            'physics_cap_frames': sum(bool(row['cap_reached']) for row in rows) if profiled else None,
            'incoming_delta_clipped_seconds': clipped,
            'total_simulation_time_lost_seconds': discarded + clipped if complete else None}


def percentile(values, fraction):
    if not values:
        raise ValueError('no samples')
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * fraction) - 1)]


def summarize(values):
    if not values or any(not math.isfinite(v) or v <= 0 for v in values):
        raise ValueError('frame intervals must be positive finite milliseconds')
    ordered = sorted(values, reverse=True)
    result = {'samples': len(values), 'average_fps': 1000 / statistics.mean(values),
              'p50_ms': percentile(values, .5), 'p95_ms': percentile(values, .95),
              'p99_ms': percentile(values, .99), 'worst_ms': max(values)}
    for fraction, name in [(.01, 'one_percent_low_fps'), (.001, 'point_one_percent_low_fps')]:
        result[name] = 1000 / statistics.mean(ordered[:max(1, math.ceil(len(values) * fraction))])
    result['hitches'] = {}
    for limit in [8.333333333, 16.666666667, 33.333333333, 50, 100]:
        count = longest_count = current_count = 0
        duration = longest_duration = 0
        for value in values:
            if value > limit:
                count += 1
                current_count += 1
                duration += value
                longest_count = max(longest_count, current_count)
                longest_duration = max(longest_duration, duration)
            else:
                current_count = 0
                duration = 0
        result['hitches'][str(limit)] = {'frames': count, 'longest_sequence_frames': longest_count,
                                       'longest_sequence_ms': longest_duration}
    result['p99_120fps'] = result['p99_ms'] <= 1000 / 120
    result['p99_60fps'] = result['p99_ms'] <= 1000 / 60
    return result
