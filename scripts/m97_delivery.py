#!/usr/bin/env python3
"""M97 editor Play/Stop, export and byte-verified relocation smoke."""
from pathlib import Path
import argparse,subprocess,os,shutil,tempfile,json,hashlib,time
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--resume',action='store_true');a=p.parse_args();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=a.resume);runs=json.loads((out/'RESULTS.json').read_text()) if a.resume else []
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',XDG_DATA_HOME=str(out/'user-data'))
def run(name,cmd,environment=None,cwd=R):
 if any(r['name']==name and r['exit']==0 for r in runs):print(name,'already passed',flush=True);return
 if(out/(name+'.log')).exists():shutil.copy2(out/(name+'.log'),out/(name+'.previous-failure.log'))
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as f:r=subprocess.run(list(map(str,cmd)),cwd=cwd,env=environment or env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
 runs.append(dict(name=name,argv=list(map(str,cmd)),exit=r.returncode,seconds=time.monotonic()-start));(out/'RESULTS.json').write_text(json.dumps(runs,indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(name+' failed; evidence retained')
project=R/'projects/living_world_lab/living_world_lab.judasproj';copied=out/'editor-project';shutil.copytree(project.parent,copied,dirs_exist_ok=True)
e=env.copy();e.update(JUDAS_EDITOR_AUTOTEST=str(out/'editor'),JUDAS_EDITOR_AUTOTEST_AUTHORING='1',XDG_CONFIG_HOME=str(out/'editor-config'));run('editor',[R/'build/judas_editor',copied/project.name],e)
text=(out/'editor.log').read_text();assert 'authored scene after play/stop is IDENTICAL' in text and 'save ok' in text;assert 'script asset='not in text
run('export',[R/'build/judas_export',project,out/'package',R/'build/judas'])
moved=Path(tempfile.mkdtemp(prefix='judas-m97-relocated-'));unrelated=Path(tempfile.mkdtemp(prefix='judas-m97-unrelated-'));shutil.copytree(out/'package',moved,dirs_exist_ok=True)
def hashes(root):return {str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest()for f in root.rglob('*')if f.is_file()}
h=hashes(out/'package');assert h==hashes(moved);assert h['judas']==hashlib.sha256((R/'build/judas').read_bytes()).hexdigest();(out/'PACKAGE_FILES.json').write_text(json.dumps(h,indent=2)+'\n')
steps=out/'moved.steps';steps.write_text(f'''FRAMES 900
FRAME_SECONDS 0.016666667
ACTION dig 1 180 181
SCREENSHOT 220 {out}/excavated.png
ACTION build 1 240 241
ACTION travel 1 300 301
SCREENSHOT 420 {out}/away.png
ACTION flat 1 460 461
SCREENSHOT 580 {out}/returned.png
ACTION origin 1 600 601
ACTION save 1 660 661
WAIT_SERVICES save 10000 680
ACTION load 1 740 741
WAIT_SERVICES save 10000 760
SCREENSHOT 860 {out}/loaded.png
''')
e=env.copy();e.update(JUDAS_TEST_SCRIPT=str(steps),JUDAS_PROFILE='1');run('relocated',[moved/'judas'],e,unrelated);assert 'script asset='not in(out/'relocated.log').read_text()
trace=(out/'relocated.log').read_text().splitlines()
for frame in [650,860]:
 row=next(line.split(',')for line in trace if line.startswith(str(frame)+','))
 assert .4<float(row[20])<1.0, 'returned construction stock fell through the floor at frame '+str(frame)

(out/'LOCATIONS.json').write_text(json.dumps(dict(moved=str(moved),unrelated=str(unrelated),package=str(out/'package')),indent=2)+'\n')
