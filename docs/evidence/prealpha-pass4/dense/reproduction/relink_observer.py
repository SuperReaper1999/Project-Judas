#!/usr/bin/env python3
"""Relink the already compiled unchanged test objects; never recompiles them."""
from pathlib import Path
import argparse,datetime,hashlib,json,re,shlex,subprocess,time,os
p=argparse.ArgumentParser();p.add_argument('--archive',type=Path,required=True);p.add_argument('--label',default='formatted');a=p.parse_args();r=Path.cwd();o=r/'.cache/prealpha-pass4/refactor/behavior';dst=r/'.cache/prealpha-pass4/dense'/a.label;dst.mkdir(exist_ok=True);old=json.loads((o/'CHECKS.json').read_text());archive=a.archive.resolve();records=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd,log,limit=90):
 start=time.monotonic()
 with log.open('wb')as stream:
  try:q=subprocess.run(list(map(str,cmd)),cwd=r,stdout=stream,stderr=subprocess.STDOUT,timeout=limit);rc=q.returncode
  except subprocess.TimeoutExpired:rc='timeout'
 out={'argv':list(map(str,cmd)),'seconds':time.monotonic()-start,'exit':rc,'log':str(log.relative_to(r))};records.append(out);return out
rec={'schema':'judas.prealpha.pass4.refactor.final_relink.v1','utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'archive':{'path':str(archive),'sha256':sha(archive)},'source_sha256':{str(p.relative_to(r)):sha(p)for p in [r/'src/PhysicsWorld.cpp',*sorted((r/'src/physics').glob('*'))]if p.is_file()},'cases':[],'commands':records,'note':'Exactly same previously compiled test-object bytes as accepted-baseline comparison; only candidate archive relink. Raw stdout differences retained, no normalization.'}
def perf_shape(line):
 if line is None:return None
 for pattern in [r'^(PERF (?:1|10|100) motors whole_motor_batch_us )[+\-0-9.eE]+( samples=120)$',r'^(PERF 100 joints mean whole-step )[+\-0-9.eE]+( ms)$']:
  match=re.fullmatch(pattern,line)
  if match:return match.groups()
 return None
for case in old['cases']:
 target=case['target'];folder=dst/target;folder.mkdir(exist_ok=True);objs=[]
 for relative,source in case['sources'].items():
  obj=o/target/(Path(relative).name+'.o')
  if sha(obj)!=source['objectSha256']:raise RuntimeError('unchanged test object hash mismatch '+target)
  objs.append(obj)
 args=shlex.split((r/'build/CMakeFiles'/f'{target}.dir/link.txt').read_text());libs=args[args.index('libjudas_engine.a')+1:];libs=[str((r/'build'/x).resolve())if x.endswith('.a')and not x.startswith('/')else x for x in libs];binary=folder/target
 link=run(['/usr/bin/c++','-O3','-DNDEBUG',*objs,'-o',binary,archive,*libs],folder/'link.log')
 if link['exit']!=0:raise RuntimeError('link failed '+target)
 log=folder/'run.log';os.environ['JUDAS_PASS4_DIRTY_REPORT']=str(folder/'observer.json');execution=run([binary],log);before=(o/target/'baseline.log').read_text().splitlines();after=log.read_text().splitlines();differences=[]
 for i in range(max(len(before),len(after))):
  left=before[i]if i<len(before)else None;right=after[i]if i<len(after)else None
  if left!=right:differences.append({'line':i+1,'baseline':left,'candidate':right,'explicit_PERF_line':bool(perf_shape(left) is not None and perf_shape(left)==perf_shape(right))})
 actual={'target':target,'baseline_exit':case['runs']['baseline']['exit'],'candidate_exit':execution['exit'],'binarySha256':sha(binary),'stdoutSha256':sha(log),'stdout_bytes_identical':(o/target/'baseline.log').read_bytes()==log.read_bytes(),'differences':differences,'only_explicit_perf_differences':all(d['explicit_PERF_line']for d in differences),'same_test_objects':case['sources']};rec['cases'].append(actual);(dst/'FINAL_RELINK.json').write_text(json.dumps(rec,indent=2)+'\n');print(target,execution['exit'],'rawDiffs',len(differences),'timingOnly',actual['only_explicit_perf_differences'],flush=True)
rec['all_pass']=all(c['baseline_exit']==c['candidate_exit']==0 for c in rec['cases']);rec['no_unexpected_stdout_differences']=all(c['only_explicit_perf_differences']for c in rec['cases']);rec['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();(dst/'FINAL_RELINK.json').write_text(json.dumps(rec,indent=2)+'\n')
