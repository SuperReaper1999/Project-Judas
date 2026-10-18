#!/usr/bin/env python3
"""Same unchanged test objects relinked against old/refactored native archives."""
from pathlib import Path
import datetime,hashlib,json,shlex,subprocess,time
r=Path.cwd();o=r/'.cache/prealpha-pass4/refactor/behavior';baseline=r/'.cache/prealpha-pass4/baseline/bin/libjudas_engine.a';candidate=o/'libjudas_engine.refactor.a';records=[]
targets=['judas_impact_timing_tests','judas_contact_cache_tests','judas_kinematic_tests','judas_physics_continuation_tests','judas_contact_lifecycle_tests','judas_character_motor_tests','judas_joint_tests','judas_particle_contact_batch_tests','judas_rigid_motion_tests','judas_impact_robustness_tests']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd,log,limit=90):
 start=time.monotonic()
 with log.open('wb')as stream:
  try:p=subprocess.run(list(map(str,cmd)),cwd=r,stdout=stream,stderr=subprocess.STDOUT,timeout=limit);rc=p.returncode
  except subprocess.TimeoutExpired:rc='timeout'
 rec={'argv':list(map(str,cmd)),'exit':rc,'seconds':time.monotonic()-start,'log':str(log.relative_to(r))};records.append(rec);return rec
result={'schema':'judas.prealpha.pass4.refactor.behavior.v1','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'base':'7da56c2cc6ce7acda827a6e4019d610622b237e2','archives':{'baseline':sha(baseline),'refactor_only':sha(candidate)},'note':'Unchanged actual test sources compiled once. Exactly the same test-object bytes and link dependencies are used for old/new archive. No output normalization.','cases':[],'commands':records}
for target in targets:
 d=o/target;d.mkdir(exist_ok=True);flaglines=(r/'build/CMakeFiles'/f'{target}.dir/flags.make').read_text().splitlines();flags=[]
 for n in ['CXX_DEFINES','CXX_INCLUDES','CXX_FLAGS']:flags+=shlex.split(next(x.split('=',1)[1]for x in flaglines if x.startswith(n+' =')))
 args=shlex.split((r/'build/CMakeFiles'/f'{target}.dir/link.txt').read_text());original_objs=[x for x in args if x.endswith('.o')];objs=[];sources={}
 for original in original_objs:
  relative=original.split('.dir/',1)[1][:-2];source=r/relative;oldsource=subprocess.check_output(['git','show','7da56c2:'+relative],cwd=r)
  if source.read_bytes()!=oldsource:raise RuntimeError('test source changed '+relative)
  obj=d/(source.name+'.o');rec=run(['/usr/bin/c++',*flags,'-c',source,'-o',obj],d/(source.name+'.compile.log'))
  if rec['exit']!=0:raise RuntimeError('compile failed '+target)
  objs.append(obj);sources[relative]={'sha256':sha(source),'objectSha256':sha(obj)}
 libs=args[args.index('libjudas_engine.a')+1:];libs=[str((r/'build'/x).resolve())if x.endswith('.a')and not x.startswith('/')else x for x in libs];case={'target':target,'sources':sources,'runs':{}}
 for label,archive in [('baseline',baseline),('refactor',candidate)]:
  binary=d/label;rec=run(['/usr/bin/c++','-O3','-DNDEBUG',*objs,'-o',binary,archive,*libs],d/(label+'.link.log'))
  if rec['exit']!=0:raise RuntimeError('link failed '+target+' '+label)
  log=d/(label+'.log');execution=run([binary],log,90);case['runs'][label]={'binarySha256':sha(binary),'stdoutSha256':sha(log),**execution}
 case['stdout_bytes_identical']=(d/'baseline.log').read_bytes()==(d/'refactor.log').read_bytes();case['both_pass']=all(x['exit']==0 for x in case['runs'].values());result['cases'].append(case)
 (o/'CHECKS.json').write_text(json.dumps(result,indent=2)+'\n');print(target,'pass',case['both_pass'],'stdoutExact',case['stdout_bytes_identical'],flush=True)
result['all_pass']=all(x['both_pass']for x in result['cases']);result['all_stdout_bytes_identical']=all(x['stdout_bytes_identical']for x in result['cases']);result['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();(o/'CHECKS.json').write_text(json.dumps(result,indent=2)+'\n');print('DONE',result['all_pass'],result['all_stdout_bytes_identical'],flush=True)
