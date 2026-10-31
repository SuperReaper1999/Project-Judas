"""Copy Beacon Grove unchanged and add a public-JS observer and real audio emitter.
Usage: python3 ... PROJECT FRESH_OUTPUT SCENE_AUTHOR
The playable game policy is preserved byte-for-byte as game-policy.js.
"""
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import wave

source, output, author = map(lambda p: Path(p).resolve(), sys.argv[1:])
if output.exists():
    raise SystemExit('Output must be fresh')
shutil.copytree(source, output, ignore=shutil.ignore_patterns('Saves', '.cache'))
project = output / 'Beacon_Grove.judasproj'
assets = output / 'Assets'
shutil.copyfile(assets / 'game.js', assets / 'game-policy.js')
subprocess.run([author, '--track-asset', project, assets / 'game-policy.js'], check=True)
(assets / 'game.js').write_text('''import Game, {properties} from './game-policy.js';
import {console,saves} from 'judas';
export {properties};
export default class extends Game {
 uiUpdate(){
  super.uiUpdate();
  if((this.observerTick=(this.observerTick||0)+1)%1===0)
   console.log('OFFLINE_PROBE '+JSON.stringify({sequence:this.observerTick,state:this.state,
    position:this.entity.transform.position,gateValid:this.properties.gate?.valid??false,
    audio:this.entity.audio,status:this.state.request?saves.status(this.state.request):null}));
 }
}
''', encoding='utf-8')
# Original, deterministic PCM; no downloaded audio or JavaScript oscillator.
with wave.open(str(assets / 'probe.wav'), 'wb') as wav:
    wav.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
    wav.writeframes(b''.join(struct.pack('<h', round(3276 * math.sin(2*math.pi*440*i/48000)))
                             for i in range(48000)))
subprocess.run([author, '--track-asset', project, assets / 'probe.wav'], check=True)
meta = (assets / 'probe.wav.judasmeta').read_text(encoding='utf-8')
asset_id = next(line.split('"')[1] for line in meta.splitlines() if line.startswith('id '))
scene = output / 'Scenes/main.judas'
named = output / 'Scenes/observed.judas'
subprocess.run([author, '--convert', 'scene', scene, named, 'named'], check=True)
data = json.loads(named.read_text(encoding='utf-8'))
player = next(o for o in data['data']['objects'] if o['id'] == 1)['fields']
player.update({'audio-emitter': [], 'audio.asset': asset_id, 'audio.enabled': True,
 'audio.play-on-start': True, 'audio.loop': True, 'audio.spatial': False,
 'audio.volume': 1, 'audio.pitch': 1, 'audio.reference-distance': 1,
 'audio.maximum-distance': 100, 'audio.rolloff': 1, 'audio.attenuation': {'symbol': 'none'}})
named.write_text(json.dumps(data, indent=2)+'\n', encoding='utf-8')
subprocess.run([author, '--validate', 'scene', named], check=True)
shutil.copyfile(named, scene)
named.unlink()
print('Fixture:', project)
