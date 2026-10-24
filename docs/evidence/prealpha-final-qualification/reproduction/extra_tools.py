from pathlib import Path
import subprocess,json,time,os
sdk=Path('/source');root=Path('/build');out=root/'extra-tool-results';out.mkdir(exist_ok=False);rows=[]
def run(tool,*args,expected=0):
 cmd=[str(sdk/tool),*map(str,args)];start=time.monotonic();p=subprocess.run(cmd,cwd='/tmp',capture_output=True,text=True,timeout=120)
 rows.append(dict(tool=tool,command=cmd,exit=p.returncode,expected=expected,seconds=time.monotonic()-start,stdout=p.stdout,stderr=p.stderr));(out/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n')
 print(tool,p.returncode,flush=True);assert p.returncode==expected,p.stdout+p.stderr
project=root/'game/SDK_First_Game.judasproj'
run('judas_terrain_author','create',root/'game/Assets/ground.judasterrain',9,9,8,8)
run('judas_terrain_author','brush',root/'game/Assets/ground.judasterrain','raise',0,0,0,2,.5)
run('judas_terrain_author','validate',root/'game/Assets/ground.judasterrain')
run('judas_terrain_author','cook',project,'Assets/ground.judasterrain')
run('judas_collision_cook','mesh',root/'sdk-final-workflows/triangle.obj',out/'triangle.judascollision','00000000000000000000000000000001',0)
run('judas_collision_cook','mesh',root/'sdk-final-workflows/triangle.obj',out/'invalid.judascollision','00000000000000000000000000000001',0,0,1,1,expected=1)
run('judas_environment_bake',root/'fixtures/studio.hdr',out/'studio.judasenv',8,8)
mod=root/'fixtures/modding_lab/modding_lab.judasproj'
run('judas_mod','kit',mod,out/'kit',sdk/'docs')
run('judas_mod','new',out/'kit',out/'new-mod','qualification-mod')
run('judas_navigation_bake',mod,'Scenes/landscape.judas')
run('judas_liquid_bake',root/'fixtures/liquid_reservoir_demo/liquid_reservoir_demo.judasproj','Scenes/lab.judas')
run('judas_server',project,28978,1)
run('judas_server',mod,28978,1,expected=1)
assert 'not supported' in rows[-1]['stderr'] or 'not supported' in rows[-1]['stdout']
print('EXTRA TOOL OPERATIONS PASS',len(rows),flush=True)
