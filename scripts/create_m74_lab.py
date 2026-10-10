#!/usr/bin/env python3
"""Original CC0 sources and an ordinary project, cooked by the shipped service.

This generates fixture art, not a retarget implementation. The engine owns all
correspondence, sampling, cooking and publication. Never overwrites edited art
without an explicit --overwrite. Blender is optional for the FBX entry control.
"""
import argparse, base64, copy, hashlib, json, math, struct, subprocess
from pathlib import Path
from create_m70_lab import make_source

ROOT = Path(__file__).resolve().parents[1]
ROLES = ['Pivot','Vertebra0','Vertebra1','NeckConnector','Cranial',
         'ShoulderA','ElbowA','PalmA','ShoulderB','ElbowB','PalmB',
         'ThighA','ShinA','SoleA','ThighB','ShinB','SoleB']

def mul(a,b):
    x,y,z,w=a; X,Y,Z,W=b
    return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
def inv(q): return [-q[0],-q[1],-q[2],q[3]]
def rot(axis,angle): return [v*math.sin(angle/2) for v in axis]+[math.cos(angle/2)]
def vec(q,v): return mul(mul(q,v+[0]),inv(q))[:3]
def matrix(q,p):
    cols=[vec(q,[1,0,0]),vec(q,[0,1,0]),vec(q,[0,0,1])]
    return [*cols[0],0,*cols[1],0,*cols[2],0,*p,1]
def append(doc,blob,values,kind):
    while len(blob)%4: blob.append(0)
    start=len(blob);flat=[x for row in values for x in (row if isinstance(row,list) else [row])]
    blob.extend(struct.pack('<'+'f'*len(flat),*flat))
    doc['bufferViews'].append({'buffer':0,'byteOffset':start,'byteLength':len(blob)-start})
    doc['accessors'].append({'bufferView':len(doc['bufferViews'])-1,'componentType':5126,'count':len(values),'type':kind})
    return len(doc['accessors'])-1

def source(path,variant):
    names=make_source(path,variant!=0);doc=json.loads(path.read_text())
    blob=bytearray(base64.b64decode(doc['buffers'][0]['uri'].split(',')[1]));nodes=doc['nodes']
    parents=[-1]*len(nodes)
    for i,n in enumerate(nodes):
        for c in n.get('children',[]): parents[c]=i
    positions=[]
    for i,n in enumerate(nodes):
        p=n.get('translation',[0,0,0]);positions.append([p[k]+(positions[parents[i]][k] if parents[i]>=0 else 0) for k in range(3)])
    roles={key:next(i for i,n in enumerate(nodes) if n['name']==name) for key,name in names.items()}
    original=copy.deepcopy(positions)
    # The broad target has a visibly T-style reference; the compact one retains
    # an A pose and a different roll basis. Meshes/binds follow their own rig.
    if variant==1:
        for side in ['A','B']:
            shoulder=roles['Shoulder'+side];elbow=roles['Elbow'+side];palm=roles['Palm'+side]
            positions[elbow][1]=positions[shoulder][1];shift=positions[shoulder][1]-positions[palm][1]
            for key,i in roles.items():
                if key=='Palm'+side or key.startswith('Digit'+side): positions[i][1]+=shift
    if variant==2:
        for p in positions[:65]: p[0]*=.77;p[1]*=1.23
    global_q=[rot([0,0,1],(.42 if i%2 else -.31)) if variant else [0,0,0,1] for i in range(len(nodes))]
    if variant==2: global_q=[mul(q,rot([1,0,0],.23*(i%3-1))) for i,q in enumerate(global_q)]
    for i,n in enumerate(nodes[:65]):
        par=parents[i];n['translation']=vec(inv(global_q[par]),[positions[i][k]-positions[par][k] for k in range(3)]) if par>=0 else positions[i]
        n['rotation']=mul(inv(global_q[par]),global_q[i]) if par>=0 else global_q[i]
    # Update only original generated fixture geometry and inverse binds. Imported
    # target content is subsequently immutable throughout all engine transfers.
    for mesh in doc['meshes']:
        attrs=mesh['primitives'][0]['attributes'];pa=doc['accessors'][attrs['POSITION']];ja=doc['accessors'][attrs['JOINTS_0']]
        pv=doc['bufferViews'][pa['bufferView']];jv=doc['bufferViews'][ja['bufferView']]
        skin=doc['skins'][next(n['skin'] for n in nodes if n.get('mesh')==doc['meshes'].index(mesh))]
        for v in range(pa['count']):
            off=pv['byteOffset']+12*v;j=struct.unpack_from('<H',blob,jv['byteOffset']+8*v)[0];bone=skin['joints'][j]
            p=list(struct.unpack_from('<fff',blob,off));delta=[positions[bone][k]-original[bone][k] for k in range(3)]
            if variant==2:
                p[0]=original[bone][0]+(p[0]-original[bone][0])*.77;p[1]=original[bone][1]+(p[1]-original[bone][1])*1.23
            struct.pack_into('<fff',blob,off,*[p[k]+delta[k] for k in range(3)])
    for skin in doc['skins']:
        a=doc['accessors'][skin['inverseBindMatrices']];v=doc['bufferViews'][a['bufferView']]
        for ordinal,i in enumerate(skin['joints']):
            q=inv(global_q[i]);p=vec(q,[-x for x in positions[i]])
            struct.pack_into('<'+'f'*16,blob,v['byteOffset']+64*ordinal,*matrix(q,p))
    # Different total hierarchy counts and intervening, UNMAPPED target helpers.
    if variant:
        pairs=[('Vertebra0','Pivot')]+([('ElbowA','ShoulderA'),('ShinB','ThighB')] if variant==1 else [])
        for child,parent in pairs:
            ci,pi=roles[child],roles[parent];helper=len(nodes)
            nodes.append({'name':'RetainedSpacer'+str(helper),'children':[ci]})
            nodes[pi]['children']=[helper if x==ci else x for x in nodes[pi]['children']]
    times=append(doc,blob,[0,.5,1,1.5,2],'SCALAR')
    def clip(name,tracks):
        channels=[];samplers=[]
        for role,channel,values in tracks:
            samplers.append({'input':times,'output':append(doc,blob,values,'VEC3' if channel=='translation' else 'VEC4'),'interpolation':'LINEAR'})
            channels.append({'sampler':len(samplers)-1,'target':{'node':roles[role],'path':channel}})
        return {'name':name,'channels':channels,'samplers':samplers}
    if variant:
        base=nodes[roles['Vertebra0']]['rotation']
        doc['animations']=[clip('Native',[('Vertebra0','rotation',[mul(base,rot([0,1,0],a)) for a in [0,.18,0,-.18,0]])])]
    else:
        pivot=nodes[roles['Pivot']]['translation']
        doc['animations']=[
            clip('Travel',[('Pivot','translation',[[pivot[0]+x,pivot[1]+y,pivot[2]+z] for x,y,z in [(0,0,0),(.35,.04,-.25),(.8,0,-.6),(1.2,.04,-.95),(1.6,0,-1.3)]]),('Pivot','rotation',[rot([0,1,0],a) for a in [0,.2,.4,.6,.8]])]),
            clip('Wave',[('OriginHelper','rotation',[rot([0,1,0],a) for a in [0,.12,.22,.12,0]]),('ShoulderA','rotation',[rot([0,0,1],a) for a in [0,-.4,-.7,-.4,0]]),('ElbowA','rotation',[rot([0,1,0],a) for a in [0,.4,-.5,.4,0]])]),
            clip('Bob',[('Pivot','translation',[[pivot[0],pivot[1]+y,pivot[2]] for y in [0,.12,.22,.12,0]])])]
    doc['buffers'][0]={'byteLength':len(blob),'uri':'data:application/octet-stream;base64,'+base64.b64encode(blob).decode()}
    path.write_text(json.dumps(doc,separators=(',',':'))+'\n')
    return {r:names[r] for r in ROLES}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--project',default='projects/retarget_lab');ap.add_argument('--sources-only',action='store_true');ap.add_argument('--overwrite',action='store_true');args=ap.parse_args()
    project=ROOT/args.project
    if (project/'retarget_lab.judasproj').exists() and not args.overwrite: raise SystemExit('Refusing to overwrite edited lab: choose fresh --project or --overwrite')
    for d in ['Sources','Imports','Assets/models','Assets/scripts','Assets/ui','Assets/fonts','Assets/prefabs','Scenes','Authoring']: (project/d).mkdir(parents=True,exist_ok=True)
    roles={n:source(project/'Sources'/(n+'.gltf'),v) for n,v in [('source',0),('broad',1),('tall',2)]}
    (project/'Sources/LICENSE.txt').write_text('Original M74 fixture geometry and motion: CC0-1.0. Derived only from original CC0 M70 fixture generator. No third-party game art.\n')
    (project/'Authoring/roles.json').write_text(json.dumps(roles,indent=2)+'\n')
    if args.sources_only: print(project);return
    assemble(project,roles)

def assemble(project,roles):
    """Filled after shared-service build; no offline transfer math lives here."""
    import shutil
    cli=ROOT/'build/judas_model_import_cli';author=ROOT/'build/judas_scene_author'
    def run(tool,*args):
        p=subprocess.run([str(tool),*map(str,args)],cwd=ROOT,text=True,capture_output=True)
        if p.returncode not in (0,2): raise RuntimeError(p.stdout+p.stderr)
        return p.stdout
    config=project/'retarget_lab.judasproj'
    run(author,'--create','project',config,'--overwrite')
    def write(p,d): p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n')
    def patch(kind,p,d):
        e=project/'Authoring/edit.json';write(e,[{'op':'replace','path':'/data','value':d}]);run(author,'--patch',kind,p,p,e,'--overwrite')
    settings=json.loads(config.read_text())['data'];settings.update(name='Retarget Lab',legacyGameplay=False,startupScene='Scenes/lab.judas',exportScenes=['Scenes/lab.judas','Scenes/region.judas'],saveIdentity=hashlib.sha256(b'M74 original lab').hexdigest())
    for name,key in [('travel','1'),('wave','2'),('bob','3'),('native','4'),('crossfade','G'),('layer','L'),('ik','I'),('physical','P'),('clock','Space'),('seek','Right'),('root','M'),('region','T'),('save','F6'),('load','F7'),('reset','R')]:
        settings['input']=[i for i in settings['input'] if i['name']!=name];settings['input'].append({'name':name,'axis':False,'bindings':[{'control':'key:'+key,'scale':1,'deadzone':0}]})
    patch('project',config,settings)
    recipes={};inspects={};ids={}
    for name in roles:
        existing=project/'Imports'/(name+'.judasimport')
        created={'recipe':str(existing)} if existing.exists() else json.loads(run(cli,'--create',project,project/'Sources'/(name+'.gltf'),'Assets/models/'+name+'.judasmodel'))
        rp=Path(created['recipe']);recipe=json.loads(rp.read_text())
        # The lab already owns canonical source art; remove only the duplicate
        # copied by this Create operation after redirecting its recipe.
        copied=project/recipe['source']
        recipe['source']='Sources/'+name+'.gltf';recipe['assetId']=hashlib.md5(('M74/'+name).encode()).hexdigest();recipe.pop('motions',None);write(rp,recipe)
        if copied != project/recipe['source'] and copied.is_file():
            copied.unlink()
            if not any(copied.parent.iterdir()): copied.parent.rmdir()
        run(cli,'--recipe',rp);recipes[name]=rp;ids[name]=recipe['assetId'];inspects[name]=json.loads(run(cli,'--inspect',project/'Assets/models'/(name+'.judasmodel')))
    keys={n:{r:next(k for k in inspects[n]['joints'] if k.split('/')[-1]==leaf) for r,leaf in roles[n].items()} for n in roles}
    for n in ['broad','tall']:
        profile={'format':'JudasRetarget','version':1,'sourceIdentity':'Sources/source.gltf','targetIdentity':ids[n],'sourceSignature':json.loads(run(cli,'--inspect-motion',project/'Sources/source.gltf'))['skeletonSignature'],'targetSignature':inspects[n]['skeletonSignature'],'modelAlignment':[0,0,0,1],'translationScale':1.2 if n=='tall' else 1,'mapping':[{'source':keys['source'][r],'target':keys[n][r]} for r in ROLES],'translationJoints':[keys[n]['Pivot']],'sourceReference':[],'targetReference':[]}
        # Explicit static local reference alignment, not a bind-pose edit.
        if n=='broad':
            doc=json.loads((project/'Sources/broad.gltf').read_text());parents={c:i for i,node in enumerate(doc['nodes']) for c in node.get('children',[])}
            def global_q(i):
                q=doc['nodes'][i].get('rotation',[0,0,0,1]);return mul(global_q(parents[i]),q) if i in parents else q
            for r,angle in [('ShoulderA',.55),('ShoulderB',-.55)]:
                i=next(i for i,node in enumerate(doc['nodes']) if node['name']==roles[n][r]);q=global_q(i)
                profile['targetReference'].append({'joint':keys[n][r],'rotation':mul(mul(inv(q),rot([0,0,1],angle)),q)})
        pp=project/'Imports'/(n+'.judasretarget');write(pp,profile)
        recipe=json.loads(recipes[n].read_text());recipe['motions']=[]
        for clip in ['Wave','Bob','Travel']:
            names=[('TravelPreserve','preserve'),('TravelExtract','extract'),('TravelInPlace','inPlace')] if clip=='Travel' else [(clip,None)]
            for name,policy in names:
                m={'source':'Sources/source.gltf','take':clip,'name':name,'retargetProfile':'Imports/'+pp.name,'sampleRate':60}
                if policy: m['rootMotion']={'policy':policy,'node':keys[n]['Pivot'],'translation':[True,False,True],'rotationAxis':[0,1,0]}
                recipe['motions'].append(m)
        write(recipes[n],recipe);run(cli,'--recipe',recipes[n])
    write(project/'Authoring/joint-keys.json',keys)
    # Assets already have normal cooked sidecars. Existing project scripts are
    # copied only when generating a separate owned disposable reproduction.
    for name in ['lab.js','math.js']:
        original=ROOT/'projects/retarget_lab/Assets/scripts'/name;dest=project/'Assets/scripts'/name
        if original.resolve()!=dest.resolve(): shutil.copy2(original,dest)
    font=project/'Assets/fonts/DejaVuSans.ttf';shutil.copy2(ROOT/'assets/fonts/DejaVuSans.ttf',font);shutil.copy2(ROOT/'assets/fonts/DejaVuSans-LICENSE.txt',font.parent/'LICENSE.txt')
    def track(p):
        identity=hashlib.md5(('M74/'+str(p.relative_to(project))).encode()).hexdigest();run(author,'--track-asset',config,p,identity);return identity
    fontid=track(font);scriptid=track(project/'Assets/scripts/lab.js');track(project/'Assets/scripts/math.js')
    ui=project/'Assets/ui/lab.judasui';run(author,'--create','ui',ui,'--overwrite');ud=json.loads(ui.read_text())['data'];base=ud['elements'][0];base.update(size=[0,0],anchorMax=[1,1]);ud['elements']=[base]
    def element(id,kind,text,offset,size,parent='canvas',**kwargs):
        e=copy.deepcopy(base);e.update(id=id,parent=parent,kind=kind,anchorMax=[0,0],size=size,offset=offset,text=text,font=fontid,fontSize=18,background=[0,0,0,0]);e.update(kwargs);ud['elements'].append(e)
    element('title',2,'RETARGET LAB / BORROWED MOTION',[18,12],[1200,32],fontSize=24)
    element('help',2,'1 Travel | 2 Wave | 3 Bob | 4 Native | G crossfade | L left-arm layer | I shared IK | P physical handoff\nSPACE pause clips | RIGHT seek | M root policy | T suspend/revisit region | F6/F7 save/load | R reload | ESC menu',[18,50],[1220,72],wrap=True)
    element('status',2,'Loading ordinary baked models…',[18,127],[1200,70],wrap=True)
    element('pause',1,'',[0,0],[380,170],anchorMin=[.5,.5],anchorMax=[.5,.5],align=[.5,.5],visible=False,flow=2,padding=[15,15,15,15],background=[.06,.09,.14,.95])
    element('resume',4,'RESUME',[0,0],[350,55],parent='pause',background=[.2,.4,.5,1]);element('reload',4,'RELOAD LAB',[0,0],[350,55],parent='pause',background=[.2,.4,.5,1]);patch('ui',ui,ud);uiid=track(ui)
    def template(components,name):
        p=project/'Authoring/template.json';run(author,'--object-template',components,name,p,'--overwrite');return json.loads(p.read_text())['data']['objects'][0]
    path=project/'Scenes/lab.judas';run(author,'--create','scene',path,'--overwrite');sd=json.loads(path.read_text())['data'];sd['settings']['name']='Borrowed Moves';sd['settings']['next-id']=1000
    def obj(id,components,name,pos,fields=None):
        o=template(components,name);o['id']=id;o['fields']['position']=pos;o['fields'].update(fields or {});sd['objects'].append(o)
    obj(1,'render,body','Ordinary floor',[0,-.15,0],{'render.half-extents':[12,.15,8],'body.half-extents':[12,.15,8],'render.color':[.12,.16,.2]})
    obj(2,'gravity','Uniform lab gravity',[0,0,0],{'gravity':{'kind':{'symbol':'uniform'},'magnitude':9.81},'gravity.region':{'shape':{'symbol':'box'},'halfExtents':[100,100,100]}})
    for id,x,n in [(10,-3.2,'source'),(11,0,'broad'),(12,3.2,'tall'),(13,5.4,'broad')]:
        obj(id,'render,animation',n+' independent instance',[x,0,0 if id!=13 else -2.5],{'render':{'symbol':'mesh'},'render.mesh-asset':ids[n],'animation.clip':'Wave' if n!='source' else 'Wave','render.color':[1,1,1]})
    obj(30,'render','Resolved left palm socket',[0,0,0],{'render.half-extents':[.055,.055,.14],'render.color':[1,.67,.2],'socket.target':11,'socket.joint':keys['broad']['PalmA'],'socket.enabled':True,'socket.position':[0,0,0],'socket.rotation':[0,0,0,1],'socket.scale':[1,1,1]})
    obj(90,'empty','Ordinary JS lab controls',[0,0,0],{'scripts':1,'script.0.id':1,'script.0.asset':scriptid,'script.0.enabled':True,'script.0.properties':{'json':{'keysJson':json.dumps(keys,separators=(',',':'))}},'ui.asset':uiid,'ui.name':'retarget_lab','ui.enabled':True})
    patch('scene',path,sd)
    chain=[keys['broad'][r] for r in ['Vertebra0','Vertebra1','ElbowA','PalmA']]
    run(author,'--fit-skeleton',path,config,11,','.join(chain),.06,path,'--overwrite')
    # Same immutable baked target also travels through the ordinary region path.
    region=project/'Scenes/region.judas';run(author,'--create','scene',region,'--overwrite');rd=json.loads(region.read_text())['data'];rd['settings']['name']='Baked region';rd['objects']=[copy.deepcopy(next(o for o in sd['objects'] if o['id']==13))];rd['objects'][0]['id']=1;rd['objects'][0]['fields']['position']=[-5,0,-2];patch('scene',region,rd)
    prefab=project/'Assets/prefabs/broad.judasprefab';run(author,'--create','prefab',prefab,'--overwrite');pd=json.loads(prefab.read_text())['data'];pd['objects']=[copy.deepcopy(next(o for o in sd['objects'] if o['id']==13))];pd['objects'][0]['id']=1;pd['objects'][0]['fields']['position']=[0,0,0];patch('prefab',prefab,pd);prefabid=track(prefab)
    # World manifest fields are populated using the normal create template.
    world=project/'Assets/world/lab.judasworld';world.parent.mkdir(parents=True,exist_ok=True);run(author,'--create','world',world,'--overwrite');wd=json.loads(world.read_text())['data'];wd['regions']=[{'id':'rig','scene':'Scenes/region.judas','origin':[0,0,0],'rotation':[0,0,0,1],'halfExtents':[10,5,10],'priority':1,'policy':'snapshot','estimatedBytes':65536,'dependencies':[]}];patch('world',world,wd);worldid=track(world)
    final=json.loads(path.read_text())['data'];settings['worldManifest']=worldid;patch('project',config,settings)
    controller=next(o for o in final['objects'] if o['id']==90);props=json.loads(controller['fields']['script.0.properties']);props['prefab']=prefabid;controller['fields']['script.0.properties']={'json':props};patch('scene',path,final)
    for p in ['edit.json','template.json']:(project/'Authoring'/p).unlink(missing_ok=True)
    print(config)

if __name__=='__main__': main()
