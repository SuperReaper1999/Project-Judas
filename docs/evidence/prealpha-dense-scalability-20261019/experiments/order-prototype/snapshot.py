#!/usr/bin/env python3
"""Freeze the isolated override for the existing matched-run driver."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import tarfile

owned = Path(__file__).resolve().parent
base = owned.parent / 'baseline-e30'
snapshot = owned / 'snapshot'
snapshot.mkdir(exist_ok=False)
(snapshot/'bin').mkdir()

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

manifest = json.loads((base/'FROZEN.json').read_text())
manifest['classification'] = 'isolated BFS support-layer order experiment; physical trajectories may change'
manifest['variant_receipt'] = str(owned/'BUILD.json')
manifest['variant_receipt_sha256'] = sha(owned/'BUILD.json')
manifest['base_manifest_sha256'] = sha(base/'FROZEN.json')
manifest['notes'].append('Source archive remains exact e30 base; isolated-overrides.tar.gz supplies the four replacement source/header files and build recipe. No production files modified.')
binary = owned/'judas_physics_stress_live_benchmark'
copied = snapshot/'bin/judas_physics_stress_live_benchmark'
shutil.copy2(binary, copied)
manifest['artifacts']['bin/judas_physics_stress_live_benchmark'] = {
    'path': str(binary), 'sha256': sha(binary), 'copy_sha256': sha(copied), 'bytes': copied.stat().st_size}
overrides = {}
names = ['ContactSolver.h', 'ContactSolver.cpp', 'ImpactSolver.h', 'ImpactSolver.cpp', 'build.py', 'BUILD.json']
with tarfile.open(snapshot/'isolated-overrides.tar.gz', 'w:gz') as tar:
    for name in names:
        path = owned/name
        tar.add(path, arcname=name)
        overrides[name] = {'sha256': sha(path), 'bytes': path.stat().st_size}
        if name.endswith(('.h', '.cpp')):
            manifest['source_inputs']['src/'+name] = overrides[name]
manifest['isolated_overrides'] = overrides
manifest['isolated_overrides_archive_sha256'] = sha(snapshot/'isolated-overrides.tar.gz')
manifest['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
(snapshot/'FROZEN.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(snapshot)
