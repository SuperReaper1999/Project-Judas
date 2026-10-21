from pathlib import Path
import csv
import json
import math
import statistics

root = Path(__file__).resolve().parent
output = root / 'final-analysis'
output.mkdir(exist_ok=True)

def rows(path):
    with path.open() as stream:
        return list(csv.DictReader(stream))

def distribution(values):
    return {'mean': statistics.mean(values), 'median': statistics.median(values),
            'p95': sorted(values)[math.ceil(.95 * len(values)) - 1], 'worst': max(values)}

def steps(path, start, end):
    data = [r for r in rows(path) if start <= int(r['step']) <= end]
    return {'steps': len(data), 'fixed_ms': distribution([float(r['fixed_ms']) for r in data]),
            'events': sum(int(r['impact_events']) for r in data),
            'queries': sum(int(r['impact_queries']) for r in data),
            'search_iterations': sum(int(r['impact_iterations']) for r in data),
            'nonfinite': sum(int(r['nonfinite_fields']) for r in data)}

fields = ['bodies', 'awake', 'sleeping', 'candidates', 'contacts', 'impact_events',
          'impact_queries', 'impact_iterations', 'impact_peak', 'trajectory_digest',
          'max_linear_speed', 'max_angular_speed', 'nonfinite_fields']
def parity(first, second):
    a, b = rows(first), rows(second)
    count = min(len(a), len(b))
    errors = [(i + 1, name) for i, (left, right) in enumerate(zip(a, b))
              for name in fields if left[name] != right[name]]
    return {'left_count': len(a), 'right_count': len(b), 'compared': count,
            'exact_rows': count - len({i for i, _ in errors}), 'mismatches': errors,
            'scope': 'Exact physical/work fields, excluding timestamps, frame assignment and timings.'}

baseline = root / 'measure/baseline'
result = {'timings': {
    'baseline900': steps(baseline / 'unobserved900/steps.csv', 1, 900),
    'baseline900_repeat': steps(baseline / 'unobserved900-B/steps.csv', 1, 900),
    'baseline_late900': steps(baseline / 'long1800/steps.csv', 901, 1800),
    'final900': steps(root / 'final900/live/steps.csv', 1, 900),
    'final1800_first900': steps(root / 'final1800/live/steps.csv', 1, 900),
    'final1800_late900': steps(root / 'final1800/live/steps.csv', 901, 1800)},
    'parity': {
        'final_vs_private_corrected_control_1800': parity(root / 'final1800/live/steps.csv', root / 'support-control/run1800/steps.csv'),
        'final900_independent_repeat_vs_final1800_prefix': parity(root / 'final900/live/steps.csv', root / 'final1800/live/steps.csv'),
        'control_observer_vs_unobserved': parity(root / 'temporal/control900/live/steps.csv', root / 'support-control/run900/steps.csv'),
        'baseline_observer_vs_unobserved': parity(root / 'temporal/baseline900/live/steps.csv', baseline / 'unobserved900/steps.csv'),
        'private_row_dependency_vs_final900': parity(root / 'row-prototype/run900-native/live/steps.csv', root / 'final900/live/steps.csv')},
    'live': {}, 'memory': {}, 'physical': {}}

for label, folder in [('baseline', root / 'support-control/baseline-live35-unsampled'),
                      ('final', root / 'final-live35')]:
    data = rows(folder / 'live/frames.csv')
    windows = {}
    for name, lower, upper in [('first0_10', 0, 10000), ('settling10_20', 10000, 20000),
                                ('late20_to_stop', 20000, math.inf)]:
        selected = [r for r in data if lower <= float(r['wall_ms']) < upper]
        durations = [float(r['interval_ms']) for r in selected]
        windows[name] = {'frames': len(selected), 'interval_ms': distribution(durations),
                         'fps_from_mean_interval': 1000 / statistics.mean(durations),
                         'fixed_steps_per_frame': statistics.mean(int(r['fixed_steps']) for r in selected),
                         'simulated_seconds_per_wall_second': sum(int(r['fixed_steps']) for r in selected) / 60 / (sum(durations) / 1000)}
    result['live'][label] = {'frames': len(data), 'whole_capture_fps': len(data) / (float(data[-1]['wall_ms']) / 1000),
                              'eight_or_more_fixed_steps_frames': sum(int(r['fixed_steps']) >= 8 for r in data),
                              'windows': windows}

for label, folder in [('baseline1800', baseline / 'long1800'), ('final1800', root / 'final1800')]:
    path = folder / 'rusage.log'
    if label == 'baseline1800' and not path.is_file():
        path = baseline / 'long1800.log'
    if path.is_file():
        result['memory'][label] = {line.split(':', 1)[0].strip(): line.split(':', 1)[1].strip()
                                  for line in path.read_text().splitlines() if ':' in line and
                                  any(name in line for name in ['Maximum resident', 'Swaps', 'Elapsed', 'Major (requiring'])}

for label in ['baseline900', 'control900']:
    data = rows(root / 'temporal' / label / 'live/temporal-quality.csv')
    result['physical'][label] = {'states': len(data)}
    for field in ['mass_kg', 'mechanical_energy_J', 'kinetic_rotational_J',
                  'deepest_actual_penetration_m', 'min_floor_clearance_m',
                  'fully_inside_floor', 'geometry_uncertain', 'finite']:
        values = [float(r[field]) for r in data]
        result['physical'][label][field] = {'first': values[0], 'min': min(values), 'max': max(values), 'last': values[-1]}
    energies = [float(r['mechanical_energy_J']) for r in data]
    result['physical'][label]['max_positive_step_energy_change_J'] = max(b - a for a, b in zip(energies, energies[1:]))

result['limitations'] = [
    'Original and corrected dense trajectories differ; exact parity is only between corrected repeats or observer/control pairs.',
    'The late target is met, but the 10-20 second settling interval still averages roughly 10 FPS.',
    'Physics settings, counts, collision fidelity, energy guard and eight-step catch-up policy are unchanged.',
    'Temporal baseline runner source-monitor warning is preserved; immutable binary/core/fixture passed their guards.',
    'Sandbox row-prototype timing used llvmpipe and is invalid as a performance comparison; only native900 is compared.',
    'Native row tracking provides no demonstrated net benefit; it remains private and is not production code.']
(output / 'RESULTS.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({'timings': result['timings'], 'live': result['live'], 'parity': result['parity']}, indent=2))
