#!/usr/bin/env python3
import pathlib,hashlib,json,shutil
r=pathlib.Path.cwd();base=r/'.cache/m78-cold-boundary/row-journal-warm';out=r/'.cache/m78-cold-boundary/oracle-boundary';out.mkdir(exist_ok=False)
s=(base/'JointSolver.cpp').read_text();s=s.replace('bool bjActive=false,bjDiscardFirstWarm=false;','bool bjActive=false,bjDiscardFirstWarm=false,bjOracleFirst=false,bjOracleDone=false;')
old='bjTick=-1;const char* discard=std::getenv("JUDAS_BOUNDARY_DISCARD_FIRST_JOINT_SEEDS");';assert s.count(old)==1
s=s.replace(old,'bjTick=-1;bjOracleDone=false;const char* oracle=std::getenv("JUDAS_BOUNDARY_ORACLE_FIRST_SOLVE");bjOracleFirst=oracle&&std::string(oracle)=="1";const char* discard=std::getenv("JUDAS_BOUNDARY_DISCARD_FIRST_JOINT_SEEDS");')
old='void JointSolver::SolveIteration(){';assert s.count(old)==1
block=r'''
 // Owned diagnostic ONLY: replace the first scalar pass with an independently
 // solved frozen full-row SPD system. Remaining nine passes and all subsequent
 // frames retain the original method. Reject an infeasible unconstrained result;
 // do not invent a changed bound or an increased production iteration budget.
 if(bjActive&&bjTick==0&&bjPrepare==1&&bjOracleFirst&&!bjOracleDone){
  bjOracleDone=true;const std::size_t n=m_rows.size();if(!n||n>64)throw std::runtime_error("oracle row count unsupported");
  std::vector<std::vector<double>> matrix(n,std::vector<double>(n));std::vector<double> c(n);
  auto invApply=[](const Row& row,RigidBody* body,glm::vec3 linear,glm::vec3 angular,glm::vec3 otherLinear,glm::vec3 otherAngular){auto inertia=body==row.a?row.inertiaA:row.inertiaB;return double(body->inverseMass)*glm::dot(glm::dvec3(linear),glm::dvec3(otherLinear))+glm::dot(glm::dvec3(angular),glm::dmat3(inertia)*glm::dvec3(otherAngular));};
  for(std::size_t i=0;i<n;++i){const auto& a=m_rows[i];auto av=bjInitialVelocity.at(a.a),bv=bjInitialVelocity.at(a.b);c[i]=glm::dot(glm::dvec3(a.linearA),glm::dvec3(av.first))+glm::dot(glm::dvec3(a.linearB),glm::dvec3(bv.first))+glm::dot(glm::dvec3(a.angularA),glm::dvec3(av.second))+glm::dot(glm::dvec3(a.angularB),glm::dvec3(bv.second))+a.bias;
   for(std::size_t j=0;j<n;++j){const auto& b=m_rows[j];double value=0;if(a.a==b.a)value+=invApply(a,a.a,a.linearA,a.angularA,b.linearA,b.angularA);if(a.a==b.b)value+=invApply(a,a.a,a.linearA,a.angularA,b.linearB,b.angularB);if(a.b==b.a)value+=invApply(a,a.b,a.linearB,a.angularB,b.linearA,b.angularA);if(a.b==b.b)value+=invApply(a,a.b,a.linearB,a.angularB,b.linearB,b.angularB);matrix[i][j]=value+(i==j?a.gamma:0);}}
  auto original=matrix;std::vector<double> target(n);for(std::size_t i=0;i<n;++i)target[i]=-c[i];
  for(std::size_t k=0;k<n;++k){std::size_t pivot=k;for(std::size_t i=k+1;i<n;++i)if(std::abs(matrix[i][k])>std::abs(matrix[pivot][k]))pivot=i;if(std::abs(matrix[pivot][k])<1e-14)throw std::runtime_error("oracle singular rows");std::swap(matrix[k],matrix[pivot]);std::swap(target[k],target[pivot]);for(std::size_t i=k+1;i<n;++i){double factor=matrix[i][k]/matrix[k][k];for(std::size_t j=k;j<n;++j)matrix[i][j]-=factor*matrix[k][j];target[i]-=factor*target[k];}}
  for(std::size_t ii=n;ii-->0;){for(std::size_t j=ii+1;j<n;++j)target[ii]-=matrix[ii][j]*target[j];target[ii]/=matrix[ii][ii];}
  double residual=0;for(std::size_t i=0;i<n;++i){const auto& row=m_rows[i];if(!std::isfinite(target[i])||target[i]<double(row.low)-1e-7||target[i]>double(row.high)+1e-7)throw std::runtime_error("oracle unconstrained optimum violates actual bounds");double value=c[i];for(std::size_t j=0;j<n;++j)value+=original[i][j]*target[j];residual=std::max(residual,std::abs(value));}
  bjWrite({{"kind","independent_oracle_first_solve"},{"rowCount",n},{"target",target},{"directResidualMixedUnits",residual},{"scope","Frozen same row equations; independently direct SPD optimum lies inside actual bounds. No command/history/input/threshold changes."}});
  for(std::size_t i=0;i<n;++i){auto& row=m_rows[i];const float total=std::clamp(float(target[i]),row.low,row.high),change=total-row.impulse;row.impulse=total;Apply(row,change);row.observation->reactionImpulse+=row.linearA*change;row.observation->reactionAngularImpulse+=row.angularA*change;if(row.cache)*row.cache=total;if(row.motor)*row.motor=total;}
  ++bjIteration;bjWrite({{"kind","iteration_begin"},{"rowCount",n},{"oracle",true}});for(std::size_t i=0;i<n;++i){auto& row=m_rows[i];bjWrite({{"kind","row_solve_after"},{"row",i},{"total",row.impulse},{"oracle",true},{"a",bjBody(*row.a)},{"b",bjBody(*row.b)},{"cacheWritten",row.cache?BJ(*row.cache):BJ(nullptr)}});}return;
 }
'''
s=s.replace(old,old+block)
(out/'JointSolver.cpp').write_text(s);shutil.copyfile(base/'BalanceLabBoundaryObserver.cpp',out/'BalanceLabBoundaryObserver.cpp')
d=json.loads((base/'IDENTITY.json').read_text());d['scope']='Owned numerical oracle diagnostic ONLY iff JUDAS_BOUNDARY_ORACLE_FIRST_SOLVE=1: first observed solver pass replaced with independent direct full frozen row SPD solution, then remaining nine original passes and all later steps. Explicitly rejects singular/over64/infeasible unconstrained optimum. No new production algorithm or higher iteration budget; commands and first actuation inhibition remain real fixture. Normal snapshot/thresholds60Hz unchanged.';d['overlaySHA256']={p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in out.glob('*.cpp')};(out/'IDENTITY.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d['overlaySHA256'],indent=2))
