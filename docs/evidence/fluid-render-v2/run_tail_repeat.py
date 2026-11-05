from pathlib import Path
import os,sys,subprocess,json,time,datetime,hashlib,csv,platform
# Repository discovery adapted for this evidence copy; original executed scripts
# remain unchanged under .cache/fluid-v2-render-audit. Existing outputs refuse overwrite.
root=next(p for p in Path(__file__).resolve().parents if (p/'engine-version.json').is_file());out=root/'.cache/fluid-v2-render-audit'
binaries={'before':out/'baseline-fluid-render-probe','v2':root/'build/judas_fluid_render_probe'}
manifest={'time_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'frames':360,'warmup':60,'order':['before','v2','v2','before'],'platform':platform.platform(),'cpu':next((s.split(':',1)[1].strip() for s in Path('/proc/cpuinfo').read_text().splitlines() if s.startswith('model name')),'unknown'),'tools':{},'runs':[]}
for name,exe in binaries.items():manifest['tools'][name]={'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'identity':json.loads(subprocess.check_output([str(exe),'--build-info-json'],text=True))}
for case in ['planet']:
 for index,kind in enumerate(manifest['order'],start=4):
  destination=out/'runs'/case/f'{index}-{kind}';project=out/'fixtures'/case/'fluid_demo.judasproj';destination.parent.mkdir(parents=True,exist_ok=True)
  env=dict(os.environ,JUDAS_PROFILE='1',JUDAS_GL_PROFILE='modern' if kind=='v2' else 'portable',JUDAS_WORLD_STATE='none',JUDAS_WINDOW_SIZE='1280x720')
  env.pop('JUDAS_TEST_SCRIPT',None);env.pop('JUDAS_PROFILE_OUTPUT',None)
  snapshot=subprocess.check_output(['nvidia-smi','--query-gpu=name,driver_version,clocks.current.graphics,clocks.current.memory,pstate,temperature.gpu,utilization.gpu,memory.used','--format=csv,noheader'],text=True).strip()
  started=time.monotonic()
  with destination.with_suffix('.log').open('w') as log:r=subprocess.run([str(binaries[kind]),str(project),str(destination),'360'],stdout=log,stderr=subprocess.STDOUT,env=env,timeout=150)
  record={'case':case,'kind':kind,'index':index,'returncode':r.returncode,'seconds':time.monotonic()-started,'gpu_before':snapshot,'output':str(destination.relative_to(root))};manifest['runs'].append(record);(out/'TAIL_REPEAT_RUNS.json').write_text(json.dumps(manifest,indent=2)+'\n')
  if r.returncode:raise SystemExit(f'failed run {record}')
  frames=list(csv.DictReader((destination/'frames.csv').open()));steps=list(csv.DictReader((destination/'steps.csv').open()));assert len(frames)==360 and len(steps)==360
  e=json.loads((destination/'environment.json').read_text());assert e['gpu_dropped']==0
  if kind=='v2':assert e['modern_tier'] is True and e['visual_gpu_particle_draws_last_frame']==0
  print(f'{case} {kind} run {index}: {record["seconds"]:.2f}s, {e["initial_particles"]} particles, {e["surface_vertices"]} vertices',flush=True)
print('Completed all matched runs',flush=True)
