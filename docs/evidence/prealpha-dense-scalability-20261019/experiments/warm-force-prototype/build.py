#!/usr/bin/env python3
"""Isolate next-step regular-support guess retention, with original CCD intact."""
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
BUILD = ROOT/'build'
BASE = OWNED.parent/'baseline-e30'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
receipt = {'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(), 'commands':[],
           'base_manifest_sha256':sha(BASE/'FROZEN.json'), 'artifacts':{},
           'scope':'one-TU isolated change: preserve ordinary supporting-force guesses for next step; never use TOI lambda'}
def run(argv,cwd=ROOT):
    receipt['commands'].append({'argv':list(map(str,argv)), 'cwd':str(cwd)})
    subprocess.run(argv,cwd=cwd,check=True)
with tarfile.open(BASE/'source-inputs.tar.gz') as tar:
    source = tar.extractfile('src/physics/WorldImpactEvents.cpp').read().decode()
    for member in tar:
        if member.isfile() and member.name.startswith('src/') and member.name.endswith(('.h','.inl')):
            path = OWNED/'include'/member.name
            path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(tar.extractfile(member).read())
(OWNED/'WorldImpactEvents.original.cpp').write_text(source)
source = source.replace('#include "ImpactCandidateCache.h"',
    '#include "ImpactCandidateCache.h"\n#include "RetainedSupportGuesses.h"',1)
publication = '    ObservationPublication cachePublication{contactCache, work.continuationCache, work.active};'
assert source.count(publication)==1
source = source.replace(publication,publication+'''
    // Published immediately before AdvanceImpacts by the ordinary force solve.
    // This immutable snapshot is an initial guess for the NEXT fixed step only.
    RetainedSupportGuesses supportingForceGuesses(contactCache);''',1)
old = '''                // A velocity-changing island invalidates its old load guesses,
                // not contact identity. New actual TOI features also need zero-load
                // identity for next step's ordinary geometric support matching.
                // Neither old warm impulses nor impact impulses are replayed here.
                for (auto &cached : contactCache)
                    if (included[cached.key.slotA] || included[cached.key.slotB]) {
                        cached.normalImpulse = 0;
                        cached.tangentImpulse = glm::vec3(0);
                    }
'''
assert source.count(old)==1
source = source.replace(old,'''                // Preserve the ordinary force solve's next-step guesses for
                // surviving contact features. The current chronological solve
                // is still cold and no impact impulse enters this warm cache.
''',1)
old = '''                    const auto &contactRows = impactSolver.Constraints();
                    for (std::size_t i = 0; i < contactRows.size(); ++i) {'''
assert source.count(old)==1
source = source.replace(old,'''                    const auto &contactRows = impactSolver.Constraints();
                    supportingForceGuesses.BeginMatchPass();
                    for (std::size_t i = 0; i < contactRows.size(); ++i) {''',1)
old = '''                        contactCache.push_back({key, row.localAnchorA, row.normal, 0, glm::vec3(0),
                                                float(row.signedSeparation),
                                                bodies[key.slotA].contactObservationRevision,
                                                bodies[key.slotB].contactObservationRevision});'''
assert source.count(old)==1
source = source.replace(old,'''                        const auto guess = supportingForceGuesses.Match(
                            key, row.localAnchorA, row.normal,
                            bodies[key.slotA].contactObservationRevision,
                            bodies[key.slotB].contactObservationRevision);
                        contactCache.push_back({key, row.localAnchorA, row.normal,
                                                guess.first, guess.second,
                                                float(row.signedSeparation),
                                                bodies[key.slotA].contactObservationRevision,
                                                bodies[key.slotB].contactObservationRevision});''',1)
old = '''            } else {
                contactCache.erase(std::remove_if(contactCache.begin(), contactCache.end(),'''
assert source.count(old)==1
source = source.replace(old,'''            } else {
                supportingForceGuesses.InvalidateBodies(included);
                contactCache.erase(std::remove_if(contactCache.begin(), contactCache.end(),''',1)
path = OWNED/'WorldImpactEvents.cpp'; path.write_text(source)
flags = {}
for line in (BUILD/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        key,value=line.split(' = ',1); flags[key]=shlex.split(value)
obj=OWNED/'WorldImpactEvents.cpp.o'
run(['/usr/bin/c++',*flags['CXX_DEFINES'],
     '-I'+str(OWNED/'include/src/physics'),'-I'+str(OWNED/'include/src'),'-I'+str(OWNED),
     *flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-MMD','-MF',str(OWNED/'warm-force.d'),
     '-c',str(path),'-o',str(obj)])
archive=OWNED/'libjudas_engine.a'; shutil.copy2(BASE/'libjudas_engine.a',archive)
run(['ar','r',str(archive),str(obj)])
link=shlex.split((BUILD/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
binary=OWNED/'judas_physics_stress_live_benchmark'
link[link.index('-o')+1]=str(binary)
link=[str(archive) if x=='libjudas_engine.a' else x for x in link]
run(link,BUILD)
for name in ['RetainedSupportGuesses.h','WorldImpactEvents.original.cpp','WorldImpactEvents.cpp',
             'WorldImpactEvents.cpp.o','build.py','libjudas_engine.a',binary.name]:
    path=OWNED/name; receipt['artifacts'][name]={'sha256':sha(path),'bytes':path.stat().st_size}
receipt['frozen_headers']={str(p.relative_to(OWNED)):{'sha256':sha(p),'bytes':p.stat().st_size}
                          for p in sorted((OWNED/'include').rglob('*')) if p.is_file()}
receipt['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
(OWNED/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
snapshot=OWNED/'snapshot'; snapshot.mkdir(exist_ok=False); (snapshot/'bin').mkdir()
shutil.copy2(binary,snapshot/'bin'/binary.name)
manifest=json.loads((BASE/'FROZEN.json').read_text())
manifest['classification']='isolated next-step ordinary-support force-guess retention; numerical trajectories may change'
manifest['base_manifest_sha256']=sha(BASE/'FROZEN.json')
manifest['variant_receipt']=str(OWNED/'BUILD.json'); manifest['variant_receipt_sha256']=sha(OWNED/'BUILD.json')
manifest['artifacts']['bin/'+binary.name]={'path':str(binary),'sha256':sha(binary),
    'copy_sha256':sha(snapshot/'bin'/binary.name),'bytes':binary.stat().st_size}
manifest['source_inputs']['src/physics/WorldImpactEvents.cpp']=receipt['artifacts']['WorldImpactEvents.cpp']
manifest['notes'].append('Baseline source archive plus isolated replacement/helper in BUILD.json; frozen baseline headers used before live includes. Contact and CCD kernels unchanged.')
with tarfile.open(snapshot/'isolated-overrides.tar.gz','w:gz') as tar:
    for name in ['RetainedSupportGuesses.h','WorldImpactEvents.cpp','build.py','BUILD.json']:
        tar.add(OWNED/name,arcname=name)
manifest['isolated_overrides_archive_sha256']=sha(snapshot/'isolated-overrides.tar.gz')
(snapshot/'FROZEN.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'snapshot':str(snapshot),'binary_sha256':sha(binary),'archive_sha256':sha(archive)},indent=2))
