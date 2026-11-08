#!/usr/bin/env python3
"""Rebuild only the owned Water V2 Workshop authoring/assets, never player saves."""
from pathlib import Path
import json
import math
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'projects/water_v2_workshop'
SOURCE = ROOT / 'projects/basic_water_lab'

def main():
    for folder in ('Assets/scripts', 'Assets/ui', 'Assets/fonts', 'Scenes'):
        (DEST / folder).mkdir(parents=True, exist_ok=True)
    for folder in ('ui', 'fonts'):
        for file in (SOURCE / 'Assets' / folder).iterdir():
            if file.is_file():
                shutil.copyfile(file, DEST / 'Assets' / folder / file.name)
    # The gameplay controller is authored directly in the project. Regenerating
    # scenes must not recreate an obsolete controller through string substitutions.
    shutil.copyfile(SOURCE / 'Assets/scripts/player.js.judasmeta', DEST / 'Assets/scripts/player.js.judasmeta')
    ui = (DEST / 'Assets/ui/menu.judasui').read_text()
    ui = ui.replace('24 24 500 220', '24 24 600 330').replace('470 70', '570 146').replace('470 60', '570 96')
    # A noninteractive reticle gives the pickup ray a visible, exact aim point.
    ui = ui.replace('1280 720 1 1 1 25', '1280 720 1 1 1 26')
    reticle = ('"crosshair" "canvas" 2 0 1 1 0 0 1 '
               '0.5 0.5 0.5 0.5 0 0 24 24 0 0 0.5 0.5 0.5 0.5 '
               '0 0 0 0 0 0 0 0 0 0 0 0 0.95 0.98 1 1 '
               '0 24 0 0 1 "+" "" "41414141414141414141414141414103"\n')
    (DEST / 'Assets/ui/menu.judasui').write_text(ui + reticle)
    # A readable standard input map, with only the logical actions used here.
    entries=[]
    for name,keys,axis in [
      ('move_x',[('key:A',-1),('key:D',1)],1),('move_y',[('key:S',-1),('key:W',1)],1),
      ('move_z',[('key:Q',-1)],1),('look_x',[('mouse:dx',1)],1),('look_y',[('mouse:dy',1)],1),
      ('look_stick_x',[('stick:RightX',1)],1),('look_stick_y',[('stick:RightY',1)],1),
      *[(name,[(f'key:{key}',1)],0) for name,key in [('reset','R'),('jump','Space'),('pause','Escape'),
        ('interact','G'),('throw','H'),('control_toggle','F'),('add_water','B'),('pool','1'),('rotated','2'),('radial','3'),('original','4'),
        ('save','F6'),('load','F7'),('ui_activate','Return'),('ui_next','Tab')]],
      ('ui_click',[('mouse:Left',1)],0),('ui_scroll',[('mouse:wheelY',1)],1)]:
        entries.append(f'{json.dumps(name)} {axis} {len(keys)} '+' '.join(f'{json.dumps(k)} {s} 0' for k,s in keys))
    inputmap='1 '+str(len(entries))+' '+' '.join(entries)+' '
    (DEST / 'water_v2_workshop.judasproj').write_text('''JudasProject 1
name "Water V2 Workshop — source preview"
startup-scene "Scenes/pool.judas"
assets-dir "Assets"
scenes-dir "Scenes"
saves-dir "Saves"
legacy-gameplay "false"
basic-water "true"
water-swimming "true"
advanced-fluid-dynamics "true"
input-map '''+json.dumps(inputmap)+'\n')
    # All game geometry is ordinary scene authoring, not a native special case.
    original=(SOURCE/'Scenes/stillwater.judas').read_text()
    player=original[original.index('object 64 "Swimmer"'):].replace('position 0 1.05 9','position 0 3.35 6')
    for mode in ('pool','rotated','radial'):
        angle=.48 if mode=='rotated' else 0
        c,s=math.cos(angle),math.sin(angle)
        def vec(v):return ' '.join(f'{n:.8g}' for n in v)
        def position(p):x,y,z=p;return vec((c*x-s*y,s*x+c*y,z))
        rotation=vec((math.cos(angle/2),0,0,math.sin(angle/2)))
        blocks=['''JudasScene 3
settings
 name "Water V2 / '''+mode+'''"
 world-origin 0 0 0
 sun-direction -0.3 0.8 0.5
 sun-color 1 0.94 0.82
 ambient 0.35 0.4 0.43
 background-color 0.48 0.66 0.76
 fluid-scale 1
 fidelity-policy none
 next-id 100
end
''']
        blocks.append(f'object 1 "Gravity"\n position {"0 -80 0" if mode=="radial" else "0 0 0"}\n rotation {rotation}\n scale 1 1 1\n gravity {"radial" if mode=="radial" else "uniform"} 9.81\n gravity.region sphere 10000\nend\n')
        blocks.append(f'''object 2 "Physical water — quantity owner"
 position 0 0 0
 rotation {rotation}
 scale 1 1 1
 fluid-volume
 fluid-volume.backend grid
 fluid-volume.spacing 0.4
 fluid-volume.count 30 10 20
 fluid-volume.emitter false
 fluid-volume.emitter-offset 0 0 0
 fluid-volume.max-particles 0
 fluid-volume.fill-height 2
 fluid-volume.density 1000
 fluid-volume.color 0.025 0.26 0.32
 fluid-volume.opacity 0.84
end
''')
        index=3
        def box(name,at,half,color,motion='static',density=500,shape='box'):
            nonlocal index
            b=f'''object {index} "{name}"
 position {position(at)}
 rotation {rotation}
 scale 1 1 1
 render {shape}
 render.half-extents {vec(half)}
 render.radius {half[0]}
 render.color {vec(color)}
 render.alpha 1
 render.secondary-color 0.8 0.8 0.8
 render.secondary-alpha 1
 render.mesh-asset ""
 render.texture-asset ""
 body {motion} {shape}
 body.half-extents {vec(half)}
 body.radius {half[0]}
 body.terrain ""
 body.mass {density*(8*math.prod(half) if shape=='box' else 4*math.pi*half[0]**3/3):.8g}
 body.friction 0.7
 body.restitution 0.05
 body.initial-velocity 0 0 0
 body.pickable true
 body.managed false
 body.compound-count 0
end
'''
            index+=1;blocks.append(b)
        box('Pool floor',(0,-.2,0),(6.4,.2,4.4),(.2,.3,.3))
        for name,p,h in [('West',(-6.2,1.6,0),(.2,1.6,4.4)),('East',(6.2,1.6,0),(.2,1.6,4.4)),
                         ('Back',(0,1.6,-4.2),(6,1.6,.2)),('Front',(0,1.2,4.2),(6,1.2,.2))]:
            box(name,p,h,(.55,.62,.6))
        for name,p,h in [('Front deck',(0,2.2,7.5),(10,.2,3.1)),('Back deck',(0,2.2,-6.5),(10,.2,2.1)),
                         ('West deck',(-8.2,2.2,0),(1.8,.2,4.4)),('East deck',(8.2,2.2,0),(1.8,.2,4.4))]:
            box(name,p,h,(.32,.38,.35))
        for i in range(4):box('Swimming steps '+str(i),(0,1.6-.4*i,3.8-.6*i),(1.1,.2,.3),(.55,.62,.6))
        box('Flow divider',(-.15,1, -1.2),(.2,1,2.8),(.5,.56,.52))
        box('Air chamber roof',(3,2.4,-1.5),(1.2,.2,1.2),(.44,.49,.48))
        box('Air chamber side A',(1.7,1.4,-1.5),(.1,1,1.3),(.44,.49,.48))
        box('Air chamber side B',(4.3,1.4,-1.5),(.1,1,1.3),(.44,.49,.48))
        box('Air chamber back',(3,1.4,-2.8),(1.3,1,.1),(.44,.49,.48))
        box('Green float',(-2,2.4,1),(.4,.4,.4),(.17,.7,.35),'dynamic',500)
        box('Blue neutral',(2,2.7,2),(.35,.35,.35),(.18,.45,.8),'dynamic',1000)
        box('Grey sink',(3,3,1),(.3,.3,.3),(.48,.5,.52),'dynamic',2000,'sphere')
        box('Carryable light crate',(-2,2.8,7),(.35,.35,.35),(.5,.3,.15),'dynamic',500)
        if mode=='rotated':
            p=player.replace('position 0 3.35 6','position '+position((0,3.35,6))).replace('rotation 1 0 0 0','rotation '+rotation)
        else:p=player
        blocks.append(p)
        (DEST/'Scenes'/f'{mode}.judas').write_text('\n'.join(blocks))
    # The workshop owns its static basin geometry; no retired demo is required.
    original=(DEST/'Authoring/planet-basin-geometry.judas').read_text()
    objects=re.findall(r'^object .*?^end',original,re.M|re.S)
    header=original[:original.index('object 1')]
    q='0.923879533 0 0 -0.382683432'
    bottom=(30-.8)/math.sqrt(2)
    objects.append(f'''object 99 "Planet basin — grid-water replacement"
 position {bottom:.9g} {bottom:.9g} 0
 rotation {q}
 scale 1 1 1
 fluid-volume
 fluid-volume.backend grid
 fluid-volume.spacing 0.4
 fluid-volume.count 20 14 20
 fluid-volume.emitter false
 fluid-volume.emitter-offset 0 0 0
 fluid-volume.max-particles 0
 fluid-volume.fill-height 3
 fluid-volume.density 1000
 fluid-volume.color 0.025 0.26 0.32
 fluid-volume.opacity 0.84
end
''')
    objects.append(player.replace('position 0 3.35 6','position 23.9002092 23.9002092 5.5').replace('rotation 1 0 0 0','rotation '+q))
    (DEST/'Scenes/planet-basin.judas').write_text((header+'\n\n'.join(objects)).rstrip()+'\n')


if __name__=='__main__':main()
