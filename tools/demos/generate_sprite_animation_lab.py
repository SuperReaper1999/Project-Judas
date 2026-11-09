#!/usr/bin/env python3
"""Generate the original sprite-workshop art and sheet, without a historical game dependency."""
import json, pathlib, shutil, struct, zlib, argparse
root=pathlib.Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('destination',nargs='?',default=str(root/'.cache/sprite-art'))
out=pathlib.Path(parser.parse_args().destination).resolve()
if out.exists(): raise SystemExit('destination must be fresh; preserve edited content')
out.mkdir(parents=True)
assets=out
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
