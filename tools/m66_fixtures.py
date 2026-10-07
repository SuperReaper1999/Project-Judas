"""Original redistributable numerical import fixtures, never a source converter."""
from pathlib import Path
import base64,json,struct
root=Path('tests/fixtures/m66');root.mkdir(parents=True,exist_ok=True)
binary=bytearray();views=[];accessors=[]
def accessor(values,typ,component=5126):
    while len(binary)%4:binary.append(0)
    start=len(binary);fmt={5126:'f',5123:'H',5125:'I'}[component]
    flat=[x for value in values for x in (value if isinstance(value,(tuple,list)) else [value])]
    binary.extend(struct.pack('<'+fmt*len(flat),*flat));views.append({'buffer':0,'byteOffset':start,'byteLength':len(binary)-start})
    accessors.append({'bufferView':len(views)-1,'componentType':component,'count':len(values),'type':typ});return len(accessors)-1
positions=[];joints=[];weights=[]
for bone in range(256):
    x=bone%16*.12-1;y=bone//16*.12
    for p in [(x,y,0),(x+.09,y,0),(x,y+.09,0)]:positions.append(p);joints.append([bone,0,0,0]);weights.append([1,0,0,0])
# Dedicated triangle uses eight distinct, nonzero high-index influences.
for p in [(-.4,-.5,0),(.4,-.5,0),(0,.3,0)]:positions.append(p);joints.append([248,249,250,251]);weights.append([.125]*4)
second=[[0]*4 for _ in range(768)]+[[252,253,254,255]]*3
weight2=[[0]*4 for _ in range(768)]+[[.125]*4]*3
p=accessor(positions,'VEC3');n=accessor([[0,0,1]]*len(positions),'VEC3');j=accessor(joints,'VEC4',5123);w=accessor(weights,'VEC4');j1=accessor(second,'VEC4',5123);w1=accessor(weight2,'VEC4')
uv=accessor([[0,0],[1,0],[0,1]]*(len(positions)//3),'VEC2');uv1=accessor([[1,1],[0,1],[1,0]]*(len(positions)//3),'VEC2')
identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];ibm=accessor([identity]*256,'MAT4')
time=accessor([0,1],'SCALAR');translation=accessor([[0,0,0],[.5,0,0]],'VEC3')
nodes=[{'name':'Rig','children':list(range(1,257))}]+[{'name':f'Joint{i:03d}'} for i in range(256)]+[{'name':'All256','mesh':0,'skin':0}]
attrs={'POSITION':p,'NORMAL':n,'TEXCOORD_0':uv,'TEXCOORD_1':uv1,'JOINTS_0':j,'WEIGHTS_0':w,'JOINTS_1':j1,'WEIGHTS_1':w1}
doc={'asset':{'version':'2.0','generator':'Judas original M66 numerical fixture'},'scene':0,'scenes':[{'nodes':[0,257]}],'nodes':nodes,'skins':[{'name':'Full256','joints':list(range(1,257)),'inverseBindMatrices':ibm}],'meshes':[{'name':'weighted-grid','primitives':[{'attributes':attrs,'material':0}]}],'materials':[{'name':'Fixture','doubleSided':True,'pbrMetallicRoughness':{'baseColorFactor':[.8,.3,.1,1],'metallicFactor':0}}],'animations':[{'name':'Translate','samplers':[{'input':time,'output':translation,'interpolation':'LINEAR'}],'channels':[{'sampler':0,'target':{'node':i,'path':'translation'}} for i in range(1,257)]}],'buffers':[{'byteLength':len(binary),'uri':'data:application/octet-stream;base64,'+base64.b64encode(binary).decode()}],'bufferViews':views,'accessors':accessors}
(root/'capacity256.gltf').write_text(json.dumps(doc,separators=(',',':'))+'\n')
# Two skins with distinct inverse binds, a static mirrored/sheared attachment,
# and a second independent rig root. Source mesh-node transforms on skins
# deliberately differ: glTF's skin pose, not that transform, is authoritative.
small=json.loads(json.dumps(doc));small['nodes']=[{'name':'Shared','children':[1,2]},{'name':'BoneA'},{'name':'BoneB'},{'name':'SkinA','mesh':0,'skin':0,'translation':[9,0,0]},{'name':'SkinB','mesh':0,'skin':1,'translation':[-9,0,0]},{'name':'OtherRig','children':[6]},{'name':'OtherBone'},{'name':'OtherSkin','mesh':0,'skin':2},{'name':'MirroredAttachment','mesh':1,'matrix':[-2,0,0,0,.25,1,0,0,0,0,1,0,2,0,0,1]}]
small['scenes']=[{'nodes':[0,3,4,5,7,8]}];small['skins']=[{'joints':[1,2]},{'joints':[2,1]},{'joints':[6]}]
# Separate small attributes, legal joints for all skins.
positionsA=accessor([[0,0,0],[1,0,0],[0,1,0]],'VEC3');jointA=accessor([[0,0,0,0]]*3,'VEC4',5123);weightsA=accessor([[1,0,0,0]]*3,'VEC4');normA=accessor([[0,0,1]]*3,'VEC3');uvA=accessor([[0,0],[1,0],[0,1]],'VEC2');bindA=accessor([identity,identity],'MAT4');b=identity.copy();b[12]=2;bindB=accessor([b,identity],'MAT4');small['skins'][0]['inverseBindMatrices']=bindA;small['skins'][1]['inverseBindMatrices']=bindB
small['meshes']=[{'name':'skinned','primitives':[{'attributes':{'POSITION':positionsA,'NORMAL':normA,'TEXCOORD_0':uvA,'TEXCOORD_1':uvA,'JOINTS_0':jointA,'WEIGHTS_0':weightsA},'material':0}]},{'name':'static','primitives':[{'attributes':{'POSITION':positionsA,'NORMAL':normA,'TEXCOORD_0':uvA},'material':0}]}]
small['animations']=[{'name':'Translate','samplers':[{'input':time,'output':translation}],'channels':[{'sampler':0,'target':{'node':1,'path':'translation'}}]}];small['bufferViews']=views;small['accessors']=accessors;small['buffers']=[{'byteLength':len(binary),'uri':'data:application/octet-stream;base64,'+base64.b64encode(binary).decode()}]
(root/'multipart.gltf').write_text(json.dumps(small,separators=(',',':'))+'\n')
# External buffers/images including a Unicode filename and encoded space.
external=json.loads(json.dumps(small));external['buffers'][0]['uri']='geometry.bin';(root/'geometry.bin').write_bytes(binary)
image=base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/ScLbtAAAAABJRU5ErkJggg==');(root/'café color.png').write_bytes(image)
external['images']=[{'uri':'café%20color.png'}];external['textures']=[{'source':0,'sampler':0}];external['samplers']=[{'wrapS':33071,'wrapT':33648,'minFilter':9728,'magFilter':9728}];external['materials'][0]['pbrMetallicRoughness']['baseColorTexture']={'index':0,'texCoord':1,'extensions':{'KHR_texture_transform':{'offset':[.1,.2],'scale':[-1,1],'rotation':.2}}};external['extensionsRequired']=['KHR_texture_transform'];external['meshes'][1]['primitives'][0]['attributes']['TEXCOORD_1']=uvA
(root/'external.gltf').write_text(json.dumps(external,separators=(',',':'))+'\n')
print('Authored capacity, multipart, external dependency fixtures')
