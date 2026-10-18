import datetime,hashlib,json,pathlib
base=pathlib.Path('.cache/prealpha-liquid-rotation');before=json.loads((base/'RESEARCH_TREE_BEFORE.json').read_text());root=pathlib.Path(before['root']);after={}
for p in sorted(root.rglob('*')):
 if p.is_file():
  s=p.stat();after[p.relative_to(root).as_posix()]={'bytes':s.st_size,'mtimeNs':s.st_mtime_ns,'mode':s.st_mode}
original=before['entries'];changed=[p for p in sorted(set(original)|set(after)) if original.get(p)!=after.get(p)]
seen={}
for manifest in [base/'research-geometry/RESEARCH_SOURCE_HASHES.json',base/'research-storage/READ_SOURCE_HASHES.json']:
 for row in json.loads(manifest.read_text())['files']:
  p=pathlib.Path(row['path']);actual=hashlib.sha256(p.read_bytes()).hexdigest();seen[str(p.relative_to(root))]={'expected':row['sha256'],'actual':actual,'match':actual==row['sha256']}
report={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'researchRoot':str(root),'files':len(after),'bytes':sum(s['bytes'] for s in after.values()),'unchangedMetadata':not changed,'changedPaths':changed,'citedHashes':seen,'allCitedHashesMatch':all(r['match'] for r in seen.values()),'scope':'All regular-file size/mtimeNs/mode versus pre-investigation snapshot; full byte hashes only for selected cited artifacts. Access times excluded. No research code executed or imported; EXP-016 not restarted.'}
(base/'RESEARCH_ISOLATION.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='citedHashes'},indent=2))
