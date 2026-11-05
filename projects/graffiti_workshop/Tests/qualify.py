#!/usr/bin/env python3
"""Real multi-process game checks, isolated saves, and relocated export.
Run from the Judas checkout with --runtime build/judas --exporter build/judas_export.
No assertions depend on screenshots alone; painting is driven through input.
"""
import argparse, json, os, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--runtime',type=Path,required=True);ap.add_argument('--exporter',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
 runtime=a.runtime.resolve();exporter=a.exporter.resolve();out=a.output.resolve()
 if out.exists():raise SystemExit('Use a new output directory to preserve prior evidence')
 out.mkdir(parents=True);project=out/'source';shutil.copytree(ROOT,project)
 env=dict(os.environ,XDG_DATA_HOME=str(out/'user-data'),JUDAS_WORLD_STATE='none')
 checks=[]
 def check(ok,message):
  checks.append({'pass':bool(ok),'message':message});print(('PASS ' if ok else 'FAIL ')+message,flush=True)
 def run(name,lines,game=project,exe=runtime,frames=720,scene=False):
  harness=out/(name+'.test');harness.write_text(f'FRAMES {frames}\nLOG_EVERY 0\n'+lines+'\nSCREENSHOT '+str(frames-5)+' '+str(out/(name+'.png'))+'\n')
  command=[str(exe)]+([str(game/'graffiti_workshop.judasproj')] if not scene else [])
  p=subprocess.run(command,cwd=game,env=dict(env,JUDAS_TEST_SCRIPT=str(harness)),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
  (out/(name+'.log')).write_text(p.stdout);check(p.returncode==0,name+' process exits successfully')
  states=[json.loads(line.split('GRAFFITI_STATE ',1)[1]) for line in p.stdout.splitlines() if 'JS: GRAFFITI_STATE ' in line]
  check(bool(states),name+' publishes real gameplay state')
  if not states:raise RuntimeError('No game telemetry: '+p.stdout[-1500:])
  return states
 states=run('paint-and-save','''ACTION spray 1 20 100
ACTION color_2 1 110 112
ACTION spray 1 120 200
ACTION color_3 1 210 212
ACTION spray 1 220 280
ACTION color_4 1 290 292
ACTION spray 1 300 340
AXIS move_x 0.5 40 90
ACTION delivery 1 350 352
ACTION spray 1 360 430
ACTION save 1 470 472
WAIT_SERVICES save 10000 650''')
 last=states[-1]
 check(last['position']['x']>1,'movement while spraying changes actual CharacterMotor position')
 check(all(n>0 for n in last['colorTotals']),'all four actual droplet colours deposit')
 check(any(s['held'] and s['pending'] for s in states),'holding receiver delivery causes real native backpressure')
 check(not last['pending'] and last['queued']==0 and last['live']==0,'save request releases held delivery, completes captured steps and consumes all deposited droplets')
 check(last['deposited']+last['rejected']==last['launched'],'every launched droplet accounted for exactly once in the bounded enclosed fixture')
 check(last['marks']==last['cells'] and last['cells']>0,'persistent game-owned mark entities exist at impact cells')
 check(last['save'] and last['save']['state']=='completed','genuine save operation completes')
 # Separate OS process; load, do not replay any spray input.
 restored=run('cold-load','ACTION load 1 5 7\nWAIT_SERVICES save 10000 10',frames=150)[-1]
 check(restored['deposited']==last['deposited'] and restored['paintHash']==last['paintHash'],'cold process restores exact coating counts, colours and impact locations')
 check(restored['cells']==last['cells'] and restored['marks']==last['marks'],'cold restore neither loses nor duplicates paint entities')
 check(restored['live']==0 and restored['queued']==0,'cold restore does not recreate consumed droplets or deposits')
 steel=run('non-paintable','AXIS move_x -1 10 66\nACTION spray 1 85 135',frames=240)[-1]
 check(steel['rejected']>0 and steel['deposited']==0,'steel physically blocks droplets while receiver tags reject paint')
 moving=run('moving-canvas','AXIS look_x 290 10 11\nACTION color_3 1 15 17\nACTION spray 1 20 110',frames=240)[-1]
 check(moving['receiverTotals'].get('103',0)>0,'angled physical droplets hit the native moving canvas')
 close=run('near-wall','AXIS move_z 1 10 180\nACTION spray 1 200 260',frames=360)[-1]
 check(close['position']['z']>-3.6 and close['deposited']>0,'native wall collision stops the player; close-range droplets still deposit')
 export=out/'export-original';p=subprocess.run([str(exporter),str(project/'graffiti_workshop.judasproj'),str(export)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
 (out/'export.log').write_text(p.stdout);check(p.returncode==0,'standalone export succeeds')
 relocated=out/'relocated'/'Graffiti Workshop';relocated.parent.mkdir();export.rename(relocated)
 # Remove the source path entirely before the standalone run.
 project.rename(out/'source-removed-after-export')
 exe=relocated/'judas'
 cold=run('relocated-load','ACTION load 1 5 7\nWAIT_SERVICES save 10000 10',game=relocated,exe=exe,frames=150,scene=True)[-1]
 check(cold['paintHash']==last['paintHash'] and cold['marks']==last['marks'],'relocated standalone loads the same durable paint with source path absent')
 (out/'RESULTS.json').write_text(json.dumps({'checks':checks,'passed':sum(c['pass'] for c in checks),'failed':sum(not c['pass'] for c in checks),'expected_coating':last},indent=2)+'\n')
 return 1 if any(not c['pass'] for c in checks) else 0
if __name__=='__main__':raise SystemExit(main())
