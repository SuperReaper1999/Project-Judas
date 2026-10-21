from pathlib import Path
import os,json,subprocess,shutil,time,hashlib,tempfile
ROOT=Path.cwd();OUT=ROOT/'.cache/contact-redesign-20261021/application';OUT.mkdir(exist_ok=True)
records=[];env=os.environ.copy();env.update(SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(OUT/'userdata'),XDG_CACHE_HOME=str(OUT/'cache'),XDG_CONFIG_HOME=str(OUT/'config'))
def run(name,argv,extra=None,cwd=ROOT,timeout=120):
 e=env.copy();e.update(extra or {});log=OUT/(name+'.log');start=time.monotonic()
 with log.open('w') as stream:r=subprocess.run(list(map(str,argv)),env=e,cwd=cwd,stdout=stream,stderr=subprocess.STDOUT,timeout=timeout)
 records.append({'name':name,'argv':list(map(str,argv)),'cwd':str(cwd),'exit':r.returncode,'seconds':time.monotonic()-start,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()});(OUT/'RESULTS.json').write_text(json.dumps(records,indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode: raise SystemExit(r.returncode)
for mode in ['write','read','read-b']:
 run('save-stream-'+mode,[ROOT/'build/judas_save_streaming_tests',ROOT/'projects/streamed_range/streamed_range.judasproj',mode,OUT/'save-stream'],timeout=180)
project=ROOT/'projects/physics_stress_lab';owned=OUT/'editor-owned/physics_stress_lab';owned.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(project,owned)
run('editor-play-stop',[ROOT/'build/judas_editor',owned/'physics_stress_lab.judasproj'],{'JUDAS_EDITOR_AUTOTEST':str(OUT/'editor')},timeout=180)
package=OUT/'exported';run('export',[ROOT/'build/judas_export',project/'physics_stress_lab.judasproj',package,ROOT/'build/judas'])
moved=Path(tempfile.mkdtemp(prefix='Judas-incremental-impact-20261021-',dir='/tmp'));shutil.copytree(package,moved,dirs_exist_ok=True)
for p in moved.rglob('*'):
 if p.is_file():p.chmod(0o555 if os.access(p,os.X_OK) else 0o444)
for p in sorted((p for p in moved.rglob('*') if p.is_dir()),reverse=True):p.chmod(0o555)
moved.chmod(0o555)
steps=OUT/'standalone.steps';steps.write_text(f'FRAMES 120\nLOG_EVERY 30\nTAP F5 5\nSCREENSHOT 100 {OUT}/standalone.png\n')
run('relocated-standalone',[moved/'judas'],{'JUDAS_TEST_SCRIPT':str(steps)},Path('/tmp'),180)
(OUT/'PACKAGE.json').write_text(json.dumps({'relocatedPackage':str(moved),'unrelatedWorkingDirectory':'/tmp','installationReadOnly':True,'humanAcceptance':False,'audioListening':False},indent=2)+'\n')
