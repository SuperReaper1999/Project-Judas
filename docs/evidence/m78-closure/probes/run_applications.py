#!/usr/bin/env python3
"""Final combined closure focused Linux matrix; unchanged controller settings and fixtures; legacy failure remains classified."""
from pathlib import Path
import datetime,hashlib,json,os,re,shutil,subprocess,time
r=Path.cwd();build=r/'.cache/m78-closure/final-build';out=r/'.cache/m78-closure/final-application-cases';ev=r/'docs/evidence/m78-closure/final-applications'
out.mkdir(parents=True,exist_ok=False);ev.mkdir(parents=True,exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
identity=json.loads((r/'docs/evidence/m78-closure/final-build/BUILD.json').read_text())
for p,v in identity['sourceSHA256'].items():assert h(r/p)==v,'source changed '+p
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(out/'data'),XDG_CONFIG_HOME=str(out/'config'))
for key in list(env):
 if key.startswith(('M78_DIAGNOSTIC_','JUDAS_DIAGNOSTIC_','JUDAS_BOUNDARY_')):del env[key]
rows=[]
def run(name,target,args=[],expected=0):
 exe=build/('judas_'+target+'_tests');cmd=[str(exe),*map(str,args)];log=ev/(name+'.log')
 if log.exists():raise SystemExit('Preserve old receipt '+str(log))
 start=time.perf_counter();p=subprocess.run(cmd,cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180);log.write_text(p.stdout)
 row=dict(name=name,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),command=cmd,exitCode=p.returncode,expectedExit=expected,programPassed=p.returncode==0,seconds=time.perf_counter()-start,executableSHA256=h(exe),logSHA256=h(log),summary=[x for x in p.stdout.splitlines()if 'SUMMARY'in x or re.search(r'\d+ checks.*\d+ failures',x)])
 if name=='legacy-a106':row['classification']='Preserved genuine legacy failure; NOT a pass or repaired legacy archive';row['snapshotSHA256']=h(args[0]/'slots/m77.save')
 rows.append(row);(ev/'LINUX_MATRIX.json').write_text(json.dumps(dict(scope=__doc__,buildSHA256=h(r/'docs/evidence/m78-closure/final-build/BUILD.json'),results=rows),indent=2)+'\n');print(name,p.returncode,row['summary'][-1:] if row['summary'] else '',flush=True)
 if p.returncode!=expected:raise SystemExit('Unexpected outcome; investigate preserved '+str(log))
run('gravity_selection','gravity_selection',[out/'gravity'])
for mode in ['step-trace','lifecycle','radial']:
 run('balance_'+mode,'balance_lab',[out/('balance-'+mode),mode])
for prefix in ['cold','cold-stand','cold-recovery']:
 for phase in ['write','read']:
  run('balance_'+prefix+'-'+phase,'balance_lab',[out/('balance-'+prefix),prefix+'-'+phase])
run('character_application','character_lab',[out/'character'])
for phase in ['write','read']:run('character_cold-'+phase,'character_lab',[out/'character-cold','cold-'+phase])
legacy=out/'legacy-a106';(legacy/'slots').mkdir(parents=True);shutil.copytree(r/'projects/balance_lab',legacy/'project');original=r/'docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save';shutil.copyfile(original,legacy/'slots/m77.save')
assert h(original)=='a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b'
run('legacy-a106','balance_lab',[legacy,'cold-read'],expected=1)
assert h(original)==h(legacy/'slots/m77.save')
