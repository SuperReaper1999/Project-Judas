#!/usr/bin/env python3
"""Replay the captured rigid body's momentum budget, without sampling wetness.

The residual includes unrecorded rigid contacts/solver effects and roundoff. It
is not automatically a fluid error. Exterior reactions explicitly not applied
by production must NOT be included as real body impulses. No acceptance gate,
surface estimator, force law or equilibrium deadline is changed by this tool.
"""
import argparse
import csv
import json
import math
from pathlib import Path


def finite_vector(value):
    if len(value) != 3 or not all(math.isfinite(x) for x in value):
        raise ValueError('Expected finite three-vector')
    return value


def budget(before, after):
    if not before['observedBody']['valid'] or not after['observedBody']['valid']:
        raise ValueError('Observed body must be valid')
    if before['observedBody'] != after['observedBody']:
        raise ValueError('Body identity changed across observation')
    mass = before['observedMass']
    dt = after['fixedStepSeconds']
    if not math.isfinite(mass) or mass <= 0 or mass != after['observedMass']:
        raise ValueError('This diagnostic requires constant positive mass')
    if not math.isfinite(dt) or dt <= 0:
        raise ValueError('Invalid fixed timestep')
    if after['completedNativeFixedSteps'] != before['completedNativeFixedSteps'] + 1:
        raise ValueError('Snapshots are not adjacent fixed-step boundaries')
    g = finite_vector(before['observedGravity'])
    if g != after['observedGravity']:
        raise ValueError('This diagnostic requires uniform gravity over the step')
    length = math.sqrt(math.fsum(x*x for x in g))
    if length == 0:
        raise ValueError('No vertical reference in zero gravity')
    up = [-x/length for x in g]
    project = lambda v: math.fsum(a*b for a, b in zip(up, finite_vector(v)))
    measurement = after['lastBodyMeasurement']
    if measurement is None:
        raise ValueError('No applied body measurement')
    if not math.isfinite(measurement['fraction']) or not 0 <= measurement['fraction'] <= 1:
        raise ValueError('Invalid reported fraction')
    density = after['couplingSettings']['restDensity']
    volume = measurement['displacedVolume']
    if not math.isfinite(density) or density <= 0 or not math.isfinite(volume) or volume < 0:
        raise ValueError('Invalid physical density/volume')
    begin_velocity = finite_vector(before['observedLinearVelocity'])
    end_velocity = finite_vector(after['observedLinearVelocity'])
    observed = mass * project([a-b for a, b in zip(end_velocity, begin_velocity)])
    gravity = mass * project(g) * dt
    hydro = project(measurement['buoyancyForce']) * dt
    drag = project(measurement['dragForce']) * dt
    contained = project(measurement['containedImpulse'])
    static_buoyancy = density * volume * length * dt
    residual = observed - math.fsum([gravity, hydro, drag, contained])
    return dict(momentum_change=observed, gravity=gravity,
                static_buoyancy=static_buoyancy,
                fluid_acceleration_load=hydro-static_buoyancy,
                hydro_total=hydro, drag=drag, contained=contained,
                unattributed_residual=residual,
                exterior_reaction_NOT_applied=project(after['exteriorReactionNotApplied']),
                begin_vertical_speed=project(before['observedLinearVelocity']),
                end_vertical_speed=project(after['observedLinearVelocity']),
                applied_fraction=measurement['fraction'])


def analyze(directory, case):
    with (directory/(case+'_observations.csv')).open(encoding='utf-8', newline='') as stream:
        observations = list(csv.DictReader(stream))
    if not observations:
        raise ValueError('No observations')
    rows = []
    previous = {}
    for observation in observations:
        step, window = int(observation['step']), observation['window']
        if step <= previous.get(window, step-1):
            raise ValueError('Unordered or duplicated observations')
        previous[window] = step
        load = lambda stage: json.loads((directory/f'{case}_{stage}{step}_boundary.json').read_text())
        before, after = load('before'), load('observe')
        if before['schema'] != 1 or after['schema'] != 1:
            raise ValueError('Unsupported snapshot schema')
        row = budget(before, after)
        row.update(step=step, window=window, material_revision_before=before['materialRevision'],
                   material_revision_after=after['materialRevision'])
        rows.append(row)
    expected = {'original': set(range(181, 241))}
    contract = directory/(case+'_settling.json')
    if contract.exists():
        d = json.loads(contract.read_text())
        expected['late'] = set(range(d['late_first_step']+1, d['late_end_step']+1))
    actual = {window: {r['step'] for r in rows if r['window'] == window} for window in previous}
    if actual != expected:
        raise ValueError('Incomplete physical observation windows')
    windows = {}
    impulse_fields = ('momentum_change', 'gravity', 'static_buoyancy', 'fluid_acceleration_load',
                      'hydro_total', 'drag', 'contained', 'unattributed_residual', 'exterior_reaction_NOT_applied')
    for window in expected:
        selected = [r for r in rows if r['window'] == window]
        windows[window] = dict(samples=len(selected),
            impulses={name: math.fsum(r[name] for r in selected) for name in impulse_fields},
            max_absolute_step_residual=max(abs(r['unattributed_residual']) for r in selected),
            mean_applied_fraction=math.fsum(r['applied_fraction'] for r in selected)/len(selected),
            begin_vertical_speed=selected[0]['begin_vertical_speed'],
            end_vertical_speed=selected[-1]['end_vertical_speed'])
    return rows, dict(schema=1, case=case, windows=windows,
        scope='Momentum accounting only; no continuum immersion oracle or replacement acceptance gate',
        unknown='Residual can include rigid contact/solver impulses and binary32 rounding',
        acceptance_changed=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--case', default='half_025')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        rows, report = analyze(args.directory, args.case)
        args.output.mkdir(parents=True, exist_ok=False)
        with (args.output/'force-budget.csv').open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
        (args.output/'RESULTS.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps(report, indent=2))
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, str(error)+'\n')


if __name__ == '__main__':
    main()
