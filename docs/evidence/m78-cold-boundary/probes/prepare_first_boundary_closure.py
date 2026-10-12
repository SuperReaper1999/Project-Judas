#!/usr/bin/env python3
"""Exact native first-boundary computational replay, DIAGNOSTIC ONLY.
The original physical state must match before any actual captured commands/seeds
are applied. This is NOT a persistence proposal or a synthetic controller effort.
"""
import pathlib,json,hashlib,shutil
r=pathlib.Path.cwd();ev=r/'docs/evidence/m78-cold-boundary';base=r/'.cache/m78-cold-boundary/row-journal-warm';out=r/'.cache/m78-cold-boundary/first-boundary-closure';out.mkdir(exist_ok=False)
p=ev/'m78-diagnostic-boundary-v1-source-joint-boundary.jsonl';records=[json.loads(x)for x in p.read_text().splitlines()];records=[x for x in records if x['tick']==0 and x['prepare']==1]
inputs=[x for x in records if x['kind']=='joint_begin'];free={x['body']:x['state']for x in records if x['kind']=='prepare_free_body'}
def f(x):
 text=repr(float(x));return text+'f'
def vec(v):return 'glm::vec3('+','.join(f(x)for x in v)+')'
def quat(v):return 'glm::quat('+','.join(f(x)for x in v)+')'
def boolean(x):return 'true'if x else'false'
s=(base/'JointSolver.cpp').read_text().replace('bjDiscardFirstWarm=false;','bjDiscardFirstWarm=false,bjReplayCapturedBoundary=false;')
old='bjTick=-1;const char* discard=';assert s.count(old)==1
s=s.replace(old,'bjTick=-1;const char* replay=std::getenv("JUDAS_BOUNDARY_REPLAY_CAPTURED_NATIVE");bjReplayCapturedBoundary=replay&&std::string(replay)=="1";const char* discard=')
block=['\n // Diagnostic exact computational-state closure only. Physical state unchanged.\n if(bjActive&&bjTick==0&&bjPrepare==0&&!impact&&bjReplayCapturedBoundary){',f'  if(inputs.size()!={len(inputs)})throw std::runtime_error("captured boundary joint count mismatch");']
for index,data in enumerate(inputs):
 block.append(f'  {{const auto& input=inputs[{index}];const auto& settings=input.state->settings;')
 block.append(f'   if(int(settings.type)!={data["type"]}||settings.anchorA!={vec(data["anchorA"])}||settings.anchorB!={vec(data["anchorB"])}||settings.frameA!={quat(data["frameA"])}||settings.frameB!={quat(data["frameB"])})throw std::runtime_error("captured boundary geometry mismatch");')
 for side in ['A','B']:
  state=free[data['body'+side]];member='a'if side=='A'else'b'
  checks=[f'body.position!={vec(state["position"])}',f'body.orientation!={quat(state["orientation"])}',f'body.linearVelocity!={vec(state["linearVelocity"])}',f'body.angularVelocity!={vec(state["angularVelocity"])}',f'body.inverseMass!={f(state["inverseMass"])}']
  for c in range(3):checks.append(f'body.inverseInertiaLocal[{c}]!={vec(state["inverseInertiaLocal"][c])}')
  block.append('   {const auto& body=*input.'+member+';if('+'||'.join(checks)+')throw std::runtime_error("captured boundary physical state mismatch");}')
 drive=data['drive'];block.append('   *input.warm={'+','.join(f(x)for x in data['warm'])+'};')
 block.append('   JointAngularDrive command;command.enabled='+boolean(drive['enabled'])+';command.target='+quat(drive['target'])+';command.stiffness='+f(drive['stiffness'])+';command.damping='+f(drive['damping'])+';command.maxTorque='+f(drive['maxTorque'])+';command.relativeAngularVelocity='+vec(drive['relativeAngularVelocity'])+';command.impulses='+vec(drive['impulses'])+';input.state->angularDrive=command;}')
block.append('  bjWrite({{"kind","actual_native_first_boundary_replayed"},{"sourceSnapshotSHA256","a106ce212416a510ec0470d2cf7abe0fc96fccdfec202c95e5209c9e56c4379b"},{"sourceJournalSHA256","'+hashlib.sha256(p.read_bytes()).hexdigest()+'"},{"scope","Actual recorded native commands and numerical seeds, after exact physical/geometry equality guard; one diagnostic boundary only. No persistence implementation."}});\n }\n')
old='void JointSolver::Prepare(const std::vector<JointInput>& inputs,float dt,bool warm,bool impact){';assert s.count(old)==1;s=s.replace(old,old+'\n'.join(block))
(out/'JointSolver.cpp').write_text(s);shutil.copyfile(base/'BalanceLabBoundaryObserver.cpp',out/'BalanceLabBoundaryObserver.cpp')
d=json.loads((base/'IDENTITY.json').read_text());d['scope']='Diagnostic only, first exact nativea106 computational-state closure: actual recorded first Prepare angular commands and prior warm seeds applied only AFTER exact current body pose/velocity/mass/inertia/jointgeometry equality. Subsequent original numerical method unchanged. Normal first controller phase/clock requires separately identified PB overlay; this is not a blind production cache serialization proposal.';d['recordedSourceJournalSHA256']=hashlib.sha256(p.read_bytes()).hexdigest();d['overlaySHA256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in out.glob('*.cpp')};(out/'IDENTITY.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d['overlaySHA256'],indent=2))
