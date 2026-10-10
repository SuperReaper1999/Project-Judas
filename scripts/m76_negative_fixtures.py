#!/usr/bin/env python3
"""Actual CLI packaging/policy rejections on disposable kit-bound projects."""
import argparse, hashlib, json, shutil, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('tool');p.add_argument('author');p.add_argument('output');a=p.parse_args()
root=Path(a.output);root.mkdir(parents=True,exist_ok=True);source=Path(a.author);tool=str(Path(a.tool).resolve());results=[]
def reject(name,edit):
    dest=root/name;shutil.copytree(source,dest);project=next(dest.glob('*.judasproj'));manifest=dest/'mod.json';m=json.loads(manifest.read_text());edit(dest,m);manifest.write_text(json.dumps(m));output=dest/'retained.judasmod';output.write_bytes(b'last good output');before=hashlib.sha256(output.read_bytes()).hexdigest()
    r=subprocess.run([tool,'package',str(project),str(output)],capture_output=True,text=True);item={'case':name,'exit':r.returncode,'diagnostic':r.stderr.strip(),'lastGood':before==hashlib.sha256(output.read_bytes()).hexdigest()};results.append(item);print(item,flush=True);assert r.returncode and item['lastGood']
reject('protected',lambda d,m:m.update(replacements=[{'target':'protected-hub','revision':'0'*64,'provider':{'kind':'scene','value':'Scenes/main.judas'}}]))
reject('missing-dependency',lambda d,m:m.update(dependencies=[{'id':'absent','version':'1.0.0'}]))
reject('hidden-executable',lambda d,m:m.update(executable=False))
reject('undeclared-references',lambda d,m:m.update(imports=[]))
reject('changed-kit-identity',lambda d,m: (d/'mod-authoring.json').write_text(json.dumps({'format':1,'kit':'Kit','identity':'0'*64,'mod':m['id'],'identities':{}})))
def stale(d,m):
    src=next((d/'Imports').glob('*.judasterrain'));j=json.loads(src.read_text());j['heights'][0]+=.125;src.write_text(json.dumps(j))
reject('stale-terrain',stale)
def bad_nav(d,m):
    nav=next((d/'Assets/navigation').glob('*.judasnav'));data=bytearray(nav.read_bytes());data[0]^=1;nav.write_bytes(data)
reject('corrupt-navigation',bad_nav)
def stale_nav(d,m):
    stale(d,m);project=next(d.glob('*.judasproj'));src=next((d/'Imports').glob('*.judasterrain'));r=subprocess.run([str(Path(tool).parent/'judas_terrain_author'),'cook',str(project),str(src.relative_to(d))],capture_output=True,text=True);assert r.returncode==0,r.stderr
reject('stale-navigation',stale_nav)
reject('incompatible-game',lambda d,m:m.update(game='different-game'))
(root/'results.json').write_text(json.dumps(results,indent=2)+'\n')
