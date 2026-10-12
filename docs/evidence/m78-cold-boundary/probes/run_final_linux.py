#!/usr/bin/env python3
"""Focused final reruns, fresh owned outputs. Does not build or change fixtures."""
from pathlib import Path
import datetime,hashlib,json,os,shutil,subprocess,time
r=Path.cwd();ev=r/'docs/evidence/m78-cold-boundary';out=r/'.cache/m78-cold-boundary/final-linux'
if out.exists():raise SystemExit('Preserve existing run: output already exists')
out.mkdir(parents=True)
prior=r/'docs/evidence/m78-continuation';env=os.environ.copy();env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(out/'data'),XDG_CONFIG_HOME=str(out/'config'))
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
manifest=json.loads((prior/'FINAL_FINGERPRINTS.json').read_text())
baseline=json.loads((r/'docs/evidence/m78/BASELINE.json').read_text())['files']
for p in ['src/PhysicsWorld.cpp','src/JointSolver.cpp','src/PhysicalBalance.cpp','src/RigidBody.cpp','src/RigidMotion.h','src/CollisionGeometry.cpp','tests/BalanceLabTests.cpp','tests/PhysicalBalanceControllerTests.cpp','tests/ProductionFluidTests.cpp']:
 expected=manifest['files'][p]['sha256'] if p in manifest['files'] else baseline[p]
 if isinstance(expected,dict):expected=expected['sha256']
 if h(r/p)!=expected:raise SystemExit('Candidate changed: '+p)
rows=[]
def run(name,old=None,expected=0,command=None):
 receipt=json.loads((prior/('midpoint-final-'+(old or name)+'.json')).read_text()) if command is None else None
 cmd=list(receipt['command']) if receipt else command
 if receipt:
  if h(r/cmd[0] if not Path(cmd[0]).is_absolute() else cmd[0])!=receipt['executableSHA256']:raise SystemExit('Binary changed: '+cmd[0])
  if len(cmd)>1:
   oldpath=Path(cmd[1]);key=oldpath.name
   if name.startswith('balance_cold-stand'):key='balance-cold-stand'
   elif name.startswith('balance_cold-recovery'):key='balance-cold-recovery'
   elif name.startswith('character_cold'):key='character-cold'
   new=out/key
   if name=='navigation':
    source=oldpath.parent;dest=out/'navigation';shutil.copytree(source,dest);cmd[1]=str(dest/oldpath.name)
   else:cmd[1]=str(new)
 start=time.perf_counter();p=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
 log=ev/('linux-final-'+name+'.log');log.write_text(p.stdout)
 record={'name':name,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'command':cmd,'exitCode':p.returncode,'expectedExit':expected,'seconds':time.perf_counter()-start,'executableSHA256':h(r/cmd[0] if not Path(cmd[0]).is_absolute() else cmd[0]),'logSHA256':h(log),'unchangedCandidateManifestSHA256':h(prior/'FINAL_FINGERPRINTS.json')}
 (ev/('linux-final-'+name+'.json')).write_text(json.dumps(record,indent=2)+'\n');rows.append(record)
 print(name,p.returncode,round(record['seconds'],3),flush=True)
 if p.returncode!=expected:raise SystemExit('Unexpected outcome; preserve evidence '+name)
for n in ['controller','motion','physical_balance_foundation','physical_animation','joint','ragdoll','contact_lifecycle','impact_timing','gravity_selection','world_persistence','prefab','streaming','navigation']:
 run(n)
for n in ['application-step','balance_lifecycle','balance_radial','balance_cold-stand_write','balance_cold-stand_read','balance_cold-recovery_write','balance_cold-recovery_read','character_application','character_cold_write','character_cold_read']:
 run(n)
exact=out/'exact-native-snapshot';(exact/'slots').mkdir(parents=True);shutil.copytree(r/'projects/balance_lab',exact/'project')
source=prior/'repro/m77-native-in-progress-step-failed.save';shutil.copyfile(source,exact/'slots/m77.save')
expected='a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b'
if h(source)!=expected:raise SystemExit('Original snapshot changed')
run('exact-native-cold-read',expected=1,command=[str(r/'.cache/m78-followup/release/judas_balance_lab_tests'),str(exact),'cold-read'])
if h(source)!=expected or h(exact/'slots/m77.save')!=expected:raise SystemExit('Snapshot changed during read')
(ev/'LINUX_FINAL_MATRIX.json').write_text(json.dumps({'scope':'Focused reruns on unchanged combined candidate; retained exact native cold failure remains a failure. No production-fluid tests/thresholds or full-suite rerun.','results':rows},indent=2)+'\n')
