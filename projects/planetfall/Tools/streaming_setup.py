"""Ordinary snapshot-owned salvage encounter, preserving physical damage."""
from pathlib import Path
import hashlib,json,re
P=Path(__file__).resolve().parents[1];ROOT=P.parents[1];A=P/'Assets';(A/'worlds').mkdir(exist_ok=True)
source=(ROOT/'projects/coarse_crash_lab/Assets/prefabs/car.judasprefab').read_text();(P/'Scenes/salvage-rover.judas').write_text(source)
manifest=A/'worlds/planetfall.judasworld';manifest.write_text('JudasWorld 1\nbudget 2 16 2 33554432 134217728 16777216 134217728\nregion "salvage-rover" "Scenes/salvage-rover.judas" 145 796374.932 65 0 0 0 1 6 6 8 5 "snapshot" 2097152 ""\n')
aid=hashlib.md5(b'planetfall/world/ownership').hexdigest();manifest.with_name(manifest.name+'.judasmeta').write_text(f'JudasAssetMeta 1\nid "{aid}"\ntype world\nsource "project-owned stream manifest"\n')
p=P/'planetfall.judasproj';s=p.read_text();s=re.sub(r'(?m)^world-manifest .*\n','',s);p.write_text(s+'world-manifest '+json.dumps(aid)+'\n')
# A visible waypoint is ordinary presentation content, with no collision policy.
source=(P/'Scenes/earth.judas').read_text();b=re.search(r'object 22100 [\s\S]*?\nend\n',source)
# NPC has been statically batched. Use an intact weapon visual schema as template.
b=re.search(r'object 12 [\s\S]*?\nend\n',source).group();b=re.sub(r'^object [^\n]*','object 39020 "Campaign waypoint / presentation only"',b);b=re.sub(r'(?m)^  (?:animation[^ ]*|parent) .*\n','',b);b=b.replace('render mesh','render sphere');b=re.sub(r'render.radius [^\n]*','render.radius .18',b);b=re.sub(r'render.color [^\n]*','render.color 1 .7 .2',b)
for p in (P/'Scenes').glob('*.judas'):
 if p.name=='salvage-rover.judas':continue
 p.write_text(p.read_text()+'\n'+b)
