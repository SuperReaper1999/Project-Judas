#!/usr/bin/env python3
"""One frozen final-fastpath TU replacement for continuous primitive TOI research."""
from pathlib import Path
import datetime,hashlib,json,shlex,subprocess,shutil,tarfile
owned=Path(__file__).resolve().parent;root=owned.parents[2];build=root/'build';base=owned.parent/'candidate-fastpaths-final'
frozen=owned.parent/'event-topology-observer-v2/include'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
flags={}
for line in (build/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
commands=[]
def run(cmd,cwd=root):
 commands.append({'argv':list(map(str,cmd)),'cwd':str(cwd)});subprocess.run(cmd,cwd=cwd,check=True)
obj=owned/'WorldCCDSearch.cpp.o'
run(['/usr/bin/c++',*flags['CXX_DEFINES'],'-I'+str(root),'-I'+str(frozen/'src/physics'),
 '-I'+str(frozen/'src'),'-I'+str(owned),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],
 '-MMD','-MF',str(owned/'world.d'),'-c',str(owned/'WorldCCDSearch.cpp'),'-o',str(obj)])
archive=owned/'libjudas_engine.a';shutil.copy2(base/'libjudas_engine.a',archive)
run(['ar','r',str(archive),str(obj)])
# Reuse the exact baseline quality-driver object. Base engine contains the final
# parity-proven geometry fastpaths; the CCD query is the only numerical override.
link=shlex.split((build/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
link=[str(owned.parent/'quality-prototype/baseline/PhysicsStressLiveBenchmark.cpp.o') if x.endswith('PhysicsStressLiveBenchmark.cpp.o') else x for x in link]
link=[str(archive) if x=='libjudas_engine.a' else x for x in link]
binary=owned/'judas_physics_stress_live_benchmark';link[link.index('-o')+1]=str(binary)
run(link,build)
# Also link the ordinary frozen-input live benchmark driver for matched timings.
normalLink=shlex.split((build/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
normalLink=[x for x in normalLink if not x.startswith('-Wl,--dependency-file=')]
normalLink=[str(archive) if x=='libjudas_engine.a' else x for x in normalLink]
normalBinary=owned/'judas_physics_stress_live_benchmark-normal'
normalLink[normalLink.index('-o')+1]=str(normalBinary)
run(normalLink,build)
files=['WorldCCDSearch.original.cpp','WorldCCDSearch.cpp','ContinuousBoxCCD.h','build-world.py','WorldCCDSearch.cpp.o','libjudas_engine.a',binary.name,normalBinary.name]
receipt={'schema':'judas.continuous-toi-prototype.build.v1','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'base_manifest_sha256':sha(base/'FROZEN.json'),'base_archive_sha256':sha(base/'libjudas_engine.a'),
 'commands':commands,'files':{n:{'sha256':sha(owned/n),'bytes':(owned/n).stat().st_size} for n in files}}
(owned/'WORLD_BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
snapshot=owned/'snapshot';(snapshot/'bin').mkdir(parents=True,exist_ok=False)
shutil.copy2(binary,snapshot/'bin'/binary.name)
manifest=json.loads((base/'FROZEN.json').read_text())
manifest['classification']='Numerical research: final parity-proven fastpaths plus continuous Cayley separation/prefix/gap-speed retiming with literal original native-query fallback; actual manifold required'
manifest['base_manifest_sha256']=sha(base/'FROZEN.json')
manifest['override_archive_sha256']=sha(archive)
manifest['variant_receipt']=str(owned/'WORLD_BUILD.json');manifest['variant_receipt_sha256']=sha(owned/'WORLD_BUILD.json')
manifest['artifacts']['bin/'+binary.name]={'path':str(binary),'sha256':sha(binary),'copy_sha256':sha(snapshot/'bin'/binary.name),'bytes':binary.stat().st_size}
manifest['source_inputs']['src/physics/WorldCCDSearch.cpp']=receipt['files']['WorldCCDSearch.cpp']
manifest['explicit_extra_source_inputs'].append('isolated-overrides:ContinuousBoxCCD.h')
manifest['notes'].append('Shared search kernel retries native from original start on unresolved precise paths. Individual 64-step and 32-bisection budgets unchanged; diagnostic work may include both attempts.')
with tarfile.open(snapshot/'isolated-overrides.tar.gz','w:gz') as tar:
 for n in files[:4]+['WORLD_BUILD.json']:tar.add(owned/n,arcname=n)
manifest['isolated_overrides_archive_sha256']=sha(snapshot/'isolated-overrides.tar.gz')
(snapshot/'FROZEN.json').write_text(json.dumps(manifest,indent=2)+'\n')
normalSnapshot=owned/'snapshot-normal';(normalSnapshot/'bin').mkdir(parents=True,exist_ok=False)
shutil.copy2(normalBinary,normalSnapshot/'bin'/binary.name)
normalManifest=json.loads(json.dumps(manifest))
normalManifest['classification']+='; ordinary benchmark driver without temporal-quality observer'
normalManifest['artifacts']['bin/'+binary.name]={'path':str(normalBinary),'sha256':sha(normalBinary),'copy_sha256':sha(normalSnapshot/'bin'/binary.name),'bytes':normalBinary.stat().st_size}
normalManifest['variant_receipt']=str(owned/'WORLD_BUILD.json')
(normalSnapshot/'FROZEN.json').write_text(json.dumps(normalManifest,indent=2)+'\n')
print(json.dumps({'quality_snapshot':str(snapshot),'quality_binary_sha256':sha(binary),
 'normal_snapshot':str(normalSnapshot),'normal_binary_sha256':sha(normalBinary),'archive':str(archive)},indent=2))
