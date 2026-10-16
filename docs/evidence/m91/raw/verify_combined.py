from pathlib import Path
import subprocess,json,hashlib,shlex,os,time
r=Path.cwd();c=r/'.cache/m91';records=[]
def run(name,cmd,graphics=False):
 start=time.monotonic();env=os.environ.copy();env['SDL_AUDIODRIVER']='dummy'
 with Path('/tmp/m91-verified-'+name+'.log').open('w')as log:code=subprocess.run(list(map(str,cmd)),cwd=r,env=env,stdout=log,stderr=subprocess.STDOUT).returncode
 records.append({'name':name,'command':list(map(str,cmd)),'exit':code,'seconds':time.monotonic()-start});(c/'VERIFIED_RUNS.json').write_text(json.dumps(records,indent=2)+'\n');print(name,code,flush=True)
 if code:raise RuntimeError(name+' failed; original output retained')
run('native',['python3',c/'run_verified_checks.py'])
run('interval',[r/'build/judas_contact_interval_tests','--benchmark'])
for w in ['free','machines_sleeping']:run('allocation-'+w,[r/'build/judas_physics_scaling_diagnostics','--workload',w,'--count','1024','--warm',300 if w=='machines_sleeping'else 60,'--steps','120','--profile',0,'--sleep',1,'--output',c/('alloc/verified-'+w)])
run('scaling',['python3','scripts/m87_benchmark.py','--binary','build/judas_physics_scaling_benchmarks','--output',c/'scaling-verified','--counts',64,256,1024,'--machine-counts',16,64,256,'--steps',120,'--profile',1,'--timeout',60])
run('scaling-repeated',['python3',c/'repeat_verified_scaling.py'])
run('dense',['python3','scripts/m91_measure.py','--before',c/'baseline/live-observer','--after','build/judas_physics_stress_live_benchmark','--scene',c/'frozen/Scenes/room1024.judas','--output',c/'dense-verified','--frames',155,240,'--timeout',75,'--repeat',1,'--labels','after','--profile',0])
run('profile',['python3','scripts/m91_measure.py','--before',c/'baseline/live-observer','--after','build/judas_physics_stress_live_benchmark','--scene',c/'frozen/Scenes/room1024.judas','--output',c/'profile-verified','--frames',270,'--timeout',75,'--repeat',1,'--labels','after','--profile',1])
run('consumer',['python3','scripts/m87_consumer_benchmark.py','--baseline',c/'baseline/judas_ball_pusher_application_tests','--candidate','build/judas_ball_pusher_application_tests','--output',c/'consumer-verified','--profile',0])
run('delivery',['python3',c/'verified_delivery.py'])
x=json.loads((c/'concave/LINK.json').read_text());cmd=x['links']['after']['command'];cmd[cmd.index('-o')+1]=str(c/'concave/after-verified');subprocess.run(cmd,cwd=r/'build',check=True);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();x['links']['after_verified']={'command':cmd,'binary_sha256':sha(c/'concave/after-verified'),'engine_sha256':sha(r/'build/libjudas_engine.a'),'PhysicsWorld_sha256':sha(r/'src/PhysicsWorld.cpp')};(c/'concave/LINK.json').write_text(json.dumps(x,indent=2)+'\n')
run('concave',[c/'concave/after-verified',c/'concave/after-verified-raw'])
run('watch',[r/'build/judas_physics_stress_live_benchmark','projects/physics_stress_lab/physics_stress_lab.judasproj',c/'watch-verified',30,100000,1,1,1,0,1,0,0])
