#!/usr/bin/env python3
from pathlib import Path
import datetime,hashlib,json,subprocess,statistics,sys
O=Path(__file__).resolve().parent;CAP=O.parent/'contact-system-research/event-3470.json';BUILD=json.loads((O/'BUILD.json').read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
report={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'snapshot':str(CAP),'snapshot_sha256':sha(CAP),'build_receipt_sha256':sha(O/'BUILD.json'),'runs':[],'scope':'Bounded exact captured-event cost and parity. Not whole-world FPS or a new accepted physical endpoint.'}
def persist():(O/'RESULTS.json').write_text(json.dumps(report,indent=2)+'\n')
def run(stem,args=(),expected=0):
 binary=O/stem;assert sha(binary)==BUILD['binaries'][stem]
 argv=[str(binary),*map(str,args)];p=subprocess.run(argv,cwd=O,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True);(O/(stem+'.stdout')).write_text(p.stdout);(O/(stem+'.stderr')).write_text(p.stderr);r={'argv':argv,'actual_exit':p.returncode,'expected_exit':expected,'expected_exit_observed':p.returncode==expected,'binary_sha256':sha(binary),'stdout':stem+'.stdout','stderr':stem+'.stderr'};report['runs'].append(r);persist();return p,r
for variant in ('baseline','candidate'):
 p,r=run('ImpactSolverTests-'+variant)
 p,r=run('MemoControls-'+variant)
 if p.returncode==0:r['controls']=json.loads(p.stdout)
p,r=run('MemoControls-eager-mutant',expected=1);r['controls']=json.loads(p.stdout)
controls={r['argv'][0].split('/')[-1]:r for r in report['runs'] if 'MemoControls-' in r['argv'][0]}
report['all_small_physical_and_diagnostic_output_bits_exact']=controls['MemoControls-baseline'].get('controls')==controls['MemoControls-candidate'].get('controls')
for variant in ('baseline','candidate'):
 output=O/('replay-'+variant+'.json');p,r=run('MemoReplay-'+variant,[CAP,output]);j=json.loads(output.read_text());r['exact_parity_flags']={k:v for k,v in j.items() if 'parity' in k or 'exact_body_row' in k};values=j['same_captured_event_32_timings_ms'];ordered=sorted(values);r['timing_ms']={'samples':len(values),'mean':statistics.mean(values),'median':statistics.median(values),'p95':ordered[min(len(values)-1,int(.95*len(values)))],'maximum':max(values),'cold_first':j['original_10_pgs_ms']};r['output_sha256']=sha(output)
report['all_expected_exits_observed']=all(r['expected_exit_observed'] for r in report['runs']);report['passed']=report['all_expected_exits_observed'] and report['all_small_physical_and_diagnostic_output_bits_exact'];persist();print(json.dumps(report,indent=2));raise SystemExit(0 if report['passed'] else 1)
