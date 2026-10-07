#!/usr/bin/env python3
"""Isolated M66 import measurements. Never edits the accepted project/originals.
Record real CLI wall time/peak RSS alongside M56 scopes; no benchmark threshold.
"""
import hashlib,json,os,shutil,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'docs/evidence/m66/import-performance'
OWN=ROOT/'.cache/m66-import-performance'
def write(p,j):p.write_text(json.dumps(j,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 assert not OUT.exists() and not OWN.exists(),'Keep earlier measurements intact.'
 OUT.mkdir(parents=True);OWN.mkdir(parents=True)
 shutil.copytree(ROOT/'projects/import_lab/Sources',OWN/'Sources')
 shutil.copytree(ROOT/'projects/import_lab/Imports',OWN/'Imports')
 results=[]
 def run(label,recipe):
  env=dict(os.environ,JUDAS_PROFILE='1',JUDAS_PROFILE_OUTPUT=str(OUT/(label+'-profile.json')))
  cmd=['/usr/bin/time','-v','-o',str(OUT/(label+'-time.txt')),str(ROOT/'build/judas_model_import_cli'),'--recipe',str(recipe)]
  start=time.monotonic();p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True)
  (OUT/(label+'.json')).write_text(p.stdout);(OUT/(label+'.stderr')).write_text(p.stderr)
  assert p.returncode in (0,2),(label,p.stdout,p.stderr)
  report=json.loads(p.stdout);rss=next(line.split(':',1)[1].strip() for line in (OUT/(label+'-time.txt')).read_text().splitlines() if 'Maximum resident set size' in line)
  results.append(dict(label=label,wallMs=(time.monotonic()-start)*1000,peakRSSKiB=int(rss),unchanged=report['unchanged'],assetId=report['assetId']))
  write(OUT/'results.json',results)
 recipe=OWN/'Imports/skate_original.judasimport';run('original-cold',recipe);run('original-unchanged',recipe)
 external=OWN/'Imports/external.judasimport';run('small-cold',external);run('small-unchanged',external)
 # Replace only a copied, valid dependency. Authoring IDs/material slots stay put.
 image=OWN/'Sources/external/café color.png';shutil.copyfile(ROOT/'projects/pose_demo/Assets/textures/particle_soft.png',image)
 run('small-changed-texture',external)
 # Same-rig animation fixture: actual source change, not renamed clip settings.
 r=OWN/'Imports/capacity256.judasimport';j=json.loads(r.read_text());source=OWN/j['source'];motion=source.parent/'motion.gltf';shutil.copyfile(source,motion)
 j['motions']=[dict(source=motion.relative_to(OWN).as_posix(),take='Translate',name='External')];j['clips']=[dict(sourceClip='External',name='External')];write(r,j)
 run('motion-baseline',r)
 bases={str(p):sha(p) for p in (OWN/'.cache/model-import').glob('*.judasmodel') if '.motion.' not in p.name}
 m=json.loads(motion.read_text())
 for animation in m['animations']:
  for sampler in animation['samplers']:sampler['interpolation']='STEP'
 write(motion,m);run('changed-compatible-motion',r)
 assert all(sha(Path(p))==h for p,h in bases.items()),'Unrelated geometry cache changed'
 write(OUT/'cache-isolation.json',dict(unrelatedBaseProductsUnchanged=True,products=len(bases)))
 print(json.dumps(results,indent=2))
if __name__=='__main__':main()
