#!/usr/bin/env python3
"""Create an editable first-person acceptance copy; never change frozen timing inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def prepare(source, destination):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if destination.exists():
        raise ValueError('playable destination must be fresh')
    if destination == source or source in destination.parents:
        raise ValueError('playable copy must be outside frozen inputs')
    if any(path.is_symlink() for path in source.rglob('*')):
        raise ValueError('playable source must contain regular files, not links')
    scene = json.loads((source / 'Scenes/combined.judas').read_text())
    terrain = json.loads((source / 'Authoring/highlands.judasterrain2').read_text())
    objects = scene['data']['objects']
    player = next(o for o in objects if o['id'] == 64)
    # This fixture's real courtyard is at y=3, z=2..11. Spawn the actual
    # capsule on it, not the downhill edge underneath a displaced camera.
    fields = player['fields']
    fields['position'] = [0, 3 + fields['motor.radius'] + fields['motor.halfHeight'] + .04, 11]
    capsules = []
    for obj in objects:
        f = obj['fields']
        if not f.get('motor.enabled'):
            continue
        if f.get('parent', 0) or f.get('scale', [1, 1, 1]) != [1, 1, 1]:
            raise ValueError('review character-space placement guard for parented/scaled fixture')
        capsules.append(dict(id=str(obj['id']), radius=f['motor.radius'], halfHeight=f['motor.halfHeight'],
                             offset=dict(zip('xyz', f.get('motor.offset', [0, 0, 0]))), skin=f['motor.skin']))
    script = (Path(__file__).parent / 'playable_combined/explorer.js').read_text()
    script = script.replace('const BUILD_CAPSULES = [];', 'const BUILD_CAPSULES = ' + json.dumps(capsules) + ';')
    script = script.replace('const TERRAIN_CELL = .5;', 'const TERRAIN_CELL = ' + str(terrain['spacing']) + ';')
    project = json.loads((source / 'terrain_v2_workshop.judasproj').read_text())
    project['data']['name'] = 'Combined Gameplay — Interaction Workshop'
    project['data']['saveIdentity'] = hashlib.sha256(('judas-playable-combined:' + str(destination)).encode()).hexdigest()
    shutil.copytree(source, destination)
    (destination / 'terrain_v2_workshop.judasproj').write_text(json.dumps(project, indent=2) + '\n')
    (destination / 'Scenes/combined.judas').write_text(json.dumps(scene, indent=2) + '\n')
    (destination / 'Assets/scripts/explorer.js').write_text(script)
    (destination / 'PLAYABLE_COPY.json').write_text(json.dumps(dict(source=str(source),
        purpose='First-person interaction acceptance, not identical to elevated-camera benchmark inputs',
        changes=['eye-level camera', 'view-ray brush aim', 'authored-capsule placement guard', 'courtyard spawn']), indent=2)+'\n')
    return destination / 'terrain_v2_workshop.judasproj'


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    print(prepare(a.source, a.out))
