from pathlib import Path
import subprocess,time,json,hashlib,os
b=Path('/build');out=b/'final-binary-checks';out.mkdir(exist_ok=False);rows=[]
commands=[('rotation',[b/'judas_mk2_rotation_tests']),('wheel',[b/'judas_wheel_tests']),('legacy-spacecraft',[b/'judas_spacecraft_frame_integration_tests','--output',out/'spacecraft']),('cold-read',[b/'judas_mk2_rotation_cold_tests','read',b/'qualification-followup/rotation-cold']),('network',[b/'judas_network_session_tests',out/'network'])]
for name,cmd in commands:
 cmd=list(map(str,cmd));start=time.monotonic()
 with (out/(name+'.log')).open('w') as f:
  try:code=subprocess.run(cmd,cwd='/source',stdout=f,stderr=subprocess.STDOUT,timeout=120,env=dict(os.environ,SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',JUDAS_WORLD_STATE='none')).returncode
  except subprocess.TimeoutExpired:code='timeout'
 rows.append(dict(name=name,command=cmd,exit=code,seconds=time.monotonic()-start,binary_sha256=hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest()))
 (out/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n');print(name,code,flush=True)
