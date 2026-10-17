#!/usr/bin/env python3
"""Editor Play/Stop and ordinary exported/relocated M96 UI runtime."""
from pathlib import Path
import os,subprocess,time,json,tempfile,hashlib,shutil,argparse
R=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser();parser.add_argument('--output',required=True);a=parser.parse_args();out=(R/a.output).resolve();out.mkdir(parents=True,exist_ok=False);runs=[]
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(JUDAS_WORLD_STATE='none',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(out/'user-data'))
def run(name,argv,environment=None,cwd=R):
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as f:r=subprocess.run(list(map(str,argv)),cwd=cwd,env=environment or env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
 runs.append(dict(name=name,argv=list(map(str,argv)),cwd=str(cwd),exit=r.returncode,seconds=time.monotonic()-start));(out/'RESULTS.json').write_text(json.dumps(dict(runs=runs),indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(name+' failed; evidence retained')
project=R/'projects/ui_showcase/ui_showcase.judasproj';editorProject=out/'editor-project';shutil.copytree(project.parent,editorProject)
editor=env.copy();editor.update(JUDAS_EDITOR_AUTOTEST=str(out/'editor'),JUDAS_EDITOR_AUTOTEST_AUTHORING='1',XDG_CONFIG_HOME=str(out/'editor-config'))
run('editor',[R/'build/judas_editor',editorProject/project.name],editor)
text=(out/'editor.log').read_text()
for receipt in ['authored scene after play/stop is IDENTICAL','save ok']:
 if receipt not in text:raise RuntimeError('missing editor receipt: '+receipt)
if 'script asset='in text:raise RuntimeError('editor JS error')
run('export',[R/'build/judas_export',project,out/'package',R/'build/judas'])
moved=Path(tempfile.mkdtemp(prefix='judas-m96-relocated-'));unrelated=Path(tempfile.mkdtemp(prefix='judas-m96-unrelated-'));shutil.copytree(out/'package',moved,dirs_exist_ok=True)
def hashes(root):return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()for p in root.rglob('*')if p.is_file()}
files=hashes(out/'package');assert files==hashes(moved);assert files['judas']==hashlib.sha256((R/'build/judas').read_bytes()).hexdigest();(out/'PACKAGE_FILES.json').write_text(json.dumps(files,indent=2)+'\n')
steps=out/'moved.steps';steps.write_text(f'''FRAMES 180
FRAME_SECONDS 0.016666667
LOG_EVERY 0
EXPECT_PAUSED 1 10
SCREENSHOT 20 {out}/welcome.png
ACTION ui_down 1 30 31
ACTION ui_activate 1 33 34
SCREENSHOT 42 {out}/settings.png
ACTION ui_down 1 48 49
ACTION ui_activate 1 51 52
SCREENSHOT 62 {out}/inventory.png
ACTION save 1 75 76
WAIT_SERVICES save 10000 80
ACTION load 1 100 101
WAIT_SERVICES save 10000 105
SCREENSHOT 150 {out}/loaded.png
''')
runtime=env.copy();runtime['JUDAS_TEST_SCRIPT']=str(steps);run('relocated',[moved/'judas'],runtime,unrelated)
if 'script asset='in(out/'relocated.log').read_text():raise RuntimeError('relocated JS failure')
print(out)
