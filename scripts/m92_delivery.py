#!/usr/bin/env python3
"""Ordinary editor, export and relocated standalone checks; only owned copies change."""
from pathlib import Path
import subprocess,os,json,hashlib,shutil,tempfile,time
R=Path(__file__).resolve().parents[1];out=R/'.cache/m92/delivery';out.mkdir(exist_ok=True);records=[];env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')};env.update(JUDAS_WORLD_STATE='none',SDL_AUDIODRIVER='dummy')
def run(name,command,cwd=R,environment=None):
 start=time.monotonic()
 with (out/(name+'.log')).open('w') as f:r=subprocess.run(list(map(str,command)),cwd=cwd,env=environment or env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
 records.append({'name':name,'argv':list(map(str,command)),'cwd':str(cwd),'exit':r.returncode,'seconds':time.monotonic()-start});(out/'RESULTS.json').write_text(json.dumps({'runs':records},indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise SystemExit(r.returncode)
project=R/'projects/rendering_lab/rendering_lab.judasproj';copy=out/'editor-project'
if not copy.exists():shutil.copytree(project.parent,copy)
editorEnv=env.copy();editorEnv.update(JUDAS_EDITOR_AUTOTEST=str(out/'editor'),XDG_CONFIG_HOME=str(out/'editor-config'))
run('editor',[R/'build/judas_editor',copy/project.name],environment=editorEnv)
log=(out/'editor.log').read_text();required=['undo restores: IDENTICAL','authored scene after play/stop is IDENTICAL','save ok']
if any(s not in log for s in required)or 'script asset=' in log:raise RuntimeError('Editor semantic receipt missing or script fault')
run('export',[R/'build/judas_export',project,out/'package',R/'build/judas'])
moved=Path(tempfile.mkdtemp(prefix='judas-m92-relocated-'));unrelated=Path(tempfile.mkdtemp(prefix='judas-m92-unrelated-'));shutil.copytree(out/'package',moved,dirs_exist_ok=True)
def hashes(root):return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in root.rglob('*') if p.is_file()}
a,b=hashes(out/'package'),hashes(moved)
if a!=b:raise RuntimeError('Relocation changed package bytes')
(out/'PACKAGE_FILES.json').write_text(json.dumps(a,indent=2)+'\n')
steps=out/'moved.steps';steps.write_text(f'''FRAMES 220
FRAME_SECONDS 0.016666667
LOG_EVERY 0
SCREENSHOT 25 {out}/standalone-gallery.png
ACTION split 1 35 36
SCREENSHOT 60 {out}/standalone-split.png
ACTION pause 1 70 71
EXPECT_PAUSED 1 73
ACTION pause 1 80 81
EXPECT_PAUSED 0 83
ACTION count_small 1 100 101
WAIT_SERVICES scene 10000 102
EXPECT_SCENE Scenes/small.judas 103
ACTION count_dense 1 125 126
WAIT_SERVICES scene 10000 127
EXPECT_SCENE Scenes/dense.judas 128
SCREENSHOT 150 {out}/standalone-dense.png
ACTION count_normal 1 170 171
WAIT_SERVICES scene 10000 172
EXPECT_SCENE Scenes/gallery.judas 173
ACTION reset 1 195 196
WAIT_SERVICES scene 10000 197
EXPECT_SCENE Scenes/gallery.judas 198
''')
packageEnv=env.copy();packageEnv['JUDAS_TEST_SCRIPT']=str(steps)
run('moved',[moved/'judas'],cwd=unrelated,environment=packageEnv)
if 'script asset=' in (out/'moved.log').read_text():raise RuntimeError('Moved package script fault')
(out/'RESULTS.json').write_text(json.dumps({'runs':records,'moved':str(moved),'unrelatedWorkingDirectory':str(unrelated),'prelaunchBytesIdentical':True,'pass':True,'humanAcceptance':False,'windows':'not run','android':'not run'},indent=2)+'\n')
