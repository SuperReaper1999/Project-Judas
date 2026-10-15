"""Rotate detailed chunks on an actual 796,375 m radius Earth.
Absolute origins are computed in double before writing the scene. The local
physics patch remains centimetre-capable; global positions are never narrowed.
"""
from pathlib import Path
import re,math,hashlib
P=Path(__file__).resolve().parents[1]
def multiply(a,b):
 w,x,y,z=a;v,X,Y,Z=b;return [w*v-x*X-y*Y-z*Z,w*X+x*v+y*Z-z*Y,w*Y-x*Z+y*v+z*X,w*Z+x*Y-y*X+z*v]
def rotate(q,v):return multiply(multiply(q,[0,*v]),[q[0],-q[1],-q[2],-q[3]])[1:]
source=(P/'Scenes/earth.judas').read_text()
for name,q in [('earth-frontier',[math.cos(math.pi/8),math.sin(math.pi/8),0,0]),('earth-highlands',[math.cos(math.pi/12),0,0,math.sin(math.pi/12)]),('earth-coast',[math.cos(math.pi/6),0,0,-.5])]:
 origin=rotate(q,[0,796375,0]);text=source.replace('name "Planetfall / earth"',f'name "Planetfall / {name}"');text=re.sub(r'world-origin [^\n]*','world-origin '+' '.join(f'{v:.17g}' for v in origin),text, count=1)
 def item(m):
  b=m.group()
  if re.search(r'(?m)^  parent ',b):return b
  for field in ['position','body.initial-velocity']:
   b=re.sub(r'(?m)^(  '+re.escape(field)+r' )([^\n]+)',lambda m:m.group(1)+' '.join(f'{v:.9g}' for v in rotate(q,list(map(float,m.group(2).split())))),b)
  b=re.sub(r'(?m)^(  rotation )([^\n]+)',lambda m:m.group(1)+' '.join(f'{v:.10g}' for v in multiply(q,list(map(float,m.group(2).split())))),b)
  return b
 text=re.sub(r'object \d+ [\s\S]*?\nend\n',item,text)
 aid=hashlib.md5(('planetfall/navigation/'+name).encode()).hexdigest();text=re.sub(r'nav.surface.asset "[^"]*"',f'nav.surface.asset "{aid}"',text)
 f=P/'Assets/navigation'/f'{name}.judasnav';f.with_name(f.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{aid}"\ntype navigation\nsource ""\n')
 (P/'Scenes'/f'{name}.judas').write_text(text)
 print(name,'Earth origin',origin,'radius',math.sqrt(sum(x*x for x in origin)))
