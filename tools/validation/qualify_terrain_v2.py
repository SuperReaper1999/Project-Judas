#!/usr/bin/env python3
"""Exercise the owned terrain game, then export, relocate and cold-restore it."""
import argparse
import datetime
import hashlib
import json
import math
import struct
import os
from pathlib import Path
import shutil
import subprocess
import time


def install_particle_probe(project):
    """An ordinary project script/scene exercises authored and runtime API state."""
    asset = '8a2200000000000000000000000000af'
    script = project / 'Assets/scripts/region_consumer.js'
    script.write_text('''export default class {
 constructor({entity}) { this.entity=entity; this.state={contacts:0,reported:false}; }
 require(value,message) { if(!value)throw new Error(message); }
 start() {
  const p=this.entity.particles;
  this.require(p.collisionRegion&&p.collisionRegionOffset.x===1&&p.collisionRegionHalfExtents.x===8,'authored region');
  this.require(this.entity.setParticles({collisionRegionOffset:{x:0,y:-4,z:0},
    collisionRegionHalfExtents:{x:12,y:16,z:12}}),'region patch');
  this.require(this.entity.particles.collisionRegionHalfExtents.y===16,'region readback');
  this.require(this.entity.burst(1),'burst');
 }
 fixedUpdate() {
  for(const hit of this.entity.takeParticleImpacts()) {
   this.require(hit.receiver==='1','logical terrain receiver');
   this.state.contacts+=hit.count;
  }
  if(this.state.contacts&&!this.state.reported&&this.entity.particleStats.live===0) {
   this.state.reported=true;
   console.log('REGION_CONSUMER_CONTACT '+JSON.stringify({contacts:this.state.contacts,stats:this.entity.particleStats}));
  }
 }
 restore() {
  const p=this.entity.particles;
  this.require(p.collisionRegion&&p.collisionRegionOffset.y===-4&&p.collisionRegionHalfExtents.y===16,'restored region');
  this.require(this.state.contacts===1,'restored game-owned contact counter');
  console.log('REGION_CONSUMER_RESTORED '+JSON.stringify({contacts:this.state.contacts,settings:p}));
 }
}''')
    script.with_suffix('.js.judasmeta').write_text(
        f'JudasAssetMeta 1\nid "{asset}"\ntype script\nsource ""\n')
    scene = project / 'Scenes/highlands.judas'
    with scene.open('a') as stream:
        stream.write(f'''object 65 "SDK local collision consumer"
 position 0 12 0
 rotation 1 0 0 0
 scale 1 1 1
 particle-emitter
 particle.enabled true
 particle.loop false
 particle.local-space false
 particle.gravity false
 particle.rate 0
 particle.lifetime 4
 particle.size 0.05
 particle.end-size 0.05
 particle.burst 0
 particle.capacity 16
 particle.spread 0 0 0
 particle.velocity 0 -12 0
 particle.variation 0 0 0
 particle.acceleration 0 0 0
 particle.texture ""
 particle.color 0.6 0.5 0.3
 particle.alpha 1
 particle.end-color 0.6 0.5 0.3
 particle.end-alpha 1
 particle.seed 1
 particle.collision true
 particle.response "remove"
 particle.impacts "reliable"
 particle.collision-region true
 particle.collision-region-offset 1 0 0
 particle.collision-region-half-extents 8 8 8
 scripts 1
 script.0.id 1
 script.0.asset "{asset}"
 script.0.enabled true
 script.0.properties "{{}}"
end
''')


def install_material_detail(project):
    """Author only the disposable consumer copy; never update existing assets/saves."""
    asset = '8a2200000000000000000000000000b0'
    texture = project / 'Assets/terrain/detail-normal.tga'
    side = 128
    header = bytearray(18)
    header[2] = 2
    header[12:16] = struct.pack('<HH', side, side)
    header[16] = 32
    header[17] = 8
    pixels = bytearray()
    # An original periodic normal source, not downloaded artwork. Its source
    # settings are retained here so the visual test is reproducible.
    def noise(x, y, cells):
        u, v = (x % side) * cells / side, (y % side) * cells / side
        ix, iy = math.floor(u), math.floor(v)
        tx, ty = u - ix, v - iy
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        def value(a, b):
            h = (((a % cells) * 73856093) ^ ((b % cells) * 19349663) ^ 83492791) & 0xffffffff
            h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
            return (h & 65535) / 65535
        return ((1 - tx) * value(ix, iy) + tx * value(ix + 1, iy)) * (1 - ty) + \
               ((1 - tx) * value(ix, iy + 1) + tx * value(ix + 1, iy + 1)) * ty
    def height(x, y):
        return .6 * noise(x, y, 8) + .3 * noise(x, y, 16) + .1 * noise(x, y, 32)
    for y in range(side):
        for x in range(side):
            nx = (height(x - 1, y) - height(x + 1, y)) * 1.5
            ny = (height(x, y - 1) - height(x, y + 1)) * 1.5
            nz = math.sqrt(1 - nx * nx - ny * ny)
            pixels.extend((round((nz * .5 + .5) * 255),
                           round((ny * .5 + .5) * 255),
                           round((nx * .5 + .5) * 255), 255))
    texture.write_bytes(header + pixels)
    texture.with_suffix('.tga.judasmeta').write_text(
        f'JudasAssetMeta 1\nid "{asset}"\ntype texture\nsource ""\n')
    source = project / 'Authoring/highlands.judasterrain2'
    data = json.loads(source.read_text())
    data['materialDetail'] = dict(tileMetres=[2, 3, 6, 4], roughness=[1, .9, .8, 1],
                                  normalTexture=asset, normalMetres=.8, normalStrength=.7)
    source.write_text(json.dumps(data, separators=(',', ':')) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--planet', action='store_true',
                        help='Exercise the geographic Workshop with the same gameplay gates.')
    parser.add_argument('--particle-region', action='store_true',
                        help='Add a normal script/scene consumer of local particle coverage (flat only).')
    parser.add_argument('--material-detail', action='store_true',
                        help='Author optional material controls in the disposable consumer copy.')
    args = parser.parse_args()
    if args.planet and args.particle_region:
        parser.error('The particle-region consumer fixture uses the flat Workshop.')
    repo = Path(__file__).resolve().parents[2]
    build = args.build.resolve()
    out = args.output.resolve()
    if out.exists() or out.is_relative_to(repo):
        parser.error('Use a fresh directory outside the checkout; evidence is never overwritten.')
    out.mkdir(parents=True)
    rows = []

    def run(name, command, cwd, env=None):
        start = time.monotonic()
        with (out / (name + '.log')).open('w') as log:
            result = subprocess.run([str(c) for c in command], cwd=cwd, env=env,
                                    stdout=log, stderr=subprocess.STDOUT, timeout=180)
        rows.append(dict(name=name, command=[str(c) for c in command], cwd=str(cwd),
                         exit=result.returncode, seconds=time.monotonic() - start))
        if result.returncode:
            raise RuntimeError(f'{name} failed; see {out / (name + ".log")}')

    record = dict(passed=False, human_acceptance=False)
    try:
        record['build'] = json.loads(subprocess.check_output(
            [str(build / 'judas'), '--build-info-json'], text=True))
        project = out / 'editable'
        project_name = 'terrain_v2_planet_workshop' if args.planet else 'terrain_v2_workshop'
        shutil.copytree(repo / 'projects' / project_name, project,
                        ignore=shutil.ignore_patterns('Saves', '*.lock'))
        if args.particle_region:
            install_particle_probe(project)
        if args.material_detail:
            install_material_detail(project)
        run('native-cook', [build / 'judas_terrain_author', 'v2-native',
                           project / 'Authoring/highlands.judasterrain2',
                           project / 'Assets/terrain/highlands.judasmodel'], repo)
        products = project / 'Assets/terrain'
        receipt = json.loads((products / 'highlands.terrain-v2-receipt.json').read_text())
        if receipt['representation'] != 'native' or receipt['collision'] is not None:
            raise RuntimeError('Native receipt must distinguish the preview from collision ownership.')
        for key, filename in [('native', 'highlands.judasterrain'),
                              ('model', 'highlands.judasmodel'), ('preview', 'highlands.preview.tga')]:
            if hashlib.sha256((products / filename).read_bytes()).hexdigest() != receipt[key]:
                raise RuntimeError(f'Cooked {key} product does not match its receipt.')
        run('export', [build / 'judas_export', project / (project_name + '.judasproj'),
                       out / 'original-package', build / 'judas'], repo)
        package = out / 'relocated game'
        (out / 'original-package').rename(package)
        if args.material_detail:
            expected = hashlib.sha256((project / 'Assets/terrain/detail-normal.tga').read_bytes()).digest()
            matches = [p for p in package.rglob('*') if p.is_file() and p.suffix == '.tga'
                       and hashlib.sha256(p.read_bytes()).digest() == expected]
            if not matches:
                raise RuntimeError('Relocated export omitted the terrain normal-detail dependency.')
            record['normal_detail_exported'] = [str(p.relative_to(package)) for p in matches]
        work = out / 'unrelated working directory'
        work.mkdir()
        # No source files can accidentally satisfy a package dependency at play time.
        project.rename(out / 'parked editable')
        scripts = {
            'write': '''FRAMES 1800
FRAME_SECONDS 0.016666667
LOG_EVERY 120
ACTION mode_raise 1 100 101
ACTION dig 1 120 121
ACTION mode_paint 1 160 161
ACTION dig 1 180 181
ACTION mode_dig 1 220 221
ACTION tunnel 1 250 251
ACTION save 1 1450 1451
WAIT_SERVICES save 10000 1453
''',
            'settle': '''FRAMES 1000
FRAME_SECONDS 0.016666667
LOG_EVERY 120
ACTION load 1 20 21
WAIT_SERVICES save 10000 30
WAIT_SERVICES scene 10000 35
ACTION save 1 900 901
WAIT_SERVICES save 10000 910
''',
            'traverse': '''FRAMES 520
FRAME_SECONDS 0.016666667
LOG_EVERY 60
ACTION load 1 20 21
WAIT_SERVICES save 10000 30
WAIT_SERVICES scene 10000 35
AXIS move_y 1 50 410
SCREENSHOT 250 TUNNEL_SCREENSHOT
ACTION save 1 450 451
WAIT_SERVICES save 10000 460
''',
            'read': '''FRAMES 180
FRAME_SECONDS 0.016666667
LOG_EVERY 30
ACTION load 1 20 21
WAIT_SERVICES save 10000 30
WAIT_SERVICES scene 10000 35
EXPECT_SCENE Scenes/highlands.judas 45
''',
        }
        def play(name, text, frame):
            harness = out / (name + '.harness')
            text = text.replace('TUNNEL_SCREENSHOT', str(out / 'tunnel.png'))
            harness.write_text(text + f'SCREENSHOT {frame} {out / (name + ".png")}\n')
            env = dict(os.environ, JUDAS_TEST_SCRIPT=str(harness),
                       JUDAS_ENGINE_ROOT=str(package / 'engine'),
                       JUDAS_WINDOW_SIZE='1280x800', XDG_DATA_HOME=str(out / 'user-data'))
            run(name, [package / 'judas', package / 'game.judasproj'], work, env)
            return (out / (name + '.log')).read_text()

        play('write', scripts['write'], 1750)
        # A scripted frame is not a worker deadline. Persisted project queues
        # resume in genuine cold processes; inspect actual queue completion
        # before walking, keeping the original full-traversal acceptance gate.
        for attempt in range(8):
            log = play('settle-' + str(attempt), scripts['settle'], 980)
            saves = [json.loads(line.split('TERRAIN_V2_SAVE_REQUEST ', 1)[1])
                     for line in log.splitlines() if 'TERRAIN_V2_SAVE_REQUEST ' in line]
            if len(saves) == 1 and saves[0]['remainingBrushes'] == 0:
                record['settlement_cold_processes'] = attempt + 1
                break
        else:
            raise RuntimeError('Project brush queue did not finish within eight bounded cold runs.')
        play('traverse', scripts['traverse'], 500)
        play('read', scripts['read'], 150)
        restored = []
        write_log = (out / 'write.log').read_text()
        for kind in ('raise', 'paint'):
            if 'TERRAIN_V2_SURFACE_DAB ' + kind not in write_log:
                raise RuntimeError('Actual gameplay did not apply the ' + kind + ' surface tool.')
        for line in (out / 'read.log').read_text().splitlines():
            marker = 'TERRAIN_V2_RESTORED '
            if marker in line:
                restored.append(json.loads(line.split(marker, 1)[1]))
        if len(restored) != 1:
            raise RuntimeError('Expected exactly one actual terrain/game restore callback.')
        state = restored[0]
        terrain = state['terrain']
        if (not terrain['ready'] or terrain['busy'] or terrain['error'] or not terrain['navigation'] or
                terrain['volumeChunks'] < 8 or state['position']['z'] > -8 or state['surfaceDabs'] < 2 or
                state['remainingBrushes'] != 0):
            raise RuntimeError('Cold restore lost the edited terrain or real tunnel traversal.')
        if args.particle_region:
            if 'REGION_CONSUMER_CONTACT ' not in write_log:
                raise RuntimeError('Ordinary authored particle did not deliver/consume its terrain impact.')
            regions = [json.loads(line.split('REGION_CONSUMER_RESTORED ', 1)[1])
                       for line in (out / 'read.log').read_text().splitlines()
                       if 'REGION_CONSUMER_RESTORED ' in line]
            if len(regions) != 1 or regions[0]['contacts'] != 1:
                raise RuntimeError('Cold load lost project-owned particle progress/settings.')
            record['particle_region'] = regions[0]
        record.update(passed=True, restored=state)
    except Exception as error:
        record['error'] = str(error)
    record.update(finished_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), runs=rows)
    record['log_sha256'] = {f.name: hashlib.sha256(f.read_bytes()).hexdigest()
                            for f in sorted(out.glob('*.log'))}
    (out / 'RESULTS.json').write_text(json.dumps(record, indent=2) + '\n')
    print(json.dumps({k: v for k, v in record.items() if k not in ('build', 'log_sha256')}, indent=2))
    return not record['passed']


if __name__ == '__main__':
    raise SystemExit(main())
