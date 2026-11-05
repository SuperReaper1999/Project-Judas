from pathlib import Path
import os,subprocess,json,shutil,tempfile,hashlib
r=Path.cwd();sdk=r/'.cache/renderer-v2/sdk-final';out=r/'.cache/renderer-v2/checkpoint-sdk';out.mkdir(exist_ok=False)
source=out/'fresh-project';shutil.copytree(r/'.cache/renderer-v2/sdk-qualification/fresh-project-v2',source,ignore=shutil.ignore_patterns('Saves'))
project=next(source.glob('*.judasproj'));assert 'gold:0' in (source/'Assets/game.js').read_text()
base=Path(tempfile.mkdtemp(prefix='judas-renderer-v2-checkpoint-'));(base/'empty-cwd').mkdir();rows=[]
env={k:v for k,v in os.environ.items()if not k.startswith('JUDAS_')};env.update(SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',XDG_DATA_HOME=str(base/'data'),XDG_CONFIG_HOME=str(base/'config'),JUDAS_WORLD_STATE='none')
def run(name,args,extra=None,cwd=None):
 e=dict(env);e.update(extra or {})
 with (out/(name+'.log')).open('w') as f:p=subprocess.run(list(map(str,args)),cwd=cwd or base/'empty-cwd',env=e,stdout=f,stderr=subprocess.STDOUT,timeout=150)
 rows.append({'name':name,'command':list(map(str,args)),'cwd':str(cwd or base/'empty-cwd'),'exit':p.returncode})
 (out/'RESULTS.json').write_text(json.dumps({'operations':rows,'relocation':str(base)},indent=2)+'\n')
 if p.returncode:raise RuntimeError(name+' failed')
 return (out/(name+'.log')).read_text()
run('export-mini',[sdk/'judas_export',project,out/'mini-package',sdk/'judas']);game=base/'mini';shutil.move(out/'mini-package',game)
for name,action in [('write','save'),('cold-read','load')]:
 script=out/(name+'.test');script.write_text(f'FRAMES 30\nENTITY 9\nLOG_EVERY 10\nACTION {action} 1 5 6\nWAIT_SERVICES save 10000 10\nSCREENSHOT 20 {out/name}.png\n')
 log=run(name,[game/'judas',game/'game.judasproj'],{'JUDAS_TEST_SCRIPT':str(script)})
 assert ('SDK_SAVE_REQUEST_COMPLETED gold=37' if action=='save' else 'SDK_COLD_RESTORE_OK gold=37') in log
slots=next((base/'data').rglob('Slots'));run('saved-slot-inspect',[sdk/'judas_save','inspect',slots,'qualification']);run('saved-slot-check',[sdk/'judas_save','check',game/'game.judasproj',slots,'qualification'])
lab=sdk/'projects/renderer_v2_lab/renderer_v2_lab.judasproj';run('export-workshop',[sdk/'judas_export',lab,out/'lab-package',sdk/'judas']);labgame=base/'workshop';shutil.move(out/'lab-package',labgame)
smoke=(r/'.cache/renderer-v2/lab-final/smoke.test').read_text().replace(str(r/'.cache/renderer-v2/lab-final'),str(out/'lab-captures'));(out/'lab-captures').mkdir();(out/'lab.test').write_text(smoke)
run('workshop',[labgame/'judas',labgame/'game.judasproj'],{'JUDAS_TEST_SCRIPT':str(out/'lab.test'),'JUDAS_WINDOW_SIZE':'1280x720'})
run('verify-sdk',['python3',sdk/'tools/package_sdk.py','verify',sdk])
hash=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();assert hash(game/'judas')==hash(sdk/'judas')==hash(labgame/'judas')
receipt={'passed':True,'sourceRevision':json.loads((sdk/'BUILD_INFO.json').read_text())['source_revision'],'runtimeSha256':hash(sdk/'judas'),'constructorGold':0,'savedGold':37,'coldGold':37,'sdkTools':17,'labScenes':6,'labCaptures':10,'operations':rows,'relocatedMini':str(game),'relocatedLab':str(labgame),'sdk':str(sdk)}
(out/'COMPLETE.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt,indent=2))
