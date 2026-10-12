#!/usr/bin/env python3
"""Compose existing owned diagnostic objects; no repeated engine compilation."""
import pathlib,shutil,shlex,subprocess,hashlib,json,sys,time
r=pathlib.Path.cwd();b=r/'.cache/m78-followup/release';out=r/'.cache/m78-continuation/variants';ev=r/'docs/evidence/m78-continuation'
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for arg in sys.argv[1:]:
 name,base_name,donor_name,source=arg.split(':');base=out/base_name;donor=out/donor_name;folder=out/name;folder.mkdir(exist_ok=False)
 for f in base.rglob('*'):
  if f.is_file() and f.suffix in ['.h','.hpp']:
   target=folder/f.relative_to(base);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,target)
 archive=folder/'libjudas_engine.a';shutil.copy2(base/'libjudas_engine.a',archive)
 for filename in [source,source+'.o']:shutil.copy2(donor/filename,folder/filename)
 subprocess.run(['ar','r',str(archive),str(folder/(source+'.o'))],check=True);subprocess.run(['ranlib',str(archive)],check=True)
 target='judas_physical_balance_controller_tests';link=shlex.split((b/'CMakeFiles'/ (target+'.dir')/'link.txt').read_text())
 link=[str(archive) if x=='libjudas_engine.a' else str(folder/'controller') if x==target else x for x in link if not x.startswith('-Wl,--dependency-file')]
 subprocess.run(link,cwd=b,check=True);start=time.perf_counter()
 with (ev/(name+'-controller.log')).open('xb') as log:code=subprocess.run([str(folder/'controller')],cwd=r,stdout=log,stderr=subprocess.STDOUT).returncode
 (ev/(name+'-bisect.json')).write_text(json.dumps({'name':name,'base':base_name,'baseArchiveSHA256':h(base/'libjudas_engine.a'),'donor':donor_name,'donorSourceSHA256':h(folder/source),'donorObjectSHA256':h(folder/(source+'.o')),'engineSHA256':h(archive),'exeSHA256':h(folder/'controller'),'exitCode':code,'seconds':time.perf_counter()-start,'fixtureSHA256':h(r/'tests/PhysicalBalanceControllerTests.cpp'),'scope':'Diagnostic object composition only; no fixture or acceptance edits'},indent=2)+'\n')
 print(name,code,flush=True)
