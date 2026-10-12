#!/usr/bin/env python3
"""Fresh-process save/stream composition and real modded Application/package ownership."""
from pathlib import Path
import os,json,hashlib,shlex,subprocess,time,shutil
R=Path.cwd();B=R/'.cache/m78-followup/release';C=R/'.cache/m78-next-risk';E=R/'docs/evidence/m78-next-risk';archive=R/'.cache/m78-lifecycle/lif003-build/libjudas_engine.a'
summary=json.loads((E/'RESULTS.json').read_text());rows=summary['runs'];binaries=summary['binaries'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',JUDAS_ENGINE_ROOT=str(R),XDG_DATA_HOME=str(C/'data'),XDG_CONFIG_HOME=str(C/'config'))
for k in list(env):
 if k.startswith(('M78_DIAGNOSTIC_','JUDAS_DIAGNOSTIC_','JUDAS_BOUNDARY_')):del env[k]
def run(name,cmd,cwd=R,extra=None,timeout=180):
 log=E/'runs'/(name+'.log');assert not log.exists(),name
 started=time.monotonic();p=subprocess.run(cmd,cwd=cwd,env=dict(env,**(extra or {})),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=timeout);log.write_text(p.stdout);row={'name':name,'command':list(map(str,cmd)),'cwd':str(cwd),'environment':{k:v for k,v in dict(env,**(extra or {})).items()if k in ['SDL_VIDEODRIVER','SDL_AUDIODRIVER','LIBGL_ALWAYS_SOFTWARE','JUDAS_ENGINE_ROOT','XDG_DATA_HOME','XDG_CONFIG_HOME']},'exitCode':p.returncode,'seconds':time.monotonic()-started,'logSHA256':sha(log),'summary':[s for s in p.stdout.splitlines()if 'SUMMARY'in s or ('checks'in s and ('failures'in s or 'integration:'in s))]};rows.append(row);summary.update(binaries=binaries,runs=rows);(E/'RESULTS.json').write_text(json.dumps(summary,indent=2)+'\n');print(name,p.returncode,row['summary'][-2:],flush=True);return p.returncode
pkg=C/'mod-package';meta=C/'mod-user';menv={'XDG_DATA_HOME':str(meta/'data'),'XDG_CONFIG_HOME':str(meta/'config')};project=pkg/'Scenes/landscape.judas'
for mode in ['probe','write','read']:
 if run('mod-landscape-'+mode,[str(C/'judas_mod_application_tests'),str(project),mode,str(C/'mod-landscape-case')],extra=menv):raise SystemExit('Preserved mod Application failure')
assert run('mod-disable-replacement',[str(C/'judas_mod'),'disable',str(pkg/'game.judasproj'),'landscape-fork'],extra=menv)==0
if run('mod-landscape-mismatch',[str(C/'judas_mod_application_tests'),str(project),'mismatch',str(C/'mod-landscape-case')],extra=menv):raise SystemExit('Preserved changed-loadout rejection failure')
