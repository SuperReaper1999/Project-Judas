#!/usr/bin/env python3
"""Recook the museum's 20 sources and compare actual bytes to the project."""
from pathlib import Path
import subprocess,hashlib,json,time
R=Path(__file__).resolve().parents[1];out=R/'.cache/m93/deterministic';out.mkdir(parents=True,exist_ok=True);rows=[]
for source in sorted((R/'projects/asset_lab/SourceArt').iterdir()):
 if source.suffix not in ['.obj','.png']:continue
 kind='meshes' if source.suffix=='.obj' else 'textures';ext='.judasmesh' if kind=='meshes' else '.judastex';destination=out/(source.stem+ext);original=R/'projects/asset_lab/Assets'/kind/(source.stem+ext);cmd=[R/'build/judas_cook',source,destination];start=time.monotonic()
 with (out/(source.stem+'.log')).open('w') as f:p=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
 digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest();same=p.returncode==0 and digest(destination)==digest(original);rows.append({'argv':list(map(str,cmd)),'exit':p.returncode,'identical':same,'sha256':digest(original),'seconds':time.monotonic()-start})
 if not same:raise RuntimeError('Cook differs: '+str(source))
(out/'RESULTS.json').write_text(json.dumps({'pass':True,'assets':rows},indent=2)+'\n');print(len(rows),'final-source cooks byte-identical')
