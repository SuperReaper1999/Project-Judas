from pathlib import Path
import json,os,subprocess,time,hashlib,shutil
root=Path('/home/conner/Documents/GitHub/Project-Judas/.cache/renderer-v2/sdk-qualification');sdk=root.parent/'sdk'
rel=json.loads((root/'RELOCATION.json').read_text());moved=Path(rel['relocated']);base=moved.parent
rows=json.loads((root/'RESULTS.json').read_text());env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')};env.update(SDL_VIDEODRIVER='x11',SDL_AUDIODRIVER='dummy',JUDAS_WORLD_STATE='none',PYTHONDONTWRITEBYTECODE='1',XDG_DATA_HOME=str(base/'data'),XDG_CONFIG_HOME=str(base/'config'))
def run(name,args,extra=None,expected=0):
 e=dict(env);e.update(extra or {});start=time.monotonic()
 with (root/(name+'.log')).open('w') as log:result=subprocess.run([str(x)for x in args],cwd=base/'empty-cwd',env=e,stdout=log,stderr=subprocess.STDOUT,timeout=120)
 rows.append({'name':name,'argv':[str(x)for x in args],'cwd':str(base/'empty-cwd'),'exit':result.returncode,'expected':expected,'seconds':time.monotonic()-start});(root/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n')
 if result.returncode!=expected:raise RuntimeError(name+' failed; see '+str(root/(name+'.log')))
 return (root/(name+'.log')).read_text()
for name,action in [('write-native','save'),('cold-read-native','load')]:
 harness=base/(name+'.test');harness.write_text('FRAMES 30\nLOG_EVERY 10\nENTITY 9\nACTION '+action+' 1 5 6\nWAIT_SERVICES save 10000 10\nSCREENSHOT 20 '+str(base/(name+'.png'))+'\n')
 log=run(name,[moved/'judas',moved/'game.judasproj'],extra={'JUDAS_TEST_SCRIPT':str(harness)})
 assert ('SDK_SAVE_REQUEST_COMPLETED'if action=='save'else'SDK_COLD_RESTORE_OK gold=37')in log,log
 assert (base/(name+'.png')).is_file()
 print(name,'PASS',flush=True)
(root/'GPU_DONE').write_text(str(base)+'\n');print('GPU_DONE',base,flush=True)
files=[p for p in(base/'data').rglob('*')if p.is_file()];(root/'USERDATA.json').write_text(json.dumps([{'path':str(p),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}for p in files],indent=2)+'\n')
slots=next(p for p in (base/'data').rglob('Slots') if p.is_dir())
run('cold-slot-inspect',[sdk/'judas_save','inspect',slots,'qualification'])
run('cold-slot-check',[sdk/'judas_save','check',moved/'game.judasproj',slots,'qualification'])
run('sdk-verify-after',['python3',sdk/'tools/package_sdk.py','verify',sdk])
(root/'COMPLETE.json').write_text(json.dumps({'passed':True,'operations':len(rows),'sdk':str(sdk),'relocated':str(moved),'hostBaseline':'local glibc2.43; not Ubuntu24 certification','coldProcessRestoredGold':37,'mk2Body':9,'sourceTreeHidden':False,'releasePackage':False,'initialRestrictedGraphicsFailure':'write.log retained; desktop-authorized rerun passed'},indent=2)+'\n')
