#!/usr/bin/env python3
"""PB-only resolved ankle-reference diagnostic; no production/test changes."""
import pathlib,shlex,subprocess,json,hashlib,shutil,time,sys
r=pathlib.Path.cwd();b=r/'.cache/m78-followup/release';ev=r/'docs/evidence/m78-continuation'
out=r/'.cache/m78-continuation/variants';owned=r/'.cache/m78-continuation/foot-reference-overlay/PhysicalBalance.cpp'
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();flags={}
for line in (b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
for arg in sys.argv[1:]:
 name,base_name=arg.split(':');base=out/base_name;folder=out/name;folder.mkdir(exist_ok=False)
 for header in base.rglob('*'):
  if header.is_file() and header.suffix in ['.h','.hpp']:
   dest=folder/header.relative_to(base);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(header,dest)
 shutil.copy2(owned,folder/'PhysicalBalance.cpp')
 archive=folder/'libjudas_engine.a';shutil.copy2(base/'libjudas_engine.a',archive);obj=folder/'PhysicalBalance.cpp.o'
 command=['c++',*flags.get('CXX_DEFINES',[]),'-I'+str(folder),*flags.get('CXX_INCLUDES',[]),*flags.get('CXX_FLAGS',[]),'-c',str(folder/'PhysicalBalance.cpp'),'-o',str(obj)]
 with (ev/(name+'-compile.log')).open('xb') as log:code=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT).returncode
 assert code==0,name
 subprocess.run(['ar','r',str(archive),str(obj)],check=True);subprocess.run(['ranlib',str(archive)],check=True)
 target='judas_physical_balance_controller_tests';link=shlex.split((b/'CMakeFiles'/ (target+'.dir')/'link.txt').read_text())
 link=[str(archive) if x=='libjudas_engine.a' else str(folder/'controller') if x==target else x for x in link if not x.startswith('-Wl,--dependency-file')]
 subprocess.run(link,cwd=b,check=True);start=time.perf_counter()
 with (ev/(name+'-controller.log')).open('xb') as log:code=subprocess.run([str(folder/'controller')],cwd=r,stdout=log,stderr=subprocess.STDOUT).returncode
 (ev/(name+'-bisect.json')).write_text(json.dumps({'name':name,'base':base_name,'sourceSHA256':h(folder/'PhysicalBalance.cpp'),'buildCommand':command,'engineSHA256':h(archive),'exeSHA256':h(folder/'controller'),'exitCode':code,'seconds':time.perf_counter()-start,'fixtureSHA256':h(r/'tests/PhysicalBalanceControllerTests.cpp'),'scope':'Diagnostic PB-only ankle reference alignment; authored controller settings unchanged'},indent=2)+'\n')
 print(name,code,flush=True)
