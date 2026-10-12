#!/usr/bin/env python3
"""Compile an observer against a specified isolated engine archive."""
import argparse, hashlib, json, pathlib, shlex, subprocess
ROOT=pathlib.Path(__file__).resolve().parents[4]
p=argparse.ArgumentParser()
p.add_argument('--name',required=True)
p.add_argument('--source',required=True,type=pathlib.Path)
p.add_argument('--archive',required=True,type=pathlib.Path)
p.add_argument('--headers',type=pathlib.Path)
p.add_argument('--extra-object',action='append',type=pathlib.Path,default=[])
p.add_argument('--target',default='judas_joint_tests')
p.add_argument('--build',type=pathlib.Path,default=ROOT/'.cache/m78-followup/release')
a=p.parse_args(); build=a.build.resolve(); folder=build/'CMakeFiles'/(a.target+'.dir')
output=ROOT/'.cache/m78-continuation'/a.name; obj=output.with_suffix('.o')
receipt=ROOT/'docs/evidence/m78-continuation'/(a.name+'-build.json')
if receipt.exists():raise SystemExit('Choose a new name; preserve build evidence')
flags={}
for line in (folder/'flags.make').read_text().splitlines():
    if ' = ' in line:
        k,v=line.split(' = ',1);flags[k]=shlex.split(v)
include=['-I'+str(a.headers.resolve())] if a.headers else []
commands=[['c++',*flags.get('CXX_DEFINES',[]),*include,
    *flags.get('CXX_INCLUDES',[]),*flags.get('CXX_FLAGS',[]),'-c',
    str(a.source.resolve()),'-o',str(obj)]]
link=shlex.split((folder/'link.txt').read_text()); replaced=False
for i,arg in enumerate(link):
    if arg.endswith('.cpp.o') and not replaced:link[i]=str(obj);replaced=True
    elif arg==a.target:link[i]=str(output)
    elif arg=='libjudas_engine.a':link[i]=str(a.archive.resolve())
link=[arg for arg in link if not arg.startswith('-Wl,--dependency-file')]
for extra in a.extra_object:
    link.insert(link.index(str(a.archive.resolve())),str(extra.resolve()))
commands.append(link)
rows=[]
for i,command in enumerate(commands):
    log=receipt.with_name(a.name+f'-build-{i}.log')
    if log.exists():raise SystemExit('Preserve prior build log')
    with log.open('wb') as stream:
        code=subprocess.run(command,cwd=build,stdout=stream,stderr=subprocess.STDOUT).returncode
    rows.append(dict(command=command,cwd=str(build),exitCode=code,log=str(log.relative_to(ROOT))))
    if code:break
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
record=dict(builds=rows,sourceSHA256=sha(a.source),archiveSHA256=sha(a.archive))
if not code:record['executableSHA256']=sha(output)
receipt.write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(dict(name=a.name,exitCode=code)));raise SystemExit(code)
