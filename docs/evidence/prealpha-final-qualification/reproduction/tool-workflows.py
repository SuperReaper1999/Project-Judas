from pathlib import Path
import subprocess,json,os,time
root=Path('/build')
sdk=Path('/source'); project=root/'game/SDK_First_Game.judasproj';game=project.parent
out=root/'sdk-final-workflows';out.mkdir(exist_ok=False);records=[]
def run(tool,*args,expected=0):
 command=[str(sdk/tool),*map(str,args)];start=time.monotonic();p=subprocess.run(command,cwd='/tmp',capture_output=True,text=True,timeout=45,env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')})
 records.append({'command':command,'exit':p.returncode,'expected':expected,'seconds':time.monotonic()-start,'stdout':p.stdout,'stderr':p.stderr});(out/'RESULTS.json').write_text(json.dumps(records,indent=2)+'\n')
 if p.returncode!=expected:raise RuntimeError(str(command)+'\n'+p.stdout+p.stderr)
run('judas_scene_author','--validate','project',project)
run('judas_scene_author','--validate','scene',game/'Scenes/main.judas')
run('judas_scene_author','--create','ui',out/'menu.judasui')
run('judas_scene_author','--validate','ui',out/'menu.judasui')
run('judas_script_check',project)
script=game/'Assets/game.js';before=script.read_bytes()
try:
 script.write_text('export default class { update( { deliberately invalid')
 run('judas_script_check',project,expected=1)
 assert 'game.js' in records[-1]['stdout']+records[-1]['stderr']
finally:script.write_bytes(before)
run('judas_script_check',project)
run('judas_script_check',out/'missing.judasproj',expected=2)
run('judas_save','identity',project)
source=out/'triangle.obj';source.write_text('o sdk_triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n')
run('judas_model_import_cli','--create',project,source,'Assets/triangle.judasmodel')
recipe=json.loads(records[-1]['stdout'])['recipe']
run('judas_model_import_cli','--recipe',recipe)
run('judas_model_import_cli','--inspect',game/'Assets/triangle.judasmodel')
run('judas_cook',game/'Assets/triangle.judasmodel',out/'triangle.judasmesh')
run('judas_volume_author','create',game/'Assets/soil.judasvolume',9,9,9,8,8,8)
run('judas_volume_author','validate',game/'Assets/soil.judasvolume')
run('judas_volume_author','brush',game/'Assets/soil.judasvolume','remove',0,0,0,2,1)
run('judas_volume_author','cook',project,'Assets/soil.judasvolume')
run('judas_deformable_bake','sheet',out/'sheet.judasdeform',4,4,2,2,1)
run('judas_export',project,root/'cli-export')
print(len(records),'actual SDK tool operations matched their expected results')
