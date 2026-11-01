#!/usr/bin/env python3
"""Rebuild Willow Town's original pixel art, scene and explicit physical map.

Only writes its own generated files under projects/willow_town. Gameplay source is
owned separately and is never overwritten. Requires Pillow; no downloaded assets.
"""
from pathlib import Path
import hashlib,json,math,random,runpy
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[1]
PROJECT=ROOT/'projects/willow_town'
ASSETS=PROJECT/'Assets'
SCALE=32
FONT_PATH='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
FONT_BOLD='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'

def asset_id(relative):return hashlib.sha256(('willow-town:'+relative).encode()).hexdigest()[:32]
def q(value):return json.dumps(value,ensure_ascii=False)
def meta(relative,kind):
    path=PROJECT/relative
    path.parent.mkdir(parents=True,exist_ok=True)
    Path(str(path)+'.judasmeta').write_text('JudasAssetMeta 1\nid '+q(asset_id(relative))+'\ntype '+kind+'\nsource "Original Willow Town / deterministic project authoring"\n')
def image(name,size):return Image.new('RGBA',size,(0,0,0,0))
def save(im,name):
    path=ASSETS/'Art'/name
    path.parent.mkdir(parents=True,exist_ok=True)
    im.save(path,optimize=False)
    meta('Assets/Art/'+name,'texture')
def font(size,bold=False):return ImageFont.truetype(FONT_BOLD if bold else FONT_PATH,size)
def label(draw,box,text,size=13):
    draw.rounded_rectangle(box,3,fill='#25473f',outline='#e6d8ac',width=2)
    draw.text(((box[0]+box[2])/2,(box[1]+box[3])/2),text,font=font(size,True),anchor='mm',fill='#fff2ce')
def rect(draw,box,fill,outline=None,width=1):draw.rectangle(tuple(int(v) for v in box),fill=fill,outline=outline,width=width)
def world(p):return ((p[0]+16)*SCALE,(11-p[1])*SCALE)

def background():
    im=Image.new('RGBA',(1024,704),'#7da36b');d=ImageDraw.Draw(im);rng=random.Random(8872)
    # Subtle meadow speckles stay subordinate to roads and buildings.
    for _ in range(4800):
        x,y=rng.randrange(1024),rng.randrange(704)
        d.line((x,y,x+2,y),fill=rng.choice(['#8caf76','#729960','#799e67']))
    def path(a,b,width=34):
        a,b=world(a),world(b)
        d.line((a,b),fill='#63865b',width=width+8)
        d.line((a,b),fill='#c8bd91',width=width)
        # Curb stones make the network legible at ordinary camera distances.
        if a[0]==b[0]:
            for y in range(int(min(a[1],b[1])),int(max(a[1],b[1])),16):
                d.line((a[0]-width/2,y,a[0]-width/2,y+11),fill='#e1d5ae',width=3)
                d.line((a[0]+width/2,y,a[0]+width/2,y+11),fill='#e1d5ae',width=3)
        else:
            for x in range(int(min(a[0],b[0])),int(max(a[0],b[0])),16):
                d.line((x,a[1]-width/2,x+11,a[1]-width/2),fill='#e1d5ae',width=3)
                d.line((x,a[1]+width/2,x+11,a[1]+width/2),fill='#e1d5ae',width=3)
    path((-14.8,0),(14.8,0),42)
    for x,end in [(-10,4),(9,4),(-10,-2.4),(0,-3.3),(10,-3)]:path((x,0),(x,end),35)
    path((-4,2),(4,2),27);path((0,0),(0,3),27)
    # Stone plaza and garden edging.
    for y in range(264,361,16):
        for x in range(366,659,24):
            d.rectangle((x+(8 if y%32 else 0),y,x+20+(8 if y%32 else 0),y+12),fill='#b8b9a0')
    # Border hedges/fences align with the invisible outer physical fence.
    for x in range(7,1024,24):
        d.rectangle((x,7,x+13,15),fill='#6a824c');d.rectangle((x,686,x+13,694),fill='#6a824c')
    for y in range(7,697,24):
        d.rectangle((7,y,15,y+13),fill='#6a824c');d.rectangle((1008,y,1016,y+13),fill='#6a824c')
    for p in [(-5.2,-1.2),(5.2,1.3),(-4.7,4),(4.1,4),(13.7,-2),(-14.7,-1.6)]:
        px,py=world(p)
        for j in range(7):
            fx=px+rng.randrange(-15,16);fy=py+rng.randrange(-7,8)
            d.rectangle((fx,fy,fx+2,fy+2),fill=rng.choice(['#f3d18b','#e5a6a0','#ece5be']))
    # Small decorative pond at the southeast edge, beyond normal job routes.
    d.ellipse((904,612,1003,671),fill='#527f76',outline='#b3b590',width=5)
    d.arc((914,622,995,662),190,350,fill='#8ab9a8',width=2)
    save(im,'town-ground.png')

def house(name,wall,roof):
    im=image(name,(256,180));d=ImageDraw.Draw(im)
    d.rectangle((2,4,254,178),fill='#475b4933')
    # Cutaway interiors: roof band at the back, every room remains visible.
    d.rectangle((6,18,250,174),fill='#c5ab79',outline='#6d624b',width=4)
    for y in range(23,172,12):
        d.line((9,y,246,y),fill='#b69b6c',width=1)
        for x in range(10+(y%24)*2,249,38):d.line((x,y,x,y+11),fill='#b69b6c')
    d.rectangle((3,4,253,24),fill=roof,outline='#4b5650',width=3)
    for x in range(10,250,18):d.line((x,6,x+5,21),fill='#ffffff29',width=2)
    d.rectangle((3,25,12,176),fill=wall,outline='#6f6b55',width=2)
    d.rectangle((244,25,253,176),fill=wall,outline='#6f6b55',width=2)
    d.rectangle((4,166,98,178),fill=wall,outline='#6f6b55',width=2)
    d.rectangle((158,166,252,178),fill=wall,outline='#6f6b55',width=2)
    # Beds behind the unobstructed rest targets (y7.2).
    for i,x in enumerate([37,101,165]):
        d.rectangle((x,38,x+48,66),fill='#4b5852',outline='#7b6d50',width=3)
        d.rectangle((x+4,42,x+44,62),fill=['#91acb2','#a99ab1','#c9ad80'][i])
        d.rectangle((x+7,43,x+15,61),fill='#f0e2c3')
        d.line((x+30,43,x+30,60),fill='#ffffff5c',width=2)
    # Dining table, rug and a little kitchen, all away from doors/routes.
    d.rectangle((22,110,68,140),fill='#678a75',outline='#b3bb8f',width=3)
    d.rectangle((188,112,230,143),fill='#8b6645',outline='#64543f',width=2)
    d.rectangle((196,120,202,126),fill='#e6d7b4');d.ellipse((212,121,220,127),fill='#c57354')
    label(d,(76,8,179,28),'WILLOW HOUSE' if name=='house-west.png' else 'MEADOW HOUSE',10)
    save(im,name)

def shop():
    im=image('shop.png',(256,205));d=ImageDraw.Draw(im)
    d.rectangle((5,4,251,199),fill='#cbb782',outline='#5e6555',width=4)
    for y in range(12,194,16):
        for x in range(10,249,16):d.rectangle((x,y,x+13,y+13),fill='#d8c99c' if (x//16+y//16)%2 else '#cab887')
    # Open north-facing entry, then side/back walls.
    d.rectangle((3,4,95,15),fill='#7b8b71',outline='#465b4c',width=2);d.rectangle((159,4,253,15),fill='#7b8b71',outline='#465b4c',width=2)
    d.rectangle((3,15,12,199),fill='#809477',outline='#465b4c',width=2)
    d.rectangle((244,15,253,199),fill='#809477',outline='#465b4c',width=2)
    d.rectangle((4,190,252,202),fill='#809477',outline='#465b4c',width=2)
    # Grocer counter and baker oven line the rear; work locations are in front.
    d.rectangle((28,157,225,186),fill='#8d6547',outline='#4d5140',width=3)
    for x in [36,62,88]:
        d.rectangle((x,163,x+20,180),fill='#bb9261',outline='#715839')
        for dx,dy in [(3,3),(10,5),(4,10),(12,11)]:d.ellipse((x+dx,164+dy,x+dx+5,169+dy),fill='#b75f48')
    d.rectangle((173,152,220,185),fill='#6e7570',outline='#46524b',width=3)
    d.rectangle((182,160,211,180),fill='#4a4840');d.arc((184,159,209,181),180,360,fill='#dfb477',width=3)
    d.rectangle((17,38,38,108),fill='#8c6b46',outline='#514e3f',width=2)
    for y in [43,64,85]:
        for x in [20,29]:d.rectangle((x,y,x+5,y+12),fill=['#ba7650','#91a15b','#d5ad69'][(y//21)%3])
    # Small shop-floor displays leave the queues and central aisle clear.
    label(d,(54,113,202,137),'WILLOW GROCER',12)
    d.text((128,145),'bread  /  fresh produce',font=font(8),fill='#4e654e',anchor='mm')
    save(im,'shop.png')

def workshop():
    im=image('workshop.png',(192,176));d=ImageDraw.Draw(im)
    d.rectangle((4,4,188,170),fill='#b9a477',outline='#575d4d',width=3)
    for y in range(10,166,10):d.line((7,y,185,y),fill='#a58e65')
    d.rectangle((3,3,62,15),fill='#8d6a4a',outline='#4e5141',width=2);d.rectangle((130,3,189,15),fill='#8d6a4a',outline='#4e5141',width=2)
    d.rectangle((3,15,11,171),fill='#9b7b54',outline='#4e5141',width=2);d.rectangle((181,15,189,171),fill='#9b7b54',outline='#4e5141',width=2)
    d.rectangle((4,163,188,173),fill='#9b7b54',outline='#4e5141',width=2)
    d.rectangle((20,119,173,150),fill='#8a6240',outline='#4a4e3d',width=3)
    d.rectangle((30,127,69,131),fill='#e0bf7f');d.rectangle((76,132,110,139),fill='#6d7974')
    d.rectangle((95,126,99,146),fill='#c69a60');d.rectangle((119,126,142,140),fill='#d8b67b')
    for y in [40,53,66]:d.rectangle((24,y,50,y+6),fill='#a78551',outline='#685b40')
    label(d,(37,30,155,54),'WORKSHOP',12)
    save(im,'workshop.png')

def garden():
    im=image('garden.png',(240,208));d=ImageDraw.Draw(im)
    d.rounded_rectangle((3,3,237,205),10,fill='#78945b',outline='#aeb67f',width=4)
    for cx in [53,120,187]:
        for cy in [90,151]:
            d.rectangle((cx-24,cy-19,cx+24,cy+19),fill='#806747',outline='#ad9565',width=3)
            for x in range(cx-16,cx+18,12):
                for y in range(cy-11,cy+12,11):
                    d.ellipse((x-4,y-3,x+4,y+3),fill='#acc37b')
                    d.line((x,y-4,x,y+4),fill='#4c7c50',width=2)
                    if (x+y)%3==0:d.rectangle((x,y,x+2,y+2),fill='#ce8e58')
    label(d,(47,14,193,41),'ALLOTMENTS',13)
    save(im,'garden.png')

def tree():
    im=image('tree.png',(64,80));d=ImageDraw.Draw(im)
    d.ellipse((6,54,59,77),fill='#40564050')
    d.rectangle((27,45,37,72),fill='#735940');d.rectangle((29,48,32,68),fill='#a27c51')
    for box,col in [((4,20,57,58),'#385f49'),((10,5,61,47),'#527f56'),((5,11,44,43),'#608f5d'),((17,2,55,33),'#6f9c63')]:d.ellipse(box,fill=col)
    for x,y in [(16,20),(29,10),(41,28),(14,37)]:d.rectangle((x,y,x+7,y+3),fill='#8aac6b')
    save(im,'tree.png')

def bench():
    im=image('bench.png',(64,32));d=ImageDraw.Draw(im)
    d.ellipse((2,11,63,30),fill='#40564038')
    d.rectangle((8,10,13,28),fill='#4f5b50');d.rectangle((51,10,56,28),fill='#4f5b50')
    for y in [4,10,16]:d.rectangle((4,y,60,y+4),fill='#aa8052',outline='#6d5a3f')
    save(im,'bench.png')

def fountain():
    im=image('fountain.png',(80,80));d=ImageDraw.Draw(im)
    d.ellipse((3,6,77,78),fill='#67796b');d.ellipse((6,5,74,73),fill='#c5c4aa',outline='#667b70',width=3)
    d.ellipse((15,14,65,63),fill='#5c9692',outline='#d5d4b6',width=3)
    d.arc((20,19,61,60),20,160,fill='#9ac8b8',width=2);d.arc((18,17,59,58),180,320,fill='#9ac8b8',width=2)
    d.rectangle((35,31,45,53),fill='#c0c9b0');d.ellipse((29,23,51,39),fill='#ddd8b9',outline='#718f85',width=2)
    d.line((40,21,40,12),fill='#b5e1d0',width=3)
    save(im,'fountain.png')

def selection_ring():
    im=image('ring.png',(64,32));d=ImageDraw.Draw(im)
    d.ellipse((3,5,61,27),fill='#e5de8a24',outline='#f5edaa',width=3)
    d.arc((7,8,57,24),195,320,fill='#a39c56',width=2)
    save(im,'ring.png')

def citizens():
    coats=['#c67e58','#6387b0','#86a664','#be9e61','#a382ac','#679e99']
    for index,coat in enumerate(coats):
        im=image('citizen',(96,32));d=ImageDraw.Draw(im)
        for frame in range(4):
            x=frame*24;sway=[0,1,0,-1][frame]
            d.ellipse((x+5,26,x+20,30),fill='#344b4a58')
            # Two leg phases and warm outlined faces work at ordinary zoom.
            d.rectangle((x+8,21,x+11,28-(frame%2)),fill='#344b55')
            d.rectangle((x+14,21,x+17,27+(frame%2)),fill='#344b55')
            d.rectangle((x+6,12+sway,x+18,22+sway),fill=coat,outline='#44514d')
            d.rectangle((x+4,14-sway,x+6,22-sway),fill='#d6b18a');d.rectangle((x+18,14+sway,x+20,22+sway),fill='#d6b18a')
            d.rectangle((x+8,4,x+17,12),fill='#dcba96',outline='#5c534b')
            d.rectangle((x+7,3,x+18,6),fill=['#745a46','#574e49','#8c6a45','#b5a078','#5b514e','#93734c'][index])
            d.point((x+10,8),fill='#35444a');d.point((x+15,8),fill='#35444a')
            if index==0:d.rectangle((x+8,1,x+17,4),fill='#eee1bd')
            if index==1:d.rectangle((x+6,4,x+19,5),fill='#bd9a63')
            if index==2:d.rectangle((x+6,4,x+19,5),fill='#d4be7c')
            if index==3:d.rectangle((x+9,13,x+15,21),fill='#e4d3a9')
            if index==4:d.rectangle((x+8,14,x+11,22),fill='#785745')
        save(im,f'resident-{index+1}.png')


def generate():
    ASSETS.mkdir(parents=True,exist_ok=True);(PROJECT/'Scenes').mkdir(exist_ok=True)
    background();house('house-west.png','#b7b493','#a16f55');house('house-east.png','#c4bf9d','#678c82')
    shop();workshop();garden();tree();bench();fountain();selection_ring();citizens()
    points={str(k):{'x':x,'y':y} for k,x,y in [
        (1101,-8,-6),(1102,0,-6),(1103,10,-5),(1104,-12,-6),(1105,0,0),(1106,10,-1.5),
        (1201,-12,7.2),(1202,-10,7.2),(1203,-8,7.2),(1204,7,7.2),(1205,9,7.2),(1206,11,7.2),
        (1301,-11,-4.3),(1302,-9,-4.3),(1401,-3,2),(1402,3,2),
        (1501,-3,-2),(1502,3,-2),(1503,-4.5,2.5),(1504,4.5,2.5)]}
    spawns=[[-4.5,.8],[-1.5,-.6],[1.5,.8],[4.5,-.6],[-.5,1.7],[7,.6]]
    blocks=[]
    def block(name,x,y,w,h):blocks.append({'name':name,'x':x,'y':y,'w':w,'h':h})
    for centre in [-10,9]:
        block('House back wall',centre,9.5,8,.2)
        block('House west wall',centre-3.9,6.8,.2,5.6)
        block('House east wall',centre+3.9,6.8,.2,5.6)
        for dx in [-2.45,2.45]:block('House entry wall',centre+dx,4.1,3.1,.2)
        for dx in [-2,0,2]:block('Bed',centre+dx,8.2,1.5,.8)
        block('Dining table',centre+2.5,5.4,1.25,.8)
    for name,cx,l,r,b,t in [('Shop',-10,-14,-6,-8.8,-2.4),('Workshop',0,-3,3,-8.8,-3.3)]:
        block(name+' back wall',cx,b+.1,r-l,.2)
        block(name+' west wall',l+.1,(b+t)/2,.2,t-b)
        block(name+' east wall',r-.1,(b+t)/2,.2,t-b)
        w=(r-l-1.8)/2
        block(name+' entry wall',l+w/2,t-.1,w,.2)
        block(name+' entry wall',r-w/2,t-.1,w,.2)
    block('Grocer and bakery counters',-10,-7.7,6.2,.9)
    block('Grocer side shelves',-13.1,-4.7,.7,2.2)
    block('Carpentry bench',0,-7.6,4.8,.9)
    block('Town fountain',0,3,1.85,1.85)
    for x in [-3,3]:block('Park bench',x,3.1,1.8,.45)
    block('West boundary',-15.8,0,.2,21.6);block('East boundary',15.8,0,.2,21.6)
    block('North boundary',0,10.8,31.6,.2);block('South boundary',0,-10.8,31.6,.2)
    trees=[(-14.8,.9),(-14.8,2.8),(-4.5,8.6),(-1.5,8.8),(2,8.5),(14.5,8.5),(14.5,1.8),(14.4,-.7),(14.4,-9.4),(4.3,-9.5),(-4.6,-9.5),(-14.7,-9.5)]
    for x,y in trees:block('Tree trunk',x,y,.32,.32)
    layout={'version':1,'bounds':{'minX':-15.6,'maxX':15.6,'minY':-10.6,'maxY':10.6},'points':points,'spawns':spawns,'obstacles':blocks}
    (ASSETS/'layout.json').write_text(json.dumps(layout,indent=2)+'\n')
    (ASSETS/'layout.js').write_text('// Generated physical layout. See tools/CreateWillowTown.py.\nexport default '+json.dumps(layout,separators=(',',':'))+';\n')
    meta('Assets/layout.js','script')
    for f,typ in [('game.js','script'),('town.js','script'),('citizen.ai.js','script'),('hud.judasui','ui')]:meta('Assets/'+f,typ)
    scene=['JudasScene 3','settings','  name "Willow Town - a little life simulation"','  world-origin 0 0 0','  sun-direction 0.4 0.7 0.35','  sun-color 1 1 1','  ambient 1 1 1','  sun-enabled false','  background-color 0.14 0.22 0.19','  fluid-scale 1','  fluid-update-rate-hz 30','  fluid-hydrostatic-drag-rate 2','  fidelity-policy none','  next-id 3000','end','']
    def obj(oid,name,x=0,y=0,texture=None,size=(1,1),layer=-2,ysort=False,pivot=(.5,.5),frame=(0,0,1,1),body=None,extra=()):
        lines=['object '+str(oid)+' '+q(name),f'  position {x} {y} 0','  rotation 1 0 0 0','  scale 1 1 1']
        lines+=list(extra)
        if texture:
            sprite={'version':1,'size':list(size),'pivot':list(pivot),'rect':list(frame),'tint':[1,1,1,1],'flipX':False,'flipY':False,'nearest':True,'pixelSnap':False,'alphaPick':True,'ySort':ysort,'sortingLayer':layer,'order':0}
            lines+=['  render sprite','  render.half-extents 0.5 0.5 0.5','  render.radius 0.5',
                    '  render.color 1 1 1','  render.alpha 1','  render.secondary-color 0.8 0.8 0.8',
                    '  render.secondary-alpha 1','  render.mesh-asset ""',
                    '  render.texture-asset '+q(asset_id('Assets/Art/'+texture)),
                    '  render.sprite-v1 '+q(json.dumps(sprite,separators=(',',':')))]
        if body:
            mode,w,h=body
            planar={'version':1,'kind':1,'radius':0,'lockRotation':True,'vertices':[[-w/2,-h/2],[w/2,-h/2],[w/2,h/2],[-w/2,h/2]]}
            lines+=['  body '+mode+' planar','  body.planar-v1 '+q(json.dumps(planar,separators=(',',':'))),'  body.half-extents 0.5 0.5 0.5','  body.radius 0.5','  body.terrain ""','  body.mass 1','  body.friction 0','  body.restitution 0','  body.initial-velocity 0 0 0','  body.pickable false','  body.managed false','  body.compound-count 0','  body.fluid-cavity-count 0']
        lines+=['end',''];scene.extend(lines)
    obj(1,'Town simulation - Assets/game.js',extra=['  scripts 1','  script.0.id 1','  script.0.asset '+q(asset_id('Assets/game.js')),'  script.0.enabled true','  script.0.properties "{}"'])
    obj(2,'Town interface',extra=['  ui.asset '+q(asset_id('Assets/hud.judasui')),'  ui.name "willow_town"','  ui.enabled true'])
    obj(10,'Meadow, streets and town green',texture='town-ground.png',size=(32,22),layer=-10)
    obj(11,'Willow House - open interior',-10,6.8,'house-west.png',(8,5.625),-3)
    obj(12,'Meadow House - open interior',9,6.8,'house-east.png',(8,5.625),-3)
    obj(13,'Willow Grocer and bakery - open interior',-10,-5.6,'shop.png',(8,6.40625),-3)
    obj(14,'Carpenter workshop - open interior',0,-6.05,'workshop.png',(6,5.5),-3)
    obj(15,'Community allotments',10,-5.6,'garden.png',(7.5,6.5),-3)
    obj(16,'Town fountain',0,3,'fountain.png',(2.5,2.5),0)
    obj(17,'West park bench',-3,3.1,'bench.png',(2,1),0)
    obj(18,'East park bench',3,3.1,'bench.png',(2,1),0)
    for i,(x,y) in enumerate(trees):obj(30+i,'Willow tree',x,y,'tree.png',(2,2.5),1,True,(.5,.15))
    for i,name in enumerate(['Mara - Baker','Eli - Carpenter','Nia - Gardener','Theo - Grocer','Ada - Courier','Sam - Park keeper']):
        obj(301+i,name,*spawns[i],f'resident-{i+1}.png',(.96,1.28),1,True,(.5,.2),(0,0,.25,1),('kinematic',.48,.48))
    obj(500,'Selected resident halo',*spawns[0],'ring.png',(1.4,.7),0)
    targetnames={1101:'Bakery oven work spot',1102:'Carpentry work spot',1103:'Vegetable garden work spot',1104:'Grocery stock work spot',1105:'Delivery hub work spot',1106:'Park work spot',1301:'Shop west serving spot',1302:'Shop east serving spot',1401:'West social spot',1402:'East social spot'}
    for oid,p in points.items():obj(int(oid),targetnames.get(int(oid),'Home bed' if oid.startswith('12') else 'Town walk destination'),p['x'],p['y'])
    for i,b in enumerate(blocks):obj(2000+i,b['name'],b['x'],b['y'],body=('static',b['w'],b['h']))
    (PROJECT/'Scenes/main.judas').write_text('\n'.join(scene))
    bindings=[('pause',0,[('key:Space',1)]),('release',0,[('key:Escape',1)]),('save_game',0,[('key:K',1)]),('load_game',0,[('key:L',1)]),('ui_click',0,[('mouse:Left',1)]),('select',0,[('mouse:Left',1)]),('ui_next',0,[('key:Tab',1)]),('ui_activate',0,[('key:Return',1)]),('move_x',1,[('key:A',-1),('key:D',1),('key:Left',-1),('key:Right',1)]),('move_y',1,[('key:S',-1),('key:W',1),('key:Down',-1),('key:Up',1)])]
    encoded='1 '+str(len(bindings))+' '
    for name,kind,controls in bindings:
        encoded+=q(name)+' '+str(kind)+' '+str(len(controls))+' '
        for control,scale in controls:encoded+=q(control)+' '+str(scale)+' 0 '
    project='\n'.join(['JudasProject 1','name "Willow Town"','editor-2d "true"','basic-water "true"','water-swimming "true"','advanced-fluid-dynamics "false"','startup-scene "Scenes/main.judas"','assets-dir "Assets"','scenes-dir "Scenes"','saves-dir "Saves"','save-identity '+q(hashlib.sha256(b'WillowTown-v1').hexdigest()),'legacy-gameplay "false"','input-map '+q(encoded),''])
    (PROJECT/'Willow_Town.judasproj').write_text(project)
    (PROJECT/'ASSET_LICENSE.txt').write_text('Willow Town original pixel art\nCopyright (c) 2026 Conner / SuperReaper1999\n\nAll images in Assets/Art are original deterministic artwork produced by\ntools/CreateWillowTown.py, under the Project Judas MIT license. No external\nmodels, textures or game assets were downloaded. Source art generation is\npreserved in that tool. Pillow and DejaVu Sans (including its Bold variant)\nare authoring dependencies only. DejaVu is distributed separately under its\npermissive font license: https://dejavu-fonts.github.io/License.html . No font\nfiles or Python dependencies are redistributed or required by the running game.\n')
    ui=ROOT/'tools/CreateWillowTownUI.py'
    if ui.exists():runpy.run_path(str(ui),run_name='__main__')
    print(PROJECT/'Willow_Town.judasproj')
    print('Game asset',asset_id('Assets/game.js'),'HUD asset',asset_id('Assets/hud.judasui'))
    print('Physical obstacles',len(blocks),'explicit destinations',len(points))
if __name__=='__main__':generate()
