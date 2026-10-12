#!/usr/bin/python3
"""Case-scoped native lifecycle handoff from actual completed receipts only."""
import hashlib,json,re
from pathlib import Path
R=Path.cwd();E=R/'docs/evidence/m78-lifecycle/native'
def rd(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def rel(p):return p.relative_to(R).as_posix()
def fp(p):return dict(path=rel(p),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
prepared=rd(E/'PREPARED_INPUTS.json');expected={r['path']:r['sha256']for r in prepared['replacements']}
expected.update({r['source']:r['sourceSHA256']for r in prepared['retainedReplacementObjects']})
scopes={
 'character_publication':'Actual authored motor creation/recreation, offsets, teleport, disabled/filter/tag publication and streaming private/publish/retire/revisit. Not runtime tag mutator behavior (separate deferred LIF-003).',
 'save_exclusion':'Actual retained/excluded/reincluded dynamic bodies, sleep/scene/contact ownership, strict malformed record rejection and retained mapped articulation restore.',
 'consumer_lifecycle':'Public Application prefab initialization/references, malformed required participant rejection without changing live physical/script/session state, subsequent fixed-step publication and valid restore lifecycle. No whole-prefab/save certificate.',
 'motor':'Original32 CharacterMotor assertions; unchanged fixture contains incidental1/10/100-motor timings, retained as VM diagnostics only, not performance acceptance.',
 'physics_continuation':'Original112 direct continuation contract assertions; unchanged fixture/settings.',
 'world_save':'Original normal persistence27 checks; strict restore and resumed physics.',
 'streaming':'Original streaming82 checks; retained runtime construction/lifetime correction.',
 'balance_lab':'Original M77 corrective step plus new-format cold-save writer/read. Cached controller endpoint acceptance supplemented by separate mixed actual-body observer.',
 'character_lab':'Original M70 new-format cold pair: actual mode authority and finite pose. Not full uninterrupted physical trajectory parity.',
 'mixed_endpoint':'Same actual-body observer: fresh-process restoration versus uninterrupted endpoint, durable entity identity, all actual body fields/current contacts; only fresh native handles and traversal order excluded.',
}
cases=[];identity=[]
for c in prepared['cases']:
 p=E/('m78-lifecycle-'+c['id']+'-build.json')
 if not p.exists():continue
 v=rd(p);objects={r['source']:r['sourceSHA256']for r in v['replacementObjects']};assert objects==expected,(c['id'],'production overlay mismatch')
 assert v['archive']['sha256']==prepared['baselineLibrary']['sha256']
 assert v['frozenReplacedMembersNotExtracted'] and v['diagnosticMapOmittedForBoundedStorage']
 runs=[]
 for r in v['runs']:
  output=(E/('m78-lifecycle-'+r['run']+'.stdout.log')).read_text(encoding='utf-8-sig')
  matches=re.findall(r'(?:SUMMARY|CHARACTER_PUBLICATION|M49|M59)\s+(\d+)\s+checks?\s+(\d+)\s+failures?',output,re.I)
  check,failed=map(int,matches[-1])if matches else (None,None)
  runs.append(dict(run=r['run'],exitCode=r['exitCode'],checks=check,failures=failed,seconds=r['seconds'],receipt=rel(p),scope=scopes[c['id']]))
 cases.append(dict(id=c['id'],receipt=rel(p),executedRuns=runs,scope=scopes[c['id']]))
 identity.append(dict(case=c['id'],executableSHA256=v['executableSHA256'],bytes=v['executableBytes'],sourceObjects=v['replacementObjects'],inputs=v['reusedOrFreshInputs'],frozenOldMembersNotExtracted=True,linkReceipt=rel(E/('m78-lifecycle-'+c['id']+'-link.log'))))
identityReport={'scope':'Actual linked native replacement objects exactly once before frozen archive; no new full archive or ABI/header change. Previous accepted PhysicsWorld object retained exactly.','preparedInputs':fp(E/'PREPARED_INPUTS.json'),'archive':prepared['baselineLibrary'],'expectedProductionSourceSHA256':expected,'binaries':identity}
(E/'FINAL_BINARY_IDENTITY.json').write_text(json.dumps(identityReport,indent=2)+'\n')
summary={'scope':'New native Windows functional lifecycle correction evidence only; VM unchanged and no Windows performance/hardware certification. No full suite.','startingHEAD':prepared['startingHEAD'],'cases':cases,'exactEngineIdentity':{'receipt':rel(E/'FINAL_BINARY_IDENTITY.json')},'physicalEndpointComparisons':[{'path':rel(E/'ACTUAL_ENDPOINT_BY_ENTITY.json'),'scope':scopes['mixed_endpoint']}]if(E/'ACTUAL_ENDPOINT_BY_ENTITY.json').exists()else [],'outstandingScopes':[scopes[c['id']]for c in prepared['cases']if not any(r['id']==c['id']for r in cases)]+['No Windows performance gate or physical GPU validation.','Native new runtime-tag mutator probe not executed; Linux LIF-003 reproduced and deferred.','Previously preserved legacy a106 snapshot/FLU-001 failures not reclassified by these lifecycle checks.','No new native SaveStreaming/Application signals/editor/moved-package rerun in this bounded matrix unless explicitly attached separately.'],'toolingFailures':[{'path':rel(E/'PREPARATION_BLOCK.json'),'scope':'Initial automatic approval review rejections; resolved by direct human compression/necessary-new-output cleanup permission. No engine assertion.'},{'path':rel(E/'PERMISSION_CHRONOLOGY.json'),'scope':'First authorized compression download failed because prior task transfer server stopped; archive untouched. Restored task server then authorized compression succeeded.'},{'path':rel(E/'HOST_RECEIPT_TIMING_FOLLOWUP.json'),'scope':'Brief world-save host receipt visibility delay interrupted host verification; durable native pass archived without test rerun.'}],'inheritedPriorSourceReceipts':[{'path':'docs/evidence/m78-closure/native/SUMMARY.json','scope':'Protected accepted previous correction closure evidence; not promoted to new final-source execution.'}],'vmConfiguration':'Established Windows10 4vCPU6GiB40GBdisk; llvmpipe software GL functional only.'}
(E/'SUMMARY.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({'cases':[(c['id'],c['executedRuns'])for c in cases],'binaryCount':len(identity)},indent=2))
