#!/usr/bin/env python3
"""Read the immutable M78 follow-up observer outputs; do not run simulation."""
import csv, hashlib, json, math, statistics
from pathlib import Path
ROOT = Path(__file__).resolve().parents[4]
INPUT = ROOT / '.cache/m78-followup/concave-final-data'
OUTPUT = Path(__file__).with_name('existing_trace_analysis.json')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def corr(rows, key):
    x, y = [r['wall_ms'] for r in rows], [r[key] for r in rows]
    a, b = statistics.mean(x), statistics.mean(y)
    divisor = math.sqrt(sum((v-a)**2 for v in x)*sum((v-b)**2 for v in y))
    return None if not divisor else sum((u-a)*(v-b) for u,v in zip(x,y))/divisor

csv_path = INPUT / 'mesh-4.csv'
profile_path = INPUT / 'mesh-4-profile.json'
rows = [{k: float(v) for k,v in r.items()} for r in csv.DictReader(csv_path.open())]
keys = list(rows[0])
ordered = sorted(rows, key=lambda r: r['wall_ms'])
# Same index as the original observer, n=900, p95 uses index 854.
p95 = ordered[854]['wall_ms']
groups = {
    'all': rows,
    'p95_tail': [r for r in rows if r['wall_ms'] >= p95],
    'over_gate_16_67_ms': [r for r in rows if r['wall_ms'] > 16.67],
    'first_120': rows[:120],
    'steps_120_to_599': rows[120:600],
    'last_300': rows[600:],
}
summary = {}
for name, group in groups.items():
    summary[name] = {
        'steps': len(group),
        'mean': {k: statistics.mean(r[k] for r in group) for k in keys if k != 'step'},
        'solver_share_of_summed_wall': sum(r['solver_ms'] for r in group)/sum(r['wall_ms'] for r in group),
    }
profile = json.loads(profile_path.read_text())
spikes = []
for frame in profile.get('spikes', []):
    scopes = {s['name']: s['inclusive_ns']/1e6 for s in frame['scopes'] if s['name'].startswith('Physics')}
    fixed = sum(s['inclusive_ns'] for s in frame['scopes'] if s['name']=='Fixed simulation step')/1e6
    spikes.append({
        'profile_frame_id': frame['id'],
        'fixed_step_ms': fixed,
        # Frame interval is intentionally not reported as current-step cost.
        'physics_scope_ms': scopes,
        'continuous_impact_share_of_fixed_step': scopes.get('Physics continuous impacts',0)/fixed,
        'counters': {c['name']: c['value'] for c in frame['counters'] if c['name'].startswith('Physics')},
    })
result = {
    'purpose': 'Read-only analysis of original unchanged 900-step final observer',
    'inputs': {str(p.relative_to(ROOT)): sha(p) for p in [csv_path, profile_path]},
    'source_at_analysis': {str(p.relative_to(ROOT)): sha(p) for p in [ROOT/'src/PhysicsWorld.cpp', ROOT/'src/CollisionGeometry.cpp', ROOT/'src/Narrowphase.cpp']},
    'group_summary': summary,
    'correlation_with_wall_ms': {k: corr(rows,k) for k in keys if k not in ['step','wall_ms']},
    'top_20_steps': sorted(rows, key=lambda r:r['wall_ms'], reverse=True)[:20],
    'spikes': spikes,
    'limitations': [
        'CSV precision is the original observer precision; use original log for exact aggregate medians/p95.',
        'Correlation identifies a measured path; it does not establish which individual CCD geometry operation dominates.',
        'geometry_predicates samples ContactGeometryDiagnostics, not separate cooked CollisionGeometryStats.',
        'The retained profiler history is only the last 120 frames plus one spike, not all 900 individual scoped traces.',
        'No new simulation, build or production source modification was performed by this script.',
    ],
}
OUTPUT.write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps({'output':str(OUTPUT.relative_to(ROOT)), 'over_gate':summary['over_gate_16_67_ms']['steps'], 'p95_tail_solver_share':summary['p95_tail']['solver_share_of_summed_wall'], 'spike_continuous_share':spikes[0]['continuous_impact_share_of_fixed_step']},indent=2))
