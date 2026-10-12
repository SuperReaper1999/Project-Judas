#!/usr/bin/env python3
"""Independent 3x3 algebra check, not a PhysicsWorld/game simulation.

Preserve an endpoint/dense-output distinction using only the Python standard
library. No production imports, time stepping, fixture changes or retuned gates.
"""
import datetime
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUTPUT = ROOT / 'docs/evidence/m78-continuation/reviewer-midpoint-dense-output.json'


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def length(a):
    return math.sqrt(dot(a, a))


def solve(matrix, residual):
    # Independent Gaussian elimination rather than the production GLM inverse.
    augmented = [list(row)+[value] for row, value in zip(matrix, residual)]
    for axis in range(3):
        pivot = max(range(axis, 3), key=lambda row: abs(augmented[row][axis]))
        augmented[axis], augmented[pivot] = augmented[pivot], augmented[axis]
        scale = augmented[axis][axis]
        if scale == 0:
            raise ArithmeticError('singular diagnostic Newton Jacobian')
        augmented[axis] = [value/scale for value in augmented[axis]]
        for row in range(3):
            if row != axis:
                scale = augmented[row][axis]
                augmented[row] = [x-scale*y for x, y in zip(augmented[row], augmented[axis])]
    return [row[3] for row in augmented]


def sample(inertia, velocity, h):
    inverse = [1/value for value in inertia]
    start = [value*omega for value, omega in zip(inertia, velocity)]
    middle = start[:]
    for iteration in range(12):
        omega = [value*m for value, m in zip(inverse, middle)]
        product = cross(middle, omega)
        residual = [m-original-.5*h*p for m, original, p in zip(middle, start, product)]
        if length(residual) <= 1e-12*max(length(start), 1e-12):
            break
        columns = []
        for axis in range(3):
            basis = [int(index == axis) for index in range(3)]
            first = cross(basis, omega)
            second = cross(middle, [value*e for value, e in zip(inverse, basis)])
            columns.append([e-.5*h*(x+y) for e, x, y in zip(basis, first, second)])
        update = solve([[columns[column][row] for column in range(3)] for row in range(3)], residual)
        middle = [value-delta for value, delta in zip(middle, update)]
    else:
        raise ArithmeticError('diagnostic midpoint did not converge')
    omega = [value*m for value, m in zip(inverse, middle)]
    energy = .5*sum(m*m*value for m, value in zip(start, inverse))
    speed = length(omega)
    unit = [value/speed for value in omega]
    projection = dot(unit, start)
    perpendicular = cross(unit, start)
    fractions = []
    for fraction in (.25, .5, .75, 1.):
        # Q(t)^T m0: independent Rodrigues rotation equivalent to the anchored
        # normalized quaternion (1, t*omega/2), with the same fixed generator.
        angle = 2*math.atan(.5*h*fraction*speed)
        cosine, sine = math.cos(angle), math.sin(angle)
        momentum = [cosine*m-sine*p+(1-cosine)*projection*u
                    for m, p, u in zip(start, perpendicular, unit)]
        current = .5*sum(m*m*value for m, value in zip(momentum, inverse))
        fractions.append(dict(fraction=fraction, relativeEnergyError=(current-energy)/energy))
    return dict(h=h, newtonIterations=iteration, residualNorm=length(residual),
                initialEnergy=energy, fixedGenerator=omega, samples=fractions)


def main():
    if OUTPUT.exists():
        raise SystemExit('Preserve existing review receipt; choose a new file for any later revision')
    cases = []
    for inertia, omega in [([1.06, .85, .29], [1, 2, 3]),
                           ([1, 2, 3], [2, 5, 10]),
                           ([1, 2, 3], [12, 15, 20]),
                           ([1, 2, 3], [20, 50, 100])]:
        rows = [sample(inertia, omega, 1/hz) for hz in (60, 120, 240, 480)]
        errors = [abs(row['samples'][1]['relativeEnergyError']) for row in rows]
        cases.append(dict(inertiaDiagonal=inertia, initialBodyAngularVelocity=omega,
                          refinement=rows, successiveHalfStepErrorRatios=[b/a for a, b in zip(errors, errors[1:])]))
    source_paths = ['src/RigidBody.cpp', 'src/RigidBody.h', 'src/RigidMotion.h', 'src/PhysicsWorld.cpp']
    result = dict(
        utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        purpose='Independent read-only endpoint versus dense-output algebra review; not final-source fingerprints',
        equations=[
            'm_mid = m0 + (h/2) * (m_mid cross I^-1 m_mid)',
            'omega_mid = I^-1 m_mid',
            'Q(t) = rotation(axis=omega_mid/|omega_mid|, angle=2 atan(t |omega_mid|/2))',
            'm(t) = Q(t)^T m0; E(t) = (1/2) m(t)^T I^-1 m(t)',
        ],
        assumptions=['Positive diagonal body inertia; initial orientation identity; no external torque/impulse.',
                     'Double precision idealized algebra. Production quaternion/velocity storage is binary32.',
                     'Same fixed generator at every interior fraction, matching the anchored dense-output policy.',
                     'This is not an actual contact simulation, nor a replacement for root CCD regression results.'],
        sourceSHA256={path: hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in source_paths},
        calculatorSHA256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        cases=cases,
        conclusions=[
            'At t=h the Cayley update satisfies the implicit midpoint endpoint invariant to solve/rounding error.',
            'At t<h the full-duration fixed generator need not conserve energy exactly.',
            'Half-step dense-output energy errors decrease approximately quadratically as h halves.',
            'A TOI split may retain interior energy error; endpoint invariants do not imply exact constrained/CCD energy conservation.',
            'World angular momentum remains preserved by the transport definition at each sample.',
            'Recomputing the midpoint at every query would change the anchored geometric path and its CCD bounds; no such production change is proposed here.',
        ],
    )
    OUTPUT.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({'output': str(OUTPUT.relative_to(ROOT)), 'cases': len(cases),
                      'halfStepErrors60Hz': [case['refinement'][0]['samples'][1]['relativeEnergyError'] for case in cases]}))


if __name__ == '__main__':
    main()
