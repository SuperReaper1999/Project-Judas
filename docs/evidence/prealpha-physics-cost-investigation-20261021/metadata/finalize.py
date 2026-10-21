from pathlib import Path
import csv,json,statistics,hashlib,datetime,subprocess,math,gzip,shutil,tarfile,ast
r=Path.cwd();w=r/'.cache/physics-exhaustive-profile-20261021';e=r/'docs/evidence/prealpha-physics-cost-investigation-20261021';e.mkdir(parents=True,exist_ok=True)
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
frozen=json.loads((w/'baseline-snapshot/FROZEN.json').read_text());mismatches=[]
for path,info in frozen['source_inputs'].items():
 p=r/path
 if not p.is_file()or h(p)!=info['sha256']:mismatches.append(path)
assert not mismatches,mismatches
assert h(r/'build/libjudas_engine.a')=='05f626d3fb5400bfd1db23ec37a3cd3e6490f8452145365662466eebccd705ec'
assert subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],text=True)==''
verification={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'head':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'branch':subprocess.check_output(['git','branch','--show-current'],text=True).strip(),'tracked_status':'','source_inputs_verified':len(frozen['source_inputs']),'source_mismatches':mismatches,'engine_core_sha256':h(r/'build/libjudas_engine.a'),'runtime_sha256':h(r/'build/judas'),'fixture_sha256':h(r/'projects/physics_stress_lab/Scenes/room1024.judas'),'refs':subprocess.check_output(['git','for-each-ref','--format=%(refname:short) %(objectname)','refs/heads/main','refs/heads/Experimental','refs/remotes/origin/main','refs/remotes/origin/Experimental'],text=True).splitlines(),'not_a_remote_fetch':True}
(w/'FINAL_VERIFICATION.json').write_text(json.dumps(verification,indent=2)+'\n')
def rows(name,file='steps.csv'):return list(csv.DictReader(open(w/name/file)))
def stats(a):
 a=sorted(a);q=lambda p:a[math.floor((len(a)-1)*p)]+(a[min(len(a)-1,math.floor((len(a)-1)*p)+1)]-a[math.floor((len(a)-1)*p)])*(((len(a)-1)*p)%1)
 return {'n':len(a),'mean_ms':statistics.mean(a),'median_ms':statistics.median(a),'p95_ms':q(.95),'worst_ms':max(a)}
run_names=['unobserved900','unobserved900-B','sampled900','disabled900','observed900','stages900','cache-control900','cache-control900-B','sleep900','long1800','observed1800-late','live35-sampled']
summary={}
for name in run_names:
 a=rows(name);windows={}
 for start,end in [(1,300),(301,600),(601,900),(300,900),(901,1800),(1,len(a))]:
  rr=[x for x in a if start<=int(x['step'])<=end]
  if rr:windows[f'{start}-{end}']={**stats([float(x['fixed_ms'])for x in rr]),'events':sum(int(x['impact_events'])for x in rr),'queries':sum(int(x['impact_queries'])for x in rr),'nonfinite_sum':sum(int(x['nonfinite_fields'])for x in rr)}
 summary[name]={'steps':len(a),'windows':windows,'quality':json.loads((w/name/'quality.json').read_text())}
frames=rows('live35-sampled','frames.csv');late=[x for x in frames if float(x['wall_ms'])>10000 and int(x['fixed_steps'])>0];summary['live35-sampled']['live_cadence']={'late_window':'frames ending after 10s, through automatic35s stop','late_fps':1000/statistics.mean(float(x['interval_ms'])for x in late),'late_mean_frame_ms':statistics.mean(float(x['interval_ms'])for x in late),'late_mean_fixed_steps_per_frame':statistics.mean(int(x['fixed_steps'])for x in late),'completed_steps':len(rows('live35-sampled')),'frames':len(frames),'real_cadence':True,'synthetic_fps_claim':False}
(w/'MEASUREMENTS.json').write_text(json.dumps(summary,indent=2)+'\n')
# Preserve exact commands. Earlier starts were not wrapped with UTC; do not invent them.
runs=[]
for name in run_names:
 binary='cache-control-live'if name.startswith('cache-control')else 'stages-live'if name=='stages900'else 'observed-live'if name in ['disabled900','observed900','observed1800-late']else 'sampled-live'
 live=name=='live35-sampled';n=1800 if name in ['long1800','observed1800-late']else 900
 argv=[str(w/binary),str(r/'projects/physics_stress_lab/Scenes/room1024.judas'),str(w/name),'35'if live else '300'if name=='observed1800-late'else '240'if name in ['long1800','observed900']else '180','10000000'if live else str(n),'0','0','0'if live else '1','0'if live else '0.016666667','0','0','1']
 env={}
 if name in ['sampled900','live35-sampled']:env['JUDAS_CPU_SAMPLE']='1'
 if name in ['observed900','observed1800-late','stages900']:
  env.update(JUDAS_DEMAND_AUDIT='1',JUDAS_AUDIT_START='901'if n==1800 else '300',JUDAS_AUDIT_END=str(n))
  if name!='stages900':env['JUDAS_CCD_AUDIT_TIMING']='1'
 if name=='sleep900':env['JUDAS_SLEEP_TRACE']='1'
 runs.append({'name':name,'argv':argv,'environment_additions':env,'cwd':str(r),'binary_sha256':h(w/binary),'log_last_modified_utc':datetime.datetime.fromtimestamp((w/(name+'.log')).stat().st_mtime,datetime.timezone.utc).isoformat(),'start_utc':'not independently recorded for this original run; matched repeats have exact timestamps in MATCHED_CACHE_REPEATS.json'})
(w/'RUNS.json').write_text(json.dumps(runs,indent=2)+'\n')
# Validate utility syntax without starting another workload.
syntax=[]
for p in sorted(w.rglob('*.py')):
 ast.parse(p.read_text(),filename=str(p));syntax.append(str(p.relative_to(w)))
(w/'UTILITY_SYNTAX.json').write_text(json.dumps({'parsed':syntax,'count':len(syntax),'scope':'Python AST parsing only; not an engine regression suite'},indent=2)+'\n')
# Durable compact evidence excludes objects/build archives/runtime binaries.
def copy(p,rel):
 d=e/rel;d.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,d)
def zipped(p,rel):
 d=e/(str(rel)+'.gz');d.parent.mkdir(parents=True,exist_ok=True)
 with open(p,'rb')as src,open(d,'wb')as out:
  with gzip.GzipFile(filename='',mode='wb',fileobj=out,mtime=0)as z:shutil.copyfileobj(src,z)
for p in sorted(w.glob('*')):
 if p.is_file()and p.suffix in ['.json','.py','.log','.md','.txt']:copy(p,Path('metadata')/p.name)
for group in ['sampler','response','ccd','events']:
 for p in sorted((w/group).rglob('*')):
  if p.is_file()and p.suffix in ['.cpp','.h','.py','.md','.txt','.json']and'__pycache__'not in str(p):copy(p,Path('observers')/p.relative_to(w))
for name in run_names:
 for p in sorted((w/name).rglob('*')):
  if not p.is_file():continue
  rel=Path('runs')/name/p.relative_to(w/name)
  if p.suffix=='.csv':zipped(p,rel)
  elif p.suffix in ['.json','.txt','.log']:copy(p,rel)
 zipped(w/(name+'.log'),Path('runs')/name/'run.log')
for folder in ['final-parity900','sampler-comparisons-all900']:
 for p in (w/folder).glob('*.json'):copy(p,Path('parity')/folder/p.name)
for p in (w/'buildsrc').glob('*'):
 if p.suffix in ['.cpp','.h']:copy(p,Path('as-built-sources')/p.name)
copy(w/'baseline-snapshot/FROZEN.json',Path('baseline/FROZEN.json'));copy(w/'baseline-snapshot/source-inputs.tar.gz',Path('baseline/source-inputs.tar.gz'))
print(json.dumps({'source_inputs_verified':len(frozen['source_inputs']),'run_count':len(run_names),'evidence':str(e),'measurements':str(w/'MEASUREMENTS.json')},indent=2))
