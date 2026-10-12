#!/usr/bin/env python3
"""Independent double-precision box QP of one frozen actual runtime joint row system.
Diagnostic only: no production iteration counts, fixture settings or state are changed.
Use bundled NumPy Python. Gradients mix linear m/s and angular rad/s row units.
"""
import argparse,json,pathlib,hashlib,itertools
import numpy as np
p=argparse.ArgumentParser();p.add_argument('journal',type=pathlib.Path);p.add_argument('--tick',type=int,default=0);p.add_argument('--prepare',type=int,default=1);p.add_argument('--root-body',type=int);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
entries=[json.loads(x)for x in a.journal.read_text().splitlines()];entries=[x for x in entries if x['tick']==a.tick and x['prepare']==a.prepare]
rows=[x for x in entries if x['kind']=='row_construct'];bodies={x['body']:x['state']for x in entries if x['kind']=='prepare_free_body'};ids=list(bodies);lookup={id:k for k,id in enumerate(ids)};n=len(rows);d=6*len(ids)
roots={x["bodyB"]for x in rows}-{x["bodyA"]for x in rows};root=a.root_body if a.root_body is not None else next(iter(roots))if len(roots)==1 else None;rootIndex=6*lookup[root]if root is not None else None
J=np.zeros((n,d));Minv=np.zeros((d,d));v0=np.zeros(d)
for id,state in bodies.items():
 k=6*lookup[id];Minv[k:k+3,k:k+3]=np.eye(3)*state['inverseMass'];Minv[k+3:k+6,k+3:k+6]=np.array(state['inverseInertiaWorld'],dtype=float).T;v0[k:k+3]=state['linearVelocity'];v0[k+3:k+6]=state['angularVelocity']
for i,row in enumerate(rows):
 for suffix in ['A','B']:
  k=6*lookup[row['body'+suffix]];J[i,k:k+3]+=row['linear'+suffix];J[i,k+3:k+6]+=row['angular'+suffix]
gamma=np.array([x['gamma']for x in rows]);bias=np.array([x['bias']for x in rows]);seed=np.array([x['initialImpulse']for x in rows]);mass=np.array([x['mass']for x in rows]);lo=np.array([-np.inf if x['low']is None else x['low']for x in rows]);hi=np.array([np.inf if x['high']is None else x['high']for x in rows]);A=J@Minv@J.T+np.diag(gamma);A=(A+A.T)*.5;c=J@v0+bias
# Feasible active-set independent solve. Solve every free linear subsystem directly;
# release active bounds only when the sign of its exact KKT gradient is violated.
x=np.clip(np.linalg.solve(A,-c),lo,hi);active=np.zeros(n,int);active[np.isfinite(lo)&(x<=lo)]=-1;active[np.isfinite(hi)&(x>=hi)]=1
for it in range(1000):
 free=np.where(active==0)[0];g=A@x+c;p=np.zeros(n);p[free]=np.linalg.solve(A[np.ix_(free,free)],-g[free]) if len(free)else[]
 if np.max(np.abs(p),initial=0)<1e-11:
  violation=np.zeros(n);violation[active==-1]=np.maximum(-g[active==-1],0);violation[active==1]=np.maximum(g[active==1],0)
  if violation.max(initial=0)<1e-10:break
  active[int(np.argmax(violation))]=0;continue
 alpha=1.;hit=-1;side=0
 for k in free:
  if p[k]>0 and np.isfinite(hi[k]):candidate=(hi[k]-x[k])/p[k];s=1
  elif p[k]<0 and np.isfinite(lo[k]):candidate=(lo[k]-x[k])/p[k];s=-1
  else:continue
  if candidate<alpha:alpha=max(0.,candidate);hit=int(k);side=s
 x+=alpha*p
 if hit>=0:active[hit]=side;x[hit]=hi[hit]if side==1 else lo[hit]
else:raise RuntimeError('independent box QP failed bounded active-set diagnostic')
def solve10(initial):
 x=initial.copy()
 for _ in range(10):
  for i in range(n):x[i]=np.clip(x[i]-(A[i]@x+c[i])*mass[i],lo[i],hi[i])
 return x
def localBox(Q,c,l,h):
 candidates=[];count=len(c)
 options=[[0]+([-1]if np.isfinite(l[i])else[])+([1]if np.isfinite(h[i])else[])for i in range(count)]
 for choice in itertools.product(*options):
  active=np.array(choice);free=np.where(active==0)[0];fixed=np.where(active!=0)[0];x=np.zeros(count)
  for i in fixed:x[i]=l[i]if active[i]==-1 else h[i]
  if len(free):x[free]=np.linalg.solve(Q[np.ix_(free,free)],-c[free]-Q[np.ix_(free,fixed)]@x[fixed])
  if np.any(x<l-1e-8)or np.any(x>h+1e-8):continue
  g=Q@x+c
  if np.any(g[active==-1]<-1e-8)or np.any(g[active==1]>1e-8):continue
  candidates.append((.5*x@Q@x+c@x,x))
 if not candidates:raise RuntimeError('local box system lacks KKT candidate')
 return min(candidates,key=lambda pair:pair[0])[1]
groups=[]
for i,row in enumerate(rows):
 category=(row['jointOrdinal'],any(v!=0 for v in row['linearA']+row['linearB']))
 if not groups or groups[-1][0]!=category:groups.append((category,[i]))
 else:groups[-1][1].append(i)
def solveBlock10(initial):
 x=initial.copy()
 for _ in range(10):
  for _,g in groups:
   if len(g)>3:
    for i in g:x[i]=np.clip(x[i]-(A[i]@x+c[i])*mass[i],lo[i],hi[i])
    continue
   ng=[i for i in range(n)if i not in g];Q=A[np.ix_(g,g)];localc=c[g]+A[np.ix_(g,ng)]@x[ng];x[g]=localBox(Q,localc,lo[g],hi[g])
 return x
jointGroups=[]
for i,row in enumerate(rows):
 category=row['jointOrdinal']
 if not jointGroups or jointGroups[-1][0]!=category:jointGroups.append((category,[i]))
 else:jointGroups[-1][1].append(i)
def solveJointBlock10(initial):
 x=initial.copy()
 for _ in range(10):
  for _,g in jointGroups:
   ng=[i for i in range(n)if i not in g];Q=A[np.ix_(g,g)];localc=c[g]+A[np.ix_(g,ng)]@x[ng];x[g]=localBox(Q,localc,lo[g],hi[g])
 return x
def solveAlternatingPoint10(initial):
 x=initial.copy()
 for iteration in range(10):
  sequence=range(n)if iteration%2==0 else range(n-1,-1,-1)
  for i in sequence:x[i]=np.clip(x[i]-(A[i]@x+c[i])*mass[i],lo[i],hi[i])
 return x
def solveAlternatingJointBlock10(initial):
 x=initial.copy()
 for iteration in range(10):
  sequence=jointGroups if iteration%2==0 else jointGroups[::-1]
  for _,g in sequence:
   ng=[i for i in range(n)if i not in g];Q=A[np.ix_(g,g)];localc=c[g]+A[np.ix_(g,ng)]@x[ng];x[g]=localBox(Q,localc,lo[g],hi[g])
 return x
actual=np.array([next(z['total']for z in entries if z['kind']=='row_solve_after'and z['iteration']==10 and z['row']==i)for i in range(n)])
objective=lambda x:float(.5*x@A@x+c@x)
scaled=np.diag(1/np.sqrt(np.diag(A)))
def metrics(lam):
 g=A@lam+c;pg=lam-np.clip(lam-g*mass,lo,hi);dv=Minv@J.T@(lam-x);kinetic=.5*dv@np.linalg.pinv(Minv)@dv
 return {'objective':objective(lam),'objectiveGap':max(0.,objective(lam)-objective(x)),'diagonalScaledProjectedKKTMax':float(np.max(np.abs(pg)/np.sqrt(mass))),'projectedImpulseResidualMaxMixedUnits':float(np.max(np.abs(pg))),'bodyVelocityErrorKineticJoules':float(kinetic),'rootBody':root,'rootLinearVelocityError':dv[rootIndex:rootIndex+3].tolist()if rootIndex is not None else None,'rootAngularVelocityError':dv[rootIndex+3:rootIndex+6].tolist()if rootIndex is not None else None,'lambda':lam.tolist()}
finalPhysical={}
for e in entries:
 if e['kind']=='row_solve_after'and e['iteration']==10:
  r=rows[e['row']];finalPhysical[r['bodyA']]=e['a'];finalPhysical[r['bodyB']]=e['b']
va=np.zeros(d)
for id,s in finalPhysical.items():k=6*lookup[id];va[k:k+3]=s['linearVelocity'];va[k+3:k+6]=s['angularVelocity']
expected=v0+Minv@J.T@actual
out={'journal':str(a.journal),'journalSHA256':hashlib.sha256(a.journal.read_bytes()).hexdigest(),'tick':a.tick,'prepare':a.prepare,'rowCount':n,'bodyOrder':ids,'angularDriveRows':sum(x['cacheIndex']>=100 for x in rows),'scope':'Frozen actual joint rows, independently solved bounded convex quadratic. No production iterations changed. Double replay ignores float rounding; compare its observed closure error. No claim different Linux/native saved states are equivalent. Contact-free initial solve only.','matrixSymmetricEigenvalues':np.linalg.eigvalsh(A).tolist(),'matrixCondition':float(np.linalg.cond(A)),'diagonallyScaledCondition':float(np.linalg.cond(scaled@A@scaled)),'diagonalVsLoggedMassMaxRelative':float(np.max(np.abs(np.diag(A)*mass-1))),'independentActiveSetIterations':it+1,'independentQPKKT':metrics(x),'runtimeActual10':metrics(actual),'doubleReplayWarm10':metrics(solve10(seed)),'doubleReplayZeroSeed10':metrics(solve10(np.zeros(n))),'doubleBlockWarm10':metrics(solveBlock10(seed)),'doubleBlockZero10':metrics(solveBlock10(np.zeros(n))),'blockGroups':[g for _,g in groups],'doubleJointBlockWarm10':metrics(solveJointBlock10(seed)),'doubleJointBlockZero10':metrics(solveJointBlock10(np.zeros(n))),'jointBlockGroups':[g for _,g in jointGroups],'doubleAlternatingPointWarm10':metrics(solveAlternatingPoint10(seed)),'doubleAlternatingPointZero10':metrics(solveAlternatingPoint10(np.zeros(n))),'doubleAlternatingJointBlockWarm10':metrics(solveAlternatingJointBlock10(seed)),'doubleAlternatingJointBlockZero10':metrics(solveAlternatingJointBlock10(np.zeros(n))),'runtimeVelocityImpulseClosureMax':float(np.max(np.abs(va-expected))),'runtimeLambdaReplayDifferenceMax':float(np.max(np.abs(actual-solve10(seed)))),'minimumBoundMargin':float(np.min(np.minimum(x-lo,hi-x)))if np.isfinite(np.min(np.minimum(x-lo,hi-x)))else None}
a.output.write_text(json.dumps(out,indent=2,allow_nan=False)+'\n');print(json.dumps({k:out[k]for k in ['rowCount','angularDriveRows','matrixCondition','diagonallyScaledCondition','runtimeVelocityImpulseClosureMax','runtimeLambdaReplayDifferenceMax']},indent=2));print('actual gap',out['runtimeActual10']['objectiveGap'],'zero gap',out['doubleReplayZeroSeed10']['objectiveGap'],'warm gap',out['doubleReplayWarm10']['objectiveGap'])
