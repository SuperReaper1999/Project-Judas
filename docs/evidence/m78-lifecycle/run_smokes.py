#!/usr/bin/env python3
"""Final lifecycle-source identity confirmation and owned editor/export/moved smoke."""
from pathlib import Path
import hashlib,json,os,shlex,shutil,subprocess,time
r=Path.cwd();base=r/'.cache/m78-followup/release';out=r/'.cache/m78-lifecycle/smokes';ev=r/'docs/evidence/m78-lifecycle/smokes';out.mkdir(exist_ok=False);ev.mkdir(exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();archive=r/'.cache/m78-lifecycle/final-build-v2/libjudas_engine.a';env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(out/'data'),XDG_CONFIG_HOME=str(out/'config'));rows=[];binaries={};links={}
def run(name,cmd,cwd=r,extra={},expected=0):
 t=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=cwd,env=dict(env,**extra),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180);log=ev/(name+'.log');log.write_bytes(p.stdout);rows.append(dict(name=name,command=list(map(str,cmd)),cwd=str(cwd),extraEnvironment=extra,exit=p.returncode,expectedExit=expected,seconds=time.monotonic()-t,logSHA256=h(log),executableSHA256=h(Path(cmd[0])) if Path(cmd[0]).is_file()else None));(ev/'RESULTS.json').write_text(json.dumps(dict(scope=__doc__,archiveSHA256=h(archive),binaries=binaries,linkInputs=links,results=rows),indent=2)+'\n');print(name,p.returncode,flush=True)
 if p.returncode!=expected:raise SystemExit('Preserved unexpected '+name)
def link(target,label,override=None):
 cmd=shlex.split((base/f'CMakeFiles/{target}.dir/link.txt').read_text());cmd=[str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')]
 if override:cmd=[str(override[1])if override[0]in x else x for x in cmd]
 exe=out/label;cmd[cmd.index('-o')+1]=str(exe);links[label]={x:h(base/x)for x in cmd if(base/x).is_file()};run(label+'-link',cmd,base);binaries[label]={'path':str(exe.relative_to(r)),'sha256':h(exe)};return exe
# The earlier serial runner reused two output names. Preserve its exact runner,
# then confirm distinct artifacts and record their actual pre-run fingerprints.
continuation=link('judas_world_persistence_tests','continuation112',('WorldPersistenceTests.cpp.o',r/'.cache/m78-closure/final-build/PhysicsContinuationTests.cpp.o'));run('continuation112-identity-confirmation',[continuation])
persistence=link('judas_world_persistence_tests','world-persistence');run('world-persistence-identity-confirmation',[persistence])
balance=link('judas_balance_lab_tests','balance-standard')
for phase in ['write','read']:run('balance-cold-'+phase+'-identity-confirmation',[balance,out/'cold', 'cold-'+phase])
editor=link('judas_editor','judas_editor');runtime=link('judas','judas')
project=out/'editor-project';shutil.copytree(r/'projects/balance_lab',project);run('editor-edit-play-stop',[editor,project/'balance_lab.judasproj'],extra={'JUDAS_EDITOR_AUTOTEST':str(out/'editor-evidence')})
package=out/'package';exporter=base/'judas_export';binaries['unchanged-exporter']={'path':str(exporter.relative_to(r)),'sha256':h(exporter)};run('normal-export',[exporter,r/'projects/balance_lab/balance_lab.judasproj',package,runtime])
moved=Path('/tmp/judas-m78-lifecycle-moved');assert not moved.exists();shutil.copytree(package,moved);harness=out/'startup.harness';harness.write_text((r/'.cache/m78-continuation/final-startup.harness').read_text().replace(str(r/'docs/evidence/m78-continuation/linux-final-startup.png'),str(ev/'startup.png')))
run('moved-standalone',[moved/'judas'],Path('/tmp'),extra={'JUDAS_ENGINE_ROOT':'/tmp/unavailable-m78-engine-root','JUDAS_TEST_SCRIPT':str(harness)})
assert h(moved/'judas')==h(runtime)
