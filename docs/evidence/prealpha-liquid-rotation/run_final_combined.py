import datetime, hashlib, json, os, pathlib, platform, subprocess, time
root=pathlib.Path.cwd(); out=root/'.cache/prealpha-liquid-rotation/final-combined';out.mkdir(exist_ok=False)
def sha(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
paths=['CMakeLists.txt','src/LiquidGeometry.cpp','src/LiquidGeometry.h','src/LiquidSurface.cpp','src/LiquidTypes.cpp','src/WorldRegions.cpp','tests/LiquidRotationTests.cpp','tests/LiquidBakeFrameTests.cpp','tests/OriginEnvironmentTests.cpp']
source=[{'path':p,'sha256':sha(p)} for p in paths]
env=os.environ.copy();env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1')
cases=[
 ('rotation','judas_liquid_rotation_tests',[],0),
 ('frame','judas_liquid_bake_frame_tests',[str(out/'frame')],0),
 ('bake-normalization','judas_origin_environment_tests',['--bake-only','normalization',str(out/'bake-normalization')],0),
 ('bake-knots','judas_origin_environment_tests',['--bake-only','knots',str(out/'bake-knots')],0),
 ('normalization-full-preserved-failure','judas_origin_environment_tests',['--bake-regression','normalization',str(out/'normalization-full')],1),
 ('knots-full-preserved-failure','judas_origin_environment_tests',['--bake-regression','knots',str(out/'knots-full')],1),
 ('origin','judas_origin_environment_tests',[str(out/'origin')],0),
 ('M54','judas_liquid_tests',[],0),('M55','judas_liquid_surface_tests',[],0),
 ('refill','judas_liquid_surface_refill_tests',[],0),
 ('M54-application','judas_liquid_application_tests',[],0),
 ('M55-application','judas_liquid_surface_application_tests',[],0),
 ('gravity-selection','judas_gravity_selection_tests',[str(out/'gravity-selection')],0),
 ('gravity-uniform','judas_uniform_gravity_scene_tests',['--output',str(out/'uniform')],0),
 ('streaming','judas_streaming_tests',[],0),
 ('world-persistence','judas_world_persistence_tests',[],0),
 ('origin-assembly','judas_origin_assembly_tests',[],0),
 ('origin-stream','judas_origin_stream_tests',[],0),
 ('character','judas_character_motor_tests',[],0)]
report=dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=source,platform=platform.platform(),environment={k:env[k] for k in ['SDL_VIDEODRIVER','SDL_AUDIODRIVER','LIBGL_ALWAYS_SOFTWARE']},runs=[])
for name,target,args,expected in cases:
 binary=root/'build'/target;log=out/(name+'.log');command=[str(binary),*args]
 record=dict(name=name,argv=command,binarySha256=sha(binary),expectedExit=expected)
 begin=time.perf_counter()
 with log.open('wb') as f:
  try: r=subprocess.run(command,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=180);record['exit']=r.returncode
  except subprocess.TimeoutExpired: record['timeout']=True;record['exit']=None
 record.update(seconds=time.perf_counter()-begin,logSha256=sha(log))
 report['runs'].append(record);(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
 print(name,record['exit'],round(record['seconds'],3),flush=True)
 if record['exit']!=expected: print('UNEXPECTED RESULT '+name,flush=True)
report['codeUnchanged']=all(sha(r['path'])==r['sha256'] for r in source)
report['unexpected']=[r['name'] for r in report['runs'] if r['exit']!=r['expectedExit']]
(out/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(bool(report['unexpected']) or not report['codeUnchanged'])
