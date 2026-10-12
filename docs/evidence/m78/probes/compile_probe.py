#!/usr/bin/env python3
"""Link an isolated audit observer to the unchanged current engine libraries."""
import pathlib,shlex,subprocess,json,hashlib,sys
r=pathlib.Path(__file__).resolve().parents[4];name=sys.argv[1];target=sys.argv[2] if len(sys.argv)>2 else 'judas_joint_tests'
s=r/'docs/evidence/m78/probes'/(name+'.cpp');out=r/'.cache/m78'/name;obj=out.with_suffix('.o')
folder=r/'build/CMakeFiles'/(target+'.dir');flags={}
for line in (folder/'flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
commands=[['c++',*flags.get('CXX_DEFINES',[]),*flags.get('CXX_INCLUDES',[]),*flags.get('CXX_FLAGS',[]),'-c',str(s),'-o',str(obj)]]
link=shlex.split((folder/'link.txt').read_text());link=[str(obj) if x.endswith('.cpp.o') else str(out) if x==target else x for x in link]
link=[x for x in link if not x.startswith('-Wl,--dependency-file')];commands.append(link)
rows=[]
for i,command in enumerate(commands):
 log=r/'docs/evidence/m78'/(name+('-compile.log' if i==0 else '-link.log'))
 if log.exists():raise SystemExit('Retain original compile evidence; use another observer name')
 with log.open('wb') as f:p=subprocess.run(command,cwd=r/'build',stdout=f,stderr=subprocess.STDOUT)
 rows.append({'command':command,'cwd':str(r/'build'),'exitCode':p.returncode,'log':str(log.relative_to(r))})
 if p.returncode:break
receipt={'builds':rows,'sourceSHA256':hashlib.sha256(s.read_bytes()).hexdigest(),'engineLibrarySHA256':hashlib.sha256((r/'build/libjudas_engine.a').read_bytes()).hexdigest()}
if len(rows)==2 and rows[-1]['exitCode']==0:receipt['executableSHA256']=hashlib.sha256(out.read_bytes()).hexdigest()
(r/'docs/evidence/m78'/(name+'-build.json')).write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt));raise SystemExit(rows[-1]['exitCode'])
