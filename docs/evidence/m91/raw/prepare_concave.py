from pathlib import Path
import subprocess,shlex,hashlib,json
root=Path(__file__).resolve().parents[2];build=root/'build';out=root/'.cache/m91/concave';out.mkdir(exist_ok=False)
original=root/'docs/evidence/m78-followup/probes/concave_final.cpp';source=out/'concave_final.cpp';source.write_bytes(original.read_bytes())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
flags={}
for line in (build/'CMakeFiles/judas_balance_lab_tests.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
obj=out/'observer.o';cmd=['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],'-I'+str(root/'third_party'),*flags['CXX_FLAGS'],'-c',str(source),'-o',str(obj)]
subprocess.run(cmd,cwd=build,check=True)
links={};original_object='CMakeFiles/judas_balance_lab_tests.dir/tests/BalanceLabTests.cpp.o'
for label in ['before','after']:
 command=shlex.split((build/'CMakeFiles/judas_balance_lab_tests.dir/link.txt').read_text());command=[x for x in command if not x.startswith('-Wl,--dependency-file=')];command[command.index(original_object)]=str(obj);command[command.index('-o')+1]=str(out/label)
 if label=='before':command[command.index('libjudas_engine.a')]=str(root/'.cache/m91/baseline/libjudas_engine.a')
 subprocess.run(command,cwd=build,check=True);links[label]={'command':command,'binary_sha256':sha(out/label)}
(out/'LINK.json').write_text(json.dumps({'source_sha256':sha(source),'original_sha256':sha(original),'compile':cmd,'links':links},indent=2)+'\n')
