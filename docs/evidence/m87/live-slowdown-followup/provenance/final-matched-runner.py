import pathlib,subprocess,os,time,json,hashlib,datetime,csv,statistics,math
root=pathlib.Path('/home/conner/Documents/GitHub/Project-Judas');out=pathlib.Path('/tmp/m87-followup-final-perf');out.mkdir(exist_ok=False)
old=root/'.cache/m87-live-followup/baseline/live-digest';new=root/'build/judas_physics_stress_live_benchmark';scene='/tmp/judas-m87-watch-ecpbqnfz/Scenes/room1024.judas'
env=os.environ.copy();env['JUDAS_ENGINE_ROOT']=str(root);env['LC_ALL']='C'
for k in list(env):
 if k.startswith('JUDAS_SUPPORT_CONTINUATION'):del env[k]
record={'startUTC':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'platform':'Native Linux','profile':False,'cadence':1/60,'visibleFPSMeasurement':False,'hostPolicy':'Other user applications untouched; only Judas benchmark/build/test workloads serialized. Not an idle-host certification.','cases':[]}
for name,binary in [('before-1',old),('final-1',new),('final-2',new),('before-2',old)]:
 case=out/name;cmd=['/usr/bin/time','-v',str(binary),scene,str(case),'100','155','0','0','1','0.016666667','0','0'];entry={'name':name,'binary':str(binary),'binarySHA256':hashlib.sha256(binary.read_bytes()).hexdigest(),'command':cmd,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat()};start=time.monotonic()
 try:
  with (out/(name+'.log')).open('w') as f:run=subprocess.run(cmd,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=90)
  entry['exitCode']=run.returncode
 except subprocess.TimeoutExpired:entry['exitCode']='TIMEOUT'
 entry['seconds']=time.monotonic()-start;r=list(csv.DictReader((case/'steps.csv').open()));v=sorted(float(x['fixed_ms']) for x in r);entry.update({'steps':len(r),'medianMs':statistics.median(v),'p95NearestRankMs':v[math.ceil(.95*len(v))-1],'maxMs':max(v),'sumMs':sum(v),'impactEvents':sum(int(x['impact_events']) for x in r),'nonfiniteFields':sum(int(x['nonfinite_fields']) for x in r),'finalDigest':r[-1]['trajectory_digest']});record['cases'].append(entry);(out/'RECEIPT.json').write_text(json.dumps(record,indent=2)+'\n');print(name,entry,flush=True)
ref=list(csv.DictReader((out/'before-1/steps.csv').open()));record['motionComparisons']={}
for name in ['final-1','final-2','before-2']:
 r=list(csv.DictReader((out/name/'steps.csv').open()));record['motionComparisons'][name]={'steps':len(r),'digestMismatches':sum(a['trajectory_digest']!=b['trajectory_digest'] for a,b in zip(ref,r)),'sameLength':len(ref)==len(r)}
record['endUTC']=datetime.datetime.now(datetime.timezone.utc).isoformat();(out/'RECEIPT.json').write_text(json.dumps(record,indent=2)+'\n');print('DONE')
