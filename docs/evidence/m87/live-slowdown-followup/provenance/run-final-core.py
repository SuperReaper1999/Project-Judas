import subprocess,pathlib,json,time,datetime,hashlib,os
root=pathlib.Path('/home/conner/Documents/GitHub/Project-Judas')
out=pathlib.Path('/tmp/m87-followup-final-core');out.mkdir(exist_ok=False)
names='rigid_motion contact_geometry rigid_contact impact_solver impact_timing impact_robustness impact_mesh_cost sphere_impact_bounds contact_cache contact_lifecycle contact_storage kinematic ragdoll post_consumer_ragdoll character_motor joint joint_scaling physics_continuation physical_animation physical_balance_foundation physical_balance_controller rigid_motion_geometry rigid_motion_bounds primitive_contact_boolean contact_sat_workspace parallel_narrowphase'.split()
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'knownBaselineFailures':['judas_physical_balance_foundation_tests'],'tests':[]}
for name in names:
 target='judas_'+name+'_tests';binary=root/'build'/target;command=[str(binary)]
 log=out/(target+'.log');start=time.monotonic();entry={'target':target,'command':command,'binarySHA256':hashlib.sha256(binary.read_bytes()).hexdigest()}
 try:
  with log.open('w') as f:run=subprocess.run(command,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=180)
  entry['exitCode']=run.returncode
 except subprocess.TimeoutExpired:entry['exitCode']='TIMEOUT'
 entry['seconds']=time.monotonic()-start;entry['log']=log.name;result['tests'].append(entry)
 (out/'RECEIPT.json').write_text(json.dumps(result,indent=2)+'\n')
 print(target,entry['exitCode'],round(entry['seconds'],3),flush=True)
result['unexpectedFailures']=[x['target'] for x in result['tests'] if x['exitCode']!=0 and x['target'] not in result['knownBaselineFailures']]
(out/'RECEIPT.json').write_text(json.dumps(result,indent=2)+'\n')
print('UNEXPECTED',result['unexpectedFailures'])
