#!/usr/bin/env python3
"""Final closure editor/export/moved smoke; owned copies, preserved legacy failure separately rerun."""
from pathlib import Path
import datetime, hashlib, json, os, shlex, shutil, subprocess, time
r=Path.cwd(); base=r/'.cache/m78-followup/release'; build=r/'.cache/m78-closure/final-build'
out=r/'.cache/m78-closure/final-smokes'; ev=r/'docs/evidence/m78-closure/final-smokes'
out.mkdir(parents=True,exist_ok=False); ev.mkdir(parents=True,exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
identity=json.loads((r/'docs/evidence/m78-closure/final-build/BUILD.json').read_text())
for p,v in identity['sourceSHA256'].items(): assert h(r/p)==v, p
archive=build/'libjudas_engine.a'; assert h(archive)==identity['archive']['sha256']
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(out/'data'),XDG_CONFIG_HOME=str(out/'config'))
rows=[]
def run(name,cmd,cwd=r,extra={},expected=0):
 start=time.perf_counter(); p=subprocess.run(list(map(str,cmd)),cwd=cwd,env=dict(env,**extra),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
 log=ev/(name+'.log'); log.write_text(p.stdout)
 row=dict(name=name,command=list(map(str,cmd)),cwd=str(cwd),environment=dict(extra),utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),exitCode=p.returncode,expectedExit=expected,programPassed=p.returncode==0,seconds=time.perf_counter()-start,logSHA256=h(log))
 rows.append(row);(ev/'RESULTS.json').write_text(json.dumps(dict(archiveSHA256=h(archive),sourceSHA256=identity['sourceSHA256'],results=rows),indent=2)+'\n')
 print(name,p.returncode,flush=True)
 if p.returncode!=expected: raise SystemExit('Unexpected outcome; preserve '+str(log))
editor=out/'editor-project';shutil.copytree(r/'projects/balance_lab',editor)
run('editor-play-stop',[build/'judas_editor',editor/'balance_lab.judasproj'],extra={'JUDAS_EDITOR_AUTOTEST':str(out/'editor-evidence')})
package=out/'package';run('export',[base/'judas_export',r/'projects/balance_lab/balance_lab.judasproj',package,build/'judas'])
moved=Path('/tmp/judas-m78-closure-moved');assert not moved.exists();shutil.copytree(package,moved)
harness=out/'startup.harness';s=(r/'.cache/m78-continuation/final-startup.harness').read_text();s=s.replace(str(r/'docs/evidence/m78-continuation/linux-final-startup.png'),str(ev/'linux-startup.png'));harness.write_text(s)
run('moved-standalone',[moved/'judas'],Path('/tmp'),{'JUDAS_ENGINE_ROOT':'/tmp/unavailable-m78-engine-root','JUDAS_TEST_SCRIPT':str(harness)})
legacy=out/'legacy-a106';(legacy/'slots').mkdir(parents=True);shutil.copytree(r/'projects/balance_lab',legacy/'project')
snapshot=r/'docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save';shutil.copyfile(snapshot,legacy/'slots/m77.save');assert h(snapshot)=='a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b'
run('legacy-a106',[build/'judas_balance_lab_tests',legacy,'cold-read'],expected=1)
assert h(snapshot)==h(legacy/'slots/m77.save')
data=json.loads((ev/'RESULTS.json').read_text());data['legacyClassification']='Original snapshot still fails; expected exit is not a validation pass.';data['executableSHA256']={str((build/t).relative_to(r)):h(build/t) for t in ['judas','judas_editor','judas_navigation_dependency_tests','judas_balance_lab_tests']};data['movedRuntimeMatchesBuilt']=h(moved/'judas')==h(build/'judas');(ev/'RESULTS.json').write_text(json.dumps(data,indent=2)+'\n')
