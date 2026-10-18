from pathlib import Path
import subprocess,json,os,datetime,hashlib,time,shutil
ROOT=Path.cwd();OUT=ROOT/'.cache/prealpha-pass3/final-linux';OUT.mkdir(exist_ok=True)
records=[]
env=os.environ.copy();env.update(SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',XDG_DATA_HOME=str(OUT/'user-data'),XDG_CONFIG_HOME=str(OUT/'config'))
def run(name,args,expected=0,extra=None,cwd=ROOT,timeout=120):
 a=[str(v) for v in args];e=env.copy();e.update(extra or {});start=time.monotonic();row={'name':name,'argv':a,'cwd':str(cwd),'startedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'expectedExit':expected,'environmentOverrides':{k:e[k] for k in ['SDL_AUDIODRIVER','JUDAS_WORLD_STATE','XDG_DATA_HOME','XDG_CONFIG_HOME',*(extra or {})]}}
 f=Path(a[0]);row['binarySha256']=hashlib.sha256(f.read_bytes()).hexdigest() if f.is_file() else None
 try:
  with (OUT/(name+'.log')).open('w') as log:r=subprocess.run(a,cwd=cwd,env=e,stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
  row['exit']=r.returncode;row['exitMatchesExpectation']=r.returncode==expected
 except subprocess.TimeoutExpired:
  row.update(exit=None,exitMatchesExpectation=False,timedOut=True)
 row['seconds']=time.monotonic()-start
 records.append(row);(OUT/'RESULTS.json').write_text(json.dumps(records,indent=2)+'\n');print(name,row['exit'],round(row['seconds'],3),flush=True)
 return row['exitMatchesExpectation']
if __name__=='__main__':
 names=json.loads((OUT/'SELECTED.json').read_text())
 args=['python3','scripts/prealpha_validate.py','--build-dir',ROOT/'build','--output',OUT/'manifest']
 for n in names:args+=['--test',n]
 run('manifest',args,timeout=180)
 for name,target in [('impact_robustness','judas_impact_robustness_tests'),('world_persistence','judas_world_persistence_tests'),('ragdoll','judas_ragdoll_tests'),('script','judas_script_tests'),('script_debug_reload','judas_script_developer_tools_tests'),('runtime_ui','judas_runtime_ui_tests'),('script_editor','judas_script_tools_editor_tests'),('consumer_debug_reload','judas_script_developer_consumer_tests')]:run(name,[ROOT/'build'/target])
 run('m77_cold_step_write',[ROOT/'build/judas_balance_lab_tests',OUT/'m77-step','cold-write'])
 run('m77_cold_step_read',[ROOT/'build/judas_balance_lab_tests',OUT/'m77-step','cold-read'])
 run('m77_cold_stand_write',[ROOT/'build/judas_balance_lab_tests',OUT/'m77-stand','cold-stand-write'])
 run('m77_cold_stand_read',[ROOT/'build/judas_balance_lab_tests',OUT/'m77-stand','cold-stand-read'])
 run('m77_cold_recovery_write',[ROOT/'build/judas_balance_lab_tests',OUT/'m77-recovery','cold-recovery-write'])
 run('m77_cold_recovery_read',[ROOT/'build/judas_balance_lab_tests',OUT/'m77-recovery','cold-recovery-read'])
 run('m70_cold_write',[ROOT/'build/judas_character_lab_tests',OUT/'m70','cold-write'])
 run('m70_cold_read',[ROOT/'build/judas_character_lab_tests',OUT/'m70','cold-read'])
 run('cookbook',[ROOT/'build/judasjs_examples_tests',OUT/'cookbook'],timeout=180)
 run('api_types_drift',['node','scripts/check_judasjs_api.mjs','--typescript',ROOT/'tools/verification/node_modules/typescript/lib/typescript.js','--runtime',OUT/'cookbook/surface.json'])
 first=OUT/'manifest/first-project/My First Game'
 edit=OUT/'editor-project';shutil.copytree(first,edit)
 run('editor_play_stop',[ROOT/'build/judas_editor',edit/'My_First_Game.judasproj'],extra={'JUDAS_EDITOR_AUTOTEST':str(OUT/'editor'),'JUDAS_EDITOR_AUTOTEST_AUTHORING':'1'})
 run('export_first_project',[ROOT/'build/judas_export',first/'My_First_Game.judasproj',OUT/'first-project-export',ROOT/'build/judas'])
 moved=Path('/tmp/Judas-PreAlphaPass3-FirstProject');assert not moved.exists();shutil.copytree(OUT/'first-project-export',moved)
 (OUT/'standalone.txt').write_text('FRAMES 45\nLOG_EVERY 0\nSCREENSHOT 30 '+str(OUT/'standalone.png')+'\n')
 run('relocated_standalone',[moved/'judas'],cwd=Path('/tmp'),extra={'JUDAS_TEST_SCRIPT':str(OUT/'standalone.txt')})
 raise SystemExit(0 if all(r['exitMatchesExpectation'] for r in records) else 1)
