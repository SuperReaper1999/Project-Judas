#!/usr/bin/env python3
"""Algebraic review of existing native target receipts, not a runtime test.

Hamilton quaternions are [w,x,y,z]. No source, fixture, threshold or solver is
changed. Grounded correction is inferred from the actual stance ankle command
and free hip/knee chain, using the verified identity rest orientations and
plantedOrientationWeight=0 of this exact fixture. This is not a claim that its
controller is correct, or that another feedback policy repairs continuation.
"""
from pathlib import Path
import hashlib
import json
import math
import struct

base = Path("docs/evidence/m78-cold-boundary")
snapshot_path = base / "m78-diagnostic-boundary-v1-source-physical-before-save.json"
journal_path = base / "m78-diagnostic-boundary-v1-source-joint-boundary.jsonl"
trace_path = base / "m78-diagnostic-boundary-v1-source-diagnostic-uninterrupted-trace.jsonl"
snapshot = json.loads(snapshot_path.read_text())
settings = json.loads(snapshot["settings"])
assert settings["plantedOrientationWeight"] == 0
assert all(m["restRotation"] == [1, 0, 0, 0] for m in snapshot["mapped"])
assert settings["legLeanFraction"] == settings["trunkLeanFraction"] == 1
assert snapshot["history"]["referenceTime"] == settings["referenceSeconds"]

def multiply(a, b):
    w,x,y,z=a; v,i,j,k=b
    return [w*v-x*i-y*j-z*k,w*i+x*v+y*k-z*j,
            w*j-x*k+y*v+z*i,w*k+x*j-y*i+z*v]

def inverse(q):
    n=sum(x*x for x in q)
    return [q[0]/n,-q[1]/n,-q[2]/n,-q[3]/n]

def normalized(q):
    n=math.sqrt(sum(x*x for x in q));return [x/n for x in q]

def angle(a,b):
    d=normalized(multiply(inverse(a),b))
    return 2*math.atan2(math.sqrt(sum(x*x for x in d[1:])),abs(d[0]))

def reconstruct(root, joints):
    result={joints[0]["bodyB"]:root}
    for j in joints:
        result[j["bodyA"]]=multiply(result[j["bodyB"]],inverse(j["drive"]["target"]))
    return result

def joints_at(path):
    result={}
    for line in path.open():
        row=json.loads(line)
        if row.get("kind")=="joint_begin" and row["prepare"]==1:
            result.setdefault(row["tick"],[]).append(row)
    return result

class Reader:
    def __init__(self, data):self.data=data;self.offset=0
    def values(self, format):
        result=struct.unpack_from('<'+format,self.data,self.offset)
        self.offset+=struct.calcsize('<'+format)
        return result[0] if len(result)==1 else list(result)
    def string(self):
        count=self.values('I');value=self.data[self.offset:self.offset+count]
        self.offset+=count;return value
    def targets(self):return [self.values('ffff') for _ in range(self.values('I'))]

def balance_history(path, owner):
    envelope=Reader(path.read_bytes());assert envelope.string()==b'JudasGameSave'
    envelope.string();payload=Reader(envelope.string());chunks={}
    for _ in range(payload.values('I')):
        name=payload.string().decode();version=payload.values('I');chunks[name]=(version,payload.string())
    assert chunks['physical-balance'][0]==1
    record=Reader(chunks['physical-balance'][1]);result=None
    for _ in range(record.values('I')):
        actor=record.values('Q');configured=record.values('B');record.string();pending=record.values('B')
        if pending:record.values('iBBB')
        if not configured:continue
        begin=record.offset
        h=dict(zip(['task','progress','stepping','effort','hasStood'],record.values('iiBBB')))
        h.update(zip(['elapsed','settled','referenceTime'],record.values('fff')))
        h['frameReady']=record.values('B');h['rootReference']=record.values('ffff');h['supportReference']=record.values('ffff')
        h['initialTargets']=record.targets();h['postStepTargets']=record.targets()
        h.update(zip(['postStepTime','swing','lastSwing'],record.values('fii')))
        for key in ['startLocal','landingLocal','landedLocal','stanceLocal','rootStartLocal','comStartLocal']:h[key]=record.values('fff')
        h['swingTime'],h['transferTime']=record.values('ff');h['lifted']=record.values('B');h['landings']=record.values('I');h['reason']=record.string().decode()
        h['frameSupport'],h['plannedSupport'],h['stanceSupport']=record.values('QQQ')
        h['historySHA256']=hashlib.sha256(record.data[begin:record.offset]).hexdigest()
        if actor==owner:result=h
    assert record.offset==len(record.data) and result is not None
    return result

def add(a,b):return [x+y for x,y in zip(a,b)]
def subtract(a,b):return [x-y for x,y in zip(a,b)]
def scale(a,s):return [x*s for x in a]
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def length(a):return math.sqrt(dot(a,a))
def tangent(a):return [a[0],0,a[2]]
def rotate(q,v):return multiply(multiply(q,[0,*v]),inverse(q))[1:]

traces=[json.loads(line) for line in trace_path.open()]
joints_by_tick=joints_at(journal_path)
save_path=base/'m78-diagnostic-boundary-v1-source-m77.save'
history=balance_history(save_path,snapshot['owner'])
assert history['historySHA256']==snapshot['historySHA256']

upright=snapshot["history"]["rootReference"]
first=traces[0]
assert first["contacts"] == [0,0]
# No accepted contacts => COM-position term is zero; gravity is ordinary +Y-up.
vx,_,vz=first["velocity"]
response=settings["velocityResponse"]
tilt=[-response*vz,0,response*vx]
size=math.sqrt(sum(x*x for x in tilt));lean=min(size,settings["maximumLean"])
correction=[math.cos(lean/2),*(x/size*math.sin(lean/2) for x in tilt)]
joints=joints_by_tick[0]
desired=reconstruct(multiply(correction,upright),joints)
without={body:multiply(inverse(correction),q) for body,q in desired.items()}
first_comparison=[]
for j in joints:
    target=multiply(inverse(without[j["bodyA"]]),without[j["bodyB"]])
    first_comparison.append({"jointOrdinal":j["jointOrdinal"],"bodyA":j["bodyA"],
        "bodyB":j["bodyB"],"angularDifferenceRadians":angle(j["drive"]["target"],target),
        "actualTarget":j["drive"]["target"],"removedCommonCorrectionTarget":target})

# The native bounded journal records only the initial boundary. Use a separate,
# clearly identified Linux continuation for the actual contacting-foot example.
# Preserve its exact small receipt/trace beside the analysis for portability.
linux_source=Path('.cache/m78-cold-boundary/row-journal/source')
linux_snapshot_path=base/'target-feedback-linux-physical-before-save.json'
linux_trace_path=base/'target-feedback-linux-trace.jsonl'
for source,target in [(linux_source/'physical-before-save.json',linux_snapshot_path),
                      (linux_source/'diagnostic-uninterrupted-trace.jsonl',linux_trace_path)]:
    if target.exists():assert target.read_bytes()==source.read_bytes()
    else:target.write_bytes(source.read_bytes())
linux_snapshot=json.loads(linux_snapshot_path.read_text())
linux_trace=[json.loads(line) for line in linux_trace_path.open()]
linux_journal_path=base/'linux-source-joint-boundary.jsonl'
linux_joints=joints_at(linux_journal_path)
ground_tick=next(row["frame"] for row in linux_trace[:8]
    if row["contacts"][0]>0 and row["swing"]==1)
joints=linux_joints[ground_tick]
by_ordinal={j["jointOrdinal"]:j for j in joints}
# Exact authored parent chain is root -> left upper -> left lower -> left foot.
hip,knee,ankle=(by_ordinal[i] for i in (0,1,2))
actual_foot=ankle["a"]["orientation"]
root_with_c=multiply(multiply(multiply(actual_foot,ankle["drive"]["target"]),
    knee["drive"]["target"]),hip["drive"]["target"])
inferred_c=multiply(root_with_c,inverse(linux_snapshot['history']['rootReference']))
desired=reconstruct(root_with_c,joints)
ground_comparison=[]
for j in joints:
    parent_no_c=multiply(inverse(inferred_c),desired[j["bodyB"]])
    if j["jointOrdinal"]==2:
        # Grounded ankle target replaces the authored foot pose with its ACTUAL
        # contacting orientation at weight zero; that orientation is not rotated
        # with the desired parent and therefore the correction does not cancel.
        target_no_c=multiply(inverse(actual_foot),parent_no_c)
    else:
        child_no_c=multiply(inverse(inferred_c),desired[j["bodyA"]])
        target_no_c=multiply(inverse(child_no_c),parent_no_c)
    ground_comparison.append({"jointOrdinal":j["jointOrdinal"],
        "groundedAnkleOverride":j["jointOrdinal"]==2,
        "angularDifferenceRadians":angle(j["drive"]["target"],target_no_c),
        "actualTarget":j["drive"]["target"],"removedCommonCorrectionTarget":target_no_c})

# Quantify the reference pose's world endpoint error caused by rotating all
# orientations after an IK solution, while keeping its root reference position
# and foothold world point fixed. The actual physical foot is NOT teleported.
support=next(b for b in snapshot['bodies'] if b['entity']==history['plannedSupport'])
support_position=support['position'];support_rotation=support['rotation']
world=lambda v:add(support_position,rotate(support_rotation,v))
start,landing,stance=(world(history[k]) for k in ['startLocal','landingLocal','stanceLocal'])
time=first['swingTime']/settings['stepSeconds'];time=max(0,min(1,time));blend=time*time*(3-2*time)
targets=[stance,add(add(scale(start,1-blend),scale(landing,blend)),[0,settings['stepHeight']*math.sin(math.pi*blend),0])]
transfer=max(0,min(1,history['transferTime']/settings['stepSeconds']));transfer=transfer*transfer*(3-2*transfer)
reference=add(add(world(history['rootStartLocal']),scale(tangent(subtract(stance,world(history['comStartLocal']))),transfer)),scale(tangent(subtract(landing,start)),.5*blend))
joint_by_ord={j['jointOrdinal']:j for j in joints_by_tick[0]}
for foot,ordinals in enumerate([(0,1,2),(3,4,5)]):
    hip,knee,ankle=(joint_by_ord[i] for i in ordinals)
    end=add(targets[foot],rotate(upright,ankle['anchorA']))
    hip_offset=rotate(upright,hip['anchorB'])
    reach=(length(subtract(knee['anchorB'],hip['anchorA']))+length(subtract(ankle['anchorB'],knee['anchorA'])))*(.9999-.0299*transfer)
    horizontal=length(tangent(subtract(subtract(end,reference),hip_offset)))
    assert horizontal<reach
    allowed=end[1]-hip_offset[1]+math.sqrt(reach*reach-horizontal*horizontal)
    if reference[1]>allowed:reference[1]=allowed
endpoint_error=[]
for foot,target in enumerate(targets):
    rotated_endpoint=add(reference,rotate(correction,subtract(target,reference)))
    endpoint_error.append({'foot':foot,'originalReferenceFootWorld':target,
        'postCommonRotationImpliedFootWorld':rotated_endpoint,
        'difference':subtract(rotated_endpoint,target),
        'distanceMeters':length(subtract(rotated_endpoint,target))})

result={
    "scope":"Algebraic analysis of actual target receipts; no runtime or test execution and no proposed controller repair",
    "firstContactFreeBoundary":{"tick":0,"contacts":first["contacts"],
        "correctionRadians":lean,"correction":correction,"joints":first_comparison},
    "firstContactingStanceBoundary":{"platform":"Linux separate continuation, not a native before/after comparison","tick":ground_tick,"contacts":linux_trace[ground_tick]["contacts"],
        "inferredCorrection":inferred_c,"inferredCorrectionRadians":angle([1,0,0,0],inferred_c),
        "inferenceAssumptions":"Identity rest orientations; same fixed uniform-up/yaw reference; initial ramp complete; no post-step blend; stance ankle weight zero",
        "joints":ground_comparison},
    "firstNativeReferenceGeometry":{"rootReferencePosition":reference,
        "correctionRadians":lean,"footReferenceErrors":endpoint_error,
        "meaning":"The post-IK all-orientation correction rotates the implied reference skeleton endpoint around its root away from the fixed authored world foothold. This is reference geometry, not an actual observed displacement or new physics command.",
        "preservedAcceptanceTouchdownToleranceMeters":settings['touchdownTolerance']},
    "authoredPolicy":{"legLeanFraction":settings["legLeanFraction"],
        "trunkLeanFraction":settings["trunkLeanFraction"],
        "effect":"SwingReferences overwrites earlier fractional desired poses, and step phase applies full common correction. Reapplying the authored fractions would be identical for this exact 1/1 fixture and cannot explain its failure."},
    "inputs":[{"path":str(p),"sha256":hashlib.sha256(p.read_bytes()).hexdigest()}
        for p in [snapshot_path,journal_path,trace_path,save_path,linux_snapshot_path,linux_trace_path,linux_journal_path]],
}
output=base/"balance-target-feedback-analysis.json"
output.write_text(json.dumps(result,indent=2,allow_nan=False)+"\n")
print(json.dumps({"path":str(output),"firstMaxTargetDifferenceRadians":max(x["angularDifferenceRadians"] for x in first_comparison),
    "contactingStanceTick":ground_tick,"groundedJointDifferencesRadians":[x["angularDifferenceRadians"] for x in ground_comparison],
    "nativeFootReferenceErrorMeters":[x['distanceMeters'] for x in endpoint_error]},indent=2))
