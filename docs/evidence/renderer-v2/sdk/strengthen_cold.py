from pathlib import Path
import hashlib,json,os,shutil,subprocess,tempfile,time
root=Path('/home/conner/Documents/GitHub/Project-Judas/.cache/renderer-v2/sdk-qualification');sdk=root.parent/'sdk';source=root/'fresh-project';project=root/'fresh-project-v2';shutil.copytree(source,project)
script=project/'Assets/game.js';s=script.read_text();assert 'this.state={gold:37}'in s and "start(){console.log('SDK_NEW_GAME"in s;s=s.replace('this.state={gold:37}','this.state={gold:0}').replace("start(){console.log('SDK_NEW_GAME","start(){this.state.gold=37;console.log('SDK_NEW_GAME");script.write_text(s)
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',PYTHONDONTWRITEBYTECODE='1')
rows=json.loads((root/'RESULTS.json').read_text())
def run(name,args,extra=None,cwd=Path('/tmp'),expected=0):
 e=dict(env);e.update(extra or {});start=time.monotonic()
 with(root/(name+'.log')).open('w')as f:r=subprocess.run([str(x)for x in args],env=e,cwd=cwd,stdout=f,stderr=subprocess.STDOUT,timeout=120)
 rows.append({'name':name,'argv':[str(x)for x in args],'cwd':str(cwd),'exit':r.returncode,'expected':expected,'seconds':time.monotonic()-start});(root/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n')
 if r.returncode!=expected:raise RuntimeError(name+' failed')
 return(root/(name+'.log')).read_text()
prj=project/'SDK_Renderer_Qualification.judasproj';run('strong-script',[sdk/'judas_script_check',prj]);run('strong-export',[sdk/'judas_export',prj,root/'cli-export-v2'])
base=Path(tempfile.mkdtemp(prefix='judas-renderer-v2-sdk-cold-oracle-',dir='/tmp'));moved=base/'game';shutil.move(root/'cli-export-v2',moved);(base/'empty-cwd').mkdir();(root/'STRONG_RELOCATION.json').write_text(json.dumps({'original':str(root/'cli-export-v2'),'relocated':str(moved),'originalRemoved':not(root/'cli-export-v2').exists()},indent=2)+'\n')
assert hashlib.sha256((moved/'judas').read_bytes()).digest()==hashlib.sha256((sdk/'judas').read_bytes()).digest()
for name,action in [('strong-write','save'),('strong-cold-read','load')]:
 harness=base/(name+'.test');harness.write_text('FRAMES 30\nLOG_EVERY 10\nENTITY 9\nACTION '+action+' 1 5 6\nWAIT_SERVICES save 10000 10\nSCREENSHOT 20 '+str(base/(name+'.png'))+'\n')
 log=run(name,[moved/'judas',moved/'game.judasproj'],extra={'JUDAS_TEST_SCRIPT':str(harness),'XDG_DATA_HOME':str(base/'data'),'XDG_CONFIG_HOME':str(base/'config')},cwd=base/'empty-cwd')
 assert('SDK_SAVE_REQUEST_COMPLETED'if action=='save'else'SDK_COLD_RESTORE_OK gold=37')in log,log
 assert(base/(name+'.png')).is_file();print(name,'PASS',flush=True)
print('GPU_FREE',flush=True)
slots=next(p for p in(base/'data').rglob('Slots')if p.is_dir());run('strong-slot-inspect',[sdk/'judas_save','inspect',slots,'qualification']);run('strong-slot-check',[sdk/'judas_save','check',moved/'game.judasproj',slots,'qualification'])
run('strong-missing-slot-negative',[sdk/'judas_save','inspect',slots,'never-created'],expected=1)
run('strong-sdk-verify',['python3',sdk/'tools/package_sdk.py','verify',sdk])
result={'passed':True,'operations':len(rows),'constructorGold':0,'newGameGold':37,'coldRestoredGold':37,'differentProcesses':True,'runtimeSha256':hashlib.sha256((moved/'judas').read_bytes()).hexdigest(),'relocated':str(moved),'savedParticipants':13,'initialFixture':'earlier constructor37 fixture retained as weaker/superseded state oracle','sourceTreeTrace':'earlier same engine-binary relocated run traced zero source/build/SDK path references; strong run not separately traced','releaseQualification':False}
(root/'STRONG_COMPLETE.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
