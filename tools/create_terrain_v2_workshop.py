#!/usr/bin/env python3
"""Author the owned Terrain V2 Workshop only. Never rewrites another project."""
from pathlib import Path
import json, math, shutil, struct, subprocess
ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'projects/terrain_v2_workshop'
SEED=ROOT/'projects/water_v2_workshop'
IDS={k:f'8a2200000000000000000000000000{v:02x}' for v,k in enumerate(('script','model','collision','grass','earth','rock','sand','data','natural_grass','natural_earth','natural_rock','natural_sand','preview','native'),1)}
def meta(path,key,kind):
    Path(str(path)+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{IDS[key]}"\ntype {kind}\nsource ""\n')
def texture(path,kind):
    # Original deterministic material source. Fine aggregate detail is render
    # only; large terrain shape comes from the shared sampled source.
    n=128;data=bytearray()
    for y in range(n):
        for x in range(n):
            h=((x*73856093)^(y*19349663)^(kind*83492791))&65535
            grain=(h/65535-.5)*.22
            veins=.10*math.sin(x*.32+math.sin(y*.19)*2) if kind==2 else .04*math.sin(x*.71+y*.27)
            value=max(0,min(255,round((.80+grain+veins)*255)))
            data.extend((value,value,value,255))
    header=bytearray(18);header[2]=2;header[12:16]=struct.pack('<HH',n,n);header[16]=32;header[17]=8
    path.write_bytes(header+data)
def main():
    for folder in ('Assets/scripts','Assets/terrain','Assets/ui','Assets/fonts','Authoring','Scenes'):(DEST/folder).mkdir(parents=True,exist_ok=True)
    for folder in ('ui','fonts'):
        for f in (SEED/'Assets'/folder).iterdir():
            if f.is_file():shutil.copyfile(f,DEST/'Assets'/folder/f.name)
    for i,key in enumerate(('grass','earth','rock','sand')):
        path=DEST/f'Assets/terrain/{key}.tga';texture(path,i);meta(path,key,'texture')
    # Reuse the locally catalogued CC0 materials, keeping their provenance.
    for key in ('grass','earth','rock','sand'):
        path=DEST/f'Assets/terrain/{key}-natural.png'
        shutil.copyfile(ROOT/f'projects/lunar_island/Assets/textures/{key}.png',path)
        meta(path,'natural_'+key,'texture')
    shutil.copyfile(ROOT/'projects/lunar_island/Assets/textures/LICENSE.txt',DEST/'Assets/terrain/REUSED_MATERIAL_NOTICES.txt')
    n=97;spacing=.5;heights=[];weights=[]
    for z in range(n):
        for x in range(n):
            px=(x-(n-1)/2)*spacing;pz=(z-(n-1)/2)*spacing
            hill=7*math.exp(-pz*pz/36-px*px/190)
            ridge=4.2*math.exp(-((px+15)**2+(pz+13)**2)/52)
            h=.65+hill+ridge+.16*math.sin(px*.35)*math.sin(pz*.29)
            heights.append(h)
            rock=min(.85,max(0,(h-3.4)/5));earth=.13+.15*math.sin(px*.21+pz*.16)**2
            weights.append([1-rock-earth,earth,rock,0])
    source={'format':'JudasTerrainV2','version':2,'samplesX':n,'samplesZ':n,'chunkCells':16,
        'spacing':spacing,'bottom':-4,'top':16,'heights':heights,'weights':weights,'edits':[],
        'layerTextures':[IDS['natural_'+k] for k in ('grass','earth','rock','sand')],
        'layerColours':[[1,1,1],[1,1,1],[1,1,1],[1,1,1]]}
    # Start intact. T opens an actual cross-chunk tunnel through this hill.
    (DEST/'Authoring/highlands.judasterrain2').write_text(json.dumps(source,separators=(',',':'))+'\n')
    model=DEST/'Assets/terrain/highlands.judasmodel'
    subprocess.run([str(ROOT/'build/judas_terrain_author'),'v2-native',str(DEST/'Authoring/highlands.judasterrain2'),str(model)],check=True)
    meta(model,'model','mesh')
    meta(model.parent/'terrain-source.js','data','script')
    meta(model.with_suffix('.preview.tga'),'preview','texture')
    meta(model.with_suffix('.judasterrain'),'native','terrain')
    controls=[('move_x',[('key:A',-1),('key:D',1)],1),('move_y',[('key:S',-1),('key:W',1)],1),
              ('look_x',[('mouse:dx',1)],1),('look_y',[('mouse:dy',1)],1)]
    for key,physical in [('jump','Space'),('pause','Escape'),('save','F6'),('load','F7'),('reset','R'),('tunnel','T'),('dig','mouse:Left'),('build','mouse:Right'),('mode_dig','1'),('mode_raise','2'),('mode_lower','3'),('mode_paint','4'),('paint_layer','5')]:
        controls.append((key,[(physical if ':' in physical else 'key:'+physical,1)],0))
    controls.extend([('ui_click',[('mouse:Left',1)],0),('ui_activate',[('key:Return',1)],0),
                     ('ui_next',[('key:Tab',1)],0),('ui_scroll',[('mouse:wheelY',1)],1)])
    inputmap='1 '+str(len(controls))+' '+' '.join(json.dumps(k)+' '+str(axis)+' '+str(len(bindings))+' '+' '.join(json.dumps(p)+' '+str(v)+' 0' for p,v in bindings) for k,bindings,axis in controls)
    (DEST/'terrain_v2_workshop.judasproj').write_text('JudasProject 1\nname "Terrain V2 — Highlands Workshop"\nstartup-scene "Scenes/highlands.judas"\nassets-dir "Assets"\nscenes-dir "Scenes"\nsaves-dir "Saves"\nlegacy-gameplay "false"\nadvanced-fluid-dynamics "false"\ninput-map '+json.dumps(inputmap)+'\n')
    header='''JudasScene 3
settings
 name "Terrain V2 — Highlands"
 world-origin 0 0 0
 sun-direction -0.4 0.8 0.35
 sun-color 1 0.94 0.82
 ambient 0.34 0.39 0.45
 background-color 0.48 0.67 0.81
 fluid-scale 1
 fidelity-policy none
 next-id 100
end
object 1 "Editable Highlands — heightfield and caves"
 terrain.asset "NATIVE"
 position 0 0 0
 rotation 1 0 0 0
 scale 1 1 1
 render mesh
 render.half-extents 24 8 24
 render.radius 1
 render.alpha 1
 render.secondary-color 0.8 0.8 0.8
 render.secondary-alpha 1
 render.texture-asset "PREVIEW"
 render.mesh-asset "MODEL"
 render.color 1 1 1
 body static box
 body.half-extents 24 8 24
 body.radius 1
 body.terrain ""
 body.mass 1
 body.initial-velocity 0 0 0
 body.pickable false
 body.managed false
 body.compound-count 0
 body.collision-asset ""
 body.friction 0.75
 body.restitution 0
 nav.surface.enabled "1"
 nav.surface.includeDynamic "0"
 nav.surface.profile "0"
 nav.surface.sources "18446744073709551615"
 nav.surface.halfExtents "25 12 25"
 nav.surface.cellSize "0.25"
 nav.surface.cellHeight "0.1"
 nav.surface.simplification "1.3"
 nav.surface.tileSize "32"
 nav.surface.minRegion "0"
 nav.surface.asset ""
end
object 2 "Gravity"
 position 0 0 0
 rotation 1 0 0 0
 scale 1 1 1
 gravity uniform 9.81
 gravity.region sphere 10000
end
'''.replace('NATIVE',IDS['native']).replace('MODEL',IDS['model']).replace('COLLISION',IDS['collision']).replace('PREVIEW',IDS['preview'])
    old=(SEED/'Scenes/pool.judas').read_text();player=old[old.index('object 64 "Swimmer"'):]
    player=player.replace('"Swimmer"','"Explorer"').replace('position 0 3.35 6','position 0 1.9 13').replace('c28ff4911a81df1950aa5391a138ca7c',IDS['script'])
    (DEST/'Scenes/highlands.judas').write_text(header+player)
    meta(DEST/'Assets/scripts/explorer.js','script','script')
    (DEST/'.gitignore').write_text('Saves/\n')
    print('Authored',DEST)
if __name__=='__main__':main()
