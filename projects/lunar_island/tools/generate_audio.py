#!/usr/bin/env python3
"""Render Lunar Island's editable SFX and existing dialogue with the local tool.

Run with the tool's Python 3.12 and PYTHONPATH=.local-tts for speech. No model
or package installation is performed. Existing cached model/voices are required.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--sfx-only', action='store_true')
    parser.add_argument('--revision', help='Use a specific cached Kokoro revision for reproduction')
    args = parser.parse_args()
    tool = args.tool.resolve()
    sys.path.insert(0, str(tool))
    from sine_sfx_designer import PRESETS, generate_samples, write_wav
    import numpy as np

    audio = ROOT / 'Assets/audio'
    source = ROOT / 'ArtSource/audio'
    source.mkdir(parents=True, exist_ok=True)
    # Deterministic recipes; existing asset identities and gameplay hooks remain.
    recipes = {
        'gather': ('Pickup', {'duration': .25, 'master_gain': .28}),
        'chop': ('Melee Impact', {'noise_color': 'brown', 'filter_cutoff_hz': 1250, 'master_gain': .65}),
        'craft': ('UI Click', {'duration': .18, 'master_gain': .32}),
        'consume': ('Pickup', {'master_gain': .22, 'filter_cutoff_hz': 1100}),
        'step': ('Hit', {'duration': .16, 'noise_color': 'brown', 'filter_cutoff_hz': 650, 'master_gain': .22}),
        'gunshot': ('Rifle', {'master_gain': .65}),
        'reload': ('UI Click', {'duration': .24, 'master_gain': .35, 'noise_enabled': True, 'noise_gain': .24}),
        'hurt': ('Hit', {'master_gain': .4, 'filter_cutoff_hz': 1300}),
    }
    effects = []
    for name, (preset, edits) in recipes.items():
        state = copy.deepcopy(PRESETS[preset])
        state.update(edits, name='Lunar Island / ' + name, noise_seed=7200 + len(effects), normalize=False)
        samples, normalized = generate_samples(state)
        write_wav(audio / (name + '.wav'), samples, normalized['sample_rate'])
        (source / (name + '.sfx.json')).write_text(json.dumps(normalized, indent=2) + '\n')
        effects.append({'id': name, 'preset': preset, 'sha256': digest(audio / (name + '.wav'))})
    (source / 'sfx_manifest.json').write_text(json.dumps({'toolCommit': subprocess.check_output(['git', '-C', str(tool), 'rev-parse', 'HEAD'], text=True).strip(), 'effects': effects}, indent=2) + '\n')
    if args.sfx_only:
        return

    # Use precisely the same installed Kokoro engine as Dialogue Studio, loading
    # it once for the batch. Explicit cached paths avoid network/download writes.
    os.environ['HF_HUB_OFFLINE'] = '1'
    os.environ['HF_HUB_DISABLE_TELEMETRY'] = '1'
    import torch
    from kokoro import KModel, KPipeline
    torch.set_num_threads(2)
    torch.manual_seed(7200)
    cache = tool / '.kokoro-cache/huggingface/models--hexgrad--Kokoro-82M'
    revision = args.revision or (cache / 'refs/main').read_text().strip()
    model_dir = cache / 'snapshots' / revision
    pipeline = KPipeline(lang_code='b', repo_id='hexgrad/Kokoro-82M', device='cpu',
                         model=KModel(repo_id='hexgrad/Kokoro-82M', config=str(model_dir / 'config.json'), model=str(model_dir / 'kokoro-v1_0.pth')).eval())
    people = json.loads((ROOT / 'Assets/scripts/encounter-data.js').read_text().split('export const PEOPLE = ')[1].rstrip(';\n'))
    scene_path = ROOT / 'Scenes/island.judas'
    scene = json.loads(scene_path.read_text())
    objects = scene['data']['objects']
    mapping, lines = {}, []
    # Two installed synthetic voices; these are not impersonations or bespoke
    # performances. Distinct pacing complements the existing character writing.
    female = {'mara', 'region_town_0', 'region_city_0', 'region_coast_0'}
    for key, actor in people['actors'].items():
        if not actor.get('friendly'):
            continue
        voice = 'bf_emma' if key in female else 'bm_george'
        speed = .98 if key == 'ivo' else 1.03
        voice_path = model_dir / 'voices' / (voice + '.pt')
        pack = torch.load(str(voice_path), weights_only=True, map_location='cpu')
        mapping[key] = []
        for index, subtitle in enumerate(actor.get('lines', [])):
            line_id = f'{key}_{index}'
            text = subtitle.split(': ', 1)[1] if key in {'mara', 'ivo'} else subtitle
            parts = [r.audio.numpy() for r in pipeline(text, voice=pack, speed=speed) if r.audio is not None]
            if not parts:
                raise RuntimeError('No speech generated for ' + line_id)
            samples = np.concatenate(parts)
            if not np.isfinite(samples).all() or float(np.max(np.abs(samples))) >= 1:
                raise RuntimeError('Invalid/clipped generated speech: ' + line_id)
            path = audio / ('voice_' + line_id + '.wav')
            write_wav(path, samples * .85, 24000)
            asset = hashlib.md5(('lunar-island-voice/' + line_id).encode()).hexdigest()
            path.with_name(path.name + '.judasmeta').write_text(f'JudasAssetMeta 1\nid "{asset}"\ntype audio\nsource "Lunar Island local Kokoro dialogue"\n')
            entity = 990000000 + len(lines)
            mapping[key].append(str(entity))
            old = next((o for o in objects if o['id'] == entity), None)
            if old and old['name'] != 'Dialogue / ' + line_id:
                raise RuntimeError('Authored ID collision')
            emitter = {'id': entity, 'name': 'Dialogue / ' + line_id, 'fields': {
                'position': [0, 0, 0], 'rotation': [1, 0, 0, 0], 'scale': [1, 1, 1],
                'audio-emitter': [], 'audio.asset': asset, 'audio.enabled': True,
                'audio.play-on-start': False, 'audio.loop': False, 'audio.spatial': False,
                'audio.volume': .85, 'audio.pitch': 1, 'audio.reference-distance': 2,
                'audio.maximum-distance': 30, 'audio.rolloff': 1,
                'audio.attenuation': {'symbol': 'none'}}}
            if old:
                objects[objects.index(old)] = emitter
            else:
                objects.append(emitter)
            lines.append({'id': line_id, 'speaker': actor['name'], 'actor': key, 'index': index,
                          'subtitle': subtitle, 'text': text, 'voice': voice, 'speed': speed,
                          'voiceSha256': digest(voice_path), 'entity': str(entity), 'asset': asset,
                          'file': path.name, 'sha256': digest(path), 'seconds': len(samples) / 24000})
            print('Rendered', line_id, f'{len(samples)/24000:.2f}s', flush=True)
    scene_path.write_text(json.dumps(scene, indent=2) + '\n')
    (ROOT / 'Assets/scripts/voice-data.js').write_text('// Generated by tools/generate_audio.py. Stable authored emitters.\nexport const VOICES = ' + json.dumps(mapping, indent=2) + ';\n')
    (source / 'dialogue_manifest.json').write_text(json.dumps({'toolCommit': subprocess.check_output(['git', '-C', str(tool), 'rev-parse', 'HEAD'], text=True).strip(), 'model': 'hexgrad/Kokoro-82M', 'modelRevision': revision, 'modelSha256': digest(model_dir / 'kokoro-v1_0.pth'), 'seed': 7200, 'lines': lines}, indent=2) + '\n')
    for name in ('voice-data', 'dialogue-voice'):
        path = ROOT / 'Assets/scripts' / (name + '.js')
        asset = hashlib.sha256(('lunar-island-v1/' + str(path.relative_to(ROOT))).encode()).hexdigest()[:32]
        Path(str(path) + '.judasmeta').write_text(f'JudasAssetMeta 1\nid "{asset}"\ntype script\nsource "Lunar Island audio authoring"\n')
    print('Completed:', len(effects), 'effects and', len(lines), 'dialogue lines')

if __name__ == '__main__':
    main()
