#!/usr/bin/env python3
"""Real selected-package cold-save control and one removed-mod negative."""
from pathlib import Path
import argparse,subprocess,shutil,json,os
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=False);env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none');source=R/'docs/evidence/m98/fixtures/mod-store';positive=out/'selected';store=positive/'user-data/judas/mods/judas-modding-lab';shutil.copytree(source,store)
def run(root,mode):
 e=env.copy();e['XDG_DATA_HOME']=str(root/'user-data')
 with(root/(mode+'.log')).open('w')as f:r=subprocess.run([R/'build/judas_mod_application_tests',R/'projects/modding_lab/modding_lab.judasproj',mode,root],cwd=R,env=e,stdout=f,stderr=subprocess.STDOUT,timeout=120)
 if r.returncode:raise RuntimeError(mode)
run(positive,'write');run(positive,'read');negative=out/'removed';shutil.copytree(positive,negative);p=negative/'user-data/judas/mods/judas-modding-lab/Loadout/selected.json';j=json.loads(p.read_text());j['selected']=[s for s in j['selected']if s['id']!='landscape-fork'];p.write_text(json.dumps(j,indent=2)+'\n');run(negative,'mismatch')
