#!/usr/bin/env python3
"""Prepare immutable benchmark copies from tracked projects, never local edits.

Private crash-yard projects are supplied separately by their owner. This tool
does not import, copy or redistribute downloaded models.
"""
import argparse
import copy
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if args.out.exists():
        parser.error('output must be a new directory')
    revision = subprocess.check_output(['git', 'rev-parse', args.revision + '^{commit}'], text=True).strip()
    names = ['animation_studio', 'renderer_v2_lab', 'terrain_v2_workshop',
             'particle_v2_lab', 'basic_water_lab']
    archive = subprocess.check_output(['git', 'archive', revision,
                                      *['projects/' + name for name in names]])
    args.out.mkdir(parents=True)
    with tarfile.open(fileobj=io.BytesIO(archive)) as source:
        for member in source:
            path = Path(member.name)
            if path.parts[0] != 'projects' or '..' in path.parts or path.is_absolute():
                raise ValueError('unexpected archive path')
            target = args.out.joinpath(*path.parts[1:])
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(source.extractfile(member).read())
            else:
                raise ValueError('benchmark source must contain regular files, not links')
    scene = args.out / 'animation_studio/Scenes/studio.judas'
    content = json.loads(scene.read_text())
    actors = [obj for obj in content['data']['objects'] if obj.get('fields', {}).get('animation.enabled')]
    if len(actors) != 1:
        raise ValueError('animation fixture changed; review workload instead of guessing')
    for index in range(128):
        actor = copy.deepcopy(actors[0])
        actor.update(id=2000 + index, name='Crowd ' + str(index))
        actor['fields']['position'] = [(index % 16 - 8) * 1.2, 0, -(index // 16) * 1.5]
        content['data']['objects'].append(actor)
    content['data']['settings']['next-id'] = 3000
    scene.with_name('crowd.judas').write_text(json.dumps(content, indent=2))
    choices = {'animation_studio': 'Scenes/crowd.judas',
               'renderer_v2_lab': 'Scenes/mixed.judas',
               'particle_v2_lab': 'Scenes/stress.judas'}
    for name, startup in choices.items():
        project = next((args.out / name).glob('*.judasproj'))
        content = json.loads(project.read_text())
        content['data']['startupScene'] = startup
        project.write_text(json.dumps(content, indent=2))
    files = {str(path.relative_to(args.out)): hashlib.sha256(path.read_bytes()).hexdigest()
             for path in sorted(args.out.rglob('*')) if path.is_file()}
    manifest = dict(source_revision=revision, files=files,
                    workload_sha256=hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest(),
                    terrain_scope='Original 48 m workshop; not the large-terrain qualification workload',
                    animation_scope='129 original animated performers including CharacterMotors',
                    particles_scope='Authored stress scene requests 100000 GPU collision particles; verify live telemetry')
    (args.out / 'WORKLOADS.json').write_text(json.dumps(manifest, indent=2))
    print(json.dumps({key: value for key, value in manifest.items() if key != 'files'}, indent=2))


if __name__ == '__main__':
    main()
