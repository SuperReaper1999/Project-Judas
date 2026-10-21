from pathlib import Path
import subprocess,os,json,hashlib,tempfile,shutil,time
r=Path.cwd();o=r/'.cache/contact-redesign-20261021/final-export';o.mkdir(exist_ok=True);env=os.environ.copy();env.update(SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(o/'userdata'),XDG_CACHE_HOME=str(o/'cache'),XDG_CONFIG_HOME=str(o/'config'));results=[]
def run(name,cmd,cwd=r,extra=None):
 e=env.copy();e.update(extra or {});t=time.monotonic()
 with(o/(name+'.log')).open('w')as f:c=subprocess.run(list(map(str,cmd)),cwd=cwd,env=e,stdout=f,stderr=subprocess.STDOUT,timeout=90).returncode
 results.append({'name':name,'argv':list(map(str,cmd)),'cwd':str(cwd),'exit':c,'seconds':time.monotonic()-t});(o/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n')
 if c:raise SystemExit(c)
run('export',[r/'build/judas_export',r/'projects/physics_stress_lab/physics_stress_lab.judasproj',o/'package',r/'build/judas'])
m=Path(tempfile.mkdtemp(prefix='Judas-incremental-contact-final-',dir='/tmp'));shutil.copytree(o/'package',m,dirs_exist_ok=True)
for p in m.rglob('*'):
 if p.is_file():p.chmod(0o555 if os.access(p,os.X_OK)else 0o444)
for p in sorted((p for p in m.rglob('*')if p.is_dir()),reverse=True):p.chmod(0o555)
m.chmod(0o555)
script=o/'standalone.steps';script.write_text(f'FRAMES 120\nLOG_EVERY 30\nTAP F5 5\nSCREENSHOT 100 {o}/standalone.png\n');run('relocated-standalone',[m/'judas'],Path('/tmp'),{'JUDAS_TEST_SCRIPT':str(script)})
(o/'PACKAGE.json').write_text(json.dumps({'path':str(m),'runtime_sha256':hashlib.sha256((m/'judas').read_bytes()).hexdigest(),'engine_archive_sha256':hashlib.sha256((r/'build/libjudas_engine.a').read_bytes()).hexdigest(),'installation_read_only':True,'source_worktree':'final candidate; parent6059 plus recorded changes; human not yet accepted'},indent=2)+'\n')
print('export and relocated standalone passed',m)
