#!/usr/bin/env python3
"""Editor Play/Stop and relocated cross-version standalone smoke."""
from pathlib import Path
import argparse,subprocess,os,shutil,tempfile,json,hashlib,time
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=False);runs=[]
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',XDG_DATA_HOME=str(out/'user-data'))
def run(name,cmd,e=None,cwd=R):
 start=time.monotonic()
 with(out/(name+'.log')).open('w')as f:r=subprocess.run(list(map(str,cmd)),cwd=cwd,env=e or env,stdout=f,stderr=subprocess.STDOUT,timeout=240)
 runs.append(dict(name=name,argv=list(map(str,cmd)),exit=r.returncode,seconds=time.monotonic()-start));(out/'RESULTS.json').write_text(json.dumps(runs,indent=2)+'\n');print(name,r.returncode,flush=True)
 if r.returncode:raise RuntimeError(name)
lab=R/'projects/save_evolution_lab';copy=out/'editor-project';shutil.copytree(lab/'v3',copy)
e=env.copy();e.update(JUDAS_EDITOR_AUTOTEST=str(out/'editor'),JUDAS_EDITOR_AUTOTEST_AUTHORING='1',XDG_CONFIG_HOME=str(out/'editor-config'));run('editor',[R/'build/judas_editor',copy/'save_evolution.judasproj'],e)
text=(out/'editor.log').read_text();assert 'authored scene after play/stop is IDENTICAL'in text and 'save ok'in text
for version in ['v1','v2','v3','failure','incompatible']:run('export-'+version,[R/'build/judas_export',lab/version/'save_evolution.judasproj',out/version,R/'build/judas'])
def hashes(root):return {str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest()for f in root.rglob('*')if f.is_file()}
moved=Path(tempfile.mkdtemp(prefix='judas-m98-relocated-'));unrelated=Path(tempfile.mkdtemp(prefix='judas-m98-unrelated-'));packageHashes={}
for v in ['v1','v3']:
 shutil.copytree(out/v,moved/v);packageHashes[v]=hashes(out/v);assert packageHashes[v]==hashes(moved/v)
steps=out/'v1.steps';steps.write_text(f'''FRAMES 420
FRAME_SECONDS 0.016666667
ACTION dig 1 180 181
ACTION collect 1 200 201
ACTION collect 1 204 205
ACTION collect 1 208 209
ACTION collect 1 212 213
ACTION collect 1 216 217
ACTION collect 1 220 221
ACTION collect 1 224 225
ACTION save 1 260 261
WAIT_SERVICES save 10000 280
SCREENSHOT 360 {out}/old-progress.png
''');e=env.copy();e['JUDAS_TEST_SCRIPT']=str(steps);run('moved-v1',[moved/'v1/judas'],e,unrelated)
slots=next((out/'user-data/judas/games').iterdir())/'Saves/Slots';source=(slots/'old-progress.save').read_bytes();assert source
steps=out/'v3.steps';steps.write_text(f'''FRAMES 460
FRAME_SECONDS 0.016666667
ACTION load 1 150 151
WAIT_SERVICES save 10000 170
SCREENSHOT 250 {out}/upgraded.png
ACTION save 1 300 301
WAIT_SERVICES save 10000 320
SCREENSHOT 400 {out}/saved-upgraded.png
''');e=env.copy();e['JUDAS_TEST_SCRIPT']=str(steps);run('moved-v3',[moved/'v3/judas'],e,unrelated)
assert (slots/'old-progress.save').read_bytes()==source
assert(slots/'upgraded-progress.save').is_file()
for name in ['moved-v1','moved-v3']:assert 'script asset='not in(out/(name+'.log')).read_text()
run('inspect-upgraded',[R/'build/judas_save','inspect',slots,'upgraded-progress'])
(out/'PACKAGE_FILES.json').write_text(json.dumps(packageHashes,indent=2)+'\n');(out/'LOCATIONS.json').write_text(json.dumps(dict(moved=str(moved),unrelated=str(unrelated),packages=str(out)),indent=2)+'\n')
