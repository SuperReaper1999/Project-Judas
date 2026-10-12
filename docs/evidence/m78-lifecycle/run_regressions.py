#!/usr/bin/env python3
"""Focused serial final-source lifecycle interactions; no full-suite run or expectation regeneration."""
from pathlib import Path
import hashlib,json,os,shlex,shutil,subprocess,time
r=Path.cwd();base=r/'.cache/m78-followup/release';out=r/'.cache/m78-lifecycle/regressions';ev=r/'docs/evidence/m78-lifecycle/regressions';out.mkdir(parents=True,exist_ok=False);ev.mkdir(exist_ok=False)
h=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();meta=json.loads((r/'docs/evidence/m78-lifecycle/final-build-v2/BUILD.json').read_text());archive=r/meta['archive']['path'];assert h(archive)==meta['archive']['sha256']
for s,v in meta['sourceSHA256'].items():assert h(r/s)==v,s
env=dict(os.environ,SDL_VIDEODRIVER='offscreen',SDL_AUDIODRIVER='dummy',LIBGL_ALWAYS_SOFTWARE='1',XDG_DATA_HOME=str(out/'data'),XDG_CONFIG_HOME=str(out/'config'))
for k in list(env):
 if k.startswith(('JUDAS_DIAGNOSTIC_','M78_DIAGNOSTIC_','JUDAS_BOUNDARY_')):del env[k]
rows=[];binaries={};links={}
def run(name,cmd,cwd=r,expected=0,extra=None):
 t=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=cwd,env=dict(env,**(extra or {})),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180);log=ev/(name+'.log');log.write_bytes(p.stdout);rows.append(dict(name=name,command=list(map(str,cmd)),cwd=str(cwd),extraEnvironment=extra or {},exit=p.returncode,expectedExit=expected,seconds=time.monotonic()-t,logSHA256=h(log)));(ev/'RESULTS.json').write_text(json.dumps(dict(scope=__doc__,archiveSHA256=h(archive),sourceSHA256=meta['sourceSHA256'],binaries=binaries,links=links,results=rows),indent=2)+'\n');summary=[x for x in p.stdout.decode(errors='replace').splitlines()if 'SUMMARY'in x or 'checks'in x and 'failures'in x];print(name,p.returncode,summary[-1:],flush=True)
 if p.returncode!=expected:raise SystemExit('Preserved unexpected '+name)
 return p

def link(target,override=None):
 cmd=shlex.split((base/f'CMakeFiles/{target}.dir/link.txt').read_text());cmd=[str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')]
 if override:
  key,obj=override;cmd=[str(obj)if key in x else x for x in cmd]
 artifact=target if not override else target+'-'+Path(override[1]).stem
 exe=out/artifact;cmd[cmd.index('-o')+1]=str(exe);links[artifact]={x:h(base/x)for x in cmd if(base/x).is_file()};run(artifact+'-link',cmd,base);binaries[artifact]={'path':str(exe.relative_to(r)),'sha256':h(exe)};return exe
# Current accepted continuation driver already compiled in the corrective checkpoint.
continuation=link('judas_world_persistence_tests',('WorldPersistenceTests.cpp.o',r/'.cache/m78-closure/final-build/PhysicsContinuationTests.cpp.o'));run('continuation112',[continuation])
for target in ['judas_physical_balance_controller_tests','judas_rigid_motion_tests','judas_physical_animation_tests','judas_joint_tests','judas_contact_lifecycle_tests','judas_impact_timing_tests','judas_world_persistence_tests','judas_prefab_tests','judas_streaming_tests','judas_character_motor_tests']:
 exe=link(target);run(target,[exe])
gravity=link('judas_gravity_selection_tests');run('gravity-selection',[gravity,out/'gravity'])
balance=link('judas_balance_lab_tests')
for mode in ['step-trace','lifecycle','radial']:run('balance-'+mode,[balance,out/('balance-'+mode),mode])
for prefix in ['cold','cold-stand','cold-recovery']:
 for phase in ['write','read']:run('balance-'+prefix+'-'+phase,[balance,out/('balance-'+prefix),prefix+'-'+phase])
character=link('judas_character_lab_tests');run('character-application',[character,out/'character'])
for phase in ['write','read']:run('character-cold-'+phase,[character,out/'character-cold','cold-'+phase])
# Same exact independent physical witness, linked to final archive. The wrapper
# contains its original Application route and no production instrumentation.
observer=link('judas_balance_lab_tests',('BalanceLabTests.cpp.o',r/'.cache/m78-closure/mixed-final/observer.o'))
case=out/'mixed-physical';run('mixed-actual-write',[observer,case,'cold-write'],extra={'JUDAS_DIAGNOSTIC_CONTINUE_WRITE':'1'});shutil.copyfile(case/'physical-final.json',ev/'physical-uninterrupted-final.json');run('mixed-actual-read',[observer,case,'cold-read']);shutil.copyfile(case/'physical-final.json',ev/'physical-restored-final.json')
# Canonical durable identity comparison: exclude ONLY generation-local handles.
a=json.loads((ev/'physical-uninterrupted-final.json').read_text());b=json.loads((ev/'physical-restored-final.json').read_text())
def canonical(x):
 if isinstance(x,dict):return {k:canonical(v)for k,v in x.items()if k!='nativeHandle'}
 if isinstance(x,list):
  y=[canonical(v)for v in x];return sorted(y,key=lambda v:json.dumps(v,sort_keys=True))
 return x
comparison={'equalAfterNativeHandleAndArrayOrderExclusion':canonical(a)==canonical(b),'uninterruptedSHA256':h(ev/'physical-uninterrupted-final.json'),'restoredSHA256':h(ev/'physical-restored-final.json'),'exclusions':['nativeHandle','array traversal order'],'saveSHA256':h(case/'slots/m77.save')};(ev/'ACTUAL_ENDPOINT_COMPARISON.json').write_text(json.dumps(comparison,indent=2)+'\n');print('actual endpoint equal',comparison['equalAfterNativeHandleAndArrayOrderExclusion'],flush=True)
if not comparison['equalAfterNativeHandleAndArrayOrderExclusion']:raise SystemExit('Actual endpoint diverged; retain both snapshots')
# Selected existing Application save+streaming cross-system route, original driver.
target='judas_save_streaming_tests';cmd=shlex.split((base/f'CMakeFiles/{target}.dir/link.txt').read_text());cmd=[str(r/'.cache/m78-closure/selected-applications-v2/judas_save_streaming_tests.o')if x.endswith('/SaveStreamingTests.cpp.o')else x.replace('CMakeFiles/'+target+'.dir/src/','CMakeFiles/judas.dir/src/')if x.endswith('.cpp.o')else str(archive)if x=='libjudas_engine.a'else x for x in cmd if not x.startswith('-Wl,--dependency-file=')];artifact=target if not override else target+'-'+Path(override[1]).stem
 exe=out/artifact;cmd[cmd.index('-o')+1]=str(exe);links[artifact]={x:h(base/x)for x in cmd if(base/x).is_file()};run(target+'-link',cmd,base);binaries[target]={'path':str(exe.relative_to(r)),'sha256':h(exe)}
for mode in ['write','read','read-b']:run('save-streaming-'+mode,[exe,r/'projects/streamed_range/streamed_range.judasproj',mode,out/'save-streaming'])
# Legacy bytes are copied, never rewritten or inferred from new-format passes.
legacy=out/'legacy-a106';(legacy/'slots').mkdir(parents=True);shutil.copytree(r/'projects/balance_lab',legacy/'project');snapshot=r/'docs/evidence/m78-continuation/repro/m77-native-in-progress-step-failed.save';assert h(snapshot)=='a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b';shutil.copyfile(snapshot,legacy/'slots/m77.save');run('legacy-a106-preserved-failure',[balance,legacy,'cold-read'],expected=1);assert h(snapshot)==h(legacy/'slots/m77.save')
