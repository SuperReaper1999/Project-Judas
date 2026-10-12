#!/usr/bin/env python3
import pathlib,json,hashlib,shutil
r=pathlib.Path.cwd();base=r/'.cache/m78-cold-boundary/oracle-boundary';out=r/'.cache/m78-cold-boundary/all-boundary-oracle';out.mkdir(exist_ok=False)
s=(base/'JointSolver.cpp').read_text().replace('bjOracleFirst=false,bjOracleDone=false;','bjOracleFirst=false,bjOracleDone=false,bjOracleAll=false;unsigned bjOracleSolves=0,bjOracleMaxActiveIterations=0;double bjOracleMaximumKKT=0;')
old='bjTick=-1;bjOracleDone=false;';assert s.count(old)==1;s=s.replace(old,old+'const char* all=std::getenv("JUDAS_BOUNDARY_ORACLE_ALL_SOLVES");bjOracleAll=all&&std::string(all)=="1";bjOracleSolves=0;bjOracleMaxActiveIterations=0;bjOracleMaximumKKT=0;')
helper=r'''
void M78BoundaryOracleReport(){if(bjFile.is_open())bjFile<<BJ({{"kind","all_boundary_oracle_summary"},{"solveCount",bjOracleSolves},{"maximumActiveSetIterations",bjOracleMaxActiveIterations},{"maximumKKTResidualMixedUnits",bjOracleMaximumKKT},{"scope","Diagnostic independent frozen joint box QP at each actual joint solve call. Original ten outer contact/joint passes,60Hz,input/controller/thresholds unchanged. Current contact contributions retained in RHS; no warm cache restored."}}).dump()<<'\n';}
'''
s=s.replace('void M78BoundaryEndTick(){bjActive=false;}','void M78BoundaryEndTick(){bjActive=false;}'+helper)
start=s.index(' // Owned diagnostic ONLY:');end=s.index('if(bjActive){++bjIteration;',start)
# end above occurs AFTER the independently solved first-boundary block.
block=r'''
 // Owned numerical oracle, never production: solve the CURRENT frozen joint
 // subproblem independently at each real solve call. Contacts have already
 // advanced this outer pass, so infer c from CURRENT velocities and remove
 // only the actual accumulated JOINT impulses. No contact effort is invented.
 if(bjOracleAll&&bjTick>=0){
  const std::size_t n=m_rows.size();if(!n)return;if(n>64)throw std::runtime_error("oracle row count unsupported");
  std::vector<std::vector<double>> matrix(n,std::vector<double>(n));std::vector<double> c(n),target(n),oldImpulse(n);
  auto invApply=[](const Row& row,RigidBody* body,glm::vec3 linear,glm::vec3 angular,glm::vec3 otherLinear,glm::vec3 otherAngular){auto inertia=body==row.a?row.inertiaA:row.inertiaB;return double(body->inverseMass)*glm::dot(glm::dvec3(linear),glm::dvec3(otherLinear))+glm::dot(glm::dvec3(angular),glm::dmat3(inertia)*glm::dvec3(otherAngular));};
  for(std::size_t i=0;i<n;++i){const auto& a=m_rows[i];oldImpulse[i]=a.impulse;double current=glm::dot(glm::dvec3(a.linearA),glm::dvec3(a.a->linearVelocity))+glm::dot(glm::dvec3(a.linearB),glm::dvec3(a.b->linearVelocity))+glm::dot(glm::dvec3(a.angularA),glm::dvec3(a.a->angularVelocity))+glm::dot(glm::dvec3(a.angularB),glm::dvec3(a.b->angularVelocity));c[i]=current+a.bias;
   for(std::size_t j=0;j<n;++j){const auto& b=m_rows[j];double value=0;if(a.a==b.a)value+=invApply(a,a.a,a.linearA,a.angularA,b.linearA,b.angularA);if(a.a==b.b)value+=invApply(a,a.a,a.linearA,a.angularA,b.linearB,b.angularB);if(a.b==b.a)value+=invApply(a,a.b,a.linearB,a.angularB,b.linearA,b.angularA);if(a.b==b.b)value+=invApply(a,a.b,a.linearB,a.angularB,b.linearB,b.angularB);matrix[i][j]=value+(i==j?a.gamma:0);c[i]-=value*b.impulse;}}
  auto linearSolve=[](std::vector<std::vector<double>> a,std::vector<double> b){const std::size_t n=b.size();for(std::size_t k=0;k<n;++k){std::size_t pivot=k;for(std::size_t i=k+1;i<n;++i)if(std::abs(a[i][k])>std::abs(a[pivot][k]))pivot=i;if(std::abs(a[pivot][k])<1e-14)throw std::runtime_error("oracle singular row subset");std::swap(a[k],a[pivot]);std::swap(b[k],b[pivot]);for(std::size_t i=k+1;i<n;++i){double factor=a[i][k]/a[k][k];for(std::size_t j=k;j<n;++j)a[i][j]-=factor*a[k][j];b[i]-=factor*b[k];}}for(std::size_t ii=n;ii-->0;){for(std::size_t j=ii+1;j<n;++j)b[ii]-=a[ii][j]*b[j];b[ii]/=a[ii][ii];}return b;};
  std::vector<double> rhs(n);for(std::size_t i=0;i<n;++i)rhs[i]=-c[i];target=linearSolve(matrix,rhs);std::vector<int> active(n);
  for(std::size_t i=0;i<n;++i){const auto& row=m_rows[i];target[i]=std::clamp(target[i],double(row.low),double(row.high));if(std::isfinite(row.low)&&target[i]<=row.low)active[i]=-1;else if(std::isfinite(row.high)&&target[i]>=row.high)active[i]=1;}
  unsigned iteration=0;double finalKKT=0;bool converged=false;
  for(;iteration<1000;++iteration){std::vector<double> gradient=c,direction(n);std::vector<std::size_t> free;for(std::size_t i=0;i<n;++i){for(std::size_t j=0;j<n;++j)gradient[i]+=matrix[i][j]*target[j];if(!active[i])free.push_back(i);}std::vector<std::vector<double>> sub(free.size(),std::vector<double>(free.size()));std::vector<double> subrhs(free.size());for(std::size_t i=0;i<free.size();++i){subrhs[i]=-gradient[free[i]];for(std::size_t j=0;j<free.size();++j)sub[i][j]=matrix[free[i]][free[j]];}if(!free.empty()){auto d=linearSolve(sub,subrhs);for(std::size_t i=0;i<free.size();++i)direction[free[i]]=d[i];}double maximumDirection=0;for(auto d:direction)maximumDirection=std::max(maximumDirection,std::abs(d));
   if(maximumDirection<1e-11){double violation=0;std::size_t release=0;for(std::size_t i=0;i<n;++i){double v=active[i]==-1?std::max(-gradient[i],0.):active[i]==1?std::max(gradient[i],0.):std::abs(gradient[i]);finalKKT=std::max(finalKKT,v);if(active[i]&&v>violation){violation=v;release=i;}}if(violation<1e-10){converged=true;break;}active[release]=0;continue;}
   double alpha=1;std::size_t hit=n;int side=0;for(auto i:free){const auto& row=m_rows[i];double candidate=1;int edge=0;if(direction[i]>0&&std::isfinite(row.high)){candidate=(row.high-target[i])/direction[i];edge=1;}else if(direction[i]<0&&std::isfinite(row.low)){candidate=(row.low-target[i])/direction[i];edge=-1;}if(candidate<alpha){alpha=std::max(0.,candidate);hit=i;side=edge;}}for(std::size_t i=0;i<n;++i)target[i]+=alpha*direction[i];if(hit<n){active[hit]=side;target[hit]=side==1?m_rows[hit].high:m_rows[hit].low;}
  }
  if(!converged)throw std::runtime_error("diagnostic active-set oracle failed bounded solve");finalKKT=0;for(std::size_t i=0;i<n;++i){double g=c[i];for(std::size_t j=0;j<n;++j)g+=matrix[i][j]*target[j];double r=target[i]<=m_rows[i].low+1e-10?std::max(-g,0.):target[i]>=m_rows[i].high-1e-10?std::max(g,0.):std::abs(g);finalKKT=std::max(finalKKT,r);if(!std::isfinite(target[i])||target[i]<double(m_rows[i].low)-1e-7||target[i]>double(m_rows[i].high)+1e-7)throw std::runtime_error("oracle infeasible result");}
  ++bjOracleSolves;bjOracleMaxActiveIterations=std::max(bjOracleMaxActiveIterations,iteration+1);bjOracleMaximumKKT=std::max(bjOracleMaximumKKT,finalKKT);
  ++bjIteration;bjWrite({{"kind","independent_oracle_all_solve"},{"rowCount",n},{"target",target},{"KKTResidualMixedUnits",finalKKT},{"activeSetIterations",iteration+1}});
  for(std::size_t i=0;i<n;++i){auto& row=m_rows[i];const float total=std::clamp(float(target[i]),row.low,row.high),change=total-row.impulse;row.impulse=total;Apply(row,change);row.observation->reactionImpulse+=row.linearA*change;row.observation->reactionAngularImpulse+=row.angularA*change;if(row.cache)*row.cache=total;if(row.motor)*row.motor=total;}
  for(std::size_t i=0;i<n;++i){auto& row=m_rows[i];bjWrite({{"kind","row_solve_after"},{"row",i},{"total",row.impulse},{"oracle",true},{"a",bjBody(*row.a)},{"b",bjBody(*row.b)},{"cacheWritten",row.cache?BJ(*row.cache):BJ(nullptr)}});}return;
 }
'''
s=s[:start]+block+s[end:]
(out/'JointSolver.cpp').write_text(s)
f=(base/'BalanceLabBoundaryObserver.cpp').read_text().replace('extern void M78BoundaryEndTick();','extern void M78BoundaryEndTick();\nextern void M78BoundaryOracleReport();')
f=f.replace('Step(world,game,window,resources,recoverySave?1080:720);Metrics(world,savedOwner);','Step(world,game,window,resources,recoverySave?1080:720);M78BoundaryOracleReport();DiagnosticRecordState(world,savedOwner,out/"physical-final.json");Metrics(world,savedOwner);')
f=f.replace('{"history",{{"elapsed",h.elapsed}', '{"history",{{"progress",int(h.progress)},{"reason",h.reason},{"elapsed",h.elapsed}')
(out/'BalanceLabBoundaryObserver.cpp').write_text(f)
d=json.loads((base/'IDENTITY.json').read_text());d['scope']='DIAGNOSTIC ONLY, every actual joint solve call after the saved boundary is independent frozen box QP (bounded<=64rows,active-set<=1000), ten ORIGINAL outer contact/joint passes,60Hz. RHS retains current contact updates and removes only actual accumulated joint impulses. No physics/controller/input/threshold change, no warm history serialized. All-frame oracle is not proposed production algorithm; prove whether converged current equations/controller actually stand.';d['overlaySHA256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in out.glob('*.cpp')};(out/'IDENTITY.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d['overlaySHA256'],indent=2))
