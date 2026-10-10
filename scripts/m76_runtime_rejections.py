#!/usr/bin/env python3
"""Corrupt selected on-disk inputs deliberately; prove actual Application rejects before VM/world activation.
This bypasses the installer intentionally to test the independent launch boundary.
Never point this tool at a user's mod store. All stores below OUTPUT are disposable.
"""
import argparse,struct,json,hashlib,subprocess,os,shutil
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('application');p.add_argument('game');p.add_argument('package');p.add_argument('output');a=p.parse_args();output=Path(a.output).resolve();output.mkdir(parents=True,exist_ok=True);raw=Path(a.package).read_bytes()
def unpack(b):
 at=0
 def read():
  nonlocal at
  n=struct.unpack_from('<I',b,at)[0];at+=4;v=b[at:at+n];at+=n;return v
 assert read()==b'JudasMod';assert struct.unpack_from('<I',b,at)[0]==1;at+=4;m=json.loads(read());n=struct.unpack_from('<I',b,at)[0];at+=4;files={}
 for _ in range(n):path=read().decode();data=read();files[path]=data
 return m,files

def encode(m,files):
 for f in m['files']:f['size']=len(files[f['path']]);f['sha256']=hashlib.sha256(files[f['path']]).hexdigest()
 def s(v):return struct.pack('<I',len(v))+v
 return s(b'JudasMod')+struct.pack('<I',1)+s(json.dumps(m,separators=(',',':'),ensure_ascii=False).encode())+struct.pack('<I',len(files))+b''.join(s(f['path'].encode())+s(files[f['path']]) for f in m['files'])
results=[]
for case in ['missing-published-reference','data-only-property-schema','wrong-asset-type']:
 root=output/case;root.mkdir();game=root/'Game';shutil.copytree(Path(a.game).parent,game,ignore=shutil.ignore_patterns('judas','*.dll','*.exe'));m,files=unpack(raw)
 if case=='missing-published-reference':m['imports'][0]['name']='unpublished-reference'
 elif case=='data-only-property-schema':
  name='Scenes/main.judas';old=files[name].decode();new=old.replace(r'\"hud\":\"modding_lab\"',r'\"hud\":123');assert new!=old;files[name]=new.encode()
 else:
  # A collision file declared as a font cannot satisfy the scene's typed collider reference.
  asset=next(x for x in m['assets'] if x['type']=='collision');asset['type']='font'
 b=encode(m,files);h=hashlib.sha256(b).hexdigest();store=root/'user/judas/mods/judas-modding-lab';(store/'Installed').mkdir(parents=True);(store/'Loadout').mkdir();filename=f"{m['id']}--{m['version']}--{h}.judasmod";(store/'Installed'/filename).write_bytes(b)
 (store/'Installed/index.json').write_text(json.dumps({'format':1,'installed':[{'id':m['id'],'version':m['version'],'sha256':h,'file':filename}]}));(store/'Loadout/selected.json').write_text(json.dumps({'format':1,'selected':[{'id':m['id'],'version':m['version'],'sha256':h,'trustedHash':''}]}))
 env=os.environ.copy();env.update(XDG_DATA_HOME=str(root/'user'),SDL_AUDIODRIVER='dummy');r=subprocess.run([str(Path(a.application).resolve()),str(game/Path(a.game).name),'probe',str(root/'capture')],env=env,cwd='/tmp',capture_output=True,text=True,timeout=30);text=r.stdout+r.stderr;(root/'application.log').write_text(text);item={'case':case,'exit':r.returncode,'diagnostic':text.strip(),'worldNeverReady':'PASS required composed terrain' not in text};results.append(item);assert r.returncode and item['worldNeverReady'],item;print(item)
(output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
