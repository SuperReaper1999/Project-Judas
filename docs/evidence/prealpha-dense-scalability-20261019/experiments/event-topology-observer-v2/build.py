#!/usr/bin/env python3
"""Compile the prepared read-only event topology observer into baseline e30."""
import datetime,hashlib,json,shlex,shutil,subprocess,tarfile
from pathlib import Path
owned=Path(__file__).resolve().parent
root=owned.parents[2]; build=root/'build'; base=owned.parent/'baseline-e30'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
receipt={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'base_manifest_sha256':sha(base/'FROZEN.json'),'commands':[],'artifacts':{}}
def run(command,cwd=root):
    receipt['commands'].append({'argv':list(map(str,command)),'cwd':str(cwd)})
    subprocess.run(command,cwd=cwd,check=True)
flags={}
for line in (build/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key,value=line.split(' = ',1);flags[key]=shlex.split(value)
obj=owned/'WorldImpactEvents.cpp.o'
run(['/usr/bin/c++',*flags['CXX_DEFINES'],'-I'+str(root),'-I'+str(owned/'include/src/physics'),
    '-I'+str(owned/'include/src'),'-I'+str(owned),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],
    '-MMD','-MF',str(owned/'observer.d'),'-c',str(owned/'WorldImpactEvents.cpp'),'-o',str(obj)])
archive=owned/'libjudas_engine.a';shutil.copy2(base/'libjudas_engine.a',archive)
run(['ar','r',str(archive),str(obj)])
link=shlex.split((build/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
binary=owned/'judas_physics_stress_live_benchmark'
link[link.index('-o')+1]=str(binary)
link=[str(archive) if x=='libjudas_engine.a' else x for x in link]
run(link,build)
for name in ['EventTopologyObserver.h','PairScheduleCapture.h','ContactCapture.h','WorldImpactEvents.cpp','WorldImpactEvents.original.cpp',
             'WorldImpactEvents.cpp.o','libjudas_engine.a',binary.name,'prepare.py','build.py']:
    p=owned/name;receipt['artifacts'][name]={'sha256':sha(p),'bytes':p.stat().st_size}
receipt['frozen_headers']={str(p.relative_to(owned)):{'sha256':sha(p),'bytes':p.stat().st_size}
                         for p in sorted((owned/'include').rglob('*')) if p.is_file()}
receipt['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
(owned/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
snapshot=owned/'snapshot';snapshot.mkdir(exist_ok=False);(snapshot/'bin').mkdir()
shutil.copy2(binary,snapshot/'bin'/binary.name)
manifest=json.loads((base/'FROZEN.json').read_text())
manifest['classification']='read-only selected chronological-event topology/source observer; no geometry query or physical mutation'
manifest['base_manifest_sha256']=sha(base/'FROZEN.json')
manifest['variant_receipt']=str(owned/'BUILD.json');manifest['variant_receipt_sha256']=sha(owned/'BUILD.json')
manifest['artifacts']['bin/'+binary.name]={'path':str(binary),'sha256':sha(binary),
    'copy_sha256':sha(snapshot/'bin'/binary.name),'bytes':binary.stat().st_size}
manifest['source_inputs']['src/physics/WorldImpactEvents.cpp']=receipt['artifacts']['WorldImpactEvents.cpp']
manifest['notes'].append('Baseline source archive plus one-TU event observer; frozen baseline headers used. Observer IO/classification cost is not a performance measurement.')
with tarfile.open(snapshot/'isolated-overrides.tar.gz','w:gz') as tar:
    for name in ['EventTopologyObserver.h','PairScheduleCapture.h','ContactCapture.h','WorldImpactEvents.cpp','prepare.py','build.py','BUILD.json']:
        tar.add(owned/name,arcname=name)
manifest['isolated_overrides_archive_sha256']=sha(snapshot/'isolated-overrides.tar.gz')
(snapshot/'FROZEN.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'snapshot':str(snapshot),'binary_sha256':sha(binary),
                 'environment':{'JUDAS_EVENT_TOPOLOGY_OUTPUT':str(owned/'events.jsonl'),
                    'JUDAS_TOI_PAIR_TRACE_OUTPUT':str(owned/'pair-trace.jsonl'),
                    'JUDAS_TOI_PAIR_TRACE_PAIR':'595,851','JUDAS_TOI_PAIR_TRACE_FIRST_STEP':'270'}},indent=2))
