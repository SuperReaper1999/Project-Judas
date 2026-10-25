from pathlib import Path
import subprocess,time,hashlib,json,resource,os
root=Path('.cache/windows-seven-followup')
cases=[('mk2-rotation-1m','mk2_rotation',[]),('mk2-physics','mk2_physics',[]),('joint','joint',[]),('motion-bounds','rigid_motion_bounds',[]),('impact-mesh','impact_mesh_cost',[]),('impact-robustness','impact_robustness',[]),('liquid-rotation','liquid_rotation',[]),('liquid-surface','liquid_surface',[]),('liquid-reservoir','liquid',[]),('preferences','editor_preferences',[]),('transport','network_transport',[]),('cold','mk2_rotation_cold',[str(root/'linux-cold-final')]),('refill','liquid_surface_refill',[])]
def small_stack():resource.setrlimit(resource.RLIMIT_STACK,(1024*1024,1024*1024))
results=[]
for name,target,args in cases:
 exe=Path('build')/f'judas_{target}_tests';started=time.monotonic();out=root/f'linux-after-{name}.log'
 with out.open('w') as log:
  try:r=subprocess.run([str(exe),*args],stdout=log,stderr=subprocess.STDOUT,env={**os.environ,'SDL_AUDIODRIVER':'dummy'},timeout=180,preexec_fn=small_stack if name.endswith('1m') else None);code=r.returncode
  except subprocess.TimeoutExpired:code='timeout'
 result={'name':name,'command':[str(exe),*args],'returncode':code,'seconds':time.monotonic()-started,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()};results.append(result)
 (root/'LINUX_RESULTS.json').write_text(json.dumps(results,indent=2)+'\n');print(name,code,round(result['seconds'],3),flush=True)
if results[0]['returncode']==0:
 a=(root/'before-linux-rotation.log').read_bytes();b=(root/'linux-after-mk2-rotation-1m.log').read_bytes()
 (root/'trajectory-comparison.json').write_text(json.dumps({'original_stack_success_sha256':hashlib.sha256(a).hexdigest(),'corrected_1m_sha256':hashlib.sha256(b).hexdigest(),'byte_identical':a==b},indent=2)+'\n');print('rotation output byte-identical:',a==b,flush=True)
raise SystemExit(any(r['returncode']!=0 for r in results))
