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
source=R/'docs/evidence/m78-next-risk/probes/ComposedStreamSave.cpp';target='judas_save_streaming_tests';flags={}
for line in (B/f'CMakeFiles/{target}.dir/flags.make').read_text().splitlines():
 if ' = 'in line:k,v=line.split(' = ',1);flags[k]=shlex.split(v)
obj=C/'composed.o';assert run('composed-compile',['c++',*flags['CXX_DEFINES'],*flags['CXX_INCLUDES'],*flags['CXX_FLAGS'],'-c',str(source),'-o',str(obj)])==0
cmd=shlex.split((B/f'CMakeFiles/{target}.dir/link.txt').read_text());cmd=[str(obj)if x.endswith('/SaveStreamingTests.cpp.o')else x.replace('CMakeFiles/'+target+'.dir/src/','CMakeFiles/judas.dir/src/')if x.endswith('.cpp.o')else x for x in cmd];cmd=[str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];exe=C/'composed';cmd[cmd.index('-o')+1]=str(exe);assert run('composed-link',cmd,B)==0;binaries['composed']={'path':str(exe.relative_to(R)),'sha256':sha(exe),'source':str(source.relative_to(R)),'sourceSHA256':sha(source),'objectSHA256':sha(obj)}
project=C/'stream-fixture-02/fixture/project/streamed_range.judasproj'
for mode in ['write','read','read-b']:
 if run('composed-'+mode,[str(exe),str(project),mode,str(C/'stream-save')]):raise SystemExit('Preserved composed failure')
# Re-link tools from unchanged tool/application objects; do not rebuild unrelated systems.
for target in ['judas_mod','judas_export','judas']:
 cmd=shlex.split((B/f'CMakeFiles/{target}.dir/link.txt').read_text());cmd=[str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];exe=C/target;cmd[cmd.index('-o')+1]=str(exe);assert run(target+'-link-current',cmd,B)==0;binaries[target]={'path':str(exe.relative_to(R)),'sha256':sha(exe)}
# Preserved ordinary terrain packages from accepted M78 follow-up, copied into new owned roots.
pkg=C/'mod-package';shutil.copytree(R/'.cache/m78-followup/review-shared-terrain-package',pkg,ignore=shutil.ignore_patterns('judas','Saves'));shutil.copyfile(C/'judas',pkg/'judas')
mods=C/'review-mods';mods.mkdir();old=Path('/tmp/judas-m78-followup-terrain-review/review-mods')
for name in ['hill-map.judasmod','landscape-fork.judasmod']:shutil.copyfile(old/name,mods/name)
meta=C/'mod-user';menv={'XDG_DATA_HOME':str(meta/'data'),'XDG_CONFIG_HOME':str(meta/'config')};project=pkg/'game.judasproj'
for name,identity,trusted in [('hill-map','2e7ab4325cd065bf8ca25f997168989f2eb5a7f52d767b705c839c4a82a14911',True),('landscape-fork','6df238d6f016cb1dc66e1a481024bc4e4eb5f74e29e01c906d525e24b84f5e41',False)]:
 assert sha(mods/(name+'.judasmod'))==identity
 assert run('mod-install-'+name,[str(C/'judas_mod'),'install',str(project),str(mods/(name+'.judasmod'))],extra=menv)==0
 assert run('mod-enable-'+name,[str(C/'judas_mod'),'enable',str(project),name,'1.0.0',identity]+(['--trust-executable']if trusted else[]),extra=menv)==0
for mode in ['probe','write','read']:
 if run('mod-'+mode,[str(C/'judas_mod_application_tests'),str(project),mode,str(C/'mod-case')],extra=menv):raise SystemExit('Preserved mod Application failure')
assert run('mod-disable-replacement',[str(C/'judas_mod'),'disable',str(project),'landscape-fork'],extra=menv)==0
if run('mod-mismatch',[str(C/'judas_mod_application_tests'),str(project),'mismatch',str(C/'mod-case')],extra=menv):raise SystemExit('Preserved changed-loadout rejection failure')
