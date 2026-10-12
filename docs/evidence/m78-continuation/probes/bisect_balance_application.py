#!/usr/bin/env python3
"""Unchanged actual GL/JS balance fixture against isolated physical archives."""
import pathlib,shlex,subprocess,json,hashlib,time,sys,os
r=pathlib.Path.cwd();b=r/'.cache/m78-followup/release';ev=r/'docs/evidence/m78-continuation';out=r/'.cache/m78-continuation/variants'
target='judas_balance_lab_tests';h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
object_path=b/'CMakeFiles/judas_balance_lab_tests.dir/tests/BalanceLabTests.cpp.o'
for variant in sys.argv[1:]:
 folder=out/variant;archive=folder/'libjudas_engine.a';exe=folder/'balance_application';owned=r/'.cache/m78-continuation'/('app-'+variant)
 assert not exe.exists() and not owned.exists(),('preserve existing',variant)
 link=shlex.split((b/'CMakeFiles'/ (target+'.dir')/'link.txt').read_text())
 link=[str(archive) if x=='libjudas_engine.a' else str(exe) if x==target else x for x in link if not x.startswith('-Wl,--dependency-file')]
 with (ev/(variant+'-balance-app-link.log')).open('xb') as f: result=subprocess.run(link,cwd=b,stdout=f,stderr=subprocess.STDOUT)
 assert result.returncode==0
 env=os.environ.copy();env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(r/'.cache/m78-continuation/data'),XDG_CONFIG_HOME=str(r/'.cache/m78-continuation/config'))
 command=[str(exe),str(owned),'step-trace'];start=time.perf_counter()
 with (ev/(variant+'-balance-app.log')).open('xb') as f: result=subprocess.run(command,cwd=r,env=env,stdout=f,stderr=subprocess.STDOUT)
 (ev/(variant+'-balance-app.json')).write_text(json.dumps({'command':command,'linkCommand':link,'archiveSHA256':h(archive),'exeSHA256':h(exe),'fixtureObjectSHA256':h(object_path),'fixtureSourceSHA256':h(r/'tests/BalanceLabTests.cpp'),'projectSourceSHA256':{str(p.relative_to(r)):h(p) for p in (r/'projects/balance_lab').rglob('*') if p.is_file()},'exitCode':result.returncode,'seconds':time.perf_counter()-start,'scope':'Unchanged actual authored project, logical J input, timestep, acceptance against isolated engine archive'},indent=2)+'\n')
 print(variant,result.returncode,flush=True)
