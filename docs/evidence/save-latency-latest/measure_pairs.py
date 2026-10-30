from pathlib import Path
import subprocess,os,json,time
root=Path.cwd();out=root/'.cache/save-latency-latest';rows=[]
for run in range(3):
 for scene in ['map','level']:
  for variant in (['before','after'] if run%2==0 else ['after','before']):
   exe=out/'baseline-bin/judas' if variant=='before' else root/'.cache/2d-stage1-build/judas'
   env=os.environ.copy();env.update(XDG_DATA_HOME=str(out/f'pair-{run}-{scene}-{variant}-user'),JUDAS_WINDOW_SIZE='800x450',JUDAS_TEST_SCRIPT=str(out/'tour.txt'))
   log=out/f'pair-{run}-{scene}-{variant}.log';start=time.monotonic()
   with log.open('w') as f:p=subprocess.run([str(exe),str(out/f'probe-{scene}/hinge.judasproj')],env=env,stdout=f,stderr=subprocess.STDOUT,timeout=40)
   samples=[json.loads(l.split('PROBE_DONE ')[1]) for l in log.read_text().splitlines() if 'PROBE_DONE ' in l]
   assert p.returncode==0 and len(samples)==11,(variant,scene,p.returncode,len(samples))
   rows.append({'run':run,'scene':scene,'variant':variant,'exit':p.returncode,'wallSeconds':time.monotonic()-start,'samples':samples});print(run,scene,variant,'OK',flush=True)
   (out/'PAIRS.json').write_text(json.dumps(rows,indent=2)+'\n')
