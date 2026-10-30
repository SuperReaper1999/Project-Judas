#!/usr/bin/env python3
"""Original deterministic pixel art and ordinary Stage 3 consumer content."""
import json, pathlib, shutil, struct, zlib, argparse
root=pathlib.Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('destination',nargs='?',default=str(root/'projects/sprite_animation_workshop'))
out=pathlib.Path(parser.parse_args().destination).resolve()
if out.exists(): raise SystemExit('destination must be fresh; preserve edited content')
shutil.copytree(root/'projects/planar_playground',out)
assets=out/'Assets'
W,H,CW=240,48,24
pixels=bytearray(W*H*4)
def rect(frame,x,y,w,h,c):
    for yy in range(max(0,y),min(H,y+h)):
        for xx in range(max(0,x),min(CW,x+w)):
            n=(yy*W+frame*CW+xx)*4; pixels[n:n+4]=bytes(c)
skin=(237,185,118,255); coat=(42,164,154,255); dark=(29,43,67,255); glove=(247,205,89,255)
for i in range(10):
    bob=1 if i in (1,3,5) else 0
    rect(i,8,6+bob,9,10,skin);rect(i,7,4+bob,10,4,(113,76,52,255));rect(i,14,9+bob,2,2,dark)
    rect(i,7,17+bob,11,15,coat);rect(i,7,17+bob,3,15,(28,117,121,255));rect(i,11,18+bob,2,8,glove)
    stride=[0,0,-3,0,3,0,2,0,1,0][i]
    rect(i,8+stride,32+bob,4,10-bob,dark);rect(i,14-stride,32+bob,4,10-bob,dark)
    rect(i,7+stride,41,6,3,dark);rect(i,13-stride,41,6,3,dark)
    if i==6: rect(i,4,9,3,13,skin);rect(i,18,9,3,13,skin)
    elif i in (8,9):rect(i,15,19,8,4,skin);rect(i,21,18,3,6,glove)
    else:rect(i,4,18+bob,3,13,skin);rect(i,18,18+bob,3,13,skin)
def chunk(tag,b):return struct.pack('!I',len(b))+tag+b+struct.pack('!I',zlib.crc32(tag+b)&0xffffffff)
raw=b''.join(b'\0'+pixels[y*W*4:(y+1)*W*4] for y in range(H))
(assets/'traveller.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',W,H,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
texture='33333333333333333333333333333331';sheetid='33333333333333333333333333333332'
def meta(path,id,kind): (assets/(path+'.judasmeta')).write_text(f'JudasAssetMeta 1\nid "{id}"\ntype {kind}\nsource "Original Stage 3 authoring; tools/demos/generate_sprite_animation_lab.py"\n')
meta('traveller.png',texture,'texture')
frames=[{'id':f'cell-0-{i}','name':name,'pixels':[i*CW,0,CW,H],'pivot':[.5,.5]} for i,name in enumerate(['Idle A','Idle B','Run left','Run centre A','Run right','Run centre B','Jump','Wind-up','Strike','Recover'])]
def clip(name,indices,times,events=[]):return {'name':name,'frames':[{'frame':f'cell-0-{i}','seconds':t} for i,t in zip(indices,times)],'events':events}
sheet={'version':1,'texture':texture,'width':W,'height':H,'pixelsPerUnit':32,'frames':frames,'clips':[clip('idle',[0,1],[.4,.6]),clip('run',[2,3,4,5],[.12,.08,.12,.08],[{'id':'left-step','name':'footstep','time':.08,'data':'left'},{'id':'right-step','name':'footstep','time':.28,'data':'right'}]),clip('jump',[6],[.5]),clip('interact',[7,8,9],[.15,.12,.23],[{'id':'strike','name':'strike','time':.18,'data':'forward-impulse'}])]}
(assets/'traveller.judassprite').write_text(json.dumps(sheet,indent=2)+'\n');meta('traveller.judassprite',sheetid,'spriteSheet')
for path in (out/'Scenes').glob('*.judas'):
    text=path.read_text()
    # Only player object 1 owns this optional animation. Never change other colliders.
    begin=text.index('\nobject 1 ');end=text.index('\nend',begin)
    actor=text[begin:end]
    playback={'version':1,'sheet':sheetid,'clip':'idle','frame':'','time':0,'speed':1,'playing':True,'loop':True}
    actor+='\n  render.sprite-playback-v1 '+json.dumps(json.dumps(playback,separators=(',',':')))
    text=text[:begin]+actor+text[end:]
    if path.name=="main.judas": text=text.replace('object 197 "Pushable crate"\n  position -3 0.2 0','object 197 "Timed strike practice crate"\n  position 1.1 0.2 0')
    path.write_text(text)
project=out/'Planar_Playground.judasproj';text=project.read_text().replace('Planar Playground','Sprite Animation Workshop').replace('5cc913378464386e7cde4a9a56f393b473f7d49ece01e277ac02a20cbceda809','42cb82afc9f2383fec62c982a94bef9fc1f54eec8de28324acef315cc666bc0d')
lines=text.splitlines()
for i,line in enumerate(lines):
    if line.startswith('input-map '):
        mapping=json.loads(line[len('input-map '):]);mapping=mapping.replace('1 18 ','1 19 ',1)+'"interact" 0 1 "key:E" 1 0 ';lines[i]='input-map '+json.dumps(mapping)
(out/'Sprite_Animation_Workshop.judasproj').write_text('\n'.join(lines)+'\n');project.unlink()
shutil.copyfile(assets/'game.js',assets/'player-motion.js');meta('player-motion.js','33333333333333333333333333333333','script')
(assets/'game.js').write_text('''import Motion from './player-motion.js';
import {input,physics} from 'judas';
export {properties} from './player-motion.js';
export default class extends Motion {
 constructor(context){super(context);this.state.strikes=0;this.state.impacts=0;this.state.footsteps=0;}
 fixedUpdate(dt){
  super.fixedUpdate(dt);
  const a=this.entity.spriteAnimation;if(!a||a.error)throw Error(a?a.error:'sprite sheet unavailable');if(!a.ready)return;
  // Each fixed callback reads the previous completed sample once. Rendering and
  // editor previews never run this code or dispatch these gameplay decisions.
  for(const event of a.events){
   if(event.name==='footstep')this.state.footsteps++;
   if(event.name==='strike'){
    this.state.strikes++;
    const pose=this.entity.transform,q=pose.rotation,sign=this.entity.sprite.flipX?-1:1;
    const forward={x:sign*(1-2*(q.y*q.y+q.z*q.z)),y:sign*2*(q.x*q.y+q.w*q.z),z:sign*2*(q.x*q.z-q.w*q.y)};
    const hit=physics.cast2D({kind:'circle',radius:.2},pose,forward,1.6,{ignored:[this.entity]});
    if(hit&&hit.entity&&hit.entity.mass>0){hit.entity.applyImpulse({x:forward.x*6,y:forward.y*6,z:forward.z*6});this.state.impacts++;}
   }
  }
  if(input.pressed('interact')&&(a.clip!=='interact'||a.finished)){
    this.entity.setSpriteAnimation({clip:'interact',time:0,playing:true,loop:false});return;
  }
  if(a.clip==='interact'&&!a.finished)return;
  const moving=Math.abs(input.axis('move_x'))+Math.abs(input.axis('move_y'))>.05;
  const clip=this.properties.sideView&&!this.state.grounded?'jump':moving?'run':'idle';
  if(a.clip!==clip||!a.playing)this.entity.setSpriteAnimation({clip,time:0,playing:true,loop:clip!=='jump'});
 }
}
''')
pg=assets/'playground.js';pg.write_text(pg.read_text().replace('PLANAR PLAYGROUND','SPRITE ANIMATION WORKSHOP').replace('F6/F7: save/load','E: timed interaction | F6/F7: save/load'))
(out/'README.md').write_text('''# Sprite Animation Workshop — Stage 3

Open `Sprite_Animation_Workshop.judasproj`. WASD/arrows move, Space jumps,
E plays an interaction whose strike marker applies a real impulse to a nearby
physical object. 1/2/3 switch platformer/top-down/rotated scenes; R restarts;
F6/F7 save/load. Idle/run/jump/interaction selection is ordinary project JS.

Right-click `traveller.png` → Create/edit sprite sheet, or double-click
`traveller.judassprite`. Slice cells 24×48. Select named frames, edit clips,
variable durations and event markers. Preview/scrub is isolated from gameplay.
Save the asset explicitly; reopen Play to inspect changes. The sheet is project
content and its PNG is an explicit export dependency. No skeleton is fabricated.

Source art and reproducible generation: `tools/demos/generate_sprite_animation_lab.py`.
This is Stage 3 content, not a release or a human-acceptance claim.
''')
print(out)
