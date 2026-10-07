#!/usr/bin/env python3
"""Original CC0 multipart articulated authoring fixture in commodity glTF.
This makes source art, not scene/physics records or a replacement importer.
"""
import base64,json,struct,sys
from pathlib import Path
out=Path(sys.argv[1]);out.parent.mkdir(parents=True,exist_ok=True)
blob=bytearray();views=[];accessors=[]
def accessor(data,kind,component=5126):
 while len(blob)%4:blob.append(0)
 flat=[x for v in data for x in (v if isinstance(v,(list,tuple)) else [v])]
 start=len(blob);blob.extend(struct.pack('<'+{5126:'f',5123:'H'}[component]*len(flat),*flat))
 views.append({'buffer':0,'byteOffset':start,'byteLength':len(blob)-start})
 accessors.append({'bufferView':len(views)-1,'componentType':component,'count':len(data),'type':kind})
 return len(accessors)-1
# Names intentionally do not imply humanoid engine semantics.
names=['Spindle','Fork','Branch-A','Tip-A','Branch-B','Tip-B'];parents=[-1,0,1,2,1,4]
local=[[0,0,0],[0,1,0],[-.65,.6,0],[0,-.7,0],[.65,.6,0],[0,-.7,0]]
global_positions=[]
for i,p in enumerate(local):global_positions.append([p[k]+(global_positions[parents[i]][k] if parents[i]>=0 else 0) for k in range(3)])
primitives=[];materials=[]
for bone,(center,half) in enumerate([( [0,.5,0],[.28,.5,.2]),([0,1.55,0],[.3,.55,.22]),([-.65,1.25,0],[.14,.35,.14]),([-.65,.65,0],[.16,.25,.15]),([.65,1.25,0],[.14,.35,.14]),([.65,.65,0],[.16,.25,.15])]):
 p=[];n=[];uv=[];indices=[]
 for normal,axes in [([1,0,0],(0,1,2)),([-1,0,0],(0,2,1)),([0,1,0],(1,2,0)),([0,-1,0],(1,0,2)),([0,0,1],(2,0,1)),([0,0,-1],(2,1,0))]:
  start=len(p)
  for a,b in [(-1,-1),(1,-1),(1,1),(-1,1)]:
   v=center.copy();v[axes[0]]+=normal[axes[0]]*half[axes[0]];v[axes[1]]+=a*half[axes[1]];v[axes[2]]+=b*half[axes[2]]
   p.append(v);n.append(normal);uv.append([(a+1)/2,(b+1)/2])
  indices.extend([start,start+1,start+2,start,start+2,start+3])
 attrs={'POSITION':accessor(p,'VEC3'),'NORMAL':accessor(n,'VEC3'),'TEXCOORD_0':accessor(uv,'VEC2'),'JOINTS_0':accessor([[bone,0,0,0]]*len(p),'VEC4',5123),'WEIGHTS_0':accessor([[1,0,0,0]]*len(p),'VEC4')}
 primitives.append({'attributes':attrs,'indices':accessor(indices,'SCALAR',5123),'material':bone})
 materials.append({'name':names[bone], 'pbrMetallicRoughness':{'baseColorFactor':[.15+bone*.1,.7-bone*.07,.8,1],'metallicFactor':0,'roughnessFactor':.7}})
binds=[]
for p in global_positions:
 matrix=[1,0,0,0,0,1,0,0,0,0,1,0,-p[0],-p[1],-p[2],1];binds.append(matrix)
nodes=[]
for i,name in enumerate(names):
 node={'name':name,'translation':local[i]};children=[j for j,p in enumerate(parents) if p==i]
 if children:node['children']=children
 nodes.append(node)
nodes.append({'name':'Multipart model','mesh':0,'skin':0})
time=accessor([0,1,2],'SCALAR');q=accessor([[0,0,0,1],[0,0,.38268343,.9238795],[0,0,0,1]],'VEC4')
doc={'asset':{'version':'2.0','generator':'Judas original CC0 M67 authoring figure'},'scene':0,'scenes':[{'nodes':[0,6]}],'nodes':nodes,'skins':[{'joints':list(range(6)),'inverseBindMatrices':accessor(binds,'MAT4')}],'meshes':[{'name':'Six physical-looking parts','primitives':primitives}],'materials':materials,'animations':[{'name':'Sway','samplers':[{'input':time,'output':q,'interpolation':'LINEAR'}],'channels':[{'sampler':0,'target':{'node':2,'path':'rotation'}},{'sampler':0,'target':{'node':4,'path':'rotation'}}]}],'buffers':[{'byteLength':len(blob),'uri':'data:application/octet-stream;base64,'+base64.b64encode(blob).decode()}],'bufferViews':views,'accessors':accessors}
out.write_text(json.dumps(doc,separators=(',',':'))+'\n')
