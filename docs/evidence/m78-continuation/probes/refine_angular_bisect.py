#!/usr/bin/env python3
"""Owned isolated angular-transport accuracy variants; no production edits."""
import pathlib,shlex,subprocess,json,hashlib,shutil,time,sys
r=pathlib.Path.cwd();b=r/'.cache/m78-followup/release'
out=r/'.cache/m78-continuation/variants';ev=r/'docs/evidence/m78-continuation'
h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
flags={}
for line in (b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
files=['RigidBody.cpp','RigidBody.h','RigidMotion.h','PhysicsWorld.cpp','PhysicsWorld.h','CollisionGeometry.cpp']
current={n:(r/'src'/n).read_text() for n in files}
def replace_function(source,name,next_name,new):
 begin=source.index('glm::vec3 '+name+'(');end=source.index(next_name,begin)
 return source[:begin]+new+source[end:]
no_predictor='''glm::vec3 AngularDriftGenerator(const glm::mat3&,const glm::quat&,const glm::vec3& velocity,float){return velocity;}

'''
no_transport='''glm::vec3 TransportAngularVelocity(const glm::mat3&,const glm::quat&,const glm::quat&,const glm::vec3& velocity){return velocity;}

'''
def rk4_source(steps):
 return '''glm::vec3 AngularDriftGenerator(const glm::mat3& inverseInertiaLocal,
 const glm::quat& orientation,const glm::vec3& velocity,float duration) {
 const glm::dmat3 inverseBody(inverseInertiaLocal);
 if(duration==0||velocity==glm::vec3(0)||!(glm::determinant(inverseBody)>0))return velocity;
 auto q0=glm::normalize(glm::dquat(orientation)),q=q0;
 auto R=glm::mat3_cast(q0);
 const auto L=R*glm::inverse(inverseBody)*glm::transpose(R)*glm::dvec3(velocity);
 auto derivative=[&](glm::dquat value){auto Q=glm::mat3_cast(glm::normalize(value));
  auto w=Q*inverseBody*glm::transpose(Q)*L;return .5*glm::dquat(0,w.x,w.y,w.z)*value;};
 constexpr int steps='''+str(steps)+''';const double dt=double(duration)/steps;
 for(int i=0;i<steps;++i){auto k1=derivative(q),k2=derivative(q+dt*.5*k1),k3=derivative(q+dt*.5*k2),k4=derivative(q+dt*k3);
  q=glm::normalize(q+(dt/6)*(k1+2.0*k2+2.0*k3+k4));}
 auto delta=q*glm::conjugate(q0);if(delta.w<0)delta=-delta;
 return glm::vec3((2.0/double(duration)/delta.w)*glm::dvec3(delta.x,delta.y,delta.z));
}

'''
implicit_midpoint='''glm::vec3 AngularDriftGenerator(const glm::mat3& inverseInertiaLocal,
 const glm::quat& orientation,const glm::vec3& velocity,float duration) {
 const glm::dmat3 inverseBody(inverseInertiaLocal);
 if(duration==0||velocity==glm::vec3(0)||!(glm::determinant(inverseBody)>0))return velocity;
 const auto R=glm::mat3_cast(glm::normalize(glm::dquat(orientation)));
 const auto m0=glm::inverse(inverseBody)*glm::transpose(R)*glm::dvec3(velocity);
 auto middle=m0;const double half=.5*double(duration);
 // Body Euler equation m'=m cross I^-1m. The implicit midpoint Cayley
 // rotation preserves both the momentum norm and quadratic kinetic energy.
 // This is a diagnostic numerical solve, with a declared fixed iteration cap.
 for(int iteration=0;iteration<12;++iteration){
  const auto omega=inverseBody*middle;
  const auto residual=middle-m0-half*glm::cross(middle,omega);
  if(glm::length(residual)<=1e-12*std::max(glm::length(m0),1e-12))
   return glm::vec3(R*omega);
  glm::dmat3 J(1);
  for(int axis=0;axis<3;++axis){glm::dvec3 e(0);e[axis]=1;
   J[axis]=e-half*(glm::cross(e,omega)+glm::cross(middle,inverseBody*e));}
  middle-=glm::inverse(J)*residual;
 }
 throw std::runtime_error("diagnostic implicit Euler midpoint did not converge");
}

'''
def run(name):
 folder=out/name;folder.mkdir(exist_ok=False);sources=dict(current);changed=['RigidBody.cpp']
 if name.startswith('right_'):
  base=out/'event_transport_right'
  sources['RigidMotion.h']=(base/'RigidMotion.h').read_text()
  sources['PhysicsWorld.cpp']=(base/'PhysicsWorld.cpp').read_text();changed.append('PhysicsWorld.cpp')
 if name.endswith('transport_only'):
  sources['RigidBody.cpp']=replace_function(sources['RigidBody.cpp'],'AngularDriftGenerator','void IntegrateRigidBody(',no_predictor)
 elif name.endswith('predictor_only'):
  sources['RigidBody.cpp']=replace_function(sources['RigidBody.cpp'],'TransportAngularVelocity','glm::vec3 AngularDriftGenerator(',no_transport)
 elif 'rk4_' in name:
  steps=int(name.rsplit('_',1)[-1]);sources['RigidBody.cpp']=replace_function(sources['RigidBody.cpp'],'AngularDriftGenerator','void IntegrateRigidBody(',rk4_source(steps))
 elif name.endswith('implicit_midpoint'):
  sources['RigidBody.cpp']='#include <stdexcept>\n#include <algorithm>\n'+replace_function(sources['RigidBody.cpp'],'AngularDriftGenerator','void IntegrateRigidBody(',implicit_midpoint)
 else:raise RuntimeError(name)
 for header in (r/'src').rglob('*'):
  if header.is_file() and header.suffix in ['.h','.hpp']:
   destination=folder/header.relative_to(r/'src');destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(header,destination)
 for n,s in sources.items():(folder/n).write_text(s)
 archive=folder/'libjudas_engine.a';shutil.copy2(b/'libjudas_engine.a',archive);records=[]
 for n in changed:
  obj=folder/(n+'.o');cmd=['c++',*flags.get('CXX_DEFINES',[]),'-I'+str(folder),*flags.get('CXX_INCLUDES',[]),*flags.get('CXX_FLAGS',[]),'-c',str(folder/n),'-o',str(obj)]
  with (ev/(name+'-compile.log')).open('wb') as log:code=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT).returncode
  records.append({'command':cmd,'exit':code});assert code==0,(name,n)
  subprocess.run(['ar','r',str(archive),str(obj)],check=True)
 subprocess.run(['ranlib',str(archive)],check=True)
 target='judas_physical_balance_controller_tests';link=shlex.split((b/'CMakeFiles'/ (target+'.dir')/'link.txt').read_text())
 link=[str(archive) if x=='libjudas_engine.a' else str(folder/'controller') if x==target else x for x in link if not x.startswith('-Wl,--dependency-file')]
 subprocess.run(link,cwd=b,check=True);start=time.perf_counter()
 with (ev/(name+'-controller.log')).open('wb') as log:code=subprocess.run([str(folder/'controller')],cwd=r,stdout=log,stderr=subprocess.STDOUT).returncode
 (ev/(name+'-bisect.json')).write_text(json.dumps({'name':name,'source':{n:h(folder/n) for n in files},'builds':records,'engineSHA256':h(archive),'exeSHA256':h(folder/'controller'),'exitCode':code,'seconds':time.perf_counter()-start,'fixtureSHA256':h(r/'tests/PhysicalBalanceControllerTests.cpp'),'scope':'Diagnostic variant only; no production/fixture changes'},indent=2)+'\n')
 print(name,code,flush=True)
for name in sys.argv[1:]:run(name)
