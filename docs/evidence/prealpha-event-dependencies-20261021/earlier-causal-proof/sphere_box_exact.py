from pathlib import Path
import json,math,hashlib
from math import comb
from itertools import product
r=Path(__file__).resolve().parent
# Mathematical helper definitions only; no old experiment is rerun or changed.
s=(r/'smooth-gap-proof.py').read_text();exec(compile(s.split('\ntraces=')[0],str(r/'smooth-gap-proof.py'),'exec'))
s=(r/'interval-separation-proof.py').read_text();defs=s[s.index('def padd('):s.rindex('\ntraces=')];exec(compile(defs,str(r/'interval-separation-proof.py'),'exec'))
def classify(ps,pb,qb,radius,half):
 rot=rotation(qb);local=[dot(sub(ps,pb),axis) for axis in rot];d2=sum((max(abs(x)-h,F(0))**2 for x,h in zip(local,half)),F(0));sign=d2-radius*radius
 return {'separated':sign>0,'squared_distance_minus_radius_squared_sign':(sign>0)-(sign<0),'distance_to_closed_box_minus_radius_m':math.sqrt(float(d2))-float(radius),'local':local}
def certificate(a,b,t0,t1,depth=0):
 mid=(t0+t1)/2;sp,_=smooth(a,mid);bp,bq=smooth(b,mid);radius=F(f32(a['shape']['radius']));half=vf(b['shape']['half_extents']);c=classify(sp,bp,bq,radius,half)
 if not c['separated']:return {'certified':False,'reason':'continuous midpoint not separated','depth':depth}
 ps,_,_=precise_polys(a,t0,t1);pb,rot,norm=precise_polys(b,t0,t1)
 projected=[psum(pmul(rot[i][j],psub(ps[j],pb[j])) for j in range(3)) for i in range(3)]
 gaps=[]
 for i,x in enumerate(c['local']):
  if abs(x)>half[i]:
   g=psub(pscale(projected[i],1 if x>0 else -1),pscale(norm,half[i]))
   if min(bernstein(g))<=0:break
   gaps.append(g)
 else:
  poly=psub(psum(pmul(g,g) for g in gaps),pscale(pmul(norm,norm),radius*radius));lower=min(bernstein(poly))
  if lower>0:return {'certified':True,'depth':depth,'minimum_bernstein_exact':str(lower),'polynomial_degree':len(poly)-1,'active_outside_axes':len(gaps),'t0':str(t0),'t1':str(t1)}
 if depth>=4:return {'certified':False,'reason':'bounded exact proof exhausted','depth':depth}
 left=certificate(a,b,t0,mid,depth+1)
 if not left['certified']:return left
 right=certificate(a,b,mid,t1,depth+1)
 if not right['certified']:return right
 return {'certified':True,'depth':depth,'children':[left,right]}

def encode32(x):return struct.unpack('<I',struct.pack('<f',x))[0]
def encode64(x):return struct.unpack('<Q',struct.pack('<d',x))[0]
def fixture(position,velocity,radius=None):
 q=[encode32(x) for x in [1,0,0,0]];v=[encode32(x) for x in position];z=[encode32(0)]*3;s={'begin':encode64(0),'end':encode64(1),'position':v,'linear_velocity':[encode32(x) for x in velocity],'orientation_wxyz':q,'drift_angular_velocity':z,'prescribed':False}
 return {'motion':[s],'shape':{'type':0 if radius else 1,'radius':encode32(radius or 0),'half_extents':[encode32(1)]*3,'pivot_offset':z}}
box=fixture([0,0,0],[0,0,0]);sphere=fixture([3,0,0],[-2,0,0],.5)
assert certificate(sphere,box,F(0),F(1,4))['certified']
assert not certificate(sphere,box,F(0),F(1))['certified']
assert not classify([F(3,2),F(0),F(0)],[F(0)]*3,[F(1),F(0),F(0),F(0)],F(1,2),[F(1)]*3)['separated']
assert classify([F(2),F(2),F(0)],[F(0)]*3,[F(1),F(0),F(0),F(0)],F(1),[F(1)]*3)['separated']
traces=[json.loads(l) for l in (r/'pair-trace.jsonl').read_text().splitlines()];replay=json.loads((r/'pair-replay.json').read_text());events=[json.loads(l) for l in (r/'events.jsonl').read_text().splitlines()];out=[];checks=0
for item in replay['queries']:
 trace=traces[item['record']-1];a,b=trace['body_a'],trace['body_b'];assert a['shape']['type']==0 and b['shape']['type']==1
 radius=F(f32(a['shape']['radius']));half=vf(b['shape']['half_extents']);assert not any(vf(a['shape']['pivot_offset'])+vf(b['shape']['pivot_offset']))
 event=next((e for e in events if e['selected']==trace['key'] and e['event_time_s']==item['expected_result']),None)
 row={'record':item['record'],'selected_event':event,'probes':[]}
 for p in item['probes']:
  t=F(f64(p['time_bits']));sa,_=smooth(a,t);sb,sq=smooth(b,t);na=vf(p['a']['position_bits']);nb=vf(p['b']['position_bits']);nq=vf(p['b']['orientation_bits_wxyz']);native=classify(na,nb,nq,radius,half);continuous=classify(sa,sb,sq,radius,half);mixedP=classify(sa,sb,nq,radius,half);mixedQ=classify(na,nb,sq,radius,half)
  assert native['separated'] != p['touching_boolean'],(item['record'],p['time'],native,p);checks+=1
  for c in [native,continuous,mixedP,mixedQ]:c.pop('local')
  row['probes'].append({'time':p['time'],'time_bits':p['time_bits'],'native':native,'continuous':continuous,'precise_centres_native_orientation':mixedP,'native_centres_precise_orientation':mixedQ})
 if event:
  t0=F(f64(trace['start']));t1=F(item['expected_result']);sa,_=smooth(a,t0);sb,sq=smooth(b,t0);start=classify(sa,sb,sq,radius,half)
  row['whole_prefix_proof']=certificate(a,b,t0,t1) if start['separated'] else {'certified':False,'reason':'continuous start already in contact'}
  sa,_=smooth(a,F(f64(trace['finish'])));sb,sq=smooth(b,F(f64(trace['finish'])));last=classify(sa,sb,sq,radius,half);last.pop('local');row['continuous_finish']=last
  print('event',event['chronological_event_in_step'],'proof',row['whole_prefix_proof']['certified'],'start',row['probes'][0]['continuous']['distance_to_closed_box_minus_radius_m'],'arrival',next(p['continuous']['distance_to_closed_box_minus_radius_m'] for p in row['probes'] if p['time']==item['expected_result']),flush=True)
 out.append(row)
j={'schema':'judas.exact-sphere-box-diagnosis.v1','method':'Exact rational sphere-to-oriented-box squared distance from captured IEEE data; no geometric epsilon. Whole-prefix certificates enclose degree-six polynomials with exact Bernstein coefficients and positive quaternion norms. Failed certificate is not classified as contact.','nativeControlProbeCount':checks,'independentPositiveNegativeControls':4,'queries':out,'inputHashes':{n:hashlib.sha256((r/n).read_bytes()).hexdigest() for n in ['pair-trace.jsonl','pair-replay.json','events.jsonl','sphere_box_exact.py']}}
(r/'sphere-box-exact.json').write_text(json.dumps(j,indent=2)+'\n');print('Native exact-distance controls',checks,'selected events',sum(x['selected_event'] is not None for x in out))
