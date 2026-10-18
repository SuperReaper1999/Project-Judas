from pathlib import Path
import subprocess,json,shlex,time
R=Path.cwd();D=R/'.cache/prealpha-pass5/ccd/union-v3';test=(R/'tests/ImpactMeshCostTests.cpp').read_text();needle='}catch(const std::runtime_error&){++result.threw;continue;}'
assert test.count(needle)==1
test=test.replace(needle,'}catch(const std::runtime_error& error){std::cerr<<"UNCERTAIN box="<<i<<" rotated="<<(frame!=glm::quat(1,0,0,0))<<" detail="<<error.what()<<"\\n";++result.threw;continue;}')
source=D/'DiagnosticImpactMeshCostTests.cpp';source.write_text(test)
rec={'purpose':'unchanged inputs/assertions with catch-message output only','original_sha256':__import__('hashlib').sha256((R/'tests/ImpactMeshCostTests.cpp').read_bytes()).hexdigest(),'commands':[]}
def run(name,args,cwd=R):
 t=time.monotonic()
 with (D/(name+'.log')).open('w') as f:r=subprocess.run(args,cwd=cwd,stdout=f,stderr=subprocess.STDOUT,timeout=30)
 rec['commands'].append({'name':name,'argv':args,'exit':r.returncode,'seconds':time.monotonic()-t});(D/'DIAGNOSTIC.json').write_text(json.dumps(rec,indent=2));print(name,r.returncode,flush=True);return r.returncode
if run('compile-diagnostic',['c++','-std=c++17','-O3','-DNDEBUG','-I'+str(R/'src'),'-c',str(source),'-o',str(D/'diagnostic.o')]):raise SystemExit(1)
build=json.loads((D/'BUILD.json').read_text());link=next(x['argv'] for x in build['commands'] if x['name']=='link-mesh');link=[x for x in link if not (x.endswith('.o') and 'ImpactMeshCostTests.cpp.o' in x)];link[1:1]=[str(D/'diagnostic.o')];link[link.index('-o')+1]=str(D/'diagnostic');
if run('link-diagnostic',link,R/'build'):raise SystemExit(1)
run('diagnostic',[str(D/'diagnostic'),'1','2x26'])
