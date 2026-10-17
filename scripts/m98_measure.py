#!/usr/bin/env python3
"""Twelve separate-process upgrades; use an existing m98_validation output."""
from pathlib import Path
import subprocess,os,re,json,statistics,time,math,argparse
R=Path(__file__).resolve().parents[1];p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--fixtures',required=True);a=p.parse_args();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=False);source=Path(a.fixtures).resolve();env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',XDG_DATA_HOME=str(source/'user-data'));rows=[]
for i in range(12):
 name=f'upgrade-{i:02}'
 with(out/(name+'.log')).open('w')as f:r=subprocess.run([str(R/'build/judas_save_evolution_application_tests'),str(R/'projects/save_evolution_lab/v3/save_evolution.judasproj'),'direct-v3',str(source/'application')],cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=90)
 if r.returncode:raise RuntimeError(name)
 log=(out/(name+'.log')).read_text();m=re.search(r'LOAD_RESULT completed migration_ms=(\S+) worker_ms=(\S+) restore_ms=(\S+)',log);assert m;rows.append(dict(migration=float(m[1]),worker=float(m[2]),restore=float(m[3])))
summary={k:dict(samples=len(rows),median=statistics.median(r[k]for r in rows),p95=sorted(r[k]for r in rows)[math.ceil(.95*len(rows))-1],worst=max(r[k]for r in rows))for k in rows[0]};(out/'RESULTS.json').write_text(json.dumps(dict(rows=rows,summary=summary),indent=2)+'\n');print(json.dumps(summary,indent=2))
