from pathlib import Path
import subprocess,hashlib,shlex,json,datetime
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[2];BUILD=ROOT/'build';BASE=ROOT/'.cache/dense-scalability-20261019/candidate-fastpaths-final/libjudas_engine.a'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
flags={}
for line in (BUILD/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
records=[]
def compile(source,obj,extra=[]):
 command=['/usr/bin/c++',*flags['CXX_DEFINES'],'-I'+str(HERE),'-I'+str(ROOT/'src'),'-I'+str(ROOT/'src/physics'),'-I'+str(ROOT),*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],*extra,'-MMD','-MF',str(obj)+'.d','-c',str(source),'-o',str(obj)]
 before={str(p):sha(p) for p in (ROOT/'src').rglob('*') if p.is_file()};own=sha(HERE/'src/physics/WorldImpactEvents.cpp')
 with obj.with_suffix('.compile.log').open('w') as f:r=subprocess.run(command,cwd=ROOT,stdout=f,stderr=subprocess.STDOUT)
 if before!={str(p):sha(p) for p in (ROOT/'src').rglob('*') if p.is_file()} or own!=sha(HERE/'src/physics/WorldImpactEvents.cpp'):raise SystemExit('source changed during compile')
 if r.returncode:print(obj.with_suffix('.compile.log').read_text());raise SystemExit(r.returncode)
 deps=Path(str(obj)+'.d').read_text().replace('\\\n',' ');paths=shlex.split(deps.split(':',1)[1]);record={'compile':command,'exit':r.returncode,'object_sha256':sha(obj),'inputs':{str(Path(p)):sha(Path(p)) for p in paths if Path(p).is_file()}};records.append(record);return record
source=HERE/'src/physics/WorldImpactEvents.cpp';obj=HERE/'WorldImpactEvents.cpp.o';compile(source,obj)
recipe=shlex.split((BUILD/'CMakeFiles/judas_engine.dir/link.txt').read_text().splitlines()[0]);ordered=[x for x in recipe if x.endswith('.o')];names=subprocess.check_output(['ar','t',str(BASE)],text=True).splitlines()
if [Path(p).name for p in ordered]!=names:raise SystemExit('baseline archive/CMake member order mismatch')
raw=BASE.read_bytes();pos=8;longnames=b'';members=[]
if raw[:8]!=b'!<arch>\n':raise SystemExit('unsupported archive')
while pos<len(raw):
 h=raw[pos:pos+60];size=int(h[48:58]);name=h[:16].decode().strip();data=raw[pos+60:pos+60+size];pos+=60+size+(size&1)
 if name=='//':longnames=data;continue
 if name in ['/','/SYM64/']:continue
 if name.startswith('/') and name[1:].isdigit():name=longnames[int(name[1:]):].split(b'/\n',1)[0].decode()
 else:name=name.rstrip('/')
 members.append((name,data))
if [p[0] for p in members]!=names:raise SystemExit('ordinal parser mismatch')
inputs=[];mapping=[]
for ordinal,(recipe,(name,data)) in enumerate(zip(ordered,members)):
 replacement=recipe.endswith('.dir/src/physics/WorldImpactEvents.cpp.o')
 chosen=obj if replacement else HERE/'members'/str(ordinal)/name
 if not replacement:chosen.parent.mkdir(parents=True,exist_ok=True);chosen.write_bytes(data)
 inputs.append(str(chosen));mapping.append({'ordinal':ordinal,'recipe':recipe,'member':name,'baseline_sha256':hashlib.sha256(data).hexdigest(),'chosen_sha256':sha(chosen),'replaced':replacement})
if sum(x['replaced'] for x in mapping)!=1:raise SystemExit('must replace exactly one qualified archive member')
archive=HERE/'libjudas_engine.a'
if archive.exists():archive.unlink()
subprocess.run(['ar','qc',str(archive),*inputs],check=True);subprocess.run(['ar','s',str(archive)],check=True)
linkbase=shlex.split((BUILD/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());linkbase=[x for x in linkbase if not x.startswith('-Wl,--dependency-file=')]
def link(binary,objects=None,lib=archive):
 command=[str(lib) if x=='libjudas_engine.a' else x for x in linkbase]
 if objects is not None:command=[x for x in command if not x.endswith('.cpp.o')];command[command.index('-o'):command.index('-o')]=[str(x) for x in objects]
 command[command.index('-o')+1]=str(binary)
 with binary.with_suffix('.link.log').open('w') as f:r=subprocess.run(command,cwd=BUILD,stdout=f,stderr=subprocess.STDOUT)
 if r.returncode:print(binary.with_suffix('.link.log').read_text());raise SystemExit(r.returncode)
 return {'link':command,'exit':r.returncode,'binary':str(binary),'binary_sha256':sha(binary)}
normal=link(HERE/'judas_physics_stress_live_benchmark')
controls=[];mutants=HERE/'mutants';mutants.mkdir(exist_ok=True)
for label,old,new in [('positive',None,None),('missing-generation','cached->boundGeneration == body.generation &&','true &&'),('missing-trajectory-time','cached->boundStart <= time && cached->boundFinish >= time &&','true &&')]:
 extra=[]
 if old:
  text=source.read_text();assert old in text;mutant=mutants/(label+'.cpp');mutant.write_text(text.replace(old,new,1));extra=['-DJUDAS_EVENT_SOURCE="'+str(mutant)+'"']
 unitobj=HERE/('guard-'+label+'.o');compile(HERE/'CachedDiscoveryTests.cpp',unitobj,extra)
 binary=HERE/('guard-'+label);record=link(binary,[unitobj],BASE)
 r=subprocess.run([str(binary)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(HERE/('guard-'+label+'.log')).write_text(r.stdout)
 record.update({'mode':label,'run_exit':r.returncode,'output':r.stdout});controls.append(record);print(label,r.returncode,r.stdout.strip(),flush=True)
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'base_archive':str(BASE),'base_archive_sha256':sha(BASE),'replacement_source_sha256':sha(source),'archive_sha256':sha(archive),'archive_members':mapping,'compilation':records,'normal':normal,'controls':controls,'no_layout_change':True}
(HERE/'BUILD.json').write_text(json.dumps(result,indent=2)+'\n');print(normal['binary'])
