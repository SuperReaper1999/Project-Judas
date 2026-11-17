"""Fail-closed comparison of matched whole-application performance captures."""
import statistics
import math


def compare(baseline, candidate, p99_regression_fraction=.10):
    """Inputs are repeated captures, each with identity and summary dictionaries.

    This checks a regression against a measured baseline, not product acceptance.
    Exceptional hitches and the absolute gameplay budgets are reported separately.
    """
    if not math.isfinite(p99_regression_fraction) or not 0 <= p99_regression_fraction <= 1:
        raise ValueError('invalid regression tolerance')
    if len(baseline) < 3 or len(candidate) < 3:
        raise ValueError('at least three independent captures per revision required')
    keys = ('workload_sha256', 'project_sha256', 'controls', 'duration_seconds',
            'resolution', 'graphics_policy', 'cpu_model', 'gpu_model', 'gpu_driver',
            'cap_hz', 'capture_policy', 'profile', 'native_capture_schema')
    identity = baseline[0]['identity']
    if any(key not in identity for key in keys):
        raise ValueError('incomplete reference workload/hardware identity')
    for key in ('workload_sha256', 'project_sha256', 'cpu_model', 'gpu_model', 'gpu_driver'):
        if not identity[key] or identity[key] in ('unknown', 'unavailable'):
            raise ValueError('unverified reference identity')
    for run in baseline + candidate:
        # Earlier capture scripts had a fixed two-second warm-up.
        if run['identity'].get('warmup_seconds', 2) != identity.get('warmup_seconds', 2):
            raise ValueError('loading/warm-up policy differs')
        if any(run['identity'].get(key) != identity[key] for key in keys):
            raise ValueError('workload, hardware or measurement policy differs')
        if run['summary']['samples'] < 1000:
            raise ValueError('insufficient steady-state frame samples')
        if run.get('dropped_events', 0) or run.get('truncated_records', 0):
            raise ValueError('incomplete diagnostic capture')
    old = statistics.median(run['summary']['p99_ms'] for run in baseline)
    new = statistics.median(run['summary']['p99_ms'] for run in candidate)
    if not math.isfinite(old) or not math.isfinite(new) or old <= 0 or new <= 0:
        raise ValueError('invalid p99')
    def maximum_observed(key):
        values = [run['summary'].get(key) for run in candidate]
        return max(values) if all(isinstance(value, (int, float)) and
                                  math.isfinite(value) and value >= 0 for value in values) else None
    return {'baseline_p99_ms': old, 'candidate_p99_ms': new,
            'p99_change_fraction': new / old - 1,
            'regression_pass': new <= old * (1 + p99_regression_fraction),
            'candidate_p99_120fps': new <= 1000 / 120,
            'candidate_p99_60fps': new <= 1000 / 60,
            'exceptional_worst_ms': max(run['summary']['worst_ms'] for run in candidate),
            'candidate_discarded_seconds': maximum_observed('physics_discarded_seconds'),
            'candidate_incoming_clipped_seconds': maximum_observed('incoming_delta_clipped_seconds'),
            'candidate_simulation_time_lost_seconds': maximum_observed('total_simulation_time_lost_seconds'),
            'human_accepted': False}
