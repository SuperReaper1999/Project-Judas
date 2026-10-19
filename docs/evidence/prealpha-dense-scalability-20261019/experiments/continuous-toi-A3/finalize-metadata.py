#!/usr/bin/env python3
"""Finish frozen metadata after an artifact-list type error; no compilation."""
from pathlib import Path
import hashlib,json,shutil,tarfile
owned=Path(__file__).resolve().parent;base=owned.parent/'candidate-fastpaths-final'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
receipt_path=owned/'WORLD_BUILD.json'
partial_path=owned/'INITIAL_PARTIAL_WORLD_BUILD.json';shutil.copy2(receipt_path,partial_path)
failed=owned/'build-world.initial-failed.py'
failed.write_text((owned/'build-world.py').read_text().replace("manifest['explicit_extra_source_inputs'].append('isolated-overrides:ContinuousBoxCCD.h')","manifest['explicit_extra_source_inputs']['continuous-toi-prototype/ContinuousBoxCCD.h']=receipt['files']['ContinuousBoxCCD.h']"))
receipt=json.loads(receipt_path.read_text())
for name in ['WorldCCDSearch.cpp','ContinuousBoxCCD.h','WorldCCDSearch.cpp.o','libjudas_engine.a','judas_physics_stress_live_benchmark','judas_physics_stress_live_benchmark-normal']:
 if sha(owned/name)!=receipt['files'][name]['sha256']:raise RuntimeError('actual compiled input/artifact mismatch: '+name)
receipt['initial_metadata_failure']={'log':'world-build.log','cause':'extra-source inventory is a list, not dict; compile and both links had already succeeded',
 'preserved_partial_receipt_sha256':sha(partial_path),'executed_initial_script_reconstructed_sha256':sha(failed)}
receipt['files']['build-world.py']={'sha256':sha(owned/'build-world.py'),'bytes':(owned/'build-world.py').stat().st_size}
receipt['metadata_finalizer']={'sha256':sha(Path(__file__)),'path':str(Path(__file__))}
receipt_path.write_text(json.dumps(receipt,indent=2)+'\n')
manifest=json.loads((base/'FROZEN.json').read_text())
manifest['classification']='Numerical research: final parity-proven fastpaths plus continuous Cayley separation/prefix/gap-speed retiming with literal original native-query fallback; actual manifold required'
manifest['base_manifest_sha256']=sha(base/'FROZEN.json');manifest['override_archive_sha256']=sha(owned/'libjudas_engine.a')
manifest['variant_receipt']=str(receipt_path);manifest['variant_receipt_sha256']=sha(receipt_path)
manifest['source_inputs']['src/physics/WorldCCDSearch.cpp']=receipt['files']['WorldCCDSearch.cpp']
manifest['explicit_extra_source_inputs'].append('isolated-overrides:ContinuousBoxCCD.h')
manifest['notes'].append('Shared search kernel retries native from original start on unresolved precise paths. Per-attempt 64-step and 32-bisection budgets unchanged; work can include both attempts. New phase retains native positive-gap advancement and native-manifold availability boundary, so this is research, not complete continuous-CCD proof.')
binary=owned/'judas_physics_stress_live_benchmark';snapshot=owned/'snapshot'
if sha(snapshot/'bin'/binary.name)!=sha(binary):raise RuntimeError('quality binary copy mismatch')
manifest['artifacts']['bin/'+binary.name]={'path':str(binary),'sha256':sha(binary),'copy_sha256':sha(snapshot/'bin'/binary.name),'bytes':binary.stat().st_size}
with tarfile.open(snapshot/'isolated-overrides.tar.gz','w:gz') as tar:
 for n in ['WorldCCDSearch.original.cpp','WorldCCDSearch.cpp','ContinuousBoxCCD.h','build-world.py','build-world.initial-failed.py','finalize-metadata.py','WORLD_BUILD.json','INITIAL_PARTIAL_WORLD_BUILD.json','world-build.log']:
  tar.add(owned/n,arcname=n)
manifest['isolated_overrides_archive_sha256']=sha(snapshot/'isolated-overrides.tar.gz')
(snapshot/'FROZEN.json').write_text(json.dumps(manifest,indent=2)+'\n')
normal=owned/'judas_physics_stress_live_benchmark-normal';normalSnapshot=owned/'snapshot-normal';(normalSnapshot/'bin').mkdir(parents=True,exist_ok=False)
shutil.copy2(normal,normalSnapshot/'bin'/binary.name)
normalManifest=json.loads(json.dumps(manifest));normalManifest['classification']+='; ordinary benchmark driver without temporal-quality observer'
normalManifest['artifacts']['bin/'+binary.name]={'path':str(normal),'sha256':sha(normal),'copy_sha256':sha(normalSnapshot/'bin'/binary.name),'bytes':normal.stat().st_size}
(normalSnapshot/'FROZEN.json').write_text(json.dumps(normalManifest,indent=2)+'\n')
print(json.dumps({'quality_snapshot':str(snapshot),'normal_snapshot':str(normalSnapshot),
 'quality_binary_sha256':sha(binary),'normal_binary_sha256':sha(normal),'archive_sha256':sha(owned/'libjudas_engine.a')},indent=2))
