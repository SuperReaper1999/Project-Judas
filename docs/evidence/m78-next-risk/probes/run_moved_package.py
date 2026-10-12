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
source=C/'mod-source';shutil.copytree(R/'.cache/m78-followup/dependency-base-project',source,ignore=shutil.ignore_patterns('Saves'));project=source/'modding_lab.judasproj';fresh=C/'fresh-mod-export'
if run('mod-fresh-export',[str(C/'judas_export'),str(project),str(fresh),str(C/'judas')]):raise SystemExit('Preserved fresh export failure')
import tempfile
moved=Path(tempfile.mkdtemp(prefix='judas-m78-mod-moved-',dir='/tmp'))/'package';shutil.copytree(fresh,moved)
(E/'MOVED_PACKAGE.json').write_text(json.dumps({'path':str(moved),'source':str(fresh),'policy':'Owned disposable package; original packages untouched'},indent=2)+'\n')
meta=C/'moved-user';menv={'XDG_DATA_HOME':str(meta/'data'),'XDG_CONFIG_HOME':str(meta/'config')};project=moved/'game.judasproj'
for name,identity,trusted in [('hill-map','2e7ab4325cd065bf8ca25f997168989f2eb5a7f52d767b705c839c4a82a14911',True),('landscape-fork','6df238d6f016cb1dc66e1a481024bc4e4eb5f74e29e01c906d525e24b84f5e41',False)]:
 assert run('moved-install-'+name,[str(C/'judas_mod'),'install',str(project),str(C/'review-mods'/(name+'.judasmod'))],extra=menv)==0
 assert run('moved-enable-'+name,[str(C/'judas_mod'),'enable',str(project),name,'1.0.0',identity]+(['--trust-executable']if trusted else[]),extra=menv)==0
if run('moved-mod-probe',[str(C/'judas_mod_application_tests'),str(moved/'Scenes/landscape.judas'),'probe',str(C/'moved-mod-case')],cwd=Path('/tmp'),extra=menv):raise SystemExit('Preserved moved mod failure')
harness=C/'moved.harness';harness.write_text("FRAMES 260\nFRAME_SECONDS 0.016666667\nWAIT_SERVICES scene 10000 0\nTAP 1 60\nWAIT_SERVICES scene 10000 61\nEXPECT_SCENE Scenes/landscape.judas 62\nTAP E 100\nTAP F6 130\nWAIT_SERVICES save 10000 131\nTAP F7 170\nWAIT_SERVICES save 10000 171\nWAIT_SERVICES scene 10000 172\nEXPECT_SCENE Scenes/landscape.judas 173\nTAP 3 210\nWAIT_SERVICES scene 10000 211\nEXPECT_SCENE Scenes/main.judas 212\n")
if run('moved-shipping-runtime',[str(moved/'judas')],cwd=Path('/tmp'),extra=dict(menv,JUDAS_TEST_SCRIPT=str(harness))):raise SystemExit('Preserved shipping runtime failure')
