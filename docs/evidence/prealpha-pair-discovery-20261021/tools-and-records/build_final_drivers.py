from pathlib import Path
import json,subprocess,hashlib,shutil,shlex
root=Path(__file__).resolve().parents[2];here=Path(__file__).resolve().parent;folder=here/'diagnostic-build'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
record=json.loads((folder/'BUILD-v3.json').read_text());source=folder/'DiagnosticLive-v4.cpp';shutil.copy2(folder/'DiagnosticLive.cpp',source)
obj=folder/'DiagnosticLive-v4.cpp.o';compile=record['compile'][:];compile[compile.index('-c')+1]=str(source);compile[compile.index('-o')+1]=str(obj)
with (folder/'compile-v4.log').open('w') as log:subprocess.run(compile,cwd=root/'build',stdout=log,stderr=subprocess.STDOUT,check=True)
rows=[]
for name,core in [('baseline',here/'baseline/libjudas_engine.a'),('combined',root/'build/libjudas_engine.a')]:
 link=record['link'][:];link=[str(obj) if x==str(folder/'DiagnosticLive.cpp.o') else str(core) if x==str(here/'baseline/libjudas_engine.a') else x for x in link];binary=here/name/'live-v4';link[link.index('-o')+1]=str(binary)
 with (here/name/'link-v4.log').open('w') as log:subprocess.run(link,cwd=root/'build',stdout=log,stderr=subprocess.STDOUT,check=True)
 rows.append({'name':name,'core_sha256':sha(core),'compile':compile,'link':link,'source_sha256':sha(source),'object_sha256':sha(obj),'binary_sha256':sha(binary)})
(here/'FINAL_DRIVER_BUILDS.json').write_text(json.dumps(rows,indent=2)+'\n')
# Deliberately omit mixed-active pairs to establish oracle sensitivity.
out=here/'both-active-control';out.mkdir();source=out/'BroadphaseTests.cpp';text=(root/'tests/BroadphaseTests.cpp').read_text();marker='namespace {'
wrapper='''class MissingMixedPairsTree : public DynamicAabbTree {
public:
 template<class Callback>void QueryPairs(const std::vector<int>& active,PairQueryScratch& scratch,Callback&& callback)const{
  DynamicAabbTree::QueryPairs(active,scratch,[&](int a,int b){
   if(std::find(active.begin(),active.end(),a)!=active.end()&&std::find(active.begin(),active.end(),b)!=active.end())callback(a,b);
  });
 }
};
''';text=text.replace(marker,wrapper+'\n'+marker,1);text=text.replace('''void TestTreePairs() {
    std::printf("A2: unordered active-leaf pair traversal against independent overlap and query oracles\\n");
    DynamicAabbTree tree;''','''void TestTreePairs() {
    std::printf("A2: unordered active-leaf pair traversal against independent overlap and query oracles\\n");
    MissingMixedPairsTree tree;''');source.write_text(text)
variables={k:shlex.split(v) for k,v in (x.split(' = ',1) for x in (root/'build/CMakeFiles/judas_broadphase_tests.dir/flags.make').read_text().splitlines() if ' = ' in x)}
obj=out/'BroadphaseTests.cpp.o';argv=['c++',*variables['CXX_DEFINES'],*variables['CXX_INCLUDES'],*variables['CXX_FLAGS'],'-c',str(source),'-o',str(obj)]
with (out/'compile.log').open('w') as log:subprocess.run(argv,cwd=root/'build',stdout=log,stderr=subprocess.STDOUT,check=True)
plans=json.loads((here/'qualification/RESULTS.json').read_text())['plans'];link=next(p['argv'][:] for p in plans if p['target']=='judas_broadphase_tests');link=[str(obj) if x.endswith('/tests/BroadphaseTests.cpp.o') else x for x in link];link[link.index('-o')+1]=str(out/'test')
with (out/'link.log').open('w') as log:subprocess.run(link,cwd=root/'build',stdout=log,stderr=subprocess.STDOUT,check=True)
with (out/'run.log').open('w') as log:code=subprocess.run([out/'test'],cwd=root,stdout=log,stderr=subprocess.STDOUT).returncode
(out/'RESULTS.json').write_text(json.dumps({'compile':argv,'link':link,'exit':code,'expected_exit':1,'source_sha256':sha(source)},indent=2)+'\n');print('both-active negative control',code)
if code!=1:raise SystemExit('negative control failed to fail')
