#!/usr/bin/env python3
"""Focused final-production M70/M77 cold continuation. Preparation never compiles/runs."""
from pathlib import Path
import sys,os,hashlib,json,subprocess,shlex,time,datetime
HERE=Path(__file__).resolve().parent;ROOT=Path('/home/conner/Documents/GitHub/Project-Judas');BUILD=Path('/home/conner/Documents/GitHub/Project-Judas/build')
OBSERVER=ROOT/'docs/evidence/m78-cold-boundary/final-current-standing/FinalCurrentStandingObserver.cpp'
OBS_SHA='715dd4ae56bc5a8b9556379d95bb5b6e8f4402fd4a60515b69ba039f0b918e13'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(path,data):path.write_text(json.dumps(data,indent=2)+'\n')
def source_inventory():
 paths=[OBSERVER,ROOT/'tests/CharacterLabTests.cpp',ROOT/'tests/BalanceLabTests.cpp',ROOT/'src/physics/WorldCCDSearch.cpp']
 names=subprocess.check_output(['git','ls-files','--','projects/character_lab','projects/balance_lab'],cwd=ROOT,text=True).splitlines()
 paths.extend(ROOT/name for name in names)
 return {str(p.relative_to(ROOT)):sha(p) for p in paths if p.is_file()}
def plan():
 if sha(OBSERVER)!=OBS_SHA:raise RuntimeError('protected observer differs')
 data={'schema':'judas.final-support.native-cold.plan.v1','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'observer_sha256':OBS_SHA,'source_inventory':source_inventory(),'production_header_policy':'Use only root Float headers, normal root build library and original BalanceLab target flags/link recipe. No precise prototype headers or libraries.','cases':[['M70 cold writer','build/judas_character_lab_tests','{output}/character','cold-write'],['M70 cold reader','build/judas_character_lab_tests','{output}/character','cold-read'],['M77 physical observer writer','current-standing-observer','{output}/balance','cold-write'],['M77 physical observer reader','current-standing-observer','{output}/balance','cold-read']],'environment_overrides':{'SDL_AUDIODRIVER':'dummy','JUDAS_DIAGNOSTIC_CONTINUE_WRITE':'1'},'native_graphics':'Current desktop DISPLAY/Wayland; no dummy video, no virtual display, no software GL override','windows':'Not rerun for this focused support fix; informational/nonblocking, no Windows pass claimed'}
 write(HERE/'PLAN.json',data);return data

def command(argv,log,cwd=ROOT,env=None,timeout=180):
 begin=time.monotonic();status={'argv':list(map(str,argv)),'cwd':str(cwd),'log':str(log)}
 with log.open('w') as stream:
  try:r=subprocess.run(argv,cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=timeout);status.update(exit=r.returncode,timed_out=False)
  except subprocess.TimeoutExpired:status.update(exit=None,timed_out=True)
 status['seconds']=time.monotonic()-begin;status['log_sha256']=sha(log);return status

def run():
 prepared=json.loads((HERE/'PLAN.json').read_text())
 if source_inventory()!=prepared['source_inventory']:raise RuntimeError('protected/final source inventory changed since preparation; inspect discrepancy')
 if sha(OBSERVER)!=OBS_SHA:raise RuntimeError('protected observer changed')
 archive=BUILD/'libjudas_engine.a';character=Path('/home/conner/Documents/GitHub/Project-Judas/.cache/support-followup-20261021/app-drivers/judas_character_lab_tests')
 if not archive.is_file() or not character.is_file():raise RuntimeError('normal final production build missing')
 # Explicitly verify the private source representation, rather than trusting a build path.
 contacts=(ROOT/'src/Contacts.h').read_text();motion=(ROOT/'src/RigidMotion.h').read_text()
 if 'glm::vec3 position' not in contacts.split('struct ContactPose {',1)[1].split('};',1)[0]:raise RuntimeError('not original Float ContactPose')
 if 'glm::vec3 position' not in motion.split('struct RigidMotionSegment {',1)[1].split('RigidBody Evaluate',1)[0]:raise RuntimeError('not original Float motion segment')
 flags={}
 flagfile=BUILD/'CMakeFiles/judas_balance_lab_tests.dir/flags.make'
 for line in flagfile.read_text().splitlines():
  if ' = ' in line:
   k,v=line.split(' = ',1);flags[k]=shlex.split(v)
 binary=HERE/'current-standing-observer';obj=HERE/'FinalCurrentStandingObserver.o'
 compile_cmd=['/usr/bin/c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],'-I'+str(ROOT/'third_party'),*flags['CXX_FLAGS'],'-MMD','-MF',str(obj)+'.d','-c',str(OBSERVER),'-o',str(obj)]
 if any('coherent-geometry-prototype' in token for token in compile_cmd):raise RuntimeError('prototype overlay leaked into compile')
 receipt={'schema':'judas.final-support.native-cold.results.v1','plan_sha256':sha(HERE/'PLAN.json'),'archive':str(archive),'archive_sha256':sha(archive),'character_binary':str(character),'character_binary_sha256':sha(character),'observer_sha256':OBS_SHA,'target_flags_sha256':sha(flagfile),'source_inventory_before':source_inventory(),'steps':[],'pass_all':False,'windows':'not rerun; informational/nonblocking'}
 def save():write(HERE/'RESULTS.json',receipt)
 status=command(compile_cmd,HERE/'observer-compile.log',BUILD);receipt['steps'].append({'name':'observer-compile',**status});save()
 if status['exit']!=0:return 1
 dep=Path(str(obj)+'.d').read_text().replace('\\\n',' ');inputs=shlex.split(dep.split(':',1)[1]);receipt['observer_inputs']={str(Path(p)):sha(Path(p)) for p in inputs if Path(p).is_file()}
 if any('coherent-geometry-prototype' in p for p in receipt['observer_inputs']):raise RuntimeError('prototype header dependency leaked')
 tokens=shlex.split((BUILD/'CMakeFiles/judas_balance_lab_tests.dir/link.txt').read_text());tokens=[x for x in tokens if not x.startswith('-Wl,--dependency-file=')]
 replaced=0
 for i,value in enumerate(tokens):
  if value.endswith('.cpp.o'):
   if value!='CMakeFiles/judas_balance_lab_tests.dir/tests/BalanceLabTests.cpp.o':raise RuntimeError('unexpected BalanceLab target object: '+value)
   tokens[i]=str(obj);replaced+=1
  elif value=='libjudas_engine.a':tokens[i]=str(archive)
 if replaced!=1:raise RuntimeError('must replace exactly original BalanceLab test object')
 tokens[tokens.index('-o')+1]=str(binary)
 status=command(tokens,HERE/'observer-link.log',BUILD);receipt['steps'].append({'name':'observer-link',**status});save()
 if status['exit']!=0:return 1
 receipt['observer_binary_sha256']=sha(binary)
 output=HERE/'native'
 if output.exists():raise RuntimeError('fresh native output required')
 output.mkdir();env=os.environ.copy()
 for key in list(env):
  if key.startswith(('JUDAS_DIAGNOSTIC_','JUDAS_BOUNDARY_','M78_DIAGNOSTIC_')):del env[key]
 env.update(SDL_AUDIODRIVER='dummy',JUDAS_DIAGNOSTIC_CONTINUE_WRITE='1')
 if env.get('SDL_VIDEODRIVER')=='dummy':raise RuntimeError('native GL requested, dummy video supplied')
 receipt['graphics_environment']={k:env.get(k) for k in ['DISPLAY','WAYLAND_DISPLAY','XDG_RUNTIME_DIR','SDL_VIDEODRIVER','SDL_AUDIODRIVER','LIBGL_ALWAYS_SOFTWARE']}
 saves={}
 for kind,executable,slot in [('character',character,'m70'),('balance',binary,'m77')]:
  for mode in ['cold-write','cold-read']:
   status=command([str(executable),str(output/kind),mode],HERE/(kind+'-'+mode+'.log'),ROOT,env)
   status.update(name=kind+'-'+mode,pass_case=status['exit']==0 and not status['timed_out'])
   if status['pass_case']:
    savefile=output/kind/'slots'/(slot+'.save');status['save_sha256']=sha(savefile)
    if mode=='cold-write':
     saves[kind]=status['save_sha256']
     if kind=='balance':(HERE/'write-physical-final.json').write_bytes((output/kind/'physical-final.json').read_bytes())
    else:
     status['save_bytes_unchanged']=status['save_sha256']==saves[kind];status['pass_case']&=status['save_bytes_unchanged']
     if kind=='balance':
      (HERE/'read-physical-final.json').write_bytes((output/kind/'physical-final.json').read_bytes())
      before=json.loads((HERE/'write-physical-final.json').read_text())['currentPhysicalEndpoint'];after=json.loads((HERE/'read-physical-final.json').read_text())['currentPhysicalEndpoint']
      compare={'exact_equal':before==after,'write_standing':before['currentPhysicalStanding'],'read_standing':after['currentPhysicalStanding'],'write':before,'read':after}
      write(HERE/'PHYSICAL_ENDPOINT_COMPARISON.json',compare);status['actual_physical_endpoint_equal_and_standing']=compare['exact_equal'] and compare['write_standing'] and compare['read_standing'];status['pass_case']&=status['actual_physical_endpoint_equal_and_standing']
   receipt['steps'].append(status);save();print(status['name'],'exit',status['exit'],'pass',status['pass_case'],flush=True)
   if not status['pass_case']:return 1
 receipt['source_inventory_after']=source_inventory();receipt['source_and_fixtures_unchanged']=receipt['source_inventory_after']==receipt['source_inventory_before'];receipt['archive_unchanged']=sha(archive)==receipt['archive_sha256'];receipt['character_binary_unchanged']=sha(character)==receipt['character_binary_sha256'];receipt['observer_unchanged']=sha(OBSERVER)==OBS_SHA
 receipt['pass_all']=all(step.get('pass_case',step['exit']==0) for step in receipt['steps']) and all(receipt[k] for k in ['source_and_fixtures_unchanged','archive_unchanged','character_binary_unchanged','observer_unchanged']);save();return 0 if receipt['pass_all']else 1

if __name__=='__main__':
 if len(sys.argv)!=2 or sys.argv[1]not in ['prepare','run']:raise SystemExit('prepare | run')
 if sys.argv[1]=='prepare':plan();print('Prepared source/command plan only; not compiled or run')
 else:raise SystemExit(run())
