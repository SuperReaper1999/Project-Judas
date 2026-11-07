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
    script = (SOURCE / 'Assets/scripts/player.js').read_text()
    script = script.replace('input,ui,world,scenes', 'input,ui,world,scenes,saves,physics')
    script = script.replace('this.state={yaw:0,pitch:-.2}', 'this.state={yaw:0,pitch:-.24,pump:false}')
    script = script.replace("'STILLWATER / BASIC WATER'", "'WATER V2 / SOURCE PREVIEW'")
    script = script.replace("'WASD + mouse: explore | Space: jump / swim up | Q: dive\\nWalk down the steps or jump into the pool. Escape: pause | R: restart'", "'WASD + mouse | Space: jump / swim | Q: dive\\nG: grab / release | H: throw | F: pump | B: splash\\n1: pool | 2: rotated | 3: radial | 4: original planet | F6/F7: save/load | Esc: pause'")
    script = script.replace('restore(){this.start();}', '''restore(){this.start();console.log('WATER_WORKSHOP_RESTORED '+JSON.stringify({pump:this.state.pump,water:world.gridWaterRegions[0]?.gridWater?.state}));}''')
    script = script.replace('  if(input.pressed(\'reset\'))scenes.reload();', '''  if(input.pressed('reset'))scenes.reload();
  for(const [key,path] of [['pool','pool'],['rotated','rotated'],['radial','radial'],['original','planet-basin']])
   if(input.pressed(key))scenes.load('Scenes/'+path+'.judas');
  if(input.pressed('save'))this.request=saves.save('water-workshop');
  if(input.pressed('load'))this.request=saves.load('water-workshop');
  if(input.pressed('control_toggle'))this.state.pump=!this.state.pump;
  if(input.pressed('interact')){
   if(this.held){this.held=null;this.entity.character.ignore([]);}
   else{const p=this.entity.transform,rotation=qm(qm(p.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));
    const eye=add(p.position,rotate(p.rotation,{x:0,y:.65,z:0}));
    const hit=physics.raycast(eye,rotate(rotation,{x:0,y:0,z:-1}),5);
    if(hit?.entity?.motionType==='dynamic'){this.held=hit.entity;this.entity.character.ignore([this.held]);}}
  }
  if(input.pressed('throw')&&this.held?.valid){
   const p=this.entity.transform,r=qm(qm(p.rotation,axis({x:0,y:1,z:0},this.state.yaw)),axis({x:1,y:0,z:0},this.state.pitch));
   this.held.applyImpulseAtPoint(mul(rotate(r,{x:0,y:0,z:-1}),this.held.mass*6),this.held.transform.position);
   this.held=null;this.entity.character.ignore([]);
  }''')
    script = script.replace(' start(){', ' start(){this.held=null;this.entity.character?.ignore([]);')
    script = script.replace('  motor.velocity=velocity;', '''  motor.velocity=velocity;
  const owner=world.gridWaterRegions[0],waterHandle=owner?.gridWater;
  if(waterHandle){const frame=owner.transform;
   const point=v=>add(frame.position,rotate(frame.rotation,v));
   if(this.state.pump){waterHandle.changeVolume(point({x:-3,y:2.6,z:-1}),.15*dt,.8);
    waterHandle.changeVolume(point({x:3,y:.8,z:0}),-.15*dt,.8);}
   if(input.pressed('add_water'))waterHandle.impulse(point({x:0,y:1.9,z:2}),rotate(frame.rotation,{x:0,y:65,z:0}),1.2);
  }
  if(this.held?.valid){
   const rotation=qm(heading,axis({x:1,y:0,z:0},this.state.pitch));
   const target=add(add(pose.position,mul(up,.45)),rotate(rotation,{x:0,y:0,z:-2.2}));
   const change=add(mul(add(target,mul(this.held.transform.position,-1)),45),mul(this.held.velocity,-11));
   this.held.applyForce(mul(add(change,mul(this.held.gravity.acceleration,-1)),this.held.mass));
  }''')
    script = script.replace('  this.hud.get(\'counter\').text=`Swimming immersion: ${Math.round(this.immersion*100)}%\\nBasic water / no particle or flow components authored`;', '''  const s=world.gridWaterRegions[0]?.gridWater?.state;
  let text=s?`Water ${s.quantity.toFixed(3)} m³ | stored ${s.storedDisplacement.toFixed(3)} m³ | swim ${Math.round(this.immersion*100)}%\\n${s.simulationMs.toFixed(2)} ms incl. geometry/body loads | pump ${this.state.pump?'ON':'OFF'}\\n${s.error||'Grid owns quantity; mesh is presentation only.'}`:'Advanced Fluids locked or disabled; use an Experimental source build.';
  if(this.request){const r=saves.status(this.request);text+='\\nSave/load: '+r?.state+' '+(r?.error||'');}
  this.hud.get('counter').text=text;''')
    script = script.replace('destroy(){input.pointerCapture=false;', 'destroy(){if(this.entity.valid)this.entity.character.ignore([]);input.pointerCapture=false;')
    (DEST / 'Assets/scripts/player.js').write_text(script)
    shutil.copyfile(SOURCE / 'Assets/scripts/player.js.judasmeta', DEST / 'Assets/scripts/player.js.judasmeta')
    ui = (DEST / 'Assets/ui/menu.judasui').read_text()
    ui = ui.replace('24 24 500 220', '24 24 640 270').replace('470 70', '610 92').replace('470 60', '610 82')
    (DEST / 'Assets/ui/menu.judasui').write_text(ui)
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
    # Preserve the original radial scene's actual planet, basin walls, stairs
    # and three props. Only its old particle seed and legacy player/HUD are
    # replaced in this owned copy. The source project is never edited.
    original=(ROOT/'projects/fluid_demo/Scenes/planet.judas').read_text()
    objects=[b for b in re.findall(r'^object .*?^end',original,re.M|re.S)
             if 'fluid-volume' not in b and not re.match(r'object (67|68) ',b)]
    header=original[:original.index('object 1')].replace('next-id 69','next-id 100')
    header=re.sub(r'name ".*?"','name "Water V2 / original planet basin"',header,count=1)
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
