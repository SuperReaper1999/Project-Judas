#!/usr/bin/env python3
import pathlib,shlex,subprocess,json,hashlib,sys
r=pathlib.Path(__file__).resolve().parents[4];build=r/'.cache/m78-followup/release';name=sys.argv[1];target=sys.argv[2] if len(sys.argv)>2 else 'judas_joint_tests'
s=r/'docs/evidence/m78-followup/probes'/(name+'.cpp');out=r/'.cache/m78-followup'/name;obj=out.with_suffix('.o');folder=build/'CMakeFiles'/(target+'.dir');flags={}
for line in (folder/'flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
commands=[['c++',*flags.get('CXX_DEFINES',[]),*flags.get('CXX_INCLUDES',[]),*flags.get('CXX_FLAGS',[]),'-c',str(s),'-o',str(obj)]]
link=shlex.split((folder/'link.txt').read_text());replaced=False
for i,x in enumerate(link):
 if x.endswith('.cpp.o') and not replaced:link[i]=str(obj);replaced=True
 elif x==target:link[i]=str(out)
commands.append([x for x in link if not x.startswith('-Wl,--dependency-file')]);rows=[]
for i,command in enumerate(commands):
 log=r/'docs/evidence/m78-followup'/(name+('-final-compile.log' if i==0 else '-final-link.log'))
 if log.exists():raise SystemExit('Preserve prior build receipt')
 with log.open('wb') as f:p=subprocess.run(command,cwd=build,stdout=f,stderr=subprocess.STDOUT)
 rows.append({'command':command,'cwd':str(build),'exitCode':p.returncode,'log':str(log.relative_to(r))})
 if p.returncode:break
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();receipt={'builds':rows,'sourceSHA256':h(s),'engineLibrarySHA256':h(build/'libjudas_engine.a')}
if len(rows)==2 and rows[-1]['exitCode']==0:receipt['executableSHA256']=h(out)
(r/'docs/evidence/m78-followup'/(name+'-final-build.json')).write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({'name':name,'exit':rows[-1]['exitCode']}));raise SystemExit(rows[-1]['exitCode'])
