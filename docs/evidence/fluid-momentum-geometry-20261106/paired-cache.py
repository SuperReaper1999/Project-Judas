import subprocess,time,json,pathlib,hashlib,re
r=pathlib.Path.cwd();c=r/'.cache/fluid-momentum-20261106';out=c/'paired-cache';out.mkdir(exist_ok=True)
bins={'previous':c/'state-probe','prepared':c/'prepared/state-probe'}
rows=[]
for sample in range(5):
 for name in (['previous','prepared'] if sample%2==0 else ['prepared','previous']):
  log=out/f'{name}-{sample}.log';prefix=out/f'{name}-{sample}-state'
  with log.open('w') as f: run=subprocess.run(['/usr/bin/time','-v','-o',str(out/f'{name}-{sample}.resource'),str(bins[name]),'3','both',str(prefix)],stdout=f,stderr=subprocess.STDOUT)
  assert run.returncode==0
  for line in log.read_text().splitlines():
   if line.startswith(('BASIN','CONTINUATION')):
    vals=dict(re.findall(r'(\w+)=([^ ]*)',line)); rows.append({'sample':sample,'backend':name,'rotated':int(vals['rotated']),'step':int(vals.get('step',1)),'ms':float(vals['ms'])})
  print(sample,name,'done',flush=True)
(out/'rows.json').write_text(json.dumps(rows,indent=2)+'\n')
for rot in [0,1]:
 for name in bins:
  values=sorted(x['ms'] for x in rows if x['rotated']==rot and x['backend']==name)
  print(rot,name,'samples',len(values),'median',values[len(values)//2],'p95',values[-1],'worst',max(values),flush=True)
for sample in range(5):
 for rot in [0,1]:
  for step in range(1,4):
   a=(out/f'previous-{sample}-state-{rot}-{step}.bin').read_bytes();b=(out/f'prepared-{sample}-state-{rot}-{step}.bin').read_bytes();assert a==b
print('All paired physical states byte-identical',flush=True)
