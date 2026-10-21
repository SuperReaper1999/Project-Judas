from pathlib import Path
import subprocess,shlex,json,hashlib,time
r=Path.cwd();w=r/'.cache/contact-redesign-20261021/negative-final';w.mkdir(exist_ok=True);b=r/'build';flags={};commands=[]
for l in(b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if' = 'in l:
  k,v=l.split(' = ',1);flags[k]=shlex.split(v)
def run(c,cwd=r,log=None):
 commands.append({'argv':list(map(str,c)),'cwd':str(cwd)})
 with Path(log).open('w')as f:return subprocess.run(list(map(str,c)),cwd=cwd,stdout=f,stderr=subprocess.STDOUT).returncode
def build(name,module,replacements):
 d=w/name;d.mkdir(exist_ok=True);s=(r/'src'/(module+'.cpp')).read_text()
 for a,z in replacements:
  if s.count(a)!=1:raise RuntimeError((name,a,s.count(a)))
  s=s.replace(a,z)
 source=d/(module+'.cpp');source.write_text(s);obj=d/(module+'.o');binary=d/'probe'
 compile=['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',source,'-o',obj]
 if run(compile,log=d/'build.log'):raise RuntimeError('compile failed '+name)
 link=shlex.split((b/'CMakeFiles/judas_growing_impact_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(binary);link.insert(link.index('libjudas_engine.a'),str(obj))
 if run(link,cwd=b,log=d/'link.log'):raise RuntimeError('link failed '+name)
 code=run([binary],log=d/'run.log');return {'name':name,'exit':code,'expected':'pass'if not replacements else'detected-broken-mechanism','pass':code==0 if not replacements else code!=0,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()}
variants=[('positive','ImpactSolver',[]),('forget-continuing-support','ImpactSolver',[('contacts[i].continuing &&','false &&')]),('rebaseline-partially-solved','ImpactSolver',[('a.linear + glm::cross(a.angular, c.velocityOffsetA)','c.bodyA->linearVelocity + glm::cross(c.bodyA->angularVelocity, c.velocityOffsetA)'),('b.linear + glm::cross(b.angular, c.velocityOffsetB)','c.bodyB->linearVelocity + glm::cross(c.bodyB->angularVelocity, c.velocityOffsetB)')]),('restart-on-growth','ImpactSolver',[('append(count);','m_solver.Clear(); append(0);')]),('omit-transient-activation','ContactSolver',[('if (changedA && !c.bodyA->IsStatic() && !seen[c.frameA])','if (false && changedA && !c.bodyA->IsStatic() && !seen[c.frameA])'),('if (changedB && !c.bodyB->IsStatic() && !seen[c.frameB])','if (false && changedB && !c.bodyB->IsStatic() && !seen[c.frameB])')])]
results=[]
for args in variants:
 results.append(build(*args));print(results[-1],flush=True)
(w/'RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');(w/'COMMANDS.json').write_text(json.dumps(commands,indent=2)+'\n');raise SystemExit(int(not all(x['pass']for x in results)))
