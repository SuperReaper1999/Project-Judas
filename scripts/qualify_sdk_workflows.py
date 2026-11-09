#!/usr/bin/env python3
"""Actual SDK author/import/cook/save/server workflows, against an external work dir.

Run the editor first to create work/game/SDK_First_Game.judasproj. Fixture copies
are supplied by the orchestrator; the SDK itself stays read-only and verified.
"""
import argparse
import hashlib
import json
import os
import re
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
    env['XDG_DATA_HOME']=str(out/'user-data')
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
    run('judas_deformable_bake','sheet',out/'sheet.judasdeform',4,4,2,2,1)
    source=game/'Authoring/ground.json';source.parent.mkdir(exist_ok=True)
    run('judas_terrain_author','v2-new',source,'highlands',33,.5,19,2)
    run('judas_terrain_author','v2-validate',source)
    run('judas_terrain_author','v2-pack',source,game/'Assets/ground.judasterrain')
    run('judas_terrain_author','v2-native',source,game/'Assets/ground.judasmodel')
    run('judas_collision_cook','mesh',obj,out/'triangle.judascollision','00000000000000000000000000000001',0)
    run('judas_collision_cook','mesh',obj,out/'bad.judascollision','00000000000000000000000000000001',0,0,1,1,expected=1)
    run('judas_environment_bake',root/'fixtures/studio.hdr',out/'invalid.judasenv',8,8,expected=1)
    run('judas_environment_bake',root/'fixtures/studio.hdr',out/'studio.judasenv',16,32)
    # Ordinary projects remain opted out. Verify rejection before explicitly
    # authoring a separate, self-contained contract for kit qualification.
    run('judas_mod','kit',project,out/'disabled-kit',sdk/'docs',expected=1)
    if 'not opted into modding' not in rows[-1]['stderr']:
        raise RuntimeError('Missing explicit mod opt-in diagnostic')
    run('judas_mod','registries',project)
    registries=rows[-1]['stdout'].strip()
    if len(registries)!=64 or any(c not in '0123456789abcdef' for c in registries):
        raise RuntimeError('Invalid registry identity')
    contract=dict(format=1,enabled=True,executable=False,game='sdk-qualification-'+hashlib.sha256(str(root).encode()).hexdigest()[:12],
                  gameVersion='1.0.0',version='1.0.0',registries=registries,
                  additions=['mesh','scene'],published={},behaviors={},
                  replacements={},extensions={},editableSources=[])
    mod=game/'ModKit.judasproj'
    # JudasProject's quoted string grammar matches JSON escaping here.
    mod.write_text(project.read_text(encoding='utf-8')+'mod-contract '+
                   json.dumps(json.dumps(contract,separators=(',',':')))+'\n',encoding='utf-8')
    run('judas_mod','kit',mod,out/'kit',sdk/'docs')
    run('judas_mod','new',out/'kit',out/'new-mod','qualification-mod')
    author=next((out/'new-mod').glob('*.judasproj'))
    manifest=author.parent/'mod.json'
    draft=json.loads(manifest.read_text(encoding='utf-8'))
    draft.update(author='Judas qualification',license='MIT',notices=['NOTICE.txt'])
    manifest.write_text(json.dumps(draft,indent=2)+'\n',encoding='utf-8')
    (author.parent/'NOTICE.txt').write_text('Original Judas triangle fixture, MIT; see the SDK LICENSE.\n',encoding='utf-8')
    run('judas_model_import_cli','--create',author,obj,'Assets/triangle.judasmodel')
    run('judas_model_import_cli','--recipe',json.loads(rows[-1]['stdout'])['recipe'])
    run('judas_mod','validate',author)
    package=out/'qualification-mod.judasmod'
    run('judas_mod','package',author,package)
    payload_hash=rows[-1]['stdout'].split()[-1]
    run('judas_mod','inspect',package)
    run('judas_mod','install',mod,package)
    run('judas_mod','enable',mod,'qualification-mod','1.0.0',payload_hash)
    run('judas_mod','resolve',mod)
    if 'qualification-mod' not in rows[-1]['stdout']:
        raise RuntimeError('Explicit mod loadout did not resolve')
    run('judas_mod','disable',mod,'qualification-mod')
    run('judas_mod','remove',mod,'qualification-mod','1.0.0',payload_hash)
    # A real independent navigation surface, not an empty-scene no-op.
    nav_scene=game/'Scenes/sdk-navigation.judas'
    scene_text=(game/'Scenes/main.judas').read_text(encoding='utf-8')
    nav_id=max(map(int,re.findall(r'^object (\d+) ',scene_text,re.MULTILINE)))+1
    scene_text=re.sub(r'next-id \d+',f'next-id {nav_id+1}',scene_text,count=1)
    nav_scene.write_text(scene_text+f'\nobject {nav_id} "SDK navigation"\n'
        ' position 0 0 0\n rotation 1 0 0 0\n scale 1 1 1\n'
        ' nav.surface.enabled "1"\n nav.surface.includeDynamic "0"\n'
        ' nav.surface.profile "0"\n nav.surface.sources "18446744073709551615"\n'
        ' nav.surface.halfExtents "8 4 8"\n nav.surface.cellSize "0.2"\n'
        ' nav.surface.cellHeight "0.1"\n nav.surface.tileSize "32"\n'
        ' nav.surface.minRegion "0"\n nav.surface.simplification "1.3"\n'
        ' nav.surface.asset ""\nend\n',encoding='utf-8')
    run('judas_navigation_bake',project,'Scenes/sdk-navigation.judas')
    baked=game/f'Assets/navigation/surface-{nav_id}.judasnav'
    if not baked.is_file() or baked.stat().st_size<128:
        raise RuntimeError('Navigation baker did not produce a real tiled surface')
    run('judas_server',project,28978,1)
    run('judas_server',project,0,1,expected=1)
    run('judas_export',project,root/'cli-export')
    (out/'COMPLETE.json').write_text(json.dumps({'passed':True,'actualToolOperations':len(rows),'tools':sorted(set(r['tool'] for r in rows))},indent=2)+'\n')
    print(len(rows),'SDK operations passed; editor/runtime are verified separately')

if __name__=='__main__':main()
