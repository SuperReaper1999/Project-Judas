#!/usr/bin/env python3
"""Read-only, bounded independent inspection of Judas M61 save envelopes."""
import argparse,hashlib,json,re
from pathlib import Path
class Reader:
 def __init__(self,data):self.data=data;self.at=0
 def number(self,size=4):
  if self.at+size>len(self.data):raise ValueError('truncated field')
  n=int.from_bytes(self.data[self.at:self.at+size],'little');self.at+=size;return n
 def string(self):
  size=self.number()
  if size>8*1024*1024 or self.at+size>len(self.data):raise ValueError('invalid string bound')
  value=self.data[self.at:self.at+size];self.at+=size;return value
 def text(self):return self.string().decode('utf-8')
 def end(self):
  if self.at!=len(self.data):raise ValueError('trailing data')
def inspect(path):
 data=Path(path).read_bytes()
 if len(data)>64*1024*1024:raise ValueError('oversized save')
 root=Reader(data);magic=root.text();header=Reader(root.string());payload=root.string();root.end()
 if magic!='JudasGameSave':raise ValueError('wrong signature')
 version=header.number();project=header.text();scene=header.text();content=header.text();name=header.text();metadata=header.text();timestamp=header.number(8);game_version=header.number();expected=header.text();diagnostic=header.text();mods=header.text() if version==3 else '';header.end()
 if version not in (2,3) or hashlib.sha256(payload).hexdigest()!=expected:raise ValueError('version/checksum mismatch')
 fields=Reader(payload);chunks={}
 count=fields.number()
 if count>32:raise ValueError('participant bound')
 for _ in range(count):
  key=fields.text();v=fields.number();body=fields.string()
  if key in chunks:raise ValueError('duplicate participant')
  chunks[key]=(v,body)
 fields.end();scripts=[];entities=None
 if 'entities' in chunks:
  scene=Reader(chunks['entities'][1]).text()
  # Inspect the persisted ordinary scene, independently of controller caches.
  # This reader supports the current JudasScene 3 envelope, not future schemas.
  if not scene.startswith('JudasScene 3\n'):raise ValueError('unsupported scene inspection schema')
  objects=re.findall(r'^object (\d+) [^\n]*\n(.*?)^end$',scene,re.M|re.S)
  tags={};bodies={}
  for ident,fields_text in objects:
   body=re.search(r'^  body (\w+) ',fields_text,re.M)
   if body:bodies[body[1]]=bodies.get(body[1],0)+1
   match=re.search(r'^  tags (\d+)$',fields_text,re.M)
   if match:
    bits=int(match[1])
    for bit in range(64):
     if bits&(1<<bit):tags.setdefault(str(bit),[]).append(ident)
  entities={'objects':len(objects),'bodies':bodies,'taggedEntityIds':tags,'sceneSha256':hashlib.sha256(scene.encode()).hexdigest()}

 if 'scripts' in chunks:
  values=Reader(chunks['scripts'][1]);count=values.number()
  if count>4096:raise ValueError('script bound')
  for _ in range(count):scripts.append({'entity':str(values.number(8)),'slot':str(values.number(8)),'state':json.loads(values.text())})
  values.end()
 return {'file':str(path),'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'version':version,'project':project,'scene':scene,'content':content,'name':name,'timestamp':timestamp,'diagnosticBuild':diagnostic,'participants':{k:{'version':v,'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}for k,(v,b)in chunks.items()},'scripts':scripts,'entities':entities,'checksumVerified':True}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('save');a=p.parse_args();print(json.dumps(inspect(a.save),indent=2))
