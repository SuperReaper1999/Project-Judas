#!/usr/bin/env python3
"""Owned editor fixture, ordinary export, relocated read-only package smoke."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
OUT=HERE/'delivery'
if OUT.exists():raise RuntimeError('Fresh output required')
OUT.mkdir()
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
core=ROOT/'build/libjudas_engine.a';core_sha=sha(core)
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')}
env.update(JUDAS_WORLD_STATE='none',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(OUT/'userdata'),XDG_CONFIG_HOME=str(OUT/'config'),XDG_CACHE_HOME=str(OUT/'cache'))
report={'core_sha256':core_sha,'results':[]}
def run(name,args,cwd=ROOT,environment=None):
    begin=time.monotonic();args=list(map(str,args))
    with (OUT/(name+'.log')).open('w')as f:
        result=subprocess.run(args,cwd=cwd,env=environment or env,stdout=f,stderr=subprocess.STDOUT,timeout=180)
    report['results'].append({'name':name,'argv':args,'cwd':str(cwd),'exit':result.returncode,'seconds':time.monotonic()-begin,'log_sha256':sha(OUT/(name+'.log'))})
    (OUT/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
    print(name,result.returncode,flush=True)
    if result.returncode:raise RuntimeError(name+' failed; evidence retained')
project=ROOT/'projects/physics_stress_lab/physics_stress_lab.judasproj'
owned=OUT/'editor-project';shutil.copytree(project.parent,owned)
scenes={str(p.relative_to(owned)):sha(p)for p in (owned/'Scenes').glob('*.judas')}
editor=env.copy();editor['JUDAS_EDITOR_AUTOTEST']=str(OUT/'editor')
run('editor',[ROOT/'build/judas_editor',owned/project.name],environment=editor)
text=(OUT/'editor.log').read_text()
assert 'authored scene after play/stop is IDENTICAL'in text,text[-2000:]
assert scenes=={str(p.relative_to(owned)):sha(p)for p in (owned/'Scenes').glob('*.judas')}
assert 'NVIDIA'in text and 'llvmpipe'not in text.lower()
run('export',[ROOT/'build/judas_export',project,OUT/'exported',ROOT/'build/judas'])
moved=Path(tempfile.mkdtemp(prefix='Judas-Physics-Broadphase-'))
shutil.copytree(OUT/'exported',moved,dirs_exist_ok=True)
def hashes(root):return {str(p.relative_to(root)):sha(p)for p in sorted(root.rglob('*'))if p.is_file()}
files=hashes(OUT/'exported');assert files==hashes(moved)
assert sha(moved/'judas')==sha(ROOT/'build/judas')
for path in moved.rglob('*'):path.chmod(0o555 if path.is_dir()or path.name=='judas'else 0o444)
moved.chmod(0o555)
steps=OUT/'standalone.steps'
steps.write_text(f'FRAMES 180\nFRAME_SECONDS 0.016666667\nTAP 3 5\nWAIT_SERVICES scene 10000 12\nEXPECT_SCENE Scenes/room1024.judas 22\nTAP F5 30\nSCREENSHOT 150 {OUT}/standalone.png\n')
runtime=env.copy();runtime['JUDAS_TEST_SCRIPT']=str(steps)
run('relocated-standalone',[moved/'judas'],cwd=Path('/tmp'),environment=runtime)
log=(OUT/'relocated-standalone.log').read_text()
assert 'NVIDIA'in log and 'llvmpipe'not in log.lower()
assert 'script asset='not in log,log[-2000:]
assert files==hashes(moved),'read-only installation changed'
assert sha(core)==core_sha
package={'path':str(moved),'runtime_sha256':sha(moved/'judas'),'physics_archive_sha256':core_sha,'files':files,'read_only_installation_unchanged':True,'editor_scenes_unchanged':scenes,'scene_checked':'Scenes/room1024.judas','working_directory':'/tmp','audio':'dummy; not human listening','human_acceptance':'pending for this new correction'}
(OUT/'PACKAGE.json').write_text(json.dumps(package,indent=2)+'\n')
report.update(pass_all=True,core_unchanged=True,package_path=str(moved))
(OUT/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
print(moved,flush=True)
