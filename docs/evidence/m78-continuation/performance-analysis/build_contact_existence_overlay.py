#!/usr/bin/env python3
"""Explicit isolated build recipe. Invoking this builds, but never runs or edits source.
Root must schedule this after the attribution measurement and other heavy jobs.
"""
import argparse,hashlib,json,pathlib,shlex,shutil,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[4]
p=argparse.ArgumentParser();p.add_argument('--overlay',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--build',type=pathlib.Path,default=ROOT/'.cache/m78-followup/release');p.add_argument('--archive',type=pathlib.Path);p.add_argument('--headers',type=pathlib.Path,default=ROOT/'src');p.add_argument('--observer',action='store_true',help='Build unchanged concave observer with only proposed Boolean query optimization')
a=p.parse_args();o=a.overlay.resolve();out=a.output.resolve();b=a.build.resolve();headers=a.headers.resolve();archive=a.archive.resolve() if a.archive else b/'libjudas_engine.a'
if out.exists():raise SystemExit('Output exists: preserve receipts and choose a new owned directory')
if not out.is_relative_to(ROOT/'.cache/m78-continuation'):raise SystemExit('Only owned continuation build outputs are permitted')
out.mkdir(parents=True);record=out/'BUILD.json';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
flags={}
for line in (b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
# Snapshot all headers used by engine compilation; never point quote includes
# at mutable source for a source/header pair changed by this proposal.
for f in headers.rglob('*'):
 if f.is_file() and f.suffix in ['.h','.hpp']:
  destination=out/f.relative_to(headers);destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,destination)
for name in ['CollisionGeometry.cpp','CollisionGeometry.h','PhysicsWorld.cpp','OriginalCookedContactOracle.cpp']:
 shutil.copy2(o/name,out/name)
base_hash=sha(archive);library=out/'libjudas_engine.a';shutil.copy2(archive,library)
commands=[];source_hashes={};
def invoke(command,name,cwd=None):
 log=out/(name+'.log')
 with log.open('wb') as f:code=subprocess.run(command,cwd=cwd or ROOT,stdout=f,stderr=subprocess.STDOUT).returncode
 commands.append({'command':command,'cwd':str(cwd or ROOT),'log':str(log),'exitCode':code})
 if code:write();raise SystemExit(code)
def compile(name):
 source=out/name;source_hashes[name]=sha(source);obj=out/(name+'.o')
 invoke(['c++',*flags.get('CXX_DEFINES',[]),'-I'+str(out),*flags.get('CXX_INCLUDES',[]),*flags.get('CXX_FLAGS',[]),'-c',str(source),'-o',str(obj)],name+'-compile')
 return obj
def write():
 result={'scope':'ISOLATED diagnostic proposal only; production source untouched; no simulation executed by build recipe','baseArchive':str(archive),'baseArchiveSHA256':base_hash,'overlayManifestSHA256':sha(o/'PROPOSAL.json'),'sources':source_hashes,'commands':commands}
 if library.exists():result['candidateArchiveSHA256']=sha(library)
 if (out/'probe').exists():result['executableSHA256']=sha(out/'probe')
 record.write_text(json.dumps(result,indent=2)+'\n')
obj=compile('CollisionGeometry.cpp');invoke(['ar','r',str(library),str(obj)],'geometry-archive')
if a.observer:
 obj=compile('PhysicsWorld.cpp');invoke(['ar','r',str(library),str(obj)],'physics-archive')
 shutil.copy2(ROOT/'docs/evidence/m78-followup/probes/concave_final.cpp',out/'Probe.cpp')
else:
 shutil.copy2(ROOT/'docs/evidence/m78-continuation/performance-analysis/contact_existence_parity.cpp',out/'Probe.cpp')
 oracle=compile('OriginalCookedContactOracle.cpp')
invoke(['ranlib',str(library)],'archive-index')
probe=compile('Probe.cpp')
target='judas_joint_tests';link=shlex.split((b/'CMakeFiles'/(target+'.dir')/'link.txt').read_text());replaced=False
for i,x in enumerate(link):
 if x.endswith('.cpp.o') and not replaced:link[i]=str(probe);replaced=True
 elif x=='libjudas_engine.a':link[i]=str(library)
 elif x==target:link[i]=str(out/'probe')
link=[x for x in link if not x.startswith('-Wl,--dependency-file')]
if not a.observer:link.insert(link.index(str(library)),str(oracle))
invoke(link,'probe-link',b);write();print(record)
