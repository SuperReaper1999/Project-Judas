#!/usr/bin/env python3
"""Restore the actual accepted M97 fixture on the current engine, in isolation."""
from pathlib import Path
import argparse,shutil,os,subprocess,json
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=False);fixture=R/'docs/evidence/m98/fixtures/m97';project=R/'projects/living_world_lab/living_world_lab.judasproj';identity=json.loads(project.read_text())['data']['saveIdentity'];slots=out/'user-data/judas/games'/identity/'Saves/Slots';slots.mkdir(parents=True);shutil.copy2(fixture/'living-review.save',slots/'living-review.save')
for f in ['expected-terrain.bin','expected-ray.txt']:shutil.copy2(fixture/f,out/f)
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',XDG_DATA_HOME=str(out/'user-data'))
with(out/'read.log').open('w')as f:r=subprocess.run([R/'build/judas_living_world_application_tests',project,'read',out],cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
assert(slots/'living-review.save').read_bytes()==(fixture/'living-review.save').read_bytes();raise SystemExit(r.returncode)
