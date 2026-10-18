"""Matched cache-only observations of unmodified assertions and callback inputs."""
from pathlib import Path
import subprocess,shlex,re,json,hashlib,time,os,sys
ROOT=next(parent for parent in Path(__file__).resolve().parents if (parent/'engine-version.json').is_file() and (parent/'src/ScriptSystem.cpp').is_file());OUT=ROOT/'.cache/prealpha-pass3/maintainability'
NATIVE=OUT/'native';NATIVE.mkdir(parents=True,exist_ok=True)
SPECS=[('script','judas_script_tests','tests/ScriptTests.cpp',False),('debug','judas_script_developer_tools_tests','tests/ScriptDeveloperToolsTests.cpp',False),('ui','judas_runtime_ui_tests','tests/RuntimeUITests.cpp',False),('joint','judas_joint_application_tests','tests/JointApplicationTests.cpp',True)]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def command(args,path):
 start=time.monotonic();result=subprocess.run(args,cwd=ROOT,capture_output=True,text=True,timeout=180)
 path.write_text(result.stdout+result.stderr)
 row={'args':args,'exit':result.returncode,'seconds':time.monotonic()-start,'log':str(path.relative_to(ROOT))}
 if result.returncode:raise RuntimeError(json.dumps(row))
 return row
def compile_baseline():
 records=[]
 base_link=shlex.split((ROOT/'build/CMakeFiles/judas_script_tests.dir/link.txt').read_text());tail=base_link[base_link.index('libjudas_engine.a'):]
 deps=[]
 for word in tail:
  if word=='libjudas_engine.a':word=os.environ.get('JUDAS_BASELINE_ARCHIVE',str(ROOT/'.cache/prealpha-pass3/baseline/bin/libjudas_engine.a'))
  elif not word.startswith('-') and not Path(word).is_absolute():word=str(ROOT/'build'/word)
  deps.append(word)
 flags=(ROOT/'build/CMakeFiles/judas_script_tests.dir/flags.make').read_text();options=[]
 for key in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:
  options+=shlex.split(re.search(r'^'+key+r' = (.*)$',flags,re.M)[1])
 baseline={'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'archive':{'path':deps[0],'sha256':sha(Path(deps[0]))},'sources':[],'dependencies':[{'path':x,'sha256':sha(Path(x))} for x in deps if x.endswith('.a')],'compile_commands':[],'tests':[]}
 apps=[]
 for name in ['Application','RuntimeOptions','RuntimeDiagnostics','TestHarness']:
  src=ROOT/('src/'+name+'.cpp');obj=NATIVE/(name+'.o')
  baseline['compile_commands'].append(command(['/usr/bin/c++',*options,'-c',str(src),'-o',str(obj)],NATIVE/(name+'-compile.log')))
  apps.append(str(obj));baseline['sources'].append({'path':str(src.relative_to(ROOT)),'sha256':sha(src),'unchanged_from_head':src.read_bytes()==subprocess.check_output(['git','show','HEAD:'+str(src.relative_to(ROOT))],cwd=ROOT)})
 for name,target,source,application in SPECS:
  src=ROOT/source;text=src.read_text();original_sha=sha(src)
  assert src.read_bytes()==subprocess.check_output(['git','show','HEAD:'+source],cwd=ROOT)
  if name=='script':text=text.replace('build/m40-script-tests','.cache/prealpha-pass3/maintainability/native-fixtures/script',1)
  if name=='debug':
   text=text.replace('.cache/m94/focused-fixture','.cache/prealpha-pass3/maintainability/native-fixtures/debug',1)
   text=text.replace('world.UpdateScripts(&input,.016f);','world.UpdateScripts(&input,.016f);std::printf("TRACE_DEBUG_INITIAL %s\\n",State(scripts).c_str());',1)
   text=text.replace('scripts.DrainSignals(&input,.02f,false);','scripts.DrainSignals(&input,.02f,false);std::printf("TRACE_DEBUG_RELOAD %s\\n",State(scripts).c_str());',1)
  if name=='ui':text=text.replace('world.DispatchUIEvents(&input,.016f);','world.DispatchUIEvents(&input,.016f);std::printf("TRACE_UI %s\\n",world.UI().CaptureState(h).c_str());')
  if name=='joint':
   needle='if(++frames==3){'
   addition='if(++frames==3){if(world.Scripts())for(const auto& observed:world.Scripts()->Capture())std::printf("TRACE_JOINT_SCRIPT %s\\n",observed.json.c_str());JointState observedJoint;if(world.Physics().GetJoint(world.RuntimeJoint(31),observedJoint))std::printf("TRACE_JOINT_PHYSICAL %.9g %.9g\\n",observedJoint.coordinate,observedJoint.motorImpulse);'
   assert needle in text;text=text.replace(needle,addition,1)
  observer=NATIVE/(name+'.cpp');observer.write_text(text);obj=NATIVE/(name+'.o')
  baseline['sources'].append({'path':source,'sha256':original_sha,'unchanged_from_head':True,'observer':str(observer.relative_to(ROOT)),'observer_sha256':sha(observer),'modification':'cache fixture paths and observation prints only; all original conditions/inputs/assertions preserved'})
  baseline['compile_commands'].append(command(['/usr/bin/c++',*options,'-c',str(observer),'-o',str(obj)],NATIVE/(name+'-compile.log')))
  for stage,archive in [('before',deps[0])]:
   binary=NATIVE/(name+'-'+stage)
   baseline['compile_commands'].append(command(['/usr/bin/c++','-O3','-DNDEBUG',str(obj),*(apps if application else []),'-o',str(binary),archive,*deps[1:]],NATIVE/(name+'-'+stage+'-link.log')))
   baseline['tests'].append({'name':name,'stage':stage,'binary':str(binary.relative_to(ROOT)),'sha256':sha(binary)})
 baseline['link_dependencies']=deps;baseline['application_objects']=apps
 (OUT/'NATIVE_COMPARISON_BUILD.json').write_text(json.dumps(baseline,indent=2)+'\n')
 print('Compiled matched baseline observers',len(SPECS),flush=True)
def relink_final():
 data=json.loads((OUT/'NATIVE_COMPARISON_BUILD.json').read_text());deps=data['link_dependencies'];deps[0]=str(ROOT/'build/libjudas_engine.a')
 # All dependencies are repointed to the final same-build versions, and their
 # hashes are reported. Physical/code observations are compared independently.
 rows=[]
 for name,target,source,application in SPECS:
  binary=NATIVE/(name+'-after');obj=NATIVE/(name+'.o')
  rows.append(command(['/usr/bin/c++','-O3','-DNDEBUG',str(obj),*(data['application_objects'] if application else []),'-o',str(binary),*deps],NATIVE/(name+'-after-link.log')))
  rows[-1]['binary_sha256']=sha(binary)
 (OUT/'NATIVE_FINAL_LINK.json').write_text(json.dumps({'archive_sha256':sha(Path(deps[0])),'commands':rows},indent=2)+'\n')
def run(stage):
 rows=[];env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LP_NUM_THREADS='2')
 for name,target,source,application in SPECS:
  binary=NATIVE/(name+'-'+stage);output=NATIVE/(stage+'-'+name+'-output');args=[str(binary)]
  if name in ['ui','joint']:args+=['--output',str(output)]
  start=time.monotonic();result=subprocess.run(args,cwd=ROOT,env=env,capture_output=True,text=True,timeout=180)
  log=NATIVE/(name+'-'+stage+'.log');log.write_text(result.stdout+result.stderr)
  observations=[line for line in result.stdout.splitlines() if line.startswith(('STATE ','NATIVE ','TRACE_'))]
  rows.append({'name':name,'args':args,'exit':result.returncode,'seconds':time.monotonic()-start,'binary_sha256':sha(binary),'log':str(log.relative_to(ROOT)),'observations':observations,'summaries':re.findall(r'^(?:SUMMARY|M94) .+$',result.stdout,re.M)})
  print(name,stage,result.returncode,rows[-1]['summaries'],flush=True)
 (OUT/('NATIVE_'+stage.upper()+'.json')).write_text(json.dumps(rows,indent=2)+'\n')
 if any(r['exit'] for r in rows):raise RuntimeError('native observer failure; see records')
if __name__=='__main__':
 mode=sys.argv[1]
 if mode=='compile-before':compile_baseline()
 elif mode=='link-after':relink_final()
 elif mode in ['before','after']:run(mode)
 else:raise ValueError(mode)
