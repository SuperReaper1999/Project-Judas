from pathlib import Path
import shlex,subprocess,json,hashlib,shutil,time
root=Path.cwd();out=root/'.cache/dense-scalability-20261019/support-prototype';base=root/'.cache/dense-scalability-20261019/baseline-e30';build=root/'build'
flags=(build/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines()
args=[]
for name in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:
 args+=shlex.split(next(x.split('=',1)[1].strip()for x in flags if x.startswith(name+' =')))
receipt={'parent':'e30d5f9bb72a1a341d3eff7485b8d6b3eb6f8ea9','commands':[],'sha256':{},'scope':'Isolated one-object support publication comparison; base archive frozen before source changes'}
def run(cmd,cwd=root):
 receipt['commands'].append({'argv':list(map(str,cmd)),'cwd':str(cwd)})
 subprocess.run(list(map(str,cmd)),cwd=cwd,check=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for stem,source in [('timed','WorldImpactEvents-timed.cpp'),('batched','WorldImpactEvents.cpp')]:
 d=out/stem;d.mkdir(exist_ok=False);obj=d/'WorldImpactEvents.cpp.o'
 run(['c++',*args,'-I'+str(out),'-I'+str(root/'src/physics'),'-c',out/source,'-o',obj])
 archive=d/'libjudas_engine.a';shutil.copy2(base/'libjudas_engine.a',archive)
 run(['ar','r',archive,obj])
 cmd=shlex.split((build/'CMakeFiles/judas_physics_stress_live_benchmark.dir/link.txt').read_text())
 cmd[cmd.index('-o')+1]=str(d/'judas_physics_stress_live_benchmark');cmd=[str(archive)if x=='libjudas_engine.a'else x for x in cmd]
 run(cmd,build)
 variant=root/('.cache/dense-scalability-20261019/candidate-support-'+stem);(variant/'bin').mkdir(parents=True,exist_ok=False)
 exe=variant/'bin/judas_physics_stress_live_benchmark';shutil.copy2(d/exe.name,exe)
 m=json.loads((base/'FROZEN.json').read_text());s=sha(exe)
 m['artifacts']={'bin/judas_physics_stress_live_benchmark':{'path':str((d/exe.name).resolve()),'sha256':s,'copy_sha256':s}}
 m['classification']='isolated support set '+stem+' one-object override';m['variant_receipt']=str(out/'BUILD.json');m['base_manifest_sha256']=sha(base/'FROZEN.json')
 (variant/'FROZEN.json').write_text(json.dumps(m,indent=2)+'\n')
 for p in [out/source,obj,archive,exe]:receipt['sha256'][str(p.relative_to(root))]=sha(p)
(out/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
