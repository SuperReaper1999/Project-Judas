#!/usr/bin/python3
"""Host archive preservation of completed final executables; no source or guest deletion."""
import pathlib,json,hashlib,zipfile
p=pathlib.Path('docs/evidence/m78-continuation/probes/windows_execute_focused_matrix.py');s=p.read_text();ns={};exec(compile(s[:s.index('try:\n # The manually completed')],str(p),'exec'),ns)
r=pathlib.Path.cwd();out=r/'.cache/m78-continuation/native-final-binaries';out.mkdir(exist_ok=True);rows=[]
for case in ['balance_lab','runtime','concave']:
 receipt=ns['reports']/('m78-combined-v2-'+case+'-archive.json')
 if not receipt.exists():ns['op']('archive',case)
 approval=r/'.cache/windows-vm-transfer'/('m78-combined-v2-'+case+'-archive-approved.json')
 if not approval.exists():ns['op']('approve',case)
 d=json.loads(receipt.read_text(encoding='utf-8-sig'));compiled=json.loads((ns['reports']/('m78-combined-v2-'+case+'.json')).read_text(encoding='utf-8-sig'));z=ns['reports']/d['archiveName'];assert hashlib.sha256(z.read_bytes()).hexdigest()==d['archiveSHA256']
 with zipfile.ZipFile(z) as archive:
  item=next(f for f in d['files'] if f['path']==compiled['executable']);data=archive.read(item['name']);assert len(data)==item['bytes'] and hashlib.sha256(data).hexdigest()==compiled['executableSHA256'];target=out/item['name'];assert not target.exists() or target.read_bytes()==data;target.write_bytes(data)
 rows.append({'case':case,'path':str(target),'bytes':len(data),'sha256':compiled['executableSHA256'],'archiveSHA256':compiled['archiveSHA256'],'hostArchiveReceipt':str(receipt),'hostArchiveBytesSHA256':d['archiveSHA256']});print(case,rows[-1]['sha256'],flush=True)
ns['collect']();(r/'docs/evidence/m78-continuation/windows-final-native-binaries.json').write_text(json.dumps({'scope':'Independently SHA/byte-verified exact final native executable bytes outside tracked repository; guest library baseline remains untouched.','executables':rows},indent=2)+'\n')
