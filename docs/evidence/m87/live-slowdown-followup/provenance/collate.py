from pathlib import Path
import csv,gzip,hashlib,io,json,math,re,shutil,statistics,subprocess,datetime,difflib
ROOT=Path('/home/conner/Documents/GitHub/Project-Judas');OUT=ROOT/'docs/evidence/m87/live-slowdown-followup';TMP=Path('/tmp/m87-live-diagnostics')
records=[];missing=[]
def preserve(source,destination,compress=False,scope=''):
 source=Path(source);target=OUT/destination
 if not source.is_file():missing.append(str(source));return
 data=source.read_bytes();target.parent.mkdir(parents=True,exist_ok=True)
 if compress:
  buf=io.BytesIO()
  with gzip.GzipFile(filename='',mode='wb',fileobj=buf,compresslevel=6,mtime=0)as f:f.write(data)
  stored=buf.getvalue()
 else:stored=data
 if target.exists()and target.read_bytes()!=stored:raise RuntimeError('Refusing to replace differing evidence: '+str(target))
 target.write_bytes(stored)
 records.append({'path':str(target.relative_to(OUT)),'source':str(source),'source_bytes':len(data),'source_sha256':hashlib.sha256(data).hexdigest(),'stored_bytes':len(stored),'stored_sha256':hashlib.sha256(stored).hexdigest(),'compression':'gzip level6,mtime0,empty filename'if compress else'none','scope':scope,'source_mtime_utc':datetime.datetime.fromtimestamp(source.stat().st_mtime,datetime.timezone.utc).isoformat()})
def put(relative,data):
 target=OUT/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(data)
# Original frozen failure: extracted physics only, never core executable memory.
for name in ['original-core.json','original-core.txt','original-core-provenance.json','extract-original-core.gdb','original-core-predicate-tls.txt','original-core-all-predicate-tls.txt','original-core-predicate-delta.json','original-core-predicate-delta.txt','read-core-predicates.gdb','read-all-core-predicates.gdb','read-core-step-predicates.gdb','analyze-existing-geometry.py','geometry-cost-summary.json','geometry-cost-fingerprints.json']:
 preserve(TMP/name,'provenance/original-failure/'+name,scope='Historical original human-live failure; unfinished step counters, not a completed-step timing.')
preserve(TMP/'original-step-disassembly.txt','provenance/original-failure/original-step-disassembly.txt.gz',True,'Original binary assembly proving geometryBefore stack offset; no binary memory payload.')
for source in sorted(Path('/tmp/m87-pile-classification').iterdir()):
 if source.suffix in ['.md','.txt','.gdb','.json','.py']:preserve(source,'provenance/content-classification/'+source.name,scope='Read-only source/core classification; original report unchanged.')
# Original current demo fixture only; ordinary project content, not protected history.
for source,relative in [('/tmp/judas-m87-watch-ecpbqnfz/game.judasproj','game.judasproj'),('/tmp/judas-m87-watch-ecpbqnfz/Scenes/room1024.judas','Scenes/room1024.judas'),('/tmp/judas-m87-watch-ecpbqnfz/Assets/scripts/lab.js','Assets/scripts/lab.js'),(ROOT/'tools/CreatePhysicsStressLab.py','CreatePhysicsStressLab.py')]:
 preserve(source,'provenance/original-fixture/'+relative+'.gz',True,'Frozen content/reproduction fixture. Project paths and all physics settings retained.')
# Existing helper source/build commands; no binaries or .o/.a.
for subdir in ['original-helper','']:
 for name in ['live.cpp','build.sh','SHA256SUMS']:
  source=TMP/subdir/name
  if source.exists():preserve(source,'provenance/helpers/'+('original/'if subdir else'expanded/')+name,scope='Historical helper build inputs; absolute original paths retained. SHA records identity, not a reproduced binary claim.')
# Hardware/build facts already captured earlier; recorded date/scope explicit.
provenance=json.loads((ROOT/'docs/evidence/m87/baseline/provenance.json').read_text())
put('provenance/RECORDED_BUILD_HOST.json',json.dumps({'note':'Reused read-only hardware/build record from original M87 baseline. Commit is the older baseline, not the corrective candidate; do not relabel as a new capture. No environment variables collected.','source':'docs/evidence/m87/baseline/provenance.json','capturedUTC':provenance['capturedUTC'],'commit':provenance['commit'],'buildCache':provenance['buildCache'],'cpu':provenance['cpu']},indent=2)+'\n')
# Every real live-loop CSV case. Separate raw counts/times from profile-on runs.
cases=[];profile_summary=[]
base_rows=list(csv.DictReader((TMP/'dense-fixed-before/steps.csv').open()));base_digests={int(r['step']):r.get('trajectory_digest')for r in base_rows}
for folder in sorted(TMP.iterdir()):
 if not folder.is_dir()or not(folder/'steps.csv').exists():continue
 name=folder.name;log=TMP/(name+'.log');logtext=log.read_text()if log.exists()else''
 cfg=re.search(r'diagnostic swap=(\S+) hidden=(\S+) tour=(\S+) frame_seconds=(\S+) profile=(\S+) periodic_export=(\S+)',logtext)
 settings=dict(zip(['swap','hidden','tour','frame_seconds','profile','periodic_export'],cfg.groups()))if cfg else{}
 for source in sorted(folder.iterdir()):
  if source.suffix=='.csv':preserve(source,'raw/live-cases/'+name+'/'+source.name+'.gz',True,'Matched or live diagnostic data; see summary for profile/cadence and finite horizon. No FPS/60Hz certification implied.')
 if log.exists():preserve(log,'raw/live-cases/'+name+'/run.log',scope='Actual retained stdout/stderr. Full argv was not retained for every early case.')
 rows=list(csv.DictReader((folder/'steps.csv').open()));r={'case':name,'steps':len(rows),'profile':settings.get('profile','unknown'),'frame_seconds':settings.get('frame_seconds','unknown'),'tour':settings.get('tour','unknown'),'max_bodies':max((int(x['bodies'])for x in rows),default=0),'max_awake':max((int(x['awake'])for x in rows),default=0),'impact_events_sum':sum(int(x['impact_events'])for x in rows),'impact_queries_sum':sum(int(x['impact_queries'])for x in rows),'nonfinite_fields_max':max((int(x.get('nonfinite_fields')or'0')for x in rows),default=0)}
 for key in ['fixed_ms','total_physics_ms','broadphase_ms','narrowphase_ms','solver_ms']:
  values=sorted(float(x[key])for x in rows)
  r[key+'_median']=statistics.median(values)if values else 0
  r[key+'_p95']=values[max(0,math.ceil(.95*len(values))-1)]if values else 0
  r[key+'_max']=max(values,default=0);r[key+'_sum']=sum(values)
 comparable=bool(rows)and name.startswith('dense-fixed-')and all(x.get('trajectory_digest')for x in rows)
 mismatches=[int(x['step'])for x in rows if int(x['step'])in base_digests and x.get('trajectory_digest')!=base_digests[int(x['step'])]]if comparable else[]
 r['digest_baseline']='dense-fixed-before'if comparable else'not compared';r['digest_mismatches']=len(mismatches)if comparable else'';r['first_digest_mismatch']=mismatches[0]if mismatches else''
 r['simulated_seconds_if_fixed_1over60']=len(rows)/60 if settings.get('frame_seconds')=='0.0166667'else'';r['settings']=settings;cases.append(r)
 # Save only bounded relevant raw M56 captures; others remain in /tmp.
 if name in ['startup-watch-before','dense-early-before','dense-rotation-profile','dense-fixed-tight-query','dense-fixed-lazy','dense-fixed-lazy-prefix','dense-fixed-manifold']:
  source=folder/'profile-final.json'
  if source.exists():
   preserve(source,'raw/profiles/'+name+'.json.gz',True,'Raw M56 JSON; timing collection changes cost, keep separate from profiler-off timing cases.')
   d=json.loads(source.read_text());profile_summary.append({'case':name,'path':'raw/profiles/'+name+'.json.gz','diagnostics':d.get('diagnostics'),'frames':len(d.get('frames',[])),'frozen':d.get('frozen'),'capture_enabled':d.get('capture_enabled')})
preserve('/tmp/judas-m87-live-profile.json','raw/profiles/original-human-overflow.json.gz',True,'Original human-live profiler overflow: incomplete event coverage preserved; not valid fine-grained timing authority.')
d=json.loads(Path('/tmp/judas-m87-live-profile.json').read_text());profile_summary.append({'case':'original-human-overflow','path':'raw/profiles/original-human-overflow.json.gz','diagnostics':d['diagnostics'],'frames':len(d['frames'])})
put('raw/PROFILE_INDEX.json',json.dumps(profile_summary,indent=2)+'\n')
put('raw/CASE_SUMMARY.json',json.dumps(cases,indent=2)+'\n')
columns=[k for k in cases[0]if k!='settings']
out=io.StringIO();writer=csv.DictWriter(out,fieldnames=columns);writer.writeheader();writer.writerows({k:v for k,v in r.items()if k!='settings'}for r in cases);put('raw/CASE_SUMMARY.csv',out.getvalue())
# Full frozen source snapshots and scripts for all meaningful prototypes.
groups={
 'loop-bookkeeping':['/tmp/m87-loop-base.cpp','/tmp/m87-loop-scalability.cpp','/tmp/m87-loop-scalability.patch','/tmp/m87-loop-scalability-notes.md','/tmp/m87-prepare-loop-patch.py'],
 'truth-cache-proposal':['/tmp/m87-loop-base.cpp','/tmp/m87-step-truth-cache.cpp','/tmp/m87-step-truth-cache.patch','/tmp/m87-step-truth-cache-notes.md','/tmp/m87-prepare-truth-cache.py'],
 'manifold-cache-rejected':['/tmp/m87-manifold-cache-base.cpp','/tmp/m87-manifold-cache.cpp','/tmp/m87-manifold-cache.patch','/tmp/m87-manifold-cache-notes.md','/tmp/m87-prepare-manifold-cache.py'],
 'lazy-rejected':['/tmp/m87-before-lazy.cpp','/tmp/m87-lazy-prefix-rejected.cpp','/tmp/m87-tight-remaining-bound/PhysicsWorld.lazy.cpp','/tmp/m87-tight-remaining-bound/pending-lazy.diff','/tmp/m87-tight-remaining-bound/prepare_lazy.py','/tmp/m87-tight-remaining-bound/PROOF.md'],
 'query-workers-rejected':['/tmp/m87-parallel-base.cpp','/tmp/m87-parallel-prototype.cpp','/tmp/m87-parallel-prototype.patch','/tmp/m87-make-parallel-prototype.py','/tmp/m87-build-parallel-prototype.sh','/tmp/m87-parallel-live.cpp','/tmp/m87-parallel-prototype-review.txt','/tmp/m87-parallel-artifacts.sha256','/tmp/m87-parallel-archive-verification.json'],
 'initial-narrowphase':['/tmp/m87-narrow-base.cpp','/tmp/m87-narrow-prototype.cpp','/tmp/m87-narrow-prototype.patch','/tmp/m87-make-narrow-prototype.py','/tmp/m87-build-narrow-prototype.sh','/tmp/m87-narrow-live.cpp','/tmp/m87-narrow-prototype-review.txt','/tmp/m87-narrow-artifacts.sha256','/tmp/m87-narrow-archive-verification.json','/tmp/m87-make-production-narrow.py','/tmp/m87-production-narrow.cpp','/tmp/m87-production-narrow.patch'],
 'persistent-deadline-rejected':[str(p)for p in Path('/tmp/m87-persistent-lazy').iterdir()if p.suffix in ['.py','.cpp','.diff','.md','.json','.log']],
 'future-tree-rejected':[str(p)for p in Path('/tmp/m87-future-tree').iterdir()if p.suffix in ['.py','.cpp','.diff','.md','.json','.log']],
 'bounds-prototypes':['/tmp/m87-pre-tight-query.cpp','/tmp/m87-geometry-stage.cpp','/tmp/m87-tight-remaining-bound/RotatingBounds.h','/tmp/m87-tight-remaining-bound/TightRemainingBound.h','/tmp/m87-tight-remaining-bound/RotationTests.cpp','/tmp/m87-tight-remaining-bound/ContainmentTests.cpp'],
}
for group,sources in groups.items():
 for source in sources:
  p=Path(source);large=p.suffix=='.cpp'and p.stat().st_size>20000 if p.exists()else False
  preserve(source,'experiments/'+group+'/'+p.name+('.gz'if large else''),large,'Historical prototype input, including rejected/unmeasured work. Captured notes retain their original preparation-time status; EXPERIMENTS.md records later observed disposition.')
# Preserve build logs separately; no implementation/binary bundles.
for source in list(TMP.glob('*build*.log'))+list(Path('/tmp').glob('m87-*prototype*.log')):
 preserve(source,'experiments/build-logs/'+source.name,scope='Historical compile/link log; no new build executed for evidence collation.')
# Frozen solver diagnostic: sources, exact input, commands and raw results only.
impact=Path('/tmp/m87-impact-quality')
for p in sorted(impact.iterdir()):
 if p.is_file()and(p.suffix in ['.cpp','.h','.py','.json','.log','.txt','.bits']):
  preserve(p,'experiments/impact-quality/'+p.name,scope='Isolated diagnostic iteration experiment, not production solver policy. Historical binary hashes identify observed inputs; binaries deliberately omitted.')
for p in sorted((impact/'frozen-replay').glob('*.csv')):preserve(p,'experiments/impact-quality/frozen-replay/'+p.name,scope='Exact same frozen island input; original iteration endpoint bit-verified, alternative iterations deliberately change mathematics.')
# Shared solver dependencies used by frozen diagnostic, captured source exact hashes.
for name in ['ImpactSolver.h','ContactSolver.h','ContactSolver.cpp','RigidBody.h','Contacts.h','ContactPreparedGeometry.h']:
 preserve(ROOT/'src'/name,'experiments/impact-quality/dependencies/'+name,scope='Source dependency snapshot at collation. Compare recorded historical source hashes before assuming rebuild identity; later Contacts declaration-only additions may exist.')
# Build/source inventory explicitly excludes executables, archives and memory dumps.
manifest={'schema':1,'captured_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Copied raw data/source only under new M87 follow-up directory. Existing milestone evidence untouched. No core, executable, object, static archive, environment dump or credentials included.','head_at_collation':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'records':records,'missing_sources':missing,'summary_statistics':'Median=statistics.median; p95=nearest-rank ceil(.95*N); sums are observed simulated-step costs, not real-time throughput. Profile-on/off runs remain explicitly separated.','provenance_limit':'Most early prototype runs did not retain a per-run complete source/binary fingerprint. Existing recorded hashes are preserved, never backfilled as if captured then. Full argv was not retained for every early run. Frozen diagnostic capture does retain historical engine archive/source/binary hashes; no claim those binaries were reproduced during collation.'}
put('provenance/COPIED_ARTIFACT_MANIFEST.json',json.dumps(manifest,indent=2)+'\n')
print(json.dumps({'copied_files':len(records),'cases':len(cases),'stored_bytes':sum(r['stored_bytes']for r in records),'missing':missing},indent=2))
