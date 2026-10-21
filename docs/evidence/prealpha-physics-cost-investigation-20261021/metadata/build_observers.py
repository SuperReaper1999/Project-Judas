from pathlib import Path
import shutil,subprocess,shlex,json,hashlib,re,sys
r=Path.cwd(); w=r/'.cache/physics-exhaustive-profile-20261021'; b=r/'build'; dst=w/'buildsrc'; dst.mkdir(exist_ok=True)
def flags(target):
 d={}
 for line in (b/f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
  if ' = ' in line:
   k,v=line.split(' = ',1);d[k]=shlex.split(v)
 return d
assert hashlib.sha256((b/'libjudas_engine.a').read_bytes()).hexdigest() == '05f626d3fb5400bfd1db23ec37a3cd3e6490f8452145365662466eebccd705ec', 'Original engine archive drifted from the recorded f5624f0b baseline'
commands=[]
def run(c,cwd=r):
 print('BUILD',Path(str(c[-1])).name,flush=True); commands.append(list(map(str,c)));subprocess.run(list(map(str,c)),cwd=cwd,check=True)
def driver(observed):
 s=(r/'tests/PhysicsStressLiveBenchmark.cpp').read_text();s='#include "MainThreadSampler.h"\n'+s
 s=s.replace(' frames<<std::setprecision(12);',' sampler::Session cpuSampler(std::filesystem::path(output)/"sampling",std::getenv("JUDAS_CPU_SAMPLE")!=nullptr);\n frames<<std::setprecision(12);')
 s=s.replace('steps.flush();});','steps.flush();cpuSampler.MarkCompletedStep(step);AUDIT_STEP});')
 s=s.replace('control.beforeShutdown=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){','control.beforeShutdown=[&](EngineHost&,RuntimeWorld& world,InteractivePlay&){cpuSampler.Stop();')
 if observed:
  s='#include "EventAudit.h"\n#include "CCDAudit.h"\n#include "ResponseAudit.h"\n'+s
  init='''std::ofstream ccdFile(output+"/ccd.csv"),responseFile(output+"/response.csv"),eventStepFile(output+"/event-steps.csv"),eventFile(output+"/events.csv");
 ccd_audit::Header(ccdFile);response_audit::Header(responseFile);event_audit::Header(eventStepFile);event_audit::EventHeader(eventFile);
 const bool auditRequested=std::getenv("JUDAS_DEMAND_AUDIT")!=nullptr;
 const unsigned auditStart=std::getenv("JUDAS_AUDIT_START")?std::stoul(std::getenv("JUDAS_AUDIT_START")):300;
 const unsigned auditEnd=std::getenv("JUDAS_AUDIT_END")?std::stoul(std::getenv("JUDAS_AUDIT_END")):900;
 const auto auditSetup=[&](unsigned next){ccd_audit::Reset();response_audit::Reset();event_audit::Reset();bool e=auditRequested&&next>=auditStart&&next<=auditEnd;ccd_audit::SetEnabled(e);response_audit::SetEnabled(e);event_audit::SetEnabled(e);};auditSetup(1);
 '''
  s=s.replace(' sampler::Session cpuSampler', ' '+init+'sampler::Session cpuSampler')
  s=s.replace('AUDIT_STEP','if(auditRequested&&step>=auditStart&&step<=auditEnd){ccd_audit::WriteStep(ccdFile,step);response_audit::WriteStep(responseFile,step);event_audit::WriteStep(eventStepFile,eventFile,step);}auditSetup(step+1);')
 else:s=s.replace('AUDIT_STEP','')
 return s
for name,source in [('MainThreadSampler.h',w/'sampler/MainThreadSampler.h'),('ResponseAudit.h',w/'response/ResponseAudit.h'),('CCDAudit.h',w/'ccd/CCDAudit.h'),('EventAudit.h',w/'events/EventAudit.h')]:shutil.copy2(source,dst/name)
mode=sys.argv[1] if len(sys.argv)>1 else 'sampler'
f=flags('judas_physics_stress_live_benchmark');src=dst/('ObservedLive.cpp' if mode=='overlay' else 'SampledLive.cpp');src.write_text(driver(mode=='overlay'));obj=src.with_suffix('.cpp.o')
run(['/usr/bin/c++',*f['CXX_DEFINES'],'-I'+str(dst),*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-g1','-c',src,'-o',obj])
core=b/'libjudas_engine.a'
if mode=='overlay':
 sources=re.findall(r'\bsrc/[\w/]+\.cpp',(r/'cmake/PhysicsWorldSources.cmake').read_text())+['src/ImpactSolver.cpp','src/ContactSolver.cpp'];objects=[];f=flags('judas_engine')
 for source in sources:
  n=Path(source).name;p=dst/n;override=(w/'ccd'/n if n=='WorldCCDSearch.cpp' else w/'events'/n if n=='WorldImpactEvents.cpp' else w/'response'/n if n in ['WorldIncrementalImpact.cpp','ImpactSolver.cpp','ContactSolver.cpp'] else r/source)
  s=override.read_text();s=s.replace('"physics/WorldInternal.h"','"WorldInternal.h"');p.write_text(s)
 for n in ['WorldInternal.h','WorldHotAccessors.h','WorldImpactQueries.h']:shutil.copy2(w/'ccd'/n,dst/n)
 shutil.copy2(r/'src/physics/WorldIncrementalImpact.h',dst/'WorldIncrementalImpact.h')
 for source in sources:
  n=Path(source).name;p=dst/n;o=dst/(n+'.o');run(['/usr/bin/c++',*f['CXX_DEFINES'],'-I'+str(dst),'-I'+str(r/'src/physics'),*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-g1','-c',p,'-o',o]);objects.append(o)
 core=w/'libjudas_engine_observed.a'
 original=shlex.split((b/'CMakeFiles/judas_engine.dir/link.txt').read_text().splitlines()[0])[3:]
 reconstructed=w/'reconstructed-original.a';reconstructed.unlink(missing_ok=True)
 run(['ar','qc',reconstructed,*original],b);run(['ranlib',reconstructed])
 if hashlib.sha256(reconstructed.read_bytes()).hexdigest()!=hashlib.sha256((b/'libjudas_engine.a').read_bytes()).hexdigest():raise RuntimeError('CMake objects do not reconstruct the frozen original archive')
 core.unlink(missing_ok=True)
 replacements={str(Path(source)):str(dst/(Path(source).name+'.o'))for source in sources}
 selected=[]
 for original_path in original:
  relative=original_path.removeprefix('CMakeFiles/judas_engine.dir/').removesuffix('.o')
  selected.append(replacements.get(relative,original_path))
 run(['ar','qc',core,*selected],b);run(['ranlib',core])
link=shlex.split((b/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(w/('observed-live' if mode=='overlay' else 'sampled-live'));link=[str(core)if x=='libjudas_engine.a' else str(obj) if x.endswith('/tests/PhysicsStressLiveBenchmark.cpp.o') else x for x in link];run(link,b)
(w/f'BUILD-{mode}.json').write_text(json.dumps({'commands':commands,'core_sha256':hashlib.sha256(core.read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256(Path(link[link.index('-o')+1]).read_bytes()).hexdigest()},indent=2)+'\n')
