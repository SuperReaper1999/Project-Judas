from pathlib import Path
import datetime,hashlib,json,os,subprocess,time
root=Path.cwd();out=root/'docs/evidence/prealpha-pass5/ci/other-local';out.mkdir(parents=True,exist_ok=False);rows=[]
commands=[['python3','scripts/prealpha_build_identity_checks.py','--output',str(out/'identity.json')],['python3','scripts/prealpha_distribution_checks.py','--output',str(out/'distribution.json')],['python3','-m','unittest','discover','-s','tests','-p','test_prealpha_validation.py','-v'],['node','tests/TypeScriptToolingTests.mjs'],['node','scripts/check_judasjs_api.mjs']]
for i,command in enumerate(commands):
 log=out/(str(i)+'.log');start=time.monotonic()
 with log.open('w')as f:p=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,timeout=180)
 row={'argv':command,'cwd':str(root),'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'exit':p.returncode,'seconds':time.monotonic()-start,'log':log.name,'sha256':hashlib.sha256(log.read_bytes()).hexdigest()};rows.append(row);(out/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n');print(command[1],p.returncode,flush=True)
 if p.returncode:raise SystemExit(p.returncode)
