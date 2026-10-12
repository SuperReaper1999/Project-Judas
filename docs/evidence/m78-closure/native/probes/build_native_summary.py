#!/usr/bin/python3
"""Summarize only executed native receipts; preserve failed setups separately."""
from pathlib import Path
import json,hashlib,re
R=Path.cwd();E=R/'docs/evidence/m78-closure/native';B=R/'.cache/m78-closure/native-binaries'
def rd(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def relative(p):return p.relative_to(R).as_posix()
def counts(run):
 stem=run if run.startswith('m78-closure-')else'm78-closure-'+run
 p=E/(stem+'.stdout.log')
 if not p.exists():return None,None,None
 txt=p.read_text(encoding='utf-8',errors='replace')
 rows=re.findall(r'(?:SUMMARY\s+)?(\d+)\s+checks[, ]+\s*(\d+)\s+failures',txt)
 if rows:a,b=map(int,rows[-1]);return a,b,relative(p)
 rows=re.findall(r'(\d+)\s+checks?\s+(\d+)\s+failures',txt)
 if rows:a,b=map(int,rows[-1]);return a,b,relative(p)
 rows=re.findall(r'Rigid motion checks\s+(\d+)\s+PASS',txt)
 if rows:return int(rows[-1]),0,relative(p)
 for line in txt.splitlines()[::-1]:
  if line.startswith('{'):
   try:j=json.loads(line)
   except ValueError:continue
   if j.get('kind')=='summary':return j.get('checks'),j.get('failures'),relative(p)
 return None,None,relative(p)
scope={
'physics_continuation':'112 actual joint continuation, completed contact publication, independent query isolation, malformed/newer/missing context rejection, real moving static-anchor pose/velocities, reduced states and offset motor reconstruction checks. Direct physics/world mechanisms, not a full CharacterLab cold trajectory oracle.',
'publication_isolation':'Direct restored-contact publication isolation across an independent CharacterMotor query/update, including the first resumed production boundary.',
'balance_lab':'Original M77 step/radial/lifecycle plus fresh ordinary standing/recovery cold pairs. Original standing() reads cached controller observation; separate actual body/current-contact witnesses below strengthen endpoint scope.',
'character_retry':'Original M70 CharacterLab baseline98, fresh cold writer10/read14: restored partial/full authority and finite pose. Not exact full articulated cold-trajectory parity; static-anchor/motor actual round-trip is independently checked by physics_continuation.',
'mixed_balance':'Original M77 public driver with one distant independently updated ordinary motor. Real cold writer/read unchanged settings/timestep/endpoint acceptance.',
'mixed_endpoint_v2':'Actual-body observer initial cold pair passed fixture checks. Writer lacked observer-only continue-write environment, so this pair alone has no uninterrupted endpoint equality claim; later proper paired observation below is authoritative for equality.',
'mixed_endpoint_pair_v2':'Same exact compiled actual-body observer reused; uninterrupted continuation versus fresh process from same save. All durable physical records and current contacts compared exactly after excluding only fresh native handles and canonical ordering. No thresholds added.',
'ordinary_actual_endpoints':'Same final actual-body observer reads byte-exact copies of fresh ordinary standing/recovery snapshots, derives actual physical standing from body pose/current contacts, independent of cached observation. Does not assert uninterrupted trajectory equality.',
'remaining_m77':'Original legacy a106 snapshot read remains failing13/1; new-format fresh corrective writer/read24/0+13/0 passes. Legacy expectedExit=1 is preserved evidence, not a pass.',
'signal_application_final':'Same final-source driver ran against stale historical guest lab.js without valid guard. Actual write10/1, read4/3 retained as stale verification-content failure; no current engine defect inferred. Exact current project copied separately and rerun without recompilation.',
'editor_v3':'Actual final-source editor edit/duplicate/undo/Play-Stop/save smoke with exact frozen unchanged editor objects and current physics/persistence replacements. Native softwareGL functional only.',
'runtime':'Final runtime compiled/linked and byte archived. Linking alone is not behavioural validation; moved-package smoke is separate.',
}
scoped_ids={'character_retry':'character_lab','gravity_retry':'gravity','mixed_endpoint_pair_v2':'mixed_physical_endpoint','ordinary_actual_endpoints':'ordinary_physical_endpoint','remaining_m77':'legacy_and_new_cold'}
cases=[];binary_rows=[]
for p in sorted(E.glob('*-build.json')):
 m=rd(p)
 if not isinstance(m.get('runs'),list):continue
 cid=m.get('case')or p.name.removeprefix('m78-closure-').removesuffix('-build.json')
 runs=[]
 for row in m['runs']:
  ident=row['run'];checks,failures,log=counts(ident)
  runs.append({'run':ident,'exitCode':row['exitCode'],'expectedExitCode':row.get('expectedExitCode'),'checks':checks,'failures':failures,'stdout':log,'status':'failed'if row['exitCode']or failures else'passed_exit_only'if checks is None else'passed_assertions'})
 cases.append({'id':scoped_ids.get(cid,cid),'actualBuildId':cid,'receipt':relative(p),'executedRuns':runs,'scope':scope.get(cid,m.get('scope','Existing focused fixture exactly as run; do not expand to an entire capability group.')),'executableSHA256':m.get('executableSHA256',m.get('reusedExecutableSHA256')),'linkReplacedMembersNotExtracted':m.get('frozenReplacedMembersNotExtracted'),'mapOmittedOnlyForStorage':m.get('diagnosticMapOmittedForBoundedStorage',False)})
 for row in m.get('generatedFiles',[]):
  host=B/cid/row['name']
  assert host.exists(),str(host)
  if host.exists():assert host.stat().st_size==row['bytes']and sha(host)==row['sha256'];binary_rows.append({'receipt':relative(p),'guestPath':row['path'],'hostPath':relative(host),'bytes':row['bytes'],'sha256':row['sha256']})
current=E/'m78-closure-current-signals.json'
if current.exists():
 m=rd(current);runs=[]
 for row in m['runs']:
  checks,failures,log=counts(row['run']);runs.append({'run':row['run'],'exitCode':row['exitCode'],'checks':checks,'failures':failures,'stdout':log,'status':'passed_assertions'if row['exitCode']== 0 and failures== 0 else'failed'})
 cases.append({'id':'signal_application','receipt':relative(current),'executedRuns':runs,'scope':m['scope']+' Actual fixed/UI lane, modal/zero-step pause, safe repeated deletion, adopted region/subscription lifecycle and fresh-process save restoration. No controller hardware or OS focus claim.','executableSHA256':m['executableSHA256'],'currentProjectManifest':relative(E/'CURRENT_SIGNALS_INPUTS.json')})
moved=E/'m78-closure-moved-package-v2.json'
if moved.exists():
 m=rd(moved);cases.append({'id':'moved_runtime','receipt':relative(moved),'executedRuns':[{'run':'moved-package-v2','exitCode':m['exitCode'],'checks':None,'failures':None,'status':'passed_exit_only'if m['exitCode']== 0 else'failed'}],'scope':m['scope'],'executableSHA256':m['runtimeSHA256']})
engine=rd(E/'m78-closure-engine.json');freeze=rd(R/'docs/evidence/m78-closure/final-build/BUILD.json')
sources=[]
for path,expected in freeze['sourceSHA256'].items():
 p=R/path;actual=sha(p);assert actual==expected,(path,actual,expected);sources.append({'path':path,'bytes':p.stat().st_size,'sha256':actual})
archive=R/'.cache/m78-cold-boundary/native-binaries/final2-repair/judas_engine.lib';assert archive.stat().st_size==engine['archive']['bytes']and sha(archive)==engine['archive']['sha256']
objs=[]
for row in engine['objects']:
 p=B/Path(row['path'].replace('\\','/')).name;assert p.stat().st_size==row['bytes']and sha(p)==row['sha256'];objs.append({'source':row['source'],'sourceSHA256':row['sourceSHA256'],'guestPath':row['path'],'hostPath':relative(p),'bytes':row['bytes'],'sha256':row['sha256']})
proof={'scope':'Verified final production source freeze, lossless host native archive/objects and all present generated executables/objects at final closure. Explicit current objects precede unchanged frozen archive; old PW/WP members are not extracted. No full rebuild/archive identity substitution or cross-platform bit-parity claim.','productionSources':sources,'archive':{**engine['archive'],'hostPath':relative(archive)},'replacementObjects':objs,'generatedFilesVerified':binary_rows}
(E/'FINAL_BINARY_IDENTITY.json').write_text(json.dumps(proof,indent=2)+'\n')
outstanding=[
 {'scope':'SaveStreaming Application write/read/read-b on final native source','status':'not_executed','reason':'Bounded isolated guest storage and sequential closure budget; lower-level streaming82 and world-save27 reran, plus actual signal region/save/reload route17+6, but they are not substitutes for this distinct fixture.'},
 {'scope':'New closure NavigationInputRace, ResolvedPrefabModPolicy and StreamingMotorPublication audit observers','status':'not_executed_native','reason':'Linux-only new audit observations; native historical narrower suites are not promoted to this behavioural coverage.'},
 {'scope':'New closure tiny save-exclusion audit probe','status':'not_executed_native','reason':'Separate newly selected Linux audit contract; not silently covered by another fixture.'},
 {'scope':'Fresh final-source export generator invocation','status':'not_executed_native','reason':'Final native runtime tested in byte-verified previously exported ordinary content; prior export receipt retained separately.'},
 {'scope':'Full M70 cold articulated trajectory equality','status':'not_demonstrated','reason':'CharacterLab pair validates authority/finite pose; actual static anchor/motor reconstruction is tested directly, not every M70 body over a resumed trajectory.'},
 {'scope':'FLU-001 legacy PBF settling correction','status':'deferred_unchanged','reason':'Original failures and separate settling-contract issue retained; no fluid implementation/threshold/research modifications or new fluid acceptance claim.'},
 {'scope':'Windows physical GPU/hardware and performance acceptance','status':'outstanding','reason':'Unchanged native Windows10 VM/softwareGL is functional validation only; historical timing diagnostic remains, Linux host is performance reference.'}
]
tooling=[
 {'id':'editor-link-storage','result':'first LNK1106 disk full and duplicated PW/WP LNK4042 preserved; duplication corrected only in verification response before successful fresh link','evidence':'m78-closure-editor-link.log'},
 {'id':'editor-preflight','result':'conservative storage preflight declined before second editor link; no runtime failure','evidence':'m78-closure-editor_v2-error.txt'},
 {'id':'editor-cleanup-pattern','result':'editor passed, cleanup name regex did not accept digit3; archival cleanup repaired without simulation rerun'},
 {'id':'publication-artifact-sharing','result':'fixture8/0; upload sharing lock corrected by disposed process and bounded artifact transfer retry, no simulation rerun'},
 {'id':'observer-include','result':'first observer compile lacked nlohmann include path; fixed verification recipe only, same unmodified observer'},
 {'id':'observer-metadata-BOM','result':'first paired observation JSON decode rejected before simulation; corrected file download decoding, original failure retained'},
 {'id':'partial-M77-matrix','result':'first seven actual M77 runs passed, storage blocked legacy fixture copy before test; remaining runs used same exact executable and new namespace'},
 {'id':'empty-archive-helper','result':'zero disk created empty helper and wrapper exited0; empty helper retained as tooling failure, later wrappers stop on errors; no pass claimed'},
 {'id':'single-item-archive-shape','result':'host post-result archival parser normalized PowerShell single-item object to list; no test rerun'},
 {'id':'gravity-link-storage','result':'first LNK1106 before execution; failed MAP archived, link-only same-object retry passed70/0'},
 {'id':'character-link-storage','result':'first LNK1106 before execution; failed MAP archived, link-only same-object retry with diagnostic MAP omitted passed98/0+10/0+14/0'},
 {'id':'stale-signals-fixture','result':'actual old guest project failure write10/1/read4/3 preserved; lab.js missing previously accepted valid guard; exact current copied project passes17/0+6/0 with same binary'},
 {'id':'moved-package-preflight','result':'first conservative 4MiB storage reserve declined before any package creation/launch; second receipt, if present, uses approved 2MiB tooling reserve and unchanged runtime/harness','evidence':'m78-closure-moved-package-error.txt'}
]
summary={'scope':'Case-scoped final native Windows functional closure only; not comprehensive M78 certification or Windows hardware/performance certification. No full suite, runtime/source edits, commits, pushes or VM configuration changes.','cases':cases,'exactEngineIdentity':{**proof,'receipt':relative(E/'FINAL_BINARY_IDENTITY.json')},'physicalEndpointComparisons':[{'path':relative(E/'ACTUAL_ENDPOINT_BY_ENTITY.json'),'scope':'All13durablebodies and16completedcontacts exact within native cold/uninterrupted pair after fresh native handle exclusion; raw record inequality preserved.'},{'path':relative(E/'ORDINARY_ACTUAL_ENDPOINTS.json'),'scope':'Actual current physical standing for ordinary standing and corrective recovery cold reads; no uninterrupted parity claim.'}],'outstandingScopes':outstanding,'toolingFailures':tooling,'inheritedPriorSourceReceipts':[{'path':'docs/evidence/m78-cold-boundary/native-final-narrow-wp','scope':'Prior-source native matrix/export evidence retained. Not relabeled as a new final-source execution.'}],'warningReview':relative(E/'WP_WARNING_REVIEW.json'),'vmConfiguration':'Windows10,4vCPU,6GiB,existing40GBdisk unchanged; GALLIUM_DRIVER=llvmpipe; native functional only.','movedContentIdentity':relative(E/'MOVED_CONTENT_IDENTITY.json')if moved.exists()else None,'movedPackagePending':not moved.exists()}
(E/'SUMMARY.json').write_text(json.dumps(summary,indent=2)+'\n');print('Native summary cases',len(cases),'generated byte records',len(binary_rows),'moved pending',not moved.exists())
