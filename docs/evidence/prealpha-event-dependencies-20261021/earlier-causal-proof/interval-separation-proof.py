#!/usr/bin/env python3
"""Prove an actual smooth OBB trajectory remains separated for an entire CCD query.

All coefficients are exact rational values from recorded IEEE inputs. Multiplying
fixed-plane corner gaps by the two strictly positive quaternion squared norms
leaves degree <=5 polynomials. Positive Bernstein coefficients prove positive
separation over the whole closed interval, without a geometric tolerance.
"""
from fractions import Fraction as F
from itertools import product
from math import comb
from pathlib import Path
import hashlib,json

OWNED=Path(__file__).resolve().parent
# Reuse the mathematical definitions without rerunning the endpoint experiment.
source=(OWNED/'smooth-gap-proof.py').read_text()
exec(compile(source.split('\ntraces=')[0],str(OWNED/'smooth-gap-proof.py'),'exec'))

def padd(a,b):
    return [(a[i] if i<len(a) else F(0))+(b[i] if i<len(b) else F(0)) for i in range(max(len(a),len(b)))]
def pscale(a,s):return [x*s for x in a]
def psub(a,b):return padd(a,pscale(b,-1))
def pmul(a,b):
    out=[F(0)]*(len(a)+len(b)-1)
    for i,x in enumerate(a):
        for j,y in enumerate(b):out[i+j]+=x*y
    return out

def psum(terms):
    out=[F(0)]
    for p in terms:out=padd(out,p)
    return out

def bernstein(p):
    n=len(p)-1
    return [sum((p[i]*F(comb(k,i),comb(n,i)) for i in range(k+1)),F(0)) for k in range(n+1)]

def precise_polys(body,start,end):
    s=segment(body,start)
    if segment(body,end) is not s:raise ValueError('interval crosses a recorded motion boundary')
    if s['prescribed']:raise ValueError('exact polynomial proof is for Cayley segments')
    elapsed=start-F(f64(s['begin']));duration=end-start
    anchor=vf(s['position']);v=vf(s['linear_velocity'])
    position=[[anchor[k]+v[k]*elapsed,v[k]*duration] for k in range(3)]
    w,x,y,z=vf(s['orientation_wxyz']);ox,oy,oz=vf(s['drift_angular_velocity'])
    derivative=[-ox*x-oy*y-oz*z,ox*w+oy*z-oz*y,-ox*z+oy*w+oz*x,ox*y-oy*x+oz*w]
    q=[[v+d*elapsed/2,d*duration/2] for v,d in zip([w,x,y,z],derivative)]
    w,x,y,z=q
    ww,xx,yy,zz=[pmul(v,v) for v in q]
    xy,xz,yz,wx,wy,wz=[pmul(a,b) for a,b in [(x,y),(x,z),(y,z),(w,x),(w,y),(w,z)]]
    twice=lambda p:pscale(p,2)
    raw=[
      [psub(psub(padd(ww,xx),yy),zz),twice(padd(xy,wz)),twice(psub(xz,wy))],
      [twice(psub(xy,wz)),psub(psub(padd(ww,yy),xx),zz),twice(padd(yz,wx))],
      [twice(padd(xz,wy)),twice(psub(yz,wx)),psub(psub(padd(ww,zz),xx),yy)]]
    norm=psum([ww,xx,yy,zz])
    if min(bernstein(norm))<=0:raise ValueError('failed to certify positive quaternion norm')
    return position,raw,norm

def projected_corner(rotation_raw,corner,axis):
    return psum(pscale(rotation_raw[k][j],corner[k]*axis[j]) for k in range(3) for j in range(3))

def certify(a,b,t0,t1,depth=0):
    midpoint=(t0+t1)/2
    pa,qa=smooth(a,midpoint);pb,qb=smooth(b,midpoint)
    ha,hb=vf(a['shape']['half_extents']),vf(b['shape']['half_extents'])
    at_mid=sat(pa,qa,pb,qb,ha,hb)
    if not at_mid['separated']:
        return {'certified':False,'reason':'smooth midpoint already overlaps','depth':depth,
                'start':str(t0),'finish':str(t1),'midpoint_sat':at_mid}
    ra,rb=rotation(qa),rotation(qb)
    axes=ra+rb+[cross(x,y) for x in ra for y in rb]
    axis=axes[at_mid['winning_axis']]
    if dot(sub(pb,pa),axis)<0:axis=mul(axis,-1)
    ap,ar,an=precise_polys(a,t0,t1);bp,br,bn=precise_polys(b,t0,t1)
    central=pmul(psum(pscale(psub(bp[j],ap[j]),axis[j]) for j in range(3)),pmul(an,bn))
    ac=[projected_corner(ar,[sgn[i]*ha[i] for i in range(3)],axis) for sgn in product([-1,1],repeat=3)]
    bc=[projected_corner(br,[sgn[i]*hb[i] for i in range(3)],axis) for sgn in product([-1,1],repeat=3)]
    minimum=None;degree=0
    for av in ac:
        for bv in bc:
            coefficients=bernstein(padd(central,psub(pmul(bv,an),pmul(av,bn))))
            lower=min(coefficients);degree=max(degree,len(coefficients)-1)
            if minimum is None or lower<minimum:minimum=lower
    if minimum>0:
        return {'certified':True,'depth':depth,'start':str(t0),'finish':str(t1),
                'fixed_axis_exact':[str(x) for x in axis],'corner_pairs':64,'polynomial_degree':degree,
                'minimum_bernstein_numerator':str(minimum.numerator),
                'minimum_bernstein_denominator':str(minimum.denominator),
                'minimum_bernstein_value_unnormalized':float(minimum)}
    if depth>=4:
        return {'certified':False,'reason':'bounded proof subdivision exhausted','depth':depth,
                'start':str(t0),'finish':str(t1)}
    left=certify(a,b,t0,midpoint,depth+1)
    if not left['certified']:return left
    right=certify(a,b,midpoint,t1,depth+1)
    if not right['certified']:return right
    return {'certified':True,'depth':depth,'start':str(t0),'finish':str(t1),'children':[left,right]}

def leaf_count(node):
    return sum(leaf_count(x) for x in node['children']) if 'children' in node else 1

def max_depth(node):
    return max(max_depth(x) for x in node['children']) if 'children' in node else node['depth']

traces=[json.loads(x) for x in (OWNED/'pair-trace.jsonl').read_text().splitlines()]
endpoint=json.loads((OWNED/'smooth-gap-proof.json').read_text())
results=[]
for item in endpoint['queries']:
    event=item['selected_event']
    if not event:continue
    if not item['probes'][0]['models']['precise_cayley_double_elapsed']['separated']:
        results.append({'record':item['record'],'event':event['chronological_event_in_step'],
                        'classification':'already-overlapping-at-query-start; no separated-plane proof attempted'})
        continue
    trace=traces[item['record']-1]
    a,b=trace['body_a'],trace['body_b']
    t0=F(f64(item['probes'][0]['time_bits']));t1=F(f64(item['probes'][-1]['time_bits']))
    proof=certify(a,b,t0,t1)
    results.append({'record':item['record'],'event':event['chronological_event_in_step'],
                    'classification':'certified-separated-for-entire-closed-query-interval' if proof['certified'] else 'not-certified',
                    'start_time_s':float(t0),'finish_time_s':float(t1),'leaf_count':leaf_count(proof),
                    'maximum_subdivision_depth':max_depth(proof),'proof':proof})
    print(item['record'],event['chronological_event_in_step'],proof['certified'],leaf_count(proof),max_depth(proof),flush=True)
result={'schema':'judas.exact-rational-obb-interval-separation.v1',
        'method':__doc__+' Exact dyadic subdivision chooses a separate fixed separating plane per interval. Each closed interval is covered; adjacent leaves meet exactly. A failed proof is not classified as contact.',
        'input_sha256':{n:hashlib.sha256((OWNED/n).read_bytes()).hexdigest() for n in ['smooth-gap-proof.py','smooth-gap-proof.json','pair-trace.jsonl']},
        'results':results}
(OWNED/'interval-separation-proof.json').write_text(json.dumps(result,indent=2)+'\n')
