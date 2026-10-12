#!/usr/bin/env python3
"""Compare guarded Linux closure with exact native uninterrupted source.

The equality guard proves the input; float output comparisons are explicitly
cross-platform and therefore numeric rather than a bitwise determinism claim.
"""
import pathlib, json, hashlib, math

r = pathlib.Path.cwd()
ev = r / 'docs/evidence/m78-cold-boundary'
source_path = ev / 'm78-diagnostic-boundary-v1-source-joint-boundary.jsonl'

def load(path):
    return [json.loads(x) for x in path.read_text().splitlines()]

def boundary(records):
    return [x for x in records if x.get('tick') == 0 and x.get('prepare') == 1]

def recursive_error(a, b):
    if isinstance(a, list):
        return max((recursive_error(x, y) for x, y in zip(a, b)), default=0.)
    if isinstance(a, dict):
        return max((recursive_error(v, b[k]) for k, v in a.items()), default=0.)
    if isinstance(a, (float, int)) and not isinstance(a, bool):
        return abs(a - b)
    return 0. if a == b else math.inf

src = boundary(load(source_path))
joint_rows = lambda rows: {x['jointOrdinal']: x for x in rows if x['kind'] == 'joint_begin'}
constraints = lambda rows: {(x['jointOrdinal'], x['cacheIndex']): x for x in rows if x['kind'] == 'row_construct'}
base_joints = joint_rows(src)
base_constraints = constraints(src)
result = dict(scope='Exact native a106 input, source commands/seeds guarded and unchanged body state. Compare first native uninterrupted boundary against Linux diagnostic closure. Output float differences do not imply cross-platform bitwise determinism.', variants={})

for name in ['commands-seeds', 'commands-seeds-normal-prepare']:
    path = ev / ('linux-first-closure-' + name + '-joint-boundary.jsonl')
    records = load(path)
    closure = boundary(records)
    joints = joint_rows(closure)
    rows = constraints(closure)
    remap = {}
    for index, a in base_joints.items():
        b = joints[index]
        for side in ['A', 'B']:
            remap[a['body' + side]] = b['body' + side]
    equation_fields = ['linearA', 'linearB', 'angularA', 'angularB', 'effective', 'mass',
                       'error', 'errorVector', 'bias', 'gamma', 'low', 'high', 'initialImpulse']
    row_errors = []
    for key, a in base_constraints.items():
        b = rows[key]
        for field in equation_fields:
            if field in a and a[field] != b.get(field):
                row_errors.append(dict(row=key, field=field,
                                       maxAbs=recursive_error(a[field], b[field])))
    prepare_end = [x for x in src if x['kind'] == 'prepare_end']
    other_prepare_end = [x for x in closure if x['kind'] == 'prepare_end']
    # Final scalar-pass row journals include each body's immediately updated
    # state. The last appearance is the actual completed 10th joint pass.
    def final_body_states(records):
        out = {}
        row_bodies = {x['row']: (x['bodyA'], x['bodyB']) for x in records if x['kind'] == 'row_construct'}
        for x in records:
            if x['kind'] == 'row_solve_after' and x['iteration'] == 10:
                a, b = row_bodies[x['row']]
                out[a] = x['a']
                out[b] = x['b']
        return out
    a_states, b_states = final_body_states(src), final_body_states(closure)
    pass_errors = []
    for body, a in a_states.items():
        b = b_states[remap[body]]
        pass_errors.append(dict(sourceBody=body, closureBody=remap[body],
                               velocityMaxAbs=max(recursive_error(a['linearVelocity'], b['linearVelocity']),
                                                  recursive_error(a['angularVelocity'], b['angularVelocity']))))
    physical_steps = []
    for step in range(1, 11):
        a_path = ev / ('m78-diagnostic-boundary-v1-source-physical-step-' + str(step) + '.json')
        b_path = ev / ('linux-first-closure-' + name + '-physical-step-' + str(step) + '.json')
        a, b = json.loads(a_path.read_text()), json.loads(b_path.read_text())
        aa, bb = {x['entity']: x for x in a['bodies']}, {x['entity']: x for x in b['bodies']}
        body_errors = []
        for ma, mb in zip(a['mapped'], b['mapped']):
            x, y = aa[ma['entity']], bb[mb['entity']]
            body_errors.append(dict(index=ma['index'],
                                   position=recursive_error(x['position'], y['position']),
                                   rotation=recursive_error(x['rotation'], y['rotation']),
                                   linearVelocity=recursive_error(x['linearVelocity'], y['linearVelocity']),
                                   angularVelocity=recursive_error(x['angularVelocity'], y['angularVelocity'])))
        physical_steps.append(dict(step=step, bodies=body_errors,
                                   sourceSwingTime=a['history']['swingTime'],
                                   closureSwingTime=b['history']['swingTime']))
    result['variants'][name] = dict(journalSHA256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                   closureGuardMarkers=[x for x in records if x['kind'] == 'actual_native_first_boundary_replayed'],
                                   sourceRows=len(base_constraints), closureRows=len(rows),
                                   rowEquationOrWarmDifferences=row_errors,
                                   completedFirstJointPassVelocityErrors=pass_errors,
                                   firstTenPhysicalStates=physical_steps)

result['sourceJournalSHA256'] = hashlib.sha256(source_path.read_bytes()).hexdigest()
(ev / 'first-boundary-closure-comparison.json').write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
for name, d in result['variants'].items():
    print(name, 'row differences', len(d['rowEquationOrWarmDifferences']),
          'final pass body differences', d['completedFirstJointPassVelocityErrors'],
          'step1 peak', {field: max(x[field] for x in d['firstTenPhysicalStates'][0]['bodies'])
                         for field in ['position', 'rotation', 'linearVelocity', 'angularVelocity']})
