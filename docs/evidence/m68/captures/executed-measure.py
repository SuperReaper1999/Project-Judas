#!/usr/bin/env python3
"""Comparable M68 captures in a fresh, explicitly named owned output directory.
OS caches are NOT purged. Heavy tasks run serially; raw stdout, GNU time and M56
captures are retained. No production project or historical evidence is written.
"""
import argparse, json, os, shutil, subprocess, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--bin',type=Path,required=True);p.add_argument('--tracks',nargs='+',choices=['runtime','import','export','authoring','resource'],required=True);a=p.parse_args()
 out=a.output.resolve();bins=a.bin.resolve()
 if out.exists():p.error('Output already exists; preserve prior captures and choose a fresh directory.')
 if ROOT==out or out in ROOT.parents:p.error('Output must be an owned child, never an ancestor of the repository.')
 out.mkdir(parents=True);own=out/'owned';own.mkdir();results=[]
 environment=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',JUDAS_ENGINE_ROOT=str(ROOT),JUDAS_WORLD_STATE='none',XDG_DATA_HOME=str(own/'user-data'))
 def run(label,cmd,cwd=ROOT,extra=None):
  env=dict(environment,JUDAS_PROFILE='1',JUDAS_PROFILE_OUTPUT=str(out/(label+'-profile.json')));env.update(extra or {})
  t=time.monotonic();r=subprocess.run(['/usr/bin/time','-v','-o',str(out/(label+'-time.txt')),*map(str,cmd)],cwd=cwd,env=env,capture_output=True,text=True,timeout=600)
  (out/(label+'.log')).write_text(r.stdout+r.stderr);row=dict(label=label,command=list(map(str,cmd)),cwd=str(cwd),wallMs=(time.monotonic()-t)*1000,exit=r.returncode)
  for line in (out/(label+'-time.txt')).read_text().splitlines():
   for key,field in [('Maximum resident set size (kbytes):','peakRSSKiB'),('User time (seconds):','userSeconds'),('System time (seconds):','systemSeconds')]:
    if key in line:row[field]=float(line.split(key)[1])
  results.append(row);(out/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(row),flush=True)
  if r.returncode not in (0,2):raise RuntimeError(label+' failed; retained log')
 if 'runtime' in a.tracks:
  original=ROOT/'docs/evidence/post_m65_consumers/scaling/original-reproduction';project=own/'scaling';shutil.copytree(original,project)
  for n in (0,400,800,6400):
   for count in (0,1,8):
    subprocess.run(['python3','make.py',str(n),'1' if count else '0'],cwd=project,check=True)
    if count==8:
     source=project/'Scenes/main.judas';text=source.read_text();block=text[text.index('object 1 '):text.index('object 1 ')+text[text.index('object 1 '):].index('\nend')+4];text=text.replace(block,'')
     for i in range(8):text+= '\n'+block.replace('object 1 ',f'object {n+20+i} ')
     source.write_text(text.replace(f'next-id {n+10}',f'next-id {n+40}'))
     script=next((project/'Assets').glob('*.js'));script.write_text("import { input } from 'judas'; export default class { constructor({entity}) { this.entity=entity; this.state={ticks:0}; } fixedUpdate(dt) { this.state.ticks++; const t=this.entity.transform; input.axis('move_x'); } }\n")
    else:next((project/'Assets').glob('*.js')).write_text('export default class { constructor(){this.state={ticks:0};} fixedUpdate(){this.state.ticks++;} }\n')
    for repeat in range(3):
     command=project/'run.txt';command.write_text('STEPS 1000\nLOG_EVERY 1000000\n')
     run(f'runtime-{n}-{count}-{repeat}',[bins/'judas',project/'repro.judasproj'],cwd='/tmp',extra={'JUDAS_TEST_SCRIPT':str(command)})
 if 'import' in a.tracks:
  model=own/'import';shutil.copytree(ROOT/'projects/import_lab/Sources',model/'Sources');shutil.copytree(ROOT/'projects/import_lab/Imports',model/'Imports')
  for name in ('external','skate_original'):
   recipe=model/'Imports'/(name+'.judasimport')
   for repeat in range(3):
    # Missing products and persistent cache; filesystem caches may remain warm.
    shutil.rmtree(model/'.cache',ignore_errors=True);shutil.rmtree(model/'Assets',ignore_errors=True)
    run(f'import-{name}-cold-{repeat}',[bins/'judas_model_import_cli','--recipe',recipe])
    run(f'import-{name}-unchanged-{repeat}',[bins/'judas_model_import_cli','--recipe',recipe])
 if 'authoring' in a.tracks:
  for repeat in range(3):run(f'authoring-{repeat}',[bins/'judas_world_authoring_performance',out/f'authoring-{repeat}'])
 if 'export' in a.tracks:
  for name,project in [('small','projects/character_demo/character_demo.judasproj'),('workshop','projects/world_workshop/world_workshop.judasproj'),('professional','projects/import_lab/import_lab.judasproj')]:
   for repeat in range(3):run(f'export-{name}-{repeat}',[bins/'judas_export',ROOT/project,own/f'package-{name}',bins/'judas'])
 if 'resource' in a.tracks:
  for repeat in range(3):run(f'resource-{repeat}',[bins/'judas_model_runtime_tests',out/f'resource-{repeat}'])
if __name__=='__main__':main()
