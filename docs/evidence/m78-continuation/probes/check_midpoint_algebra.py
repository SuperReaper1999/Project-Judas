#!/usr/bin/env python3
"""Independent scalar arithmetic for actual observed rigid-body states.

No engine library, controller or acceptance predicate is imported. The logged
body inertia is diagonal for the unchanged box fixture. This is an algebra
check, not an application pass criterion or an alternative physics solver.
"""
import json, math, pathlib, struct

root = pathlib.Path.cwd()
evidence = root / 'docs/evidence/m78-continuation'
summary = json.loads((evidence / 'consumed-energy-summary.json').read_text())

def dot(a, b): return sum(x*y for x,y in zip(a,b))
def add(a, b): return [x+y for x,y in zip(a,b)]
def mul(a, s): return [x*s for x in a]
def cross(a,b): return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def norm(a): return math.sqrt(dot(a,a))
def rotate(q,v):
    q=mul(q,1/norm(q));w=q[0];xyz=q[1:]
    return add(v,add(mul(cross(xyz,v),2*w),mul(cross(xyz,cross(xyz,v)),2)))
def solve(a,b):
    rows=[list(row)+[rhs] for row,rhs in zip(a,b)]
    for i in range(3):
        pivot=max(range(i,3),key=lambda j:abs(rows[j][i]));rows[i],rows[pivot]=rows[pivot],rows[i]
        d=rows[i][i];assert abs(d)>1e-30
        rows[i]=[x/d for x in rows[i]]
        for j in range(3):
            if j!=i:
                factor=rows[j][i];rows[j]=[x-factor*y for x,y in zip(rows[j],rows[i])]
    return [row[3] for row in rows]
def midpoint(m0,B,h):
    m=list(m0)
    for iteration in range(13):
        omega=[b*x for b,x in zip(B,m)]
        residual=add(add(m,mul(m0,-1)),mul(cross(m,omega),-.5*h))
        relative=norm(residual)/norm(m0)
        if relative<1e-12:return m,relative,iteration
        assert iteration<12
        columns=[]
        for i in range(3):
            e=[0,0,0];e[i]=1
            columns.append(add(e,mul(add(cross(e,omega),cross(m,[b*x for b,x in zip(B,e)])),-.5*h)))
        matrix=[[columns[c][r] for c in range(3)] for r in range(3)]
        m=add(m,mul(solve(matrix,residual),-1))
    raise RuntimeError('independent midpoint did not converge')
def energy(m,B):return .5*sum(b*x*x for b,x in zip(B,m))
def body_endpoint(m0,g,h):
    # World momentum stays fixed; body momentum rotates by Q^T.
    return rotate([1,*mul(g,-.5*h)],m0)

cases=[]
for variant in ['consumed_explicit','consumed_implicit']:
    worst=summary['variants'][variant]['summary']['worst32']
    selected=[worst[0]]
    if variant=='consumed_explicit':selected.append(next(x for x in worst if x['step']==558 and x['owner']==11))
    for record in selected:
        q=record['q0'];bodyOmega=rotate([q[0],*mul(q[1:],-1)],record['omega'])
        B=record['inverseI'];m0=[w/b for w,b in zip(bodyOmega,B)]
        # All original motion segments finish at binary32 fixed dt unless TOI
        # splits them. Generator horizon is the remaining time from anchor.
        dt=struct.unpack('f',struct.pack('f',1/60))[0]
        H=struct.unpack('f',struct.pack('f',dt-record['begin']))[0]
        consumed=record['end']-record['begin']
        middle,residual,iterations=midpoint(m0,B,H)
        implicit=[b*x for b,x in zip(B,middle)]
        explicit=add(bodyOmega,mul([b*x for b,x in zip(B,cross(m0,bodyOmega))],.5*H))
        m1=add(mul(middle,2),mul(m0,-1))
        full=body_endpoint(m0,implicit,H)
        E0=energy(m0,B)
        result={'observedVariant':variant,'originalObservedRecord':record,'anchorHorizon':H,'consumedDuration':consumed,
                'midpointNewtonUpdates':iterations,'normalizedResidual':residual,
                'midpointEquationEndpointMatchesCayley':norm(add(m1,mul(full,-1)))/norm(m0),
                'midpointEnergyIdentityRelative':(energy(m1,B)-E0)/E0,
                'midpointMomentumNormIdentityRelative':(dot(m1,m1)-dot(m0,m0))/dot(m0,m0)}
        for name,g in [('explicit',explicit),('implicit',implicit)]:
            result[name]={'fullHorizonEnergyDelta':energy(body_endpoint(m0,g,H),B)-E0,
                          'consumedDurationEnergyDelta':energy(body_endpoint(m0,g,consumed),B)-E0}
        cases.append(result)
(evidence/'midpoint-independent-algebra.json').write_text(json.dumps({
    'scope':'Independent body-frame scalar algebra at logged actual application states; no gains/fixture/controller changes',
    'caveat':'Same input comparisons show exact full-horizon invariants; neither anchored constant generator exactly preserves energy at a shortened TOI time',
    'cases':cases},indent=2)+'\n')
for c in cases:
    print(c['observedVariant'],c['originalObservedRecord']['step'],c['originalObservedRecord']['owner'],
          'residual',c['normalizedResidual'],'Cayley error',c['midpointEquationEndpointMatchesCayley'],
          'explicit',c['explicit'],'implicit',c['implicit'])
