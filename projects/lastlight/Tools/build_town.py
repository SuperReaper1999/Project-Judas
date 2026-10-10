#!/usr/bin/env python3
"""Reproducible content authoring only. Never changes Judas engine source."""
from pathlib import Path
import json,hashlib,shutil,math
P=Path(__file__).resolve().parents[1]; A=P/'Assets'; ids=json.loads((A/'ids.json').read_text())
def track(path,kind):
 id=hashlib.md5(('lastlight/'+str(path.relative_to(P))).encode()).hexdigest();path.with_name(path.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{id}"\ntype {kind}\nsource ""\n');ids[path.stem]=id;return id
for path in (A/'scripts').glob('*.js'):track(path,'script')
for name in ['character_fighter','fist_forearm','zombie_animation','rocket_launcher','grenade','rocket']:track(A/'models'/f'{name}.glb','mesh')
for path in (A/'audio').glob('*.wav'):track(path,'audio')
from trip_rig import ragdoll_fields
from build_breakables import author_assets, scene_entity
from ui_shop import augment_ui
from ui_inventory import augment_inventory_ui
from ordnance_prefabs import author_ordnance
from build_framewalk_world import author_framewalk_assets, author_framewalk_world
source=P.parents[1]/'projects/shooter_game/Assets'
shutil.copy2(source/'textures/soft.png',A/'textures/soft.png');track(A/'textures/soft.png','texture')
FONT=ids['DejaVuSans'];fontold='be36a149b9918581ce406f3632b79cab'
lines=(source/'ui/range.judasui').read_text().splitlines();out=[]
for line in lines[2:]:
 if line.startswith('"language"'):continue
 # Strip localization bindings inherited from Spring Range.
 import re
 line=re.sub(r'"range\.[^"]*"','""',line).replace(fontold,FONT)
 line=line.replace('JUDAS / SPRING RANGE','LASTLIGHT / TOWN DEFENCE').replace('RANGE PAUSED','LASTLIGHT PAUSED')
 line=line.replace('Clear every plate once. Hits score again after the hinge settles.','Hold the neighbourhood against zombie waves. Enter houses and move crates to fortify doorways.')
 line=line.replace('WASD / left stick: move   Mouse / right stick: aim   LMB / RT: fire   Space / A: jump   V / Y: view   R: restart   Esc: pause','WASD move | Ctrl aim | LMB fire | RMB punch | F heavy | Q shove | Space jump | E mantle | B carry/drop | T turn prop | 1/2 weapons | G grenade | N shop | I inventory | C collect | X gravity reset | V view | R reload | Esc menu')
 out.append(line)
# Hit vignette is a top/bottom red strip; never blocks input.
hurt=next(l for l in out if l.startswith('"cross_h"')).replace('"cross_h"','"hurt"').replace('0.5 0.5 0.5 0.5 -7 -1 14 2','0 0 1 0 0 0 0 8').replace('0.9 0.96 1 0.9','0.8 0.08 0.06 0.8')
out.append(hurt);out=augment_ui(out,FONT);out=augment_inventory_ui(out,FONT)
(A/'ui/lastlight.judasui').write_text('JudasUI 2\n1280 720 1 1 0 '+str(len(out))+'\n'+'\n'.join(out)+'\n');track(A/'ui/lastlight.judasui','ui')
def transform(pos=(0,0,0),scale=(1,1,1),yaw=0):return f'  position {" ".join(map(str,pos))}\n  rotation {math.cos(yaw/2)} 0 {math.sin(yaw/2)} 0\n  scale {" ".join(map(str,scale))}\n'
def render(shape='box',half=(.5,.5,.5),color=(1,1,1),asset=''):
 return f'  render {shape}\n  render.half-extents {" ".join(map(str,half))}\n  render.radius .5\n  render.color {" ".join(map(str,color))}\n  render.alpha 1\n  render.secondary-color .8 .8 .8\n  render.secondary-alpha 1\n  render.mesh-asset "{asset}"\n  render.texture-asset ""\n'
def body(half,kind='static',mass=10):return f'  body {kind} box\n  body.half-extents {" ".join(map(str,half))}\n  body.radius .5\n  body.terrain ""\n  body.mass {mass}\n  body.friction .7\n  body.restitution .1\n  body.initial-velocity 0 0 0\n  body.pickable false\n  body.managed false\n  body.compound-count 0\n  body.fluid-cavity-count 0\n'
def script(name,props={}):return f'  scripts 1\n  script.0.id 1\n  script.0.asset "{ids[name]}"\n  script.0.enabled true\n  script.0.properties {json.dumps(json.dumps(props,separators=(",",":")))}\n'
def motor():return '''  motor.enabled true
  motor.radius .3
  motor.halfHeight .6
  motor.offset 0 .9 0
  motor.stepHeight .35
  motor.supportDistance .15
  motor.skin .02
  motor.maxSlopeDegrees 48
  motor.gravityScale 1
  motor.reorientationDegreesPerSecond 120
  motor.interactionMass 80
  motor.maxPushImpulse 15
  motor.collisionLayer 0
  motor.collisionMask 18446744073709551615
  motor.requiredTags 0
  motor.excludedTags 0
'''
def anim(clip='Idle'):return f'  animation.enabled true\n  animation.play-on-start true\n  animation.loop true\n  animation.clip "{clip}"\n  animation.speed 1\n  animation.time 0\n'
def audio(name,spatial=True):return f'''  audio-emitter
  audio.asset "{ids[name]}"
  audio.enabled true
  audio.play-on-start false
  audio.loop false
  audio.spatial {str(spatial).lower()}
  audio.volume .28
  audio.pitch 1
  audio.reference-distance 2
  audio.maximum-distance 45
  audio.rolloff 1
  audio.attenuation inverse
'''
header='''JudasScene 3
settings
  name "Lastlight"
  world-origin 0 0 0
  sun-direction -.4 .8 .3
  sun-color 2.0 1.65 1.35
  ambient .38 .43 .5
  background-color .34 .43 .54
  fluid-scale 13
  fluid-update-rate-hz 30
  fluid-hydrostatic-drag-rate 2
  fidelity-policy none
  next-id 2000
  linear-rendering true
  exposure 1
end
'''
author_assets(ids,track,header)
author_ordnance(ids,track,header)
author_framewalk_assets(ids,track,header)
# These ordinary dynamic props can be carried by the project's generic grab
# script. Pickable is authored data; no scene-specific runtime branch exists.
for kind in ['crate','barrel']:
 path=A/'prefabs'/f'breakable_{kind}.judasprefab'
 path.write_text(path.read_text().replace('  body.pickable false\n','  body.pickable true\n'))
for kind in ['soldier','zombie']:
 prefab=header+f'object 1 "{kind.title()}"\n'+transform()+motor()+script('enemy',{'kind':kind,**({'variant':'auto'} if kind=='zombie' else {})})+'''  tags 1
  nav.agent.enabled "1"
  nav.agent.avoidance "1"
  nav.agent.profile "0"
  nav.agent.areas "18446744073709551615"
  nav.agent.speed "2.8"
  nav.agent.arrival "0.8"
  nav.agent.cornerDistance "0.3"
  nav.agent.repathSeconds "0.6"
  nav.agent.costs ""
'''+(audio('shot') if kind=='soldier' else '')+'end\n'
 prefab+='object 2 "Animated visual"\n'+transform()+render('mesh',asset=ids['zombie_animation'] if kind=='zombie' else ids['character_'+kind])+anim('ZombieIdle' if kind=='zombie' else 'Idle')+(ragdoll_fields() if kind=='zombie' else '')+'  parent 1\nend\n'
 if kind=='soldier':prefab+='object 3 "Soldier rifle"\n'+transform((.26,1.05,.35),(.6,.6,.6))+render('mesh',asset=ids['weapon_compact_rifle'])+'  parent 2\nend\n'
 path=A/'prefabs'/f'{kind}.judasprefab';path.write_text(prefab);track(path,'prefab')
objects=[];counter=100
def obj(id,name,fields):objects.append(f'object {id} "{name}"\n'+fields+'end\n')
def box(name,pos,half,color,physical=True):
 global counter
 counter+=1;obj(counter,name,transform(pos)+render(half=half,color=color)+(body(half) if physical else ''));return counter
def prop(name,pos,yaw=0,scale=1,collision=None,dynamic=False):
 global counter
 counter+=1;obj(counter,name.replace('_',' ').title(),transform(pos,(scale,scale,scale),yaw)+render('mesh',asset=ids[name])+(body(collision,'dynamic' if dynamic else 'static',20)+'  tags 2\n' if collision else ''))
obj(1,'Neighbourhood foundation',transform((0,-.5,0))+render(half=(48,.5,54),color=(.2,.27,.25))+body((48,.5,54)))
obj(2,'Faithful gravity',transform()+'  gravity uniform 9.81\n  gravity.region sphere 180\n')
obj(10,'Town defender',transform((0,.08,18))+motor()+script('player',{'zombie':ids['zombie'],'rocket':ids['rocket_projectile'],'grenade':ids['grenade_projectile'],'framewalkBoots':ids['framewalk_boots_pickup'],'rocketsPickup':ids['rockets_pickup'],'grenadesPickup':ids['grenades_pickup']})+audio('rifle_fire',False)+f'  ui.asset "{ids["lastlight"]}"\n  ui.name "lastlight_ui"\n  ui.enabled true\n  audio-listener\n  listener.enabled true\n  listener.follow-view true\n')
obj(11,'Defender avatar',transform((0,0,18),yaw=math.pi)+render('mesh',asset=ids['character_fighter'])+anim())
obj(12,'Defender rifle',transform()+render('mesh',asset=ids['weapon_compact_rifle']))
for hand in [13,14]:obj(hand,'Combat fist '+str(hand),transform((0,-100,0))+render('mesh',asset=ids['fist_forearm'])+'  render-visible false\n')
obj(15,'Defender launcher',transform((0,-100,0))+render('mesh',asset=ids['rocket_launcher'])+'  render-visible false\n')
for id,c in [(20,(1,.55,.1)),(21,(1,.85,.4)),(22,(1,.18,.08))]:
 obj(id,'Feedback '+str(id),transform((0,-100,0))+f'''  particle-emitter
  particle.enabled true
  particle.loop false
  particle.local-space false
  particle.gravity false
  particle.rate 0
  particle.lifetime .18
  particle.size .07
  particle.end-size .01
  particle.burst 0
  particle.capacity 160
  particle.spread .04 .04 .04
  particle.velocity 0 .8 0
  particle.variation 1.8 1.8 1.8
  particle.acceleration 0 0 0
  particle.texture "{ids['soft']}"
  particle.color {' '.join(map(str,c))}
  particle.alpha 1
  particle.end-color .9 .1 .04
  particle.end-alpha 0
  particle.seed {id}
'''+(audio('impact') if id==20 else ''))
for id,name,spatial in [(23,'reload_click',False),(24,'melee_thud',True),(25,'empty_click',False),(26,'explosion',True)]:
 obj(id,name.replace('_',' ').title()+' cue',transform((0,-100,0))+audio(name,spatial))
# Streets and broad outer yards create genuine routes around the neighbourhood.
# The visual asphalt overlays the one continuous physical foundation.
box('Main street',(0,.006,0),(6,.006,49),(.13,.17,.21),False)
for z in [-39,-20,-2,17,37]:box('Neighbourhood cross street',(0,.009,z),(42,.006,2.4),(.13,.17,.21),False)
for x in [-28,28]:box('Back street',(x,.012,0),(3,.006,48),(.15,.2,.23),False)
for x in [-6.9,6.9]:box('Raised sidewalk',(x,.075,0),(.85,.075,48),(.5,.55,.59))
for z in range(-45,48,4):box('Road dash',(0,.018,z),(.08,.006,.8),(.83,.7,.4),False)

HOUSE_Z=[-29,-12,7,26]
def house(side,z,index):
 x=side*16;h=3.4;hx=4.6;hz=4.8;th=.14
 color=[(.66,.38,.25),(.27,.47,.52),(.71,.61,.39),(.5,.38,.48)][(index+(side==1))%4]
 front=x-side*hx;back=x+side*hx
 name=f'House {"west" if side<0 else "east"} {index+1}'
 # No house-sized solid body: floors, roof, wall strips and furniture are
 # separate ordinary colliders. The 1.9 m doorway is physically empty.
 box(name+' floor',(x,.045,z),(hx-.14,.045,hz-.14),(.47,.4,.3))
 box(name+' roof',(x,h+.12,z),(hx+.25,.12,hz+.25),(.18,.25,.3))
 for edge in [-1,1]:box(name+' side wall',(x,h/2,z+edge*hz),(hx,.5*h,th),color)
 # Street wall: centered open door and two open windows, with real sills.
 for lo,hi in [(-hz,-3.45),(-1.95,-.95),(.95,1.95),(3.45,hz)]:
  box(name+' front wall',(front,h/2,z+(lo+hi)/2),(th,h/2,(hi-lo)/2),color)
 box(name+' door lintel',(front,(2.35+h)/2,z),(th,(h-2.35)/2,.95),color)
 for wz in [-2.7,2.7]:
  box(name+' window sill wall',(front,.525,z+wz),(th,.525,.75),color)
  box(name+' window lintel',(front,(2.25+h)/2,z+wz),(th,(h-2.25)/2,.75),color)
  box(name+' window sill',(front-side*.08,1.065,z+wz),(.21,.045,.85),(.77,.71,.56))
 # The rear doorway opens into the yard; it is not a decorative locked door.
 for lo,hi in [(-hz,2.05),(3.95,hz)]:
  box(name+' rear wall',(back,h/2,z+(lo+hi)/2),(th,h/2,(hi-lo)/2),color)
 box(name+' rear door lintel',(back,(2.35+h)/2,z+3),(th,(h-2.35)/2,.95),color)
 for doorx,doorz in [(front,z),(back,z+3)]:
  for edge in [-1,1]:box(name+' door frame',(doorx,1.175,doorz+edge*1.015),(.19,1.175,.065),(.78,.73,.61),False)
  box(name+' door frame header',(doorx,2.405,doorz),(.19,.055,1.08),(.78,.73,.61),False)
 # Two rooms share an unobstructed central 1.9 m passage.
 for lo,hi in [(-hx,-.95),(.95,hx)]:
  box(name+' room partition',(x+(lo+hi)/2,h/2,z+1.2),((hi-lo)/2,h/2,.1),(.74,.71,.61))
 box(name+' room passage lintel',(x,(2.35+h)/2,z+1.2),(.95,(h-2.35)/2,.1),(.74,.71,.61))
 # Low bed/table/shelves provide interiors and useful defence cover without
 # cluttering either doorway or the central route through the rooms.
 bedx=x+side*2.3
 box(name+' bed frame',(bedx,.23,z+3.05),(1.1,.14,.85),(.27,.18,.12))
 box(name+' mattress',(bedx,.43,z+3.05),(1.08,.09,.83),(.54,.66,.61))
 box(name+' pillow',(bedx-side*.65,.55,z+3.05),(.31,.045,.65),(.81,.79,.67),False)
 box(name+' table top',(x-side*2.0,.82,z-2.15),(.95,.1,.7),(.43,.29,.16))
 for dx in [-.8,.8]:
  for dz in [-.52,.52]:box(name+' table leg',(x-side*2.0+dx,.405,z-2.15+dz),(.07,.315,.07),(.27,.18,.1))
 box(name+' cupboard',(x+side*3.7,.68,z-2.7),(.45,.59,.85),(.33,.29,.22))
 box(name+' cupboard doors',(x+side*3.19,.68,z-2.7),(.055,.53,.79),(.49,.38,.24),False)
 box(name+' rug',(x,.096,z-.5),(1.9,.006,1.4),(.34,.48,.5),False)
 prop('produce_crate',(x+side*3.6,.09,z-.7),yaw=side*math.pi/2,scale=.8)
 # Front and rear paths remain level with the exterior ground.
 box(name+' front path',(front-side*1.5,.014,z),(1.5,.008,1.15),(.43,.47,.46),False)
 box(name+' garden path',(back+side*3.7,.014,z+3),(3.7,.008,1.15),(.39,.44,.4),False)
 box(name+' porch awning',(front-side*.7,2.65,z),(.9,.08,1.3),(.2,.29,.32))
for side in [-1,1]:
 for i,z in enumerate(HOUSE_Z):house(side,z,i)
for x in [-8.5,8.5]:
 for z in [-39,-20,-2,17,37]:prop('street_lamp',(x,0,z),yaw=math.pi if x<0 else 0)
for x,z in [(-38,-39),(38,-38),(-38,-14),(38,-12),(-38,12),(45,12),(-38,40),(45,42),(-17,44),(17,44)]:prop('broadleaf_tree',(x,0,z),scale=1.35)
for x,z in [(-8,21),(8,21),(-8,-37),(8,-37)]:prop('bench',(x,0,z),yaw=math.pi)
prop('cloth_market_canopy',(-16,0,41),scale=1.1)
prop('produce_crate',(-15,0,42));prop('cloth_laundry_line',(24,0,5),yaw=math.pi/2)
prop('bus_stop_shelter',(-8.6,0,-12),yaw=math.pi/2)
prop('fire_hydrant',(8.4,0,17));prop('trash_bin',(-8.4,0,15));prop('planter',(8.4,0,-10));prop('utility_box',(-9,0,2))
# An ordinary tagged merchant assembly; the project player's vendor helper owns
# prices/menu behaviour. The merchant is not a special engine object.
obj(60,'Neighbourhood supply merchant',transform((-8.5,.9,21.1))+body((.3,.9,.3))+'  tags 8\n')
obj(61,'Merchant visual',transform((0,-.9,0))+render('mesh',asset=ids['character_adventurer'])+anim()+'  parent 60\n')
prop('cloth_market_canopy',(-8.5,0,20),scale=.85)
box('Supply counter',(-8.5,.4,19.6),(1.25,.4,.35),(.42,.28,.15))
box('Supply counter amber sign',(-8.5,.85,19.2),(1.05,.075,.04),(.85,.58,.15),False)
prop('produce_crate',(-7.7,.8,19.55),scale=.65)
for x,z in [(-4,9),(4,7),(-3,-11),(3,-18)]:
 box('Mantle hurdle',(x,.45,z),(1.2,.45,.45),(.28,.47,.44));prop('traffic_cone',(x+1.6,0,z))
box('Climbable delivery ledge',(4, .67,15),(1.6,.67,1.7),(.43,.48,.51))
box('Low delivery step',(6.5,.3,15),(1.6,.3,.7),(.4,.45,.48))
prop('dumpster',(34,.6,-3),collision=(.85,.6,.6));prop('metal_crate',(-34,.5,3),collision=(.6,.5,.5))
# Broad outer paths and low perimeter edging replace the earlier close walls.
# All houses can be approached from front, rear and the cross streets.
for pos,half in [((-48,.35,0),(.25,.35,54)),((48,.35,0),(.25,.35,54)),((0,.35,-54),(48,.35,.25)),((0,.35,54),(48,.35,.25))]:box('Neighbourhood edge',pos,half,(.2,.28,.31))
obj(50,'Town navigation',transform()+'''  nav.surface.asset ""
  nav.surface.cellHeight "0.1"
  nav.surface.cellSize "0.25"
  nav.surface.enabled "1"
  nav.surface.halfExtents "48 12 54"
  nav.surface.includeDynamic "0"
  nav.surface.minRegion "2"
  nav.surface.profile "0"
  nav.surface.simplification "1.3"
  nav.surface.sources "18446744073709551615"
  nav.surface.tileSize "32"
''')
def movable(id,kind,pos,yaw=0):objects.append(scene_entity(id,kind,pos,ids,yaw).replace('  body.pickable false\n','  body.pickable true\n'))
for id,kind,pos in [(700,'crate',(-3,.51,15)),(702,'barrel',(3,.55,11)),(704,'crate',(-4,.51,-7)),(706,'barrel',(4,.55,-14)),(708,'crate',(-23,.51,21)),(710,'barrel',(23,.55,21))]:movable(id,kind,pos)
# Every house has front/back movable fortification pieces. Their dimensions fit
# through the 1.9 m openings; scripts control intent while physics resolves motion.
next_breakable=712
for side in [-1,1]:
 for z in HOUSE_Z:
  movable(next_breakable,'crate',(side*9.6,.51,z+1.8),yaw=math.pi/2);next_breakable+=2
  movable(next_breakable,'barrel',(side*22,.55,z+3));next_breakable+=2
# Low actual obstacles make a shove trip easy to recognize near the plaza.
for x in [-2.8,2.8]:box('Low trip rail',(x,.25,4),(.85,.25,.18),(.37,.26,.18))
objects.extend(author_framewalk_world(obj,ids))
assert counter<700,'Ordinary town entity IDs overlap the stable breakable range'
(P/'Scenes/town.judas').write_text(header+'\n'.join(objects))
# Project-owned logical controls. These are content, not native input policy.
actions={'move_x':(1,[('key:A',-1),('key:D',1),('stick:LeftX',1)]),'move_y':(1,[('key:S',-1),('key:W',1),('stick:LeftY',-1)]),'look_x':(1,[('mouse:dx',1)]),'look_y':(1,[('mouse:dy',1)]),'look_stick_x':(1,[('stick:RightX',1)]),'look_stick_y':(1,[('stick:RightY',1)]),'fire':(0,[('mouse:Left',1),('stick:RightTrigger',1)]),'jump':(0,[('key:Space',1),('pad:South',1)]),'climb':(0,[('key:E',1),('pad:West',1)]),'sprint':(0,[('key:Left Shift',1)]),'camera_toggle':(0,[('key:V',1),('pad:North',1)]),'reload':(0,[('key:R',1)]),'punch':(0,[('mouse:Right',1)]),'heavy':(0,[('key:F',1)]),'shove':(0,[('key:Q',1)]),'interact':(0,[('key:B',1)]),'rotate':(0,[('key:T',1)]),'pause':(0,[('key:Escape',1),('pad:Start',1)]),'ui_click':(0,[('mouse:Left',1)]),'ui_activate':(0,[('key:Return',1),('pad:South',1)]),'ui_up':(0,[('key:Up',1)]),'ui_down':(0,[('key:Down',1)])}
actions.update({'aim':(0,[('key:Left Ctrl',1),('stick:LeftTrigger',1)]),
 'weapon_rifle':(0,[('key:1',1)]),'weapon_launcher':(0,[('key:2',1)]),
 'grenade':(0,[('key:G',1)]),'vendor':(0,[('key:N',1)]),
 'inventory':(0,[('key:I',1)]),'collect':(0,[('key:C',1)]),
 'gravity_reset':(0,[('key:X',1)])})
parts=['1',str(len(actions))]
for name,(axis,bindings) in actions.items():
 parts += [json.dumps(name),str(axis),str(len(bindings))]
 for control,scale in bindings:parts += [json.dumps(control),str(scale),'.15' if 'stick:' in control else '0']
project='JudasProject 1\nname "Lastlight"\nstartup-scene "Scenes/town.judas"\nassets-dir "Assets"\nscenes-dir "Scenes"\nsaves-dir "Saves"\nlegacy-gameplay "false"\ninput-map '+json.dumps(' '.join(parts))+'\n'
project+='classification '+json.dumps('JudasClassification 1 5 5 0 "enemy" 1 "physical" 2 "destructible" 3 "vendor" 4 "inventory_item" 1 1 0 "Default" 1 1 0 "Default" ')+'\n'
project+='navigation '+json.dumps('JudasNavigationProject 1 1 1 0 "Default" 1 1 0 "Default" 0.3 1.8 48 0.35')+'\n'
(P/'lastlight.judasproj').write_text(project);(A/'ids.json').write_text(json.dumps(ids,indent=2));print('Authored',len(objects),'town entities')
