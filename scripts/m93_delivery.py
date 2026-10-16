#!/usr/bin/env python3
"""Editor, stale-cook rejection, closure export and relocated game smoke."""
from pathlib import Path
import subprocess,os,json,hashlib,shutil,tempfile,time
R=Path(__file__).resolve().parents[1];out=R/'.cache/m93/delivery';out.mkdir(parents=True,exist_ok=True);records=[];env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')};env.update(JUDAS_WORLD_STATE='none',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(out/'user-data'))
def run(name,command,cwd=R,environment=None,expect=0):
 start=time.monotonic()
 with (out/(name+'.log')).open('w') as f:r=subprocess.run(list(map(str,command)),cwd=cwd,env=environment or env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
 records.append({'name':name,'argv':list(map(str,command)),'cwd':str(cwd),'exit':r.returncode,'expected':expect,'seconds':time.monotonic()-start});(out/'RESULTS.json').write_text(json.dumps({'runs':records},indent=2)+'\n');print(name,r.returncode,flush=True)
 if (r.returncode!=0 if expect==0 else r.returncode==0):raise RuntimeError(name+' unexpected exit')
project=R/'projects/asset_lab/asset_lab.judasproj';copy=out/'editor-project'
if copy.exists():raise RuntimeError('Use fresh owned delivery output')
shutil.copytree(project.parent,copy)
editorEnv=env.copy();editorEnv.update(JUDAS_EDITOR_AUTOTEST=str(out/'editor'),XDG_CONFIG_HOME=str(out/'editor-config'))
run('editor',[R/'build/judas_editor',copy/project.name],environment=editorEnv)
log=(out/'editor.log').read_text()
if any(s not in log for s in ['undo restores: IDENTICAL','authored scene after play/stop is IDENTICAL','save ok'])or 'script asset=' in log:raise RuntimeError('Editor semantic receipt missing or script fault')
# The unchanged prior accepted cook must be rejected when its real source changes.
source=copy/'SourceArt/form0.obj';original=source.read_bytes();source.write_bytes(original+b'\n# changed source\n')
run('stale-export',[R/'build/judas_export',copy/project.name,out/'stale-package',R/'build/judas'],expect=1)
if 'stale' not in (out/'stale-export.log').read_text().lower():raise RuntimeError('Expected stale-generation diagnostic')
source.write_bytes(original)
run('export',[R/'build/judas_export',project,out/'package',R/'build/judas'])
moved=Path(tempfile.mkdtemp(prefix='judas-m93-relocated-'));unrelated=Path(tempfile.mkdtemp(prefix='judas-m93-unrelated-'));shutil.copytree(out/'package',moved,dirs_exist_ok=True)
def hashes(root):return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in root.rglob('*') if p.is_file()}
a,b=hashes(out/'package'),hashes(moved)
if a!=b:raise RuntimeError('Relocation changed package bytes')
if any('SourceArt' in f or f.endswith('.judascook') for f in a):raise RuntimeError('Authoring data bloats runtime closure')
(out/'PACKAGE_FILES.json').write_text(json.dumps(a,indent=2)+'\n')
steps=out/'moved.steps';steps.write_text(f'''FRAMES 600
FRAME_SECONDS 0.016666667
LOG_EVERY 0
SCREENSHOT 90 {out}/standalone-museum.png
ACTION split 1 100 101
SCREENSHOT 150 {out}/standalone-split.png
ACTION split 1 165 166
ACTION pause 1 180 181
EXPECT_PAUSED 1 184
ACTION pause 1 190 191
EXPECT_PAUSED 0 194
ACTION district0 1 205 206
ACTION save 1 220 221
WAIT_SERVICES save 10000 225
ACTION district2 1 240 241
SCREENSHOT 280 {out}/standalone-district3.png
ACTION load 1 300 301
WAIT_SERVICES save 10000 305
EXPECT_SCENE Scenes/museum.judas 310
ACTION detail 1 330 331
WAIT_SERVICES scene 10000 335
EXPECT_SCENE Scenes/full_detail.judas 340
SCREENSHOT 400 {out}/standalone-full.png
ACTION detail 1 430 431
WAIT_SERVICES scene 10000 435
EXPECT_SCENE Scenes/museum.judas 440
ACTION reset 1 465 466
WAIT_SERVICES scene 10000 470
EXPECT_SCENE Scenes/museum.judas 475
SCREENSHOT 560 {out}/standalone-reloaded.png
''')
packageEnv=env.copy();packageEnv.update(JUDAS_TEST_SCRIPT=str(steps),JUDAS_PROFILE='1',JUDAS_PROFILE_OUTPUT=str(out/'standalone-profile.json'))
run('moved',[moved/'judas'],cwd=unrelated,environment=packageEnv)
if 'script asset=' in (out/'moved.log').read_text():raise RuntimeError('Moved package script fault')
(out/'RESULTS.json').write_text(json.dumps({'runs':records,'moved':str(moved),'unrelatedWorkingDirectory':str(unrelated),'prelaunchBytesIdentical':True,'noSourceArtOrRecipes':True,'pass':True,'humanAcceptance':False,'windows':'not run','android':'not run'},indent=2)+'\n')
