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
import math
import shutil
from pathlib import Path
import subprocess
import tarfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--large-terrain-tool', type=Path,
                        help='optionally cook a declared 1 km terrain workload using this native terrain tool')
    parser.add_argument('--scene-tool', type=Path,
                        help='optionally compose a declared terrain/32-character/5000-particle case using the native scene author')
    args = parser.parse_args()
    if args.scene_tool and not args.large_terrain_tool:
        parser.error('--scene-tool also requires the terrain tool for the combined courtyard')
    if args.out.exists():
        parser.error('output must be a new directory')
    revision = subprocess.check_output(['git', 'rev-parse', args.revision + '^{commit}'], text=True).strip()
    names = ['animation_studio', 'renderer_v2_lab', 'terrain_v2_workshop',
             'particle_v2_lab', 'basic_water_lab', 'willow_town',
             'vehicle_v2_workshop', 'developer_lab']
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
    # Bounded game-owned state evidence: a fast static hillside must never pass
    # as an excavation workload merely because the process returned success.
    explorer = args.out / 'terrain_v2_workshop/Assets/scripts/explorer.js'
    code = explorer.read_text()
    if code.count(' update(dt){') != 1:
        raise ValueError('terrain controller changed; review the capture driver')
    explorer.write_text(code.replace(' update(dt){', ''' update(dt){
  this.benchmarkAge=(this.benchmarkAge||0)+dt;
  if(this.benchmarkAge>=1){this.benchmarkAge=0;const s=this.owner.terrain.state;
   console.log('WHOLE_ENGINE_TERRAIN '+JSON.stringify({revision:s.revision,volumeChunks:s.volumeChunks,remaining:this.state.queue.length,ready:s.ready,busy:s.busy}));}
''', 1))
    # Real four-contact IK through the ordinary game API. The other performers
    # retain their normal animation and CharacterMotor work. No solver limits
    # or engine settings are lowered for this workload.
    ik = args.out / 'animation-ik'
    shutil.copytree(args.out / 'animation_studio', ik)
    fixture = json.loads(subprocess.check_output(
        ['git', 'show', revision + ':tests/fixtures/ik/configuration.judas']))
    settings = next(json.loads(obj['fields']['animation.full-body'])
                    for obj in fixture['data']['objects'] if obj.get('fields', {}).get('animation.full-body'))
    script = ik / 'Assets/scripts/benchmark-ik.js'
    script.write_text("import {world} from 'judas';\nconst SETTINGS=" + json.dumps(settings) + ";\n" + '''
export default class {
 constructor(){this.state={ready:false};this.age=0;}
 fixedUpdate(dt){
  if(!this.state.ready){
   const actors=Array.from({length:16},(_,i)=>world.entity(String(2000+i)));
   if(actors.some(actor=>!actor.animation.info.ready))return;
   for(const actor of actors){
    const targets=[['palmA','PalmA'],['palmB','PalmB'],['soleA','SoleA'],['soleB','SoleB']].map(([chain,joint],i)=>{
     const transform=actor.animation.jointTransform(joint,'model');
     const p=transform.position;
     return {id:chain,chain,space:'model',position:[p.x+(i<2?(i===0?.06:-.06):0),p.y+(i<2?.04:0),p.z],positionWeight:1,orientationWeight:0};
    });
    actor.animation.configureIK({...SETTINGS,targets});
   }
   this.state.ready=true;console.log('WHOLE_ENGINE_IK_READY actors=16 contacts=64');
  }
  this.age+=dt;
  if(this.age>=1){this.age=0;const statuses=Array.from({length:16},(_,i)=>world.entity(String(2000+i)).animation.ikStatus);
   console.log('WHOLE_ENGINE_IK_STATUS '+JSON.stringify({ready:statuses.filter(x=>x.ready).length,converged:statuses.filter(x=>x.converged).length,iterations:statuses.map(x=>x.iterations)}));}
 }
}
''')
    identifier = hashlib.sha256(b'Judas whole-engine IK workload').hexdigest()[:32]
    script.with_name(script.name + '.judasmeta').write_text(
        'JudasAssetMeta 1\nid "' + identifier + '"\ntype script\nsource "Assets/scripts/benchmark-ik.js"\n')
    crowd = ik / 'Scenes/crowd.judas'
    content = json.loads(crowd.read_text())
    controller = next(obj for obj in content['data']['objects'] if obj['id'] == 90)
    fields = controller['fields']; index = fields['scripts']; fields['scripts'] += 1
    for key, value in {'id':99,'asset':identifier,'enabled':True,'properties':'{}'}.items():
        fields['script.' + str(index) + '.' + key] = value
    crowd.write_text(json.dumps(content, indent=2))
    fleet = args.out / 'vehicle-fleet'
    shutil.copytree(args.out / 'vehicle_v2_workshop', fleet)
    control = fleet / 'Assets/Scripts/workshop.js'
    text = control.read_text()
    original = 'const active=selected.id===vehicle.id;'
    if text.count(original) != 1:
        raise ValueError('vehicle control fixture changed; review fleet workload')
    control.write_text(text.replace(original, 'const active=true;'))
    # Keep the driving fixture on its authored ground for sustained captures.
    # Plain held-forward runs can leave the map and become a falling-body test.
    for name, source in [('vehicle-cycle', 'vehicle_v2_workshop'),
                         ('vehicle-fleet-cycle', 'vehicle-fleet')]:
        destination = args.out / name
        shutil.copytree(args.out / source, destination)
        control = destination / 'Assets/Scripts/workshop.js'
        text = control.read_text()
        original = "const pedals=resolveVehiclePedals(this.state.pedals,input.axis('vehicle_throttle'),\n            vehicleForwardSpeed(world.entity(selected.id)),input.held('vehicle_brake'));"
        replacement = """if(input.axis('vehicle_throttle')>.5)this.state.benchmarkTick=(this.state.benchmarkTick||0)+1;
        const phase=((this.state.benchmarkTick||0)/60)%12;
        const request=!this.state.benchmarkTick?0:phase<3?1:phase<6?0:phase<9?-1:0;
        const braking=!this.state.benchmarkTick||phase>=3&&phase<6||phase>=9;
        const pedals=resolveVehiclePedals(this.state.pedals,request,
            vehicleForwardSpeed(world.entity(selected.id)),braking);
        if(this.state.benchmarkTick%60===0)console.log('BENCHMARK_VEHICLE '+JSON.stringify(vehicles.map(v=>{const e=world.entity(v.id);return {id:v.id,position:e.transform.position,velocity:e.velocity,support:wheelsOf(e).filter(w=>w.wheel.state.contact).length};})));"""
        if text.count(original) != 1:
            raise ValueError('vehicle pedal fixture changed; review controlled workload')
        control.write_text(text.replace(original, replacement))
    if args.large_terrain_tool:
        large = args.out / 'terrain-large'
        shutil.copytree(args.out / 'terrain_v2_workshop', large)
        authoring = large / 'Authoring/highlands.judasterrain2'
        terrain = json.loads(authoring.read_text())
        terrain.update(samplesX=513, samplesZ=513, chunkCells=32, spacing=2, bottom=-64, top=96)
        terrain['heights'] = [30 + 12 * math.sin(x * .035) * math.cos(z * .027)
                              for z in range(513) for x in range(513)]
        terrain['weights'] = [[1, 0, 0, 0] for _ in terrain['heights']]
        authoring.write_text(json.dumps(terrain, separators=(',', ':')))
        subprocess.run([str(args.large_terrain_tool.resolve()), 'v2-native', str(authoring),
                        str(large / 'Assets/terrain/highlands.judasmodel')], check=True)
        scene = large / 'Scenes/highlands.judas'
        text = scene.read_text()
        if 'position 0 1.9 13' not in text:
            raise ValueError('terrain player fixture changed; review initial placement')
        # Source grids are centred. Place the player just above the actual
        # bilinear surface so the unsimulated editor camera sees the landscape.
        gx, gz = 256., 256. + 13 / terrain['spacing']
        iz = int(gz)
        height = (1 - (gz - iz)) * terrain['heights'][iz * 513 + int(gx)] + (gz - iz) * terrain['heights'][(iz + 1) * 513 + int(gx)]
        scene.write_text(text.replace('position 0 1.9 13', f'position 0 {height + 1.9:.9f} 13'))
    if args.scene_tool:
        combined = args.out / 'combined'
        shutil.copytree(args.out / 'terrain_v2_workshop', combined)
        authoring = combined / 'Authoring/highlands.judasterrain2'
        courtyard = json.loads(authoring.read_text())
        # Keep the simulated crowd on a real, flat authored courtyard. With
        # no NPC movement controller, placing actors on the steep hill lets
        # gravity carry them out of the map, silently removing render work.
        for iz in range(courtyard['samplesZ']):
            z = (iz - (courtyard['samplesZ'] - 1) / 2) * courtyard['spacing']
            for ix in range(courtyard['samplesX']):
                x = (ix - (courtyard['samplesX'] - 1) / 2) * courtyard['spacing']
                weight = max(0., min(1., (8 - abs(x)) / 2, z / 2, (13 - z) / 2))
                index = iz * courtyard['samplesX'] + ix
                courtyard['heights'][index] = (1 - weight) * courtyard['heights'][index] + weight * 3
        authoring.write_text(json.dumps(courtyard, separators=(',', ':')))
        subprocess.run([str(args.large_terrain_tool.resolve()), 'v2-native', str(authoring),
                        str(combined / 'Assets/terrain/highlands.judasmodel')], check=True)
        scene = combined / 'Scenes/combined.judas'
        subprocess.run([str(args.scene_tool.resolve()), '--convert', 'scene',
                        str(combined / 'Scenes/highlands.judas'), str(scene), 'named'], check=True)
        content = json.loads(scene.read_text())
        studio = json.loads((args.out / 'animation_studio/Scenes/studio.judas').read_text())
        actor = next(obj for obj in studio['data']['objects'] if obj['id'] == 10)
        ground = json.loads((combined / 'Authoring/highlands.judasterrain2').read_text())
        def surface(x, z):
            gx = x / ground['spacing'] + (ground['samplesX'] - 1) / 2
            gz = z / ground['spacing'] + (ground['samplesZ'] - 1) / 2
            ix, iz = int(gx), int(gz)
            tx, tz = gx - ix, gz - iz
            heights, width = ground['heights'], ground['samplesX']
            return sum(heights[(iz + dz) * width + ix + dx] *
                       (tx if dx else 1 - tx) * (tz if dz else 1 - tz)
                       for dx in (0, 1) for dz in (0, 1))
        for index in range(32):
            obj = copy.deepcopy(actor)
            obj.update(id=2000 + index, name='Combined actor ' + str(index))
            x, z = (index % 8 - 3.5) * 1.3, 4 + index // 8 * 1.4
            obj['fields']['position'] = [x, surface(x, z) + .08, z]
            obj['fields']['parent'] = 0
            content['data']['objects'].append(obj)
        particles = json.loads((args.out / 'particle_v2_lab/Scenes/stress.judas').read_text())
        emitter = copy.deepcopy(next(obj for obj in particles['data']['objects']
                                     if obj.get('fields', {}).get('particle.burst') == 100000))
        emitter.update(id=2100, name='Combined 5000 GPU collision particles')
        emitter['fields'].update({'particle.burst': 5000, 'particle.capacity': 5000,
                                 'position': [0, 12, 0],
                                 'particle.spread': [5, 3, 5]})
        content['data']['objects'].append(emitter)
        content['data']['settings']['next-id'] = 2200
        scene.write_text(json.dumps(content, indent=2))
        for source, relative in [(args.out / 'animation_studio', 'Assets/models/performer.judasmodel'),
                                  (args.out / 'particle_v2_lab', 'Assets/textures/disc.png')]:
            destination = combined / relative
            if destination.exists() or destination.with_name(destination.name + '.judasmeta').exists():
                raise ValueError('combined asset destination is already occupied')
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source / relative, destination)
            shutil.copy2(source / (relative + '.judasmeta'), str(destination) + '.judasmeta')
        project = combined / 'terrain_v2_workshop.judasproj'
        text = project.read_text()
        original = 'startup-scene "Scenes/highlands.judas"'
        if text.count(original) != 1:
            raise ValueError('terrain project fixture changed; review combined startup')
        project.write_text(text.replace(original, 'startup-scene "Scenes/combined.judas"'))
        # The borrowed emitter's tags belong to its project, not to the scene
        # file. Carry that registry through the normal project authoring tool;
        # otherwise validation correctly rejects its unknown emitter tag.
        converted = combined / 'combined-project.named.json'
        subprocess.run([str(args.scene_tool.resolve()), '--convert', 'project',
                        str(project), str(converted), 'named'], check=True)
        definition = json.loads(converted.read_text())
        particle_project = json.loads((args.out / 'particle_v2_lab/particle_v2_lab.judasproj').read_text())
        categories = particle_project['data']['classification']
        for registry in ['collision', 'render']:
            if definition['data']['classification'][registry] != categories[registry]:
                raise ValueError('combined fixture registries changed; review their mapping')
        if definition['data']['classification']['tags']['entries']:
            raise ValueError('combined terrain has authored tags; review their mapping')
        definition['data']['classification'] = copy.deepcopy(categories)
        project.write_text(json.dumps(definition, indent=2))
        converted.unlink()
        script = combined / 'Assets/scripts/explorer.js'
        code = script.read_text()
        # A normal elevated follow view exposes the combined actors instead of
        # timing a camera whose near hillside hides the whole character crowd.
        code = code.replace('pitch:-.05', 'pitch:-.35')
        code = code.replace('add(p,{x:0,y:.65,z:0})', 'add(p,{x:0,y:9,z:7})')
        script.write_text(code.replace(' update(dt){', ''' update(dt){
  this.benchmarkCombinedAge=(this.benchmarkCombinedAge||0)+dt;
  if(this.benchmarkCombinedAge>=1){this.benchmarkCombinedAge=0;
   const actors=Array.from({length:32},(_,i)=>world.entity(String(2000+i)));
   const particles=world.entity('2100').particleStats;
   const heights=actors.map(a=>a.transform.position.y);
   console.log('WHOLE_ENGINE_COMBINED '+JSON.stringify({actors:actors.filter(a=>a.animation.info.ready).length,supported:actors.filter(a=>a.character.state.supported).length,minY:Math.min(...heights),maxY:Math.max(...heights),first:actors[0].transform.position,live:particles.live,backend:particles.active,contacts:particles.contacts}));}
''', 1))
    files = {str(path.relative_to(args.out)): hashlib.sha256(path.read_bytes()).hexdigest()
             for path in sorted(args.out.rglob('*')) if path.is_file()}
    manifest = dict(source_revision=revision, files=files, state_contract=1,
                    workload_sha256=hashlib.sha256(json.dumps(files, sort_keys=True).encode()).hexdigest(),
                    terrain_scope='Original 48 m workshop; not the large-terrain qualification workload',
                    large_terrain_scope='Optional 1024 m, 513x513 samples, 32-cell chunks; same workshop materials',
                    animation_scope='129 original animated performers including CharacterMotors',
                    ik_scope='Same crowd plus 16 four-position-contact full-body IK actors, original 64-iteration ceiling',
                    fleet_scope='All four existing public workshop vehicle configurations receive the normal project throttle policy',
                    vehicle_cycle_scope='Held W starts measured 3s drive, 3s brake, 3s reverse, 3s brake cycles; inspect support/position telemetry and end capture',
                    combined_scope='Optional terrain with an authored flat courtyard, 32 original animated CharacterMotors and 5000 live GPU collision particles',
                    particles_scope='Authored stress scene requests 100000 GPU collision particles; verify live telemetry')
    (args.out / 'WORKLOADS.json').write_text(json.dumps(manifest, indent=2))
    print(json.dumps({key: value for key, value in manifest.items() if key != 'files'}, indent=2))


if __name__ == '__main__':
    main()
