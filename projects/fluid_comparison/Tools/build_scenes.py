#!/usr/bin/env python3
"""Regenerate only this project's two editable comparison scenes."""
from pathlib import Path
import hashlib
import json

PROJECT = Path(__file__).resolve().parents[1]
blocks = []
stairs = []


def vector(values):
    return ' '.join(f'{value:.9g}' for value in values)


def obj(name, position, half=None, mass=None, color=(.4, .45, .5), alpha=1, extra=''):
    identity = len(blocks) + 1
    text = f'object {identity} "{name}"\n  position {vector(position)}\n  rotation 1 0 0 0\n  scale 1 1 1\n'
    if half is not None:
        kind = 'dynamic' if mass is not None else 'static'
        text += f'''  render box
  render.half-extents {vector(half)}
  render.radius 0.5
  render.color {vector(color)}
  render.alpha {alpha}
  render.secondary-color .8 .8 .8
  render.secondary-alpha 1
  render.mesh-asset ""
  render.texture-asset ""
  body {kind} box
  body.half-extents {vector(half)}
  body.radius 0.5
  body.terrain ""
  body.mass {mass if mass is not None else 1}
  body.friction .8
  body.restitution 0
  body.initial-velocity 0 0 0
  body.pickable {'true' if mass is not None else 'false'}
  body.managed false
  body.compound-count 0
'''
    blocks.append(text + extra + 'end\n')
    return identity


def hud(identity):
    return f'''object {identity} "Comparison controls"
  position 0 0 0
  rotation 1 0 0 0
  scale 1 1 1
  ui.asset "fa11c0de000000000000000000000001"
  ui.name "game_ui"
  ui.enabled true
  scripts 1
  script.0.id 1
  script.0.asset "fa11c0de000000000000000000000002"
  script.0.enabled true
  script.0.properties "{{}}"
end
'''


obj('Reference floor / uniform gravity', (0, -.25, 0), (8.5, .25, 9),
    color=(.22, .27, .29), extra='  gravity uniform 9.81\n  gravity.region box 100 100 100\n')
for sign in (-1, 1):
    obj('Clear long wall', (sign * 6.15, 1.4, 0), (.15, 1.4, 4.3),
        color=(.25, .55, .65), alpha=.24)
    obj('Short wall', (0, 1.4, sign * 4.15), (6, 1.4, .15),
        color=(.45, .53, .58), alpha=.32)
    obj('Long rim', (sign * 6.15, 2.8, 0), (.17, .06, 4.3), color=(.8, .83, .84))
    obj('Short rim', (0, 2.8, sign * 4.15), (6, .06, .17), color=(.8, .83, .84))
obj('Entry and observation deck', (0, 1.4, 5.9), (7, 1.4, 1.6), color=(.6, .58, .48))
for index in range(1, 15):
    top = index * .2
    position, half = (0, top / 2, .35 + index * .25), (.9, top / 2, .18)
    stairs.append((position, half))
    obj(f'Exit stair {index:02d} / .2 m rise', position, half, color=(.71, .72, .68))
obj('Floating green block / 500 kg per m3', (-3.5, 3.32, 5.4), (.5, .5, .5),
    mass=500, color=(.18, .78, .35))
obj('Sinking grey block / 2000 kg per m3', (-5, 3.32, 5.4), (.5, .5, .5),
    mass=2000, color=(.4, .43, .47))
obj('Neutral blue block / 1000 kg per m3', (3.5, 3.32, 5.4), (.5, .5, .5),
    mass=1000, color=(.18, .48, .9))

# The two scenes use identical scale/spacing/cadence. The rectangular stock is
# 24x4x16 before excluding actual stair geometry; no emitter or endless refill.
spacing = .5
clearance = .018 * 10
water = set()
for y in range(4):
    for z in range(16):
        for x in range(24):
            point = ((x - 11.5) * spacing, (y + .5) * spacing, (z - 7.5) * spacing)
            blocked = any(all(abs(value - center) <= extent + clearance
                              for value, center, extent in zip(point, position, half))
                          for position, half in stairs)
            if not blocked:
                water.add((x, y, z))
particle_count = len(water)
# Coalesce identical stock cells into a few editable volume boxes. This changes
# no particle positions or total stock, and keeps the hierarchy manageable.
while water:
    x, y, z = min(water, key=lambda cell: (cell[1], cell[2], cell[0]))
    nx = 1
    while (x + nx, y, z) in water:
        nx += 1
    nz = 1
    while all((xx, y, z + nz) in water for xx in range(x, x + nx)):
        nz += 1
    ny = 1
    while all((xx, y + ny, zz) in water
              for zz in range(z, z + nz) for xx in range(x, x + nx)):
        ny += 1
    for yy in range(y, y + ny):
        for zz in range(z, z + nz):
            for xx in range(x, x + nx):
                water.remove((xx, yy, zz))
    center = ((x + (nx - 1) / 2 - 11.5) * spacing,
              (y + .5) * spacing, (z + (nz - 1) / 2 - 7.5) * spacing)
    obj('Reference water / stock volume', center,
        extra=f'''  fluid-volume
  fluid-volume.spacing {spacing}
  fluid-volume.count {nx} {ny} {nz}
  fluid-volume.emitter false
  fluid-volume.emitter-offset 0 0 0
  fluid-volume.max-particles 0
''')
obj('Player / dry entry deck', (0, 3.8, 6.7), extra='''  player-start first-person
  player-start.yaw 0
  player-start.density 950
  player-start.fluid-drag 2
  player-start.swim-acceleration 4
''')
blocks.append(hud(len(blocks) + 1))
header = f'''JudasScene 3
settings
  name "Large rectangular fluid reference"
  world-origin 0 0 0
  sun-direction .4 .7 .35
  sun-color 1 .98 .92
  ambient .3 .32 .35
  fluid-scale 10
  fluid-update-rate-hz 30
  fluid-hydrostatic-drag-rate 2
  fidelity-policy none
  next-id {len(blocks) + 1}
end

'''
(PROJECT / 'Scenes/rectangular_pool.judas').write_text(header + '\n'.join(blocks))
original = (PROJECT / 'Source/original_terrain.judas').read_bytes()
awkward = original.decode().replace('  next-id 9\n', '  next-id 10\n') + '\n' + hud(9)
(PROJECT / 'Scenes/awkward_planet.judas').write_text(awkward)
(PROJECT / 'Source/authoring.json').write_text(json.dumps({
    'originalSceneSHA256': hashlib.sha256(original).hexdigest(),
    'reference': {'innerDimensionsM': [12, 8], 'initialWaterHeightM': 2,
                  'spacingM': spacing, 'fluidScale': 10, 'fluidUpdateRateHz': 30,
                  'unclippedGrid': [24, 4, 16], 'actualParticles': particle_count,
                  'particleMassKg': 1000 * spacing ** 3,
                  'initialMassKg': particle_count * 1000 * spacing ** 3,
                  'stairClearanceM': clearance},
    'awkward': 'Original objects1-8 unchanged. Only next-id metadata and HUD object9 added.'
}, indent=2) + '\n')
print(f'Generated reference: {particle_count} particles, {len(blocks)} objects; original terrain snapshot preserved.')
