#!/usr/bin/env python3
"""Run one bounded correction check with immutable command/source/binary receipts."""
import argparse,datetime,hashlib,json,os,pathlib,subprocess,time
r=pathlib.Path(__file__).resolve().parents[4];ev=r/'docs/evidence/m78-followup'
p=argparse.ArgumentParser();p.add_argument('name');p.add_argument('--cwd',default=str(r));p.add_argument('--timeout',type=float,default=600);p.add_argument('command',nargs=argparse.REMAINDER);a=p.parse_args();cmd=a.command
if cmd and cmd[0]=='--':cmd=cmd[1:]
if not cmd:raise SystemExit('missing command')
log=ev/(a.name+'.log');receipt=ev/(a.name+'.json')
if log.exists() or receipt.exists():raise SystemExit('Preserve receipt; choose a new case name')
env=os.environ.copy();env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(r/'.cache/m78-followup/data'),XDG_CONFIG_HOME=str(r/'.cache/m78-followup/config'))
start=time.perf_counter();utc=datetime.datetime.now(datetime.timezone.utc).isoformat()
with log.open('wb') as out:
 try:code=subprocess.run(cmd,cwd=a.cwd,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=a.timeout).returncode
 except subprocess.TimeoutExpired:code='timeout'
h=lambda f:hashlib.sha256(pathlib.Path(f).read_bytes()).hexdigest()
exe=pathlib.Path(cmd[0]);record={'command':cmd,'cwd':a.cwd,'utc':utc,'seconds':time.perf_counter()-start,'exitCode':code,'environment':{k:env[k] for k in ['SDL_VIDEODRIVER','SDL_AUDIODRIVER','LIBGL_ALWAYS_SOFTWARE','XDG_DATA_HOME','XDG_CONFIG_HOME']},'executableSHA256':h(exe) if exe.is_file() else None,'logSHA256':h(log),'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=r,text=True).strip()}
receipt.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps({'case':a.name,'exitCode':code,'seconds':record['seconds'],'lastLines':log.read_text(errors='replace').splitlines()[-3:]}));raise SystemExit(code if isinstance(code,int) and code>=0 else 1)
