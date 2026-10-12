#!/usr/bin/env python3
"""One explicitly scheduled Linux TU, unchanged Release link template; no CMake edits."""
import argparse,hashlib,json,pathlib,shlex,subprocess,time
p=argparse.ArgumentParser();p.add_argument('--execute',action='store_true');p.add_argument('--build-dir',type=pathlib.Path,default=pathlib.Path('.cache/m78-followup/release'));p.add_argument('--engine-archive',type=pathlib.Path,default=pathlib.Path('.cache/m78-closure/final-build/libjudas_engine.a'));p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--expected-archive-sha256',default='a5ec91426ce0f8476db3bbdbda8ad52f3094d38c37b64a0052c7ef4bd37c2e1f');a=p.parse_args()
repo=pathlib.Path.cwd().resolve();src=repo/'docs/evidence/m78-closure/audit-lifecycle/save-exclusion/SaveExclusion.cpp';build=(repo/a.build_dir).resolve();archive=(repo/a.engine_archive).resolve();out=(repo/a.output).resolve();target='judas_world_persistence_tests'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
if sha(archive)!=a.expected_archive_sha256:raise SystemExit('engine archive hash differs: require explicit root-approved identity')
flags=build/f'CMakeFiles/{target}.dir/flags.make';link=build/f'CMakeFiles/{target}.dir/link.txt';settings=dict(line.split(' = ',1) for line in flags.read_text().splitlines() if ' = ' in line)
obj=out/'SaveExclusion.o';exe=out/'SaveExclusion.exe'
compilecmd=['/usr/bin/c++']+sum([shlex.split(settings[k]) for k in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']],[])+['-c',str(src),'-o',str(obj)]
linkcmd=[]
for token in shlex.split(link.read_text()):
 if token.startswith('-Wl,--dependency-file='):continue
 if token.endswith('.cpp.o'):token=str(obj)
 elif token==target:token=str(exe)
 elif token=='libjudas_engine.a':token=str(archive)
 elif token.endswith('.a') or (token.endswith('.so') and not token.startswith('/')):token=str((build/token).resolve())
 linkcmd.append(token)
identity={'scope':'Linux only; one newly authored driver; not executed unless root GO','HEAD':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'source':str(src),'sourceSHA256':sha(src),'helperSHA256':sha(src.parent.parent/'FixtureSupport.h'),'engineArchive':str(archive),'engineArchiveSHA256':sha(archive),'flagsSHA256':sha(flags),'linkTemplateSHA256':sha(link),'linkedLibrarySHA256':{token:sha(pathlib.Path(token)) for token in linkcmd if token.endswith(('.a','.so')) and pathlib.Path(token).is_file()},'compile':compilecmd,'link':linkcmd,'run':[str(exe),str(out/'fixture')],'execute':a.execute}
if not a.execute:print(json.dumps(identity,indent=2));raise SystemExit(0)
if out.exists():raise SystemExit('output exists: preserve prior evidence and choose a fresh path')
out.mkdir(parents=True);(out/'IDENTITY.json').write_text(json.dumps(identity,indent=2)+'\n');receipts={}
for name,cmd in [('compile',compilecmd),('link',linkcmd),('run',identity['run'])]:
 start=time.monotonic();r=subprocess.run(cmd,cwd=repo,stdout=subprocess.PIPE,stderr=subprocess.PIPE);(out/(name+'.stdout.log')).write_bytes(r.stdout);(out/(name+'.stderr.log')).write_bytes(r.stderr);receipts[name]={'command':cmd,'exitCode':r.returncode,'seconds':time.monotonic()-start}
 if name=='link' and not r.returncode:receipts['executableSHA256']=sha(exe)
 (out/'RECEIPTS.json').write_text(json.dumps(receipts,indent=2)+'\n');print(json.dumps(receipts[name]),flush=True)
 if r.returncode:raise SystemExit(r.returncode)
