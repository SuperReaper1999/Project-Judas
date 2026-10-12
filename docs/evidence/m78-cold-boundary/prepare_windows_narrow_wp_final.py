#!/usr/bin/env python3
"""Freeze only the approved narrow WorldPersistence follow-up for native reruns."""
import argparse,hashlib,json,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];T=ROOT/'.cache/windows-vm-transfer';R=T/'reports'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--approved-wp-sha256',required=True);args=ap.parse_args();wp=ROOT/'src/WorldPersistence.cpp';assert sha(wp)==args.approved_wp_sha256;assert sha(wp)!='81e74af801d5601b5995f94532ca62da387065f750137a2c61a87349841fdeb3','No approved correction present'
 previous=json.loads((T/'m78-final-cold-repair-v1.json').read_text());m=dict(previous);m.update({'label':'cold-repair-final-v2','scope':'Final actual production candidate with approved generic current-physical-state persistence correction. Narrow affected native reruns, original fixtures/acceptance and legacy a106 unchanged; Windows VM functional-only.','scratch':'C:\\JudasValidation\\M78ContinuationBuild\\cold-repair-final-v2','output':'C:\\JudasValidation\\M78ColdRepairFinalV2Results','baselineLibrary':{'path':'C:\\JudasValidation\\M78ContinuationBuild\\cold-repair-final-v1\\judas_engine.lib','bytes':67015480,'sha256':'3554c52ca3f8961ca61551074d616f24d2fe0024ecae75f17aef97431e6de83e'},'replacements':[{'path':'src/WorldPersistence.cpp','objectName':'WorldPersistence.obj','sha256':sha(wp)}]})
 paths=list(sorted((ROOT/'src').rglob('*.h')))+[wp];cases=[]
 for c0 in previous['cases']:
  if c0['id'] not in ['world_save','balance_lab','character_lab','runtime','physics_continuation']:continue
  c=json.loads(json.dumps(c0))
  for u in c['inputs']:
   p=ROOT/u['path'];old=u['sha256'];u['sha256']=sha(p);u['bytes']=p.stat().st_size
   if c['id']=='physics_continuation':
    receipt=json.loads((R/'m78-final-repair-physics_continuation-build.json').read_text(encoding='utf-8-sig'));o=receipt['objects'][0];u['reuseOriginal']=u['sha256']==o['sourceSHA256'];u['originalObjectSHA256']=o['objectSHA256'];u['objectURL']='http://192.168.122.1:8872/m78-final2-physics-continuation-0.obj';data=(ROOT/'.cache/m78-cold-boundary/native-binaries/final-repair/physics_continuation-0.obj').read_bytes();assert hashlib.sha256(data).hexdigest()==o['objectSHA256'];(T/'m78-final2-physics-continuation-0.obj').write_bytes(data)
   elif u['sha256']!=old:u['reuseOriginal']=False
   paths.append(p) # Preserve exact source for reused-object identity and final freeze checks.
  for run in c['runs']:run['arguments']=[a.replace('M78ColdRepairFinalResults','M78ColdRepairFinalV2Results') for a in run.get('arguments',[])]
  cases.append(c)
 # The narrowed current-body capture guard also affects inactive streaming state.
 oldmatrix=json.loads((T/'m78-continuation-direct-combined-v2.json').read_text(encoding='utf-8-sig'))
 c=json.loads(json.dumps(next(c for c in oldmatrix['cases'] if c['id']=='streaming')))
 receipt=json.loads((R/'m78-combined-v2-streaming.json').read_text(encoding='utf-8-sig'));o=receipt['compiles'][0];source=ROOT/'tests/WorldStreamingTests.cpp'
 assert sha(source)==o['sourceSHA256'],'Streaming fixture changed unexpectedly'
 with zipfile.ZipFile(R/'m78-combined-v2-streaming-archive.zip') as z:data=z.read('streaming-0.obj')
 assert hashlib.sha256(data).hexdigest()==o['objectSHA256']
 (T/'m78-final2-streaming-0.obj').write_bytes(data)
 c['inputs']=[{'path':'tests/WorldStreamingTests.cpp','sha256':sha(source),'bytes':source.stat().st_size,'reuseOriginal':True,'originalObjectSHA256':o['objectSHA256'],'objectURL':'http://192.168.122.1:8872/m78-final2-streaming-0.obj','objectName':'streaming-0.obj'}]
 c['runs']=[{'id':'streaming','arguments':[]}];cases.insert(1,c);paths.append(source)
 m['cases']=cases;m['files']=[];archive='m78-final-cold-repair-source-v2.zip'
 with zipfile.ZipFile(T/archive,'w',zipfile.ZIP_DEFLATED) as z:
  for p in sorted(set(paths)):
   rel=p.relative_to(ROOT).as_posix();z.write(p,rel);m['files'].append({'path':rel,'bytes':p.stat().st_size,'sha256':sha(p)})
 m['archiveName']=archive;m['archiveSHA256']=sha(T/archive)
 p=T/'m78-final-cold-repair-v2.json';p.write_text(json.dumps(m,indent=2)+'\n');(ROOT/'docs/evidence/m78-cold-boundary/native-final-narrow-wp-manifest.json').write_bytes(p.read_bytes())
 print(json.dumps({'archiveSHA256':m['archiveSHA256'],'WPsha256':sha(wp),'cases':[c['id'] for c in cases],'freshTestCompiles':[u['path'] for c in cases for u in c['inputs'] if not u['reuseOriginal']]},indent=2))
if __name__=='__main__':main()
