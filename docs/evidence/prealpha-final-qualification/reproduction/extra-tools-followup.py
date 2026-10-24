from pathlib import Path
import subprocess,json,time,os
sdk=Path('/source');root=Path('/build');out=root/'extra-tool-followup';out.mkdir(exist_ok=False);rows=[]
def run(tool,*args,expected=0):
 cmd=[str(sdk/tool),*map(str,args)];start=time.monotonic();p=subprocess.run(cmd,cwd='/tmp',capture_output=True,text=True,timeout=120)
 rows.append(dict(tool=tool,command=cmd,exit=p.returncode,expected=expected,seconds=time.monotonic()-start,stdout=p.stdout,stderr=p.stderr));(out/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n')
 print(tool,p.returncode,flush=True);assert p.returncode==expected,p.stdout+p.stderr
project=root/'game/SDK_First_Game.judasproj'
run('judas_environment_bake',root/'fixtures/studio.hdr',out/'studio.judasenv',16,32)
mod=root/'fixtures/modding_lab/modding_lab.judasproj'
run('judas_mod','kit',mod,out/'kit',sdk/'docs')
run('judas_mod','new',out/'kit',out/'new-mod','qualification-mod')
run('judas_navigation_bake',mod,'Scenes/landscape.judas')
run('judas_liquid_bake',root/'fixtures/liquid_reservoir_demo/liquid_reservoir_demo.judasproj','Scenes/lab.judas')
run('judas_server',project,28978,1)
run('judas_server',mod,28978,1,expected=1)
assert 'not supported' in rows[-1]['stderr'] or 'not supported' in rows[-1]['stdout']
print('EXTRA TOOL OPERATIONS PASS',len(rows),flush=True)
