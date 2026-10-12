#!/usr/bin/env python3
"""Immutable M78 case receipt; product code and accepted tests stay unchanged."""
import argparse,datetime,hashlib,json,os,pathlib,subprocess,time
ROOT=pathlib.Path(__file__).resolve().parents[4]
parser=argparse.ArgumentParser()
parser.add_argument('case');parser.add_argument('--cwd',default=str(ROOT/'.cache/m78/frozen'))
parser.add_argument('--timeout',type=int,default=300);parser.add_argument('command',nargs=argparse.REMAINDER)
parser.add_argument('--env',action='append',default=[])
a=parser.parse_args();command=a.command
if command and command[0]=='--':command=command[1:]
command[0]=str(pathlib.Path(command[0]).resolve())
folder=ROOT/'docs/evidence/m78/cases';folder.mkdir(exist_ok=True)
receipt=folder/(a.case+'.json');log=folder/(a.case+'.log')
if receipt.exists() or log.exists():raise SystemExit('Existing evidence retained; use a new case identity')
env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')}
user=ROOT/'.cache/m78/case-data';user.mkdir(exist_ok=True)
env.update(SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(user/'data'),XDG_CONFIG_HOME=str(user/'config'))
for setting in a.env:
 key,value=setting.split('=',1);env[key]=value
row={'case':a.case,'checkpoint':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'command':command,'cwd':a.cwd,'environment':{k:env[k] for k in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','LIBGL_ALWAYS_SOFTWARE','XDG_DATA_HOME','XDG_CONFIG_HOME')},'utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
row['explicitEnvironment']=a.env
binary=pathlib.Path(command[0]);row['executableSHA256']=hashlib.sha256(binary.read_bytes()).hexdigest()
start=time.monotonic()
with log.open('wb') as f:
 try:
  p=subprocess.run(command,cwd=a.cwd,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=a.timeout)
  row.update(exitCode=p.returncode,status='exited-zero' if p.returncode==0 else 'failed')
 except subprocess.TimeoutExpired:row.update(exitCode=None,status='timeout')
 except OSError as e:row.update(exitCode=None,status='observer-launch-error',error=str(e))
row.update(seconds=time.monotonic()-start,log=str(log.relative_to(ROOT)),logSHA256=hashlib.sha256(log.read_bytes()).hexdigest())
receipt.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps(row))
raise SystemExit(0 if row['status']=='exited-zero' else 1)
