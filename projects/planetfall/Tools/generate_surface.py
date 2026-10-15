"""Original terrain content, using Prompt3D's mesh writer, not engine physics."""
from pathlib import Path
import sys,math
import numpy as np
from prompt3d.geometry import Scene,Prim
from prompt3d.export import to_glb
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
for name,R,color in [('earth',796375,[.24,.40,.24]),('moon',217175,[.47,.48,.52]),('mars',423687.5,[.57,.28,.16]),('derelict',796375,[.24,.27,.30])]:
 V=[];F=[];n=81;w=640
 for z in range(n):
  for x in range(n):
   xx=(x/(n-1)-.5)*w;zz=(z/(n-1)-.5)*w;r2=xx*xx+zz*zz
   # Rationalized spherical sag avoids subtracting two near-equal large floats.
   y=-r2/(R+math.sqrt(R*R-r2)); edge=max(abs(xx),abs(zz)); h=max(0,(edge-170)/140)*5*(1+.55*math.sin(xx*.04)*math.cos(zz*.036))
   V.append([xx,y+h-.05,zz])
 for z in range(n-1):
  for x in range(n-1):
   a=z*n+x;F.extend([[a,a+n,a+1],[a+1,a+n,a+n+1]])
 scene=Scene(name);scene.material('land','#'+''.join(f'{round(c*255):02x}' for c in color));scene.add('ground','land',Prim(V,[[0,1,0]]*len(V),F));to_glb(scene,out/(name+'_surface.glb'),textures=False,flat=True)
 # Blue seam: authored faceted crystal, interactive mine consumes finite deposits.
scene=Scene('aether');scene.material('crystal','#24bae8');from prompt3d.geometry import sphere,scale
for i in range(5):scene.add('seam','crystal',sphere(.55,segments=6,rings=3),scale(.6,1.5,1),t=((i%3)*.45,.6,(i//3)*.45))
to_glb(scene,out/'aether.glb',textures=False,flat=True)
