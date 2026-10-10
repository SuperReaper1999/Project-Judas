#!/usr/bin/env python3
"""Bounded many-small registered assets through ordinary kit authoring services."""
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('tool');p.add_argument('kit');p.add_argument('destination');p.add_argument('package');a=p.parse_args()
def run(*args):
    r=subprocess.run([a.tool,*map(str,args)],capture_output=True,text=True);assert r.returncode==0,r.stderr;return r.stdout.strip()
project=Path(run('new',a.kit,a.destination,'small-assets'));root=project.parent
for n in range(100):
    identity=run('identity',project,f'{n+100:032x}');file=root/f'Assets/scripts/helper-{n}.js';file.parent.mkdir(exist_ok=True);file.write_text(f'export const value={n};\n');Path(str(file)+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{identity}"\ntype script\nsource "Original M76 100-file fixture"\n')
m=json.loads((root/'mod.json').read_text());m.update(executable=True,author='Judas',license='CC0');(root/'mod.json').write_text(json.dumps(m,indent=2));print(run('package',project,a.package))
