import sys,os,json,time,subprocess,datetime
from pathlib import Path
root=Path(__file__).resolve().parent
name,cwd,*command=sys.argv[1:]
env={k:v for k,v in os.environ.items() if k in {'HOME','USER','LOGNAME','LANG','LC_ALL','LC_CTYPE','TMPDIR','DISPLAY','WAYLAND_DISPLAY','XDG_RUNTIME_DIR','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS'}}
env.update(PATH='/usr/bin:/bin:/usr/sbin:/sbin',XDG_CACHE_HOME=str(root/'os-cache'),PYTHONNOUSERSITE='1')
started=time.monotonic();result={'name':name,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'argv':command,'cwd':cwd,'environmentPolicy':'system PATH; inherited user identity/display only; no compiler/engine/package-cache overrides; no Judas cache reuse'}
with (root/'receipts'/f'{name}.log').open('w') as log:
 try:r=subprocess.run(command,cwd=cwd,env=env,stdout=log,stderr=subprocess.STDOUT);result['exit']=r.returncode
 except Exception as e:result.update(exit=-1,error=str(e))
result['seconds']=time.monotonic()-started
(root/'receipts'/f'{name}.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));raise SystemExit(result['exit'])
