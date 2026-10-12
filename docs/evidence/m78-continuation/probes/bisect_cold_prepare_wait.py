#!/usr/bin/env python3
"""Isolate restored Prepare inhibition; exact native saved snapshot, original M77 fixture contracts."""
import pathlib,shlex,subprocess,os,shutil,hashlib,time,json
r=pathlib.Path.cwd();base=r/'.cache/m78-followup/release';ev=r/'docs/evidence/m78-continuation';out=r/'.cache/m78-continuation/cold-wait-isolation';out.mkdir(exist_ok=False)
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
original=r/'src/PhysicalBalance.cpp';text=original.read_text();before='if(m_waitingForSample){m_waitingForSample=false;m_observation={};return;} // Reacquire completed contacts after a restored world steps.'
assert text.count(before)==1
source=text.replace(before,'if(m_waitingForSample){m_waitingForSample=false;m_observation={};} // Diagnostic: normal physical observation/reference evaluation; no fabricated contacts.')
owned=out/'PhysicalBalance.cpp';owned.write_text(source)
fields={}
for line in (base/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);fields[k]=shlex.split(v)
receipts=[]
def run(cmd,log,cwd=r,env=None):
 start=time.perf_counter();p=subprocess.run(cmd,cwd=cwd,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 (ev/log).write_text(p.stdout);receipts.append(dict(command=cmd,cwd=str(cwd),exitCode=p.returncode,seconds=time.perf_counter()-start,log=log));return p
cmd=['c++',*fields['CXX_DEFINES'],*fields['CXX_INCLUDES'],*fields['CXX_FLAGS'],'-c',str(owned),'-o',str(out/'PhysicalBalance.cpp.o')]
result=run(cmd,'cold-wait-bypass-compile.log');assert result.returncode==0,result.stdout
archive=out/'libjudas_engine.a';shutil.copyfile(base/'libjudas_engine.a',archive)
result=run(['ar','r',str(archive),str(out/'PhysicalBalance.cpp.o')],'cold-wait-bypass-archive.log');assert result.returncode==0
link=shlex.split((base/'CMakeFiles/judas_balance_lab_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')]
link[link.index('CMakeFiles/judas_balance_lab_tests.dir/tests/BalanceLabTests.cpp.o')]=str(r/'.cache/m78-continuation/cold-state-observer/observer.o');link[link.index('libjudas_engine.a')]=str(archive);link[link.index('-o')+1]=str(out/'observer')
result=run(link,'cold-wait-bypass-link.log',cwd=base);assert result.returncode==0,result.stdout
native=r/'.cache/m78-continuation/native-cold-step-diagnostics/m78-combined-v2-balance-raw-outputs/balance-cold-step/slots/m77.save'
env=os.environ.copy();env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(r/'.cache/m78-continuation/data'),XDG_CONFIG_HOME=str(r/'.cache/m78-continuation/config'))
for name,exe in [('baseline',r/'.cache/m78-continuation/cold-state-observer/observer'),('bypass',out/'observer')]:
 project=out/name;project.mkdir();shutil.copytree(r/'projects/balance_lab',project/'project');(project/'slots').mkdir();shutil.copyfile(native,project/'slots/m77.save')
 result=run([str(exe),str(project),'cold-read'],'cold-wait-'+name+'-run.log',env=env);print(name,result.returncode);print('\n'.join(result.stdout.splitlines()[-8:]));assert h(native)==h(project/'slots/m77.save')
identity=dict(scope='Owned PhysicalBalance.cpp overlay only removes first restored Prepare return. Actual current physical body observation and empty completed contact ledger; no synthesized support/warm history, unchanged acceptance/60Hz/inputs.',
 originalControllerSHA256=h(original),overlayControllerSHA256=h(owned),originalFixtureSHA256=h(r/'tests/BalanceLabTests.cpp'),observerSourceSHA256=h(r/'.cache/m78-continuation/cold-state-observer/BalanceLabStateObserver.cpp'),nativeSnapshotSHA256=h(native),baseArchiveSHA256=h(base/'libjudas_engine.a'),overlayArchiveSHA256=h(archive),exeSHA256=h(out/'observer'),steps=receipts)
(ev/'cold-wait-isolation.json').write_text(json.dumps(identity,indent=2)+'\n')
