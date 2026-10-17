#!/usr/bin/env python3
"""Editor Play/Stop + ordinary closure export + byte-identical moved runtime."""
from pathlib import Path
import os,subprocess,time,json,shutil,tempfile,hashlib
R=Path(__file__).resolve().parents[1];out=R/'.cache/m94/delivery';out.mkdir(parents=True,exist_ok=False);runs=[]
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(JUDAS_WORLD_STATE='none',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(out/'user-data'))
def run(name,cmd,cwd=R,environment=None):
 start=time.monotonic()
 with (out/(name+'.log')).open('w')as f:r=subprocess.run(list(map(str,cmd)),cwd=cwd,env=environment or env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
 runs.append({'name':name,'argv':list(map(str,cmd)),'cwd':str(cwd),'exit':r.returncode,'seconds':time.monotonic()-start});(out/'RESULTS.json').write_text(json.dumps({'runs':runs},indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(name+' failed; evidence retained')
project=R/'projects/developer_lab/developer_lab.judasproj';copy=out/'editor-project';shutil.copytree(project.parent,copy)
editor=env.copy();editor.update(JUDAS_EDITOR_AUTOTEST=str(out/'editor'),XDG_CONFIG_HOME=str(out/'editor-config'))
run('editor',[R/'build/judas_editor',copy/project.name],environment=editor)
text=(out/'editor.log').read_text()
for required in ['undo restores: IDENTICAL','authored scene after play/stop is IDENTICAL','save ok']:
 if required not in text:raise RuntimeError('Editor receipt missing: '+required)
if 'script asset='in text:raise RuntimeError('Unexpected editor script fault')
run('export',[R/'build/judas_export',project,out/'package',R/'build/judas'])
moved=Path(tempfile.mkdtemp(prefix='judas-m94-relocated-'));unrelated=Path(tempfile.mkdtemp(prefix='judas-m94-unrelated-'));shutil.copytree(out/'package',moved,dirs_exist_ok=True)
def hashes(root):return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()for p in root.rglob('*')if p.is_file()}
files=hashes(out/'package');assert files==hashes(moved)
assert not any('judas_editor'in f or 'judas_script_check'in f or 'ScriptTools'in f for f in files)
(out/'PACKAGE_FILES.json').write_text(json.dumps(files,indent=2)+'\n')
steps=out/'moved.steps';steps.write_text(f'''FRAMES 160
FRAME_SECONDS 0.016666667
LOG_EVERY 0
SCREENSHOT 25 {out}/standalone-workshop.png
ACTION pause 1 35 36
EXPECT_PAUSED 1 40
ACTION pause 1 50 51
EXPECT_PAUSED 0 55
ACTION reset 1 70 71
WAIT_SERVICES scene 10000 75
EXPECT_SCENE Scenes/workshop.judas 80
SCREENSHOT 140 {out}/standalone-reloaded.png
''');game=env.copy();game['JUDAS_TEST_SCRIPT']=str(steps)
run('relocated',[moved/'judas'],cwd=unrelated,environment=game)
if 'script asset='in(out/'relocated.log').read_text():raise RuntimeError('Unexpected moved game fault')
(out/'RESULTS.json').write_text(json.dumps({'runs':runs,'moved':str(moved),'unrelatedCwd':str(unrelated),'byteIdenticalBeforeLaunch':True,'editorToolsNotPackaged':True,'humanAccepted':False,'windows':'not run','android':'not run','pass':True},indent=2)+'\n')
