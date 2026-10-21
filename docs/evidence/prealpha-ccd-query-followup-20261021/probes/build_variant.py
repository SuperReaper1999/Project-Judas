import argparse,hashlib,json,subprocess,shlex,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[2];here=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('name');ap.add_argument('sources',nargs='+');ap.add_argument('--core',type=Path);a=ap.parse_args()
out=here/'variants'/a.name;out.mkdir(parents=True)
variables={k:shlex.split(v) for k,v in (line.split(' = ',1) for line in (root/'build/CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines() if ' = ' in line)}
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
core=a.core.resolve() if a.core else here/'baseline/libjudas_engine.a';archive=out/'libjudas_engine.a';shutil.copy2(core,archive)
report={'baseline_core':sha(core),'sources':{},'commands':[]}
for source in map(lambda s:Path(s).resolve(),a.sources):
 obj=out/(source.name+'.o');argv=['/usr/bin/c++',*variables['CXX_DEFINES'],'-I'+str(source.parent),'-I'+str(root/'src/physics'),*variables['CXX_INCLUDES'],*variables['CXX_FLAGS'],'-g1','-c',str(source),'-o',str(obj)]
 report['sources'][str(source)]=sha(source);report['commands'].append(argv)
 with (out/(source.name+'.compile.log')).open('w') as log:
  code=subprocess.run(argv,cwd=root/'build',stdout=log,stderr=subprocess.STDOUT).returncode
 if code:print((out/(source.name+'.compile.log')).read_text()[-4000:]);raise SystemExit(code)
 subprocess.run(['ar','r',str(archive),str(obj)],check=True)
subprocess.run(['ranlib',str(archive)],check=True)
link=json.loads((here/'diagnostic-build/BUILD-v3.json').read_text())['link'];link=[str(archive) if x==str(here/'baseline/libjudas_engine.a') else x for x in link];link[link.index('-o')+1]=str(out/'live')
report['commands'].append(link)
report['driver_object_sha256']=sha(here/'diagnostic-build/DiagnosticLive.cpp.o')
report['driver_source_sha256']=sha(here/'diagnostic-build/DiagnosticLive.cpp')
with (out/'link.log').open('w') as log:code=subprocess.run(link,cwd=root/'build',stdout=log,stderr=subprocess.STDOUT).returncode
report.update(exit=code,core_sha256=sha(archive),binary_sha256=sha(out/'live') if code==0 else None);(out/'BUILD.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='commands' and k!='sources'}))
raise SystemExit(code)
