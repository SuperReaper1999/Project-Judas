#!/usr/bin/env python3
"""Hash this bounded performance investigation; do not copy large build artifacts."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4];HERE=Path(__file__).resolve().parent
paths=set()
for p in HERE.rglob('*'):
 if p.is_file() and p.name!='MEASUREMENT_FINGERPRINTS.json' and '__pycache__' not in p.parts:paths.add(p)
ev=ROOT/'docs/evidence/m78-continuation'
for pattern in ['*existence*.json','*existence*.log','*attribution*.json','*attribution*.log','parity-gap-*.json','parity-gap-*.log']:
 paths.update(p for p in ev.glob(pattern) if p.is_file())
paths.add(ROOT/'docs/evidence/m78-followup/linux-concave-combined-final.log')
paths.add(ROOT/'docs/evidence/m78-followup/probes/concave_final.cpp')
paths.add(ROOT/'projects/post_m65_consumers/Scenes/skate/park.judas')
for dirname in ['.cache/m78-followup/concave-final-data','.cache/m78-continuation/concave-existence-isolated-data','.cache/m78-continuation/concave-attribution-spikes-data']:
 paths.update(p for p in (ROOT/dirname).glob('*') if p.is_file())
for dirname in ['.cache/m78-continuation/existence-observer','.cache/m78-continuation/existence-parity-v2']:
 for name in ['BUILD.json','CollisionGeometry.cpp','CollisionGeometry.h','PhysicsWorld.cpp','OriginalCookedContactOracle.cpp','Probe.cpp','libjudas_engine.a','probe']:
  p=ROOT/dirname/name
  if p.is_file():paths.add(p)
for name in ['.cache/m78-continuation/contact-existence-parity-corrected','.cache/m78-continuation/concave-attribution-original']:
 p=ROOT/name
 if p.is_file():paths.add(p)
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while chunk:=f.read(1024*1024):h.update(chunk)
 return h.hexdigest()
rows={str(p.relative_to(ROOT)):{'bytes':p.stat().st_size,'sha256':digest(p)} for p in sorted(paths)}
output=HERE/'MEASUREMENT_FINGERPRINTS.json'
output.write_text(json.dumps({'scope':'Bounded isolated CCD performance/parity evidence; excludes this self-referential manifest; hashes build outputs without adding them to tracked evidence','files':rows},indent=2)+'\n')
print(json.dumps({'files':len(rows),'output':str(output.relative_to(ROOT))}))
