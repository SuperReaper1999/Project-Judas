#!/usr/bin/env python3
"""Paired accepted-VM versus candidate-VM disabled-debugger timing."""
from pathlib import Path
import subprocess,json,re,statistics,datetime,hashlib,platform,argparse
R=Path(__file__).resolve().parents[1];parser=argparse.ArgumentParser();parser.add_argument('--output',default='.cache/m94/measure-final');args=parser.parse_args();out=R/args.output;out.mkdir(parents=True,exist_ok=False)
paths={'accepted':R/'.cache/m94/baseline/vm_benchmark','candidate':R/'build/judas_script_vm_benchmark'};samples={k:[]for k in paths};checksums=set();commands=[]
for round in range(5):
 for name in (['accepted','candidate']if round%2==0 else ['candidate','accepted']):
  p=paths[name];raw=subprocess.check_output([str(p)],cwd=R,text=True);(out/f'{round}-{name}.log').write_text(raw);rows=re.findall(r'batch=(\d+) us_per_call=([\d.]+) checksum=([\d.]+)',raw)
  if len(rows)!=10:raise RuntimeError('Incomplete benchmark output')
  checksums.add(tuple(r[2]for r in rows));samples[name]+=[float(r[1])for r in rows[1:]];commands.append([str(p)])
if len(checksums)!=1:raise RuntimeError('Numerical endpoint changed')
def summarize(values):
 values=sorted(values);return {'median_us':statistics.median(values),'p95_us':values[int(.95*(len(values)-1))],'worst_batch_mean_us':values[-1],'batch_samples':len(values)}
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'platform':platform.platform(),'timing':'100 calls/batch; 15000 arithmetic/branch iterations/call; discard first batch; five alternating paired process rounds; wall clock','baseline_commit':'3306b5aa66a2e649bad06abc8c5cba452058d3fa','identical_checksums':True,'commands':commands,'results':{k:summarize(v)for k,v in samples.items()},'binary_sha256':{k:hashlib.sha256(p.read_bytes()).hexdigest()for k,p in paths.items()}}
result['median_overhead_percent']=(result['results']['candidate']['median_us']/result['results']['accepted']['median_us']-1)*100
(out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result['results'],indent=2));print('median overhead %',result['median_overhead_percent'])
