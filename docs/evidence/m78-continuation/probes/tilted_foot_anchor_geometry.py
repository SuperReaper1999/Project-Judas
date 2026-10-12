#!/usr/bin/env python3
"""Read-only analytic anchor observer; no engine, fixture or threshold changes."""
import hashlib,json,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[4]

def rotate(axis,angle,p):
 n=math.sqrt(sum(x*x for x in axis));a=[x/n for x in axis]
 c,s=math.cos(angle),math.sin(angle);dot=sum(x*y for x,y in zip(a,p))
 cross=[a[1]*p[2]-a[2]*p[1],a[2]*p[0]-a[0]*p[2],a[0]*p[1]-a[1]*p[0]]
 return [c*p[i]+s*cross[i]+(1-c)*dot*a[i] for i in range(3)]

def length(p):return math.sqrt(sum(x*x for x in p))

def subtract(a,b):return [x-y for x,y in zip(a,b)]
anchor=[0,.15,0];contact=[0,-.12,0];tilt=.6
physical=rotate([1,0,0],tilt,anchor)
old=anchor[:]
world_axis=[1,2,3];world_angle=.9
arbitrary_physical=rotate(world_axis,world_angle,physical)
arbitrary_old=rotate(world_axis,world_angle,old)
result={
 'purpose':'Independent rigid-transform anchor geometry; diagnoses a reference mismatch, not a new success threshold or proof of controller repair',
 'rule':'world joint anchor = foot origin + foot rotation * local anchor; original fixture pivot offset is zero',
 'same_existing_setting':'plantedOrientationWeight = 0 means existing ankle drive selects the observed contacting foot orientation',
 'example':{'foot_rotation_axis':[1,0,0],'foot_rotation_radians':tilt,'ankle_anchor_local_m':anchor,'original_upright_offset_m':old,'actual_rigid_anchor_offset_m':physical,'old_reference_error_m':length(subtract(old,physical)),'consistent_reference_error_m':0.0},
 'arbitrary_setup_rotation':{'axis':world_axis,'radians':world_angle,'old_upright_offset_m':arbitrary_old,'physical_offset_m':arbitrary_physical,'same_error_m':length(subtract(arbitrary_old,arbitrary_physical))},
 'contact_offset_geometry':{'local_m':contact,'physical_offset_m':rotate([1,0,0],tilt,contact),'upright_m':contact,'error_m':length(subtract(contact,rotate([1,0,0],tilt,contact))),'note':'Recorded only. Overlay does not change touchdown acceptance or root-height policy.'},
 'production_files':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['src/PhysicalBalance.cpp','src/PhysicalBalance.h','tests/PhysicalBalanceControllerTests.cpp']},
 'limitations':['This is an illustrative tilted pose, not the measured exact failing touchdown angle.','Preserved motor orientation feedback is a deliberate existing policy, not a proposal to set it to 1.','The existing common balance correction acts on leg orientations after IK and remains unchanged in the isolated overlay.']
}
assert abs(result['example']['old_reference_error_m']-2*.15*math.sin(tilt/2))<1e-14
assert abs(result['arbitrary_setup_rotation']['same_error_m']-result['example']['old_reference_error_m'])<1e-14
output=ROOT/'docs/evidence/m78-continuation/tilted-foot-anchor-geometry.json'
output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result['example']))
