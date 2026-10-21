from pathlib import Path
import subprocess,json,shutil,hashlib,datetime
r=Path(__file__).resolve().parents[2];p=Path(__file__).resolve().parent;old=r/'.cache/physics-hitch-followup-20261021'
sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
base=p/'baseline';base.mkdir(exist_ok=True)
fixture=r/'projects/physics_stress_lab/Scenes/room1024.judas'
assert sha(fixture)=='a2caf62a295243d301149202c8a0a5eedafff256d68de0e3cda988846b2e05bf'
assert sha(p/'accepted.a')=='eab321ffb7a997355987a42229424f1953765979164a6da94d1e2d65787781e5'
(base/'FROZEN.json').write_text(json.dumps({'accepted_head':'45f0614fa2eb21a6dc534d555f8abc87a31f4901','fixture':{'path':str(fixture),'sha256':sha(fixture)},'core_sha256':sha(p/'accepted.a')},indent=2)+'\n')
runner=p/'run_current.py';shutil.copy2(old/'run_current.py',runner)
out=p/'combined-driver';out.mkdir();shutil.copy2(r/'build/libjudas_engine.a',out/'libjudas_engine.a')
a=json.loads((old/'FINAL_DRIVER_BUILDS.json').read_text())[1]['link'];a=[str(out/'live') if x==str(old/'combined/live-v4') else str(out/'libjudas_engine.a') if x==str(r/'build/libjudas_engine.a') else x for x in a]
with (out/'link.log').open('w') as log:subprocess.run(a,cwd=r/'build',stdout=log,stderr=subprocess.STDOUT,check=True)
(out/'BUILD.json').write_text(json.dumps({'link':a,'core_sha256':sha(out/'libjudas_engine.a'),'binary_sha256':sha(out/'live'),'driver_source_sha256':sha(old/'diagnostic-build/DiagnosticLive-v4.cpp')},indent=2)+'\n')
plans=[('root-accepted1800-a','accepted','1800','stationary',False),('root-candidate1800-a','candidate','1800','stationary',False),('root-candidate1800-b','candidate','1800','stationary',False),('root-accepted1800-b','accepted','1800','stationary',False),('root-accepted-wasd35','accepted','live35','wasd',False),('root-candidate-wasd35','candidate','live35','wasd',False),('root-candidate-profile900','candidate','900','stationary',True)]
results=[]
for name,which,mode,camera,profile in plans:
 binary=p/'accepted-live' if which=='accepted' else out/'live';core=p/'accepted.a' if which=='accepted' else out/'libjudas_engine.a'
 args=['python3',str(runner),'--binary',str(binary),'--core',str(core),'--output',str(p/name),'--mode',mode,'--camera',camera,'--extra-source',str(r/'src/physics/WorldCCDQuery.h'),'--extra-source',str(r/'src/physics/WorldCCDDispatch.cpp'),'--extra-source',str(r/'src/physics/ContactGroupUpdates.h'),'--execute']
 if profile:args.extend(['--profile','--stage-capture'])
 with (p/(name+'.log')).open('w') as log:code=subprocess.run(args,cwd=r,stdout=log,stderr=subprocess.STDOUT).returncode
 results.append({'name':name,'exit':code,'argv':args,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat()});(p/'COMBINED_RUNS.json').write_text(json.dumps(results,indent=2)+'\n');print(name,code,flush=True)
 if code:raise SystemExit(code)
