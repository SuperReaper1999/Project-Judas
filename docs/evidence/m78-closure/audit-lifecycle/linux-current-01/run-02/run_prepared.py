#!/usr/bin/env python3
"""Explicitly scheduled standalone compilation/execution, never a CMake mutation."""
import argparse,hashlib,json,pathlib,shlex,subprocess,time
parser=argparse.ArgumentParser()
parser.add_argument('--execute',action='store_true',help='Required to compile or run; root schedules Linux/Windows sequentially.')
parser.add_argument('--repo',type=pathlib.Path,default=pathlib.Path.cwd())
parser.add_argument('--build-dir',type=pathlib.Path,default=pathlib.Path('build'))
parser.add_argument('--engine-archive',type=pathlib.Path,required=True)
parser.add_argument('--output',type=pathlib.Path,required=True)
parser.add_argument('--only',choices=['streaming','mod-policy','navigation'])
a=parser.parse_args();repo=a.repo.resolve();build=(repo/a.build_dir).resolve();archive=(repo/a.engine_archive).resolve();source=repo/'docs/evidence/m78-closure/audit-lifecycle';out=(repo/a.output).resolve()
cases=[('streaming','StreamingMotorPublication.cpp','judas_streaming_tests'),('mod-policy','ResolvedPrefabModPolicy.cpp','judas_mod_content_tests'),('navigation','NavigationInputRace.cpp','judas_navigation_dependency_tests')]
if a.only:cases=[v for v in cases if v[0]==a.only]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def command(c,path):
 start=time.monotonic();result=subprocess.run(c,cwd=repo,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
 path.with_suffix('.stdout.log').write_bytes(result.stdout);path.with_suffix('.stderr.log').write_bytes(result.stderr)
 return {'command':c,'cwd':str(repo),'exitCode':result.returncode,'seconds':time.monotonic()-start}
recipes=[]
for name,filename,target in cases:
 flags=build/f'CMakeFiles/{target}.dir/flags.make';link=build/f'CMakeFiles/{target}.dir/link.txt';obj=out/(name+'.o');exe=out/(name+'.exe')
 lines=dict(line.split(' = ',1) for line in flags.read_text().splitlines() if ' = ' in line)
 compile=['/usr/bin/c++']+sum([shlex.split(lines[k]) for k in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']],[])+['-c',str(source/filename),'-o',str(obj)]
 raw=shlex.split(link.read_text());linkcmd=[]
 for token in raw:
  if token.startswith('-Wl,--dependency-file='):continue
  if token.endswith('.cpp.o'):token=str(obj)
  elif token==target:token=str(exe)
  elif token=='libjudas_engine.a':token=str(archive)
  elif token.endswith('.a') or (token.endswith('.so') and not token.startswith('/')):token=str((build/token).resolve())
  linkcmd.append(token)
 recipes.append({'name':name,'source':str(source/filename),'sourceSHA256':sha(source/filename),'compile':compile,'link':linkcmd,'run':[str(exe),str(out/(name+'-fixture'))],'originalFlags':str(flags),'originalLink':str(link),'templateFlagsSHA256':sha(flags),'templateLinkSHA256':sha(link)})
identity={'schema':1,'engineArchive':str(archive),'engineArchiveSHA256':sha(archive),'fixtureSupportSHA256':sha(source/'FixtureSupport.h'),'recipes':recipes,'scheduled':a.execute}
if not a.execute:print(json.dumps(identity,indent=2));raise SystemExit(0)
if out.exists():raise SystemExit('output already exists; choose a fresh owned directory')
out.mkdir(parents=True);(out/'IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n');receipts=[]
for rec in recipes:
 row={'name':rec['name']};row['compile']=command(rec['compile'],out/(rec['name']+'-compile'))
 if not row['compile']['exitCode']:
  row['link']=command(rec['link'],out/(rec['name']+'-link'))
  if not row['link']['exitCode']:
   row['executableSHA256']=sha(pathlib.Path(rec['run'][0]));row['run']=command(rec['run'],out/(rec['name']+'-run'))
 receipts.append(row);(out/'RECEIPTS.json').write_text(json.dumps(receipts,indent=2)+'\n')
 print(json.dumps(row),flush=True)
raise SystemExit(int(any(r.get('run',{}).get('exitCode',1) for r in receipts)))
