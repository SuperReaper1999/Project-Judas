#!/usr/bin/env python3
"""Real editor import/timeline/Play-Stop, closure export and relocated launch."""
from pathlib import Path
import os,subprocess,time,json,shutil,tempfile,hashlib,argparse
R=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser();parser.add_argument('--output',default='.cache/m95/delivery');args=parser.parse_args();out=R/args.output;out.mkdir(parents=True,exist_ok=False);runs=[]
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(JUDAS_WORLD_STATE='none',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(out/'user-data'))
def run(name,cmd,cwd=R,environment=None):
 start=time.monotonic()
 with (out/(name+'.log')).open('w')as f:r=subprocess.run(list(map(str,cmd)),cwd=cwd,env=environment or env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
 runs.append(dict(name=name,argv=list(map(str,cmd)),cwd=str(cwd),exit=r.returncode,seconds=time.monotonic()-start));(out/'RESULTS.json').write_text(json.dumps(dict(runs=runs),indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(name+' failed; raw evidence retained')
project=R/'projects/animation_studio/animation_studio.judasproj';copy=out/'editor-project';shutil.copytree(project.parent,copy)
editor=env.copy();editor.update(JUDAS_EDITOR_AUTOTEST=str(out/'editor'),JUDAS_EDITOR_AUTOTEST_IMPORT=str(copy/'Imports/performer.judasimport'),JUDAS_EDITOR_AUTOTEST_ANIMATION='1',XDG_CONFIG_HOME=str(out/'editor-config'))
run('editor',[R/'build/judas_editor',copy/project.name],environment=editor)
text=(out/'editor.log').read_text()
for required in ['authored scene after play/stop is IDENTICAL','save ok','[M95 timeline] PASS: clip draft and real GPU preview','[M95 timeline] PASS: switching clips retains unsaved marker draft','[M95 timeline] PASS: explicit event draft save','[M95 timeline] PASS: normal asynchronous recook publishes edited marker']:
 if required not in text:raise RuntimeError('Editor receipt missing: '+required)
if '[M95 timeline] FAIL'in text or 'script asset='in text:raise RuntimeError('Editor failure retained')
run('export',[R/'build/judas_export',project,out/'package',R/'build/judas'])
moved=Path(tempfile.mkdtemp(prefix='judas-m95-relocated-'));unrelated=Path(tempfile.mkdtemp(prefix='judas-m95-unrelated-'));shutil.copytree(out/'package',moved,dirs_exist_ok=True)
def hashes(root):return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()for p in root.rglob('*')if p.is_file()}
files=hashes(out/'package');assert files==hashes(moved);assert not any('judas_editor'in f or 'ScriptTools'in f for f in files)
(out/'PACKAGE_FILES.json').write_text(json.dumps(files,indent=2)+'\n')
steps=out/'moved.steps';steps.write_text(f'''FRAMES 380
FRAME_SECONDS 0.016666667
LOG_EVERY 0
ACTION forward 1 10 110
ACTION strike 1 120 121
SCREENSHOT 160 {out}/standalone-studio.png
ACTION interact 1 175 176
ACTION pause 1 195 196
EXPECT_PAUSED 1 200
ACTION pause 1 210 211
EXPECT_PAUSED 0 215
ACTION rotate 1 230 231
WAIT_SERVICES scene 10000 235
EXPECT_SCENE Scenes/rotated.judas 245
ACTION forward 1 260 310
SCREENSHOT 320 {out}/standalone-rotated.png
ACTION reset 1 335 336
WAIT_SERVICES scene 10000 340
EXPECT_SCENE Scenes/rotated.judas 350
''')
runtime=env.copy();runtime['JUDAS_TEST_SCRIPT']=str(steps);run('relocated',[moved/'judas'],cwd=unrelated,environment=runtime)
if 'script asset='in(out/'relocated.log').read_text():raise RuntimeError('Unexpected relocated script fault')
assert hashlib.sha256((out/'package/judas').read_bytes()).hexdigest()==hashlib.sha256((R/'build/judas').read_bytes()).hexdigest()
print(out,flush=True)
