#!/usr/bin/env python3
"""Actual SDK author/import/cook/save/server workflows, against an external work dir.

Run the editor first to create work/game/SDK_First_Game.judasproj. Fixture copies
are supplied by the orchestrator; the SDK itself stays read-only and verified.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk',type=Path,required=True);p.add_argument('--work',type=Path,required=True)
    a=p.parse_args();sdk=a.sdk.resolve();root=a.work.resolve();out=root/'tool-operations';out.mkdir(exist_ok=False)
    project=root/'game/SDK_First_Game.judasproj';game=project.parent;rows=[]
    ext='.exe' if os.name=='nt' else ''
    env={k:v for k,v in os.environ.items() if not k.startswith('JUDAS_')}
    def run(tool,*args,expected=0):
        command=[str(sdk/(tool+ext)),*map(str,args)];start=time.monotonic()
        child=subprocess.run(command,cwd=out,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120,env=env)
        rows.append(dict(tool=tool,command=command,exit=child.returncode,expected=expected,seconds=time.monotonic()-start,stdout=child.stdout,stderr=child.stderr))
        (out/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
        if child.returncode!=expected:raise RuntimeError(tool+': '+child.stdout+child.stderr)
    run('judas_scene_author','--validate','project',project)
    run('judas_scene_author','--validate','scene',game/'Scenes/main.judas')
    run('judas_scene_author','--create','ui',out/'menu.judasui')
    run('judas_scene_author','--validate','ui',out/'menu.judasui')
    run('judas_script_check',project)
    script=game/'Assets/game.js';original=script.read_bytes()
    try:
        script.write_text('export default class { update( { deliberately invalid')
        run('judas_script_check',project,expected=1)
        if 'game.js' not in rows[-1]['stdout']+rows[-1]['stderr']:raise RuntimeError('Missing actionable script location')
    finally:script.write_bytes(original)
    run('judas_script_check',project)
    run('judas_save','identity',project)
    obj=out/'triangle.obj';obj.write_text('o triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n')
    run('judas_model_import_cli','--create',project,obj,'Assets/triangle.judasmodel')
    recipe=json.loads(rows[-1]['stdout'])['recipe'];run('judas_model_import_cli','--recipe',recipe)
    run('judas_model_import_cli','--inspect',game/'Assets/triangle.judasmodel')
    run('judas_cook',game/'Assets/triangle.judasmodel',out/'triangle.judasmesh')
    run('judas_volume_author','create',game/'Assets/soil.judasvolume',9,9,9,8,8,8)
    run('judas_volume_author','validate',game/'Assets/soil.judasvolume')
    run('judas_volume_author','brush',game/'Assets/soil.judasvolume','remove',0,0,0,2,1)
    run('judas_volume_author','cook',project,'Assets/soil.judasvolume')
    run('judas_deformable_bake','sheet',out/'sheet.judasdeform',4,4,2,2,1)
    run('judas_terrain_author','create',game/'Assets/ground.judasterrain',9,9,8,8)
    run('judas_terrain_author','brush',game/'Assets/ground.judasterrain','raise',0,0,0,2,.5)
    run('judas_terrain_author','validate',game/'Assets/ground.judasterrain')
    run('judas_terrain_author','cook',project,'Assets/ground.judasterrain')
    run('judas_collision_cook','mesh',obj,out/'triangle.judascollision','00000000000000000000000000000001',0)
    run('judas_collision_cook','mesh',obj,out/'bad.judascollision','00000000000000000000000000000001',0,0,1,1,expected=1)
    run('judas_environment_bake',root/'fixtures/studio.hdr',out/'studio.judasenv',8,8)
    mod=root/'fixtures/modding_lab/modding_lab.judasproj'
    run('judas_mod','kit',mod,out/'kit',sdk/'docs')
    run('judas_mod','new',out/'kit',out/'new-mod','qualification-mod')
    run('judas_navigation_bake',mod,'Scenes/landscape.judas')
    run('judas_liquid_bake',root/'fixtures/liquid_reservoir_demo/liquid_reservoir_demo.judasproj','Scenes/lab.judas')
    run('judas_server',project,28978,1)
    run('judas_server',mod,28978,1,expected=1)
    run('judas_export',project,root/'cli-export')
    (out/'COMPLETE.json').write_text(json.dumps({'passed':True,'actualToolOperations':len(rows),'tools':sorted(set(r['tool'] for r in rows))},indent=2)+'\n')
    print(len(rows),'SDK operations passed; editor/runtime are verified separately')

if __name__=='__main__':main()
