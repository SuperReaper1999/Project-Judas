import subprocess,os,json,time
from pathlib import Path
root=Path.cwd();out=root/'.cache/save-latency-latest';build=root/'.cache/2d-stage1-build';env=os.environ.copy();env['XDG_DATA_HOME']=str(out/'qualification-user');env.pop('JUDAS_TEST_SCRIPT',None)
cases=[('storage',['judas_save_storage_tests',str(out/'m61-storage-save-speed')]),('lifecycle',['judas_consumer_lifecycle_tests',str(out/'lifecycle')]),('evolution',['judas_save_evolution_tests',str(out/'evolution')]),('cycles',['judas_save_performance_tests',str(out/'original/hinge/hinge.judasproj'),str(out/'cycles')])]
rows=[]
for name,args in cases:
 start=time.monotonic()
 with (out/('checks-'+name+'.log')).open('w') as f:r=subprocess.run([str(build/args[0]),*args[1:]],env=env,stdout=f,stderr=subprocess.STDOUT,timeout=90)
 rows.append({'test':name,'exit':r.returncode,'wallSeconds':time.monotonic()-start});print(name,r.returncode,flush=True)
(out/'CHECKS.json').write_text(json.dumps(rows,indent=2)+'\n');raise SystemExit(any(r['exit'] for r in rows))
