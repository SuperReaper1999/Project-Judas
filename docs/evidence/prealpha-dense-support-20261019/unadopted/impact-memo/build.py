#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,shlex,shutil,subprocess,sys,datetime
O=Path(__file__).resolve().parent;R=O.parents[2];B=R/'build';BASE=O.parent/'candidate-fastpaths-final/libjudas_engine.a';CAP=O.parent/'contact-system-research'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def run(argv,tag,cwd=R):
 p=subprocess.run(list(map(str,argv)),cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(O/(tag+'.log')).write_text(p.stdout);receipt['commands'].append({'argv':list(map(str,argv)),'cwd':str(cwd),'exit':p.returncode,'log':tag+'.log'});persist();
 if p.returncode:raise SystemExit(p.returncode)
def persist():(O/'BUILD.json').write_text(json.dumps(receipt,indent=2)+'\n')
receipt={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'base_archive':str(BASE),'base_archive_sha256':sha(BASE),'commands':[]}
f={}
for l in (B/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in l:k,v=l.split(' = ',1);f[k]=shlex.split(v)
common=['/usr/bin/c++',*f['CXX_DEFINES'],*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-I'+str(R),'-I'+str(CAP)]
run([*common,'-c',O/'ImpactSolver.cpp','-o',O/'ImpactSolver.cpp.o'],'compile-memo')
archive=O/'libjudas_engine.a';shutil.copy2(BASE,archive);names=subprocess.check_output(['ar','t',BASE],text=True).splitlines();assert names.count('ImpactSolver.cpp.o')==1
run(['ar','r',archive,O/'ImpactSolver.cpp.o'],'replace-one-object');run(['ranlib',archive],'index-archive');assert subprocess.check_output(['ar','t',archive],text=True).splitlines()==names
# Parse every ordered member including duplicate filenames, excluding GNU symbol
# and long-name tables; verify all non-replacement bytes, not just member names.
def members(path):
 data=Path(path).read_bytes();assert data[:8]==b'!<arch>\n';p=8;long=b'';out=[]
 while p<len(data):
  h=data[p:p+60];assert h[58:60]==b'`\n';name=h[:16].decode().strip();n=int(h[48:58]);v=data[p+60:p+60+n];p+=60+n+(n%2)
  if name=='//':long=v;continue
  if name in ('/','/SYM64/'):continue
  if name.startswith('/') and name[1:].isdigit():name=long[int(name[1:]):].split(b'/\n',1)[0].decode()
  else:name=name.rstrip('/')
  out.append((name,hashlib.sha256(v).hexdigest()))
 return out
before=members(BASE);after=members(archive);assert len(before)==len(after);diff=[]
for i,(a,b) in enumerate(zip(before,after)):
 assert a[0]==b[0]
 if a!=b:diff.append({'ordinal':i,'name':a[0],'before':a[1],'after':b[1]})
assert len(diff)==1 and diff[0]['name']=='ImpactSolver.cpp.o';receipt['ordered_member_changes']=diff;receipt['archive_sha256']=sha(archive)
link=shlex.split((B/'CMakeFiles/judas_impact_solver_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=') and not x.endswith('tests/ValidationMain.cpp.o')]
for stem in ('MemoReplay','MemoControls'):
 obj=O/(stem+'.cpp.o');run([*common,'-c',O/(stem+'.cpp'),'-o',obj],'compile-'+stem)
 for variant,lib in [('baseline',BASE),('candidate',archive)]:
  binary=O/(stem+'-'+variant);cmd=[str(obj) if x.endswith('tests/ImpactSolverTests.cpp.o') else str(lib) if x=='libjudas_engine.a' else x for x in link];cmd[cmd.index('-o')+1]=str(binary);run(cmd,'link-'+stem+'-'+variant,B);receipt.setdefault('binaries',{})[binary.name]=sha(binary)
# Untouched original regression gets the unchanged provenance wrapper.
full=shlex.split((B/'CMakeFiles/judas_impact_solver_tests.dir/link.txt').read_text());full=[x for x in full if not x.startswith('-Wl,--dependency-file=')]
for variant,lib in [('baseline',BASE),('candidate',archive)]:
 binary=O/('ImpactSolverTests-'+variant);cmd=[str(lib) if x=='libjudas_engine.a' else x for x in full];cmd[cmd.index('-o')+1]=str(binary);run(cmd,'link-ImpactSolverTests-'+variant,B);receipt.setdefault('binaries',{})[binary.name]=sha(binary)
# Detecting control: eager validation of an otherwise unused static orientation.
mutant=(O/'ImpactSolver.cpp').read_text().replace('        const Budget budget = Measure(*b);','        (void)ContactRotation(b->orientation);\n        const Budget budget = Measure(*b);');(O/'EagerRotationMutant.cpp').write_text(mutant);run([*common,'-c',O/'EagerRotationMutant.cpp','-o',O/'EagerRotationMutant.cpp.o'],'compile-eager-mutant')
# Explicit object precedes archive; original ImpactSolver object is not pulled.
cmd=[str(O/'MemoControls.cpp.o') if x.endswith('tests/ImpactSolverTests.cpp.o') else str(BASE) if x=='libjudas_engine.a' else x for x in link];cmd.insert(cmd.index(str(BASE)),str(O/'EagerRotationMutant.cpp.o'));cmd[cmd.index('-o')+1]=str(O/'MemoControls-eager-mutant');run(cmd,'link-eager-mutant',B);receipt.setdefault('binaries',{})['MemoControls-eager-mutant']=sha(O/'MemoControls-eager-mutant')
receipt['source_hashes']={p.name:sha(p) for p in O.glob('*.cpp')};persist();print(O/'BUILD.json')
