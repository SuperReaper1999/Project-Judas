#!/usr/bin/env python3
"""Build a read-only one-event observer from the exact frozen baseline."""
import datetime
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tarfile

OWNED = Path(__file__).resolve().parent
ROOT = OWNED.parents[2]
BUILD = ROOT / 'build'
BASE = OWNED.parent / 'baseline-e30'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
receipt = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'commands': [],
           'base_manifest_sha256': sha(BASE/'FROZEN.json'), 'artifacts': {}}
def run(argv, cwd=ROOT):
    receipt['commands'].append({'argv': list(map(str, argv)), 'cwd': str(cwd)})
    subprocess.run(argv, cwd=cwd, check=True)
with tarfile.open(BASE/'source-inputs.tar.gz') as tar:
    source = tar.extractfile('src/ImpactSolver.cpp').read().decode()
(OWNED/'ImpactSolver.original.cpp').write_text(source)
source = source.replace('} // namespace\n\nImpactResult',
    '} // namespace\n\n#include "ContactCapture.h"\n\nImpactResult', 1)
source = source.replace('    auto run = [&](bool safe) {',
    '    auto captured = contact_capture::Begin(contacts, dt, forceInelastic, joints && !joints->Empty());\n    auto run = [&](bool safe) {', 1)
prefix, suffix = source.rsplit('    return result;\n}', 1)
source = prefix + '    contact_capture::End(captured, m_solver.Constraints(), result);\n    return result;\n}' + suffix
(OWNED/'ImpactSolver.cpp').write_text(source)
flags = {}
for line in (BUILD/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1); flags[key] = shlex.split(value)
obj = OWNED/'ImpactSolver.cpp.o'
run(['/usr/bin/c++', *flags['CXX_DEFINES'], *flags['CXX_INCLUDES'], *flags['CXX_FLAGS'],
     '-I'+str(ROOT), '-I'+str(OWNED), '-MMD', '-MF', str(OWNED/'capture.d'),
     '-c', str(OWNED/'ImpactSolver.cpp'), '-o', str(obj)])
archive = OWNED/'libjudas_engine.a'
shutil.copy2(BASE/'libjudas_engine.a', archive)
run(['ar', 'r', str(archive), str(obj)])
link = shlex.split((BUILD/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
link = [x for x in link if not x.startswith('-Wl,--dependency-file=')]
binary = OWNED/'judas_physics_stress_live_benchmark'
link[link.index('-o')+1] = str(binary)
link = [str(archive) if x == 'libjudas_engine.a' else x for x in link]
run(link, BUILD)
for name in ['ContactCapture.h', 'ImpactSolver.original.cpp', 'ImpactSolver.cpp',
             'ImpactSolver.cpp.o', 'capture.d', 'build.py', 'libjudas_engine.a', binary.name]:
    path = OWNED/name
    receipt['artifacts'][name] = {'sha256': sha(path), 'bytes': path.stat().st_size}
receipt['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
(OWNED/'BUILD.json').write_text(json.dumps(receipt, indent=2)+'\n')
snapshot = OWNED/'snapshot'
snapshot.mkdir(exist_ok=False)
(snapshot/'bin').mkdir()
shutil.copy2(binary, snapshot/'bin'/binary.name)
manifest = json.loads((BASE/'FROZEN.json').read_text())
manifest['classification'] = 'read-only, explicitly selected one-event exact-bit capture; no physics mutation'
manifest['base_manifest_sha256'] = sha(BASE/'FROZEN.json')
manifest['variant_receipt'] = str(OWNED/'BUILD.json')
manifest['variant_receipt_sha256'] = sha(OWNED/'BUILD.json')
manifest['artifacts']['bin/'+binary.name] = {'path': str(binary), 'sha256': sha(binary),
    'copy_sha256': sha(snapshot/'bin'/binary.name), 'bytes': binary.stat().st_size}
manifest['source_inputs']['src/ImpactSolver.cpp'] = receipt['artifacts']['ImpactSolver.cpp']
manifest['notes'].append('Exact baseline source archive plus isolated observer override in BUILD.json. Capture has no physical state writes; its one-time IO cost is not a performance result.')
with tarfile.open(snapshot/'isolated-overrides.tar.gz', 'w:gz') as tar:
    for name in ['ContactCapture.h', 'ImpactSolver.cpp', 'build.py', 'BUILD.json']:
        tar.add(OWNED/name, arcname=name)
manifest['isolated_overrides_archive_sha256'] = sha(snapshot/'isolated-overrides.tar.gz')
(snapshot/'FROZEN.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(json.dumps({'snapshot': str(snapshot), 'binary_sha256': sha(binary),
    'environment': {'JUDAS_CONTACT_CAPTURE_EVENT': '3470',
                    'JUDAS_CONTACT_CAPTURE_PATH': str(OWNED/'event-3470.json')}}, indent=2))
