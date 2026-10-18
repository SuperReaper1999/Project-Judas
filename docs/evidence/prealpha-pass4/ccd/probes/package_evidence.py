from pathlib import Path
import hashlib,json,shutil,subprocess
root=Path.cwd();cache=root/'.cache/prealpha-pass4/ccd';out=root/'docs/evidence/prealpha-pass4/ccd';out.mkdir(parents=True,exist_ok=True);(out/'.gitattributes').write_text('* -text\n')
variants=['episode-v1','episode-v2','episode-v3','episode-v4','episode-v5','episode-v6','episode-v6-front-fast','episode-v6-no-history']
for name in variants:
 source=cache/name;target=out/'experiments'/name;target.mkdir(parents=True,exist_ok=True)
 shutil.copytree(source/'sources',target/'sources',dirs_exist_ok=True)
 for path in source.glob('*.json'):shutil.copy2(path,target/path.name)
 for path in source.glob('*.log'):
  if path.stat().st_size:shutil.copy2(path,target/path.name)
 if (source/'uncanonical-failed').exists():shutil.copytree(source/'uncanonical-failed',target/'uncanonical-failed',dirs_exist_ok=True)
 if (source/'prepare.py').exists():shutil.copy2(source/'prepare.py',target/'prepare.py')
for folder in ['strict-v6-front-fast','review']:
 shutil.copytree(cache/folder,out/folder,dirs_exist_ok=True)
probes=out/'probes';probes.mkdir(exist_ok=True)
for name in ['build_variant.py','build_probe.py','compile_geometry_variant.py','EpisodeTrace.cpp','EpisodeGeometryProbe.cpp','EpisodePolicyEquivalence.cpp','EpisodeWorldLifecycle.cpp','MinimalContinuationRepro.cpp','compare_strict.py','package_evidence.py']:
 shutil.copy2(cache/name,probes/name)
base=out/'baseline-source';base.mkdir(exist_ok=True)
for name in ['src/PhysicsWorld.cpp','src/PhysicsWorld.h','src/CollisionGeometry.cpp','src/CollisionGeometry.h','src/Contacts.h','src/Narrowphase.h','src/CollisionShapes.h','src/ContactPreparedGeometry.h','tests/ImpactMeshCostTests.cpp']:
 (base/Path(name).name).write_bytes(subprocess.run(['git','show','7da56c2cc6ce7acda827a6e4019d610622b237e2:'+name],capture_output=True,check=True).stdout)
shutil.copy2(root/'.cache/prealpha-pass4/baseline/FROZEN.json',out/'BASELINE_FROZEN.json')
files=[]
for path in sorted(out.rglob('*')):
 if path.is_file() and path.name!='MANIFEST.json':files.append({'path':str(path.relative_to(out)),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'bytes':path.stat().st_size})
(out/'MANIFEST.json').write_text(json.dumps({'schema':1,'kind':'unshipped CCD research, no engine policy change','accepted_baseline':'7da56c2cc6ce7acda827a6e4019d610622b237e2','files':files},indent=2)+'\n')
print('files',len(files),'bytes',sum(x['bytes'] for x in files))
