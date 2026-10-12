#!/usr/bin/env python3
"""Observe torque-free anchored drift energy; never changes physical inputs."""
import pathlib,shutil,shlex,subprocess,json,hashlib,sys
r=pathlib.Path.cwd();b=r/'.cache/m78-followup/release';out=r/'.cache/m78-continuation/variants';ev=r/'docs/evidence/m78-continuation'
flags={};h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for line in (b/'CMakeFiles/judas_engine.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
wrapper=r'''
glm::vec3 AngularDriftGenerator(const glm::mat3& inverseInertiaLocal,
 const glm::quat& orientation,const glm::vec3& velocity,float duration) {
 const auto generator=DiagnosticAngularDriftGeneratorUnobserved(inverseInertiaLocal,orientation,velocity,duration);
 const glm::dmat3 B(inverseInertiaLocal);
 if(duration>0 && glm::determinant(B)>0 && velocity!=glm::vec3(0)) {
  const auto R0=glm::mat3_cast(glm::normalize(glm::dquat(orientation)));
  const auto L=R0*glm::inverse(B)*glm::transpose(R0)*glm::dvec3(velocity);
  const glm::quat angular(0,generator.x,generator.y,generator.z);
  const auto advanced=orientation+.5f*duration*(angular*orientation);
  const auto q1=advanced!=orientation?glm::normalize(advanced):orientation;
  const auto R1=glm::mat3_cast(glm::normalize(glm::dquat(q1)));
  const double E0=.5*glm::dot(glm::dvec3(velocity),L),E1=.5*glm::dot(L,R1*B*glm::transpose(R1)*L);
  std::printf("GYRO_DRIFT {\"h\":%.17g,\"E0\":%.17g,\"E1\":%.17g,\"delta\":%.17g,\"relative\":%.17g,\"omega\":[%.9g,%.9g,%.9g],\"q0\":[%.9g,%.9g,%.9g,%.9g],\"inverseI\":[%.9g,%.9g,%.9g],\"generator\":[%.9g,%.9g,%.9g]}\n",double(duration),E0,E1,E1-E0,(E1-E0)/std::max(E0,1e-30),velocity.x,velocity.y,velocity.z,orientation.w,orientation.x,orientation.y,orientation.z,inverseInertiaLocal[0][0],inverseInertiaLocal[1][1],inverseInertiaLocal[2][2],generator.x,generator.y,generator.z);
 }
 return generator;
}
'''
for arg in sys.argv[1:]:
 name,base_name,source_base=arg.split(':');base=out/base_name;donor=out/source_base;folder=out/name;folder.mkdir(exist_ok=False)
 for f in base.rglob('*'):
  if f.is_file() and f.suffix in ['.h','.hpp']:
   dest=folder/f.relative_to(base);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
 source=(donor/'RigidBody.cpp').read_text();old='glm::vec3 AngularDriftGenerator(';assert source.count(old)==1
 source=source.replace(old,'glm::vec3 DiagnosticAngularDriftGeneratorUnobserved(',1)
 point=source.index('void IntegrateRigidBody(');source='#include <cstdio>\n#include <algorithm>\n'+source[:point]+wrapper+'\n'+source[point:]
 (folder/'RigidBody.cpp').write_text(source);obj=folder/'RigidBody.cpp.o';archive=folder/'libjudas_engine.a';shutil.copy2(base/'libjudas_engine.a',archive)
 command=['c++',*flags.get('CXX_DEFINES',[]),'-I'+str(folder),*flags.get('CXX_INCLUDES',[]),*flags.get('CXX_FLAGS',[]),'-c',str(folder/'RigidBody.cpp'),'-o',str(obj)]
 with (ev/(name+'-compile.log')).open('xb') as f:code=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT).returncode
 assert code==0
 subprocess.run(['ar','r',str(archive),str(obj)],check=True);subprocess.run(['ranlib',str(archive)],check=True)
 (ev/(name+'-energy-observer-build.json')).write_text(json.dumps({'command':command,'base':base_name,'sourceBase':source_base,'baseArchiveSHA256':h(base/'libjudas_engine.a'),'sourceSHA256':h(folder/'RigidBody.cpp'),'engineSHA256':h(archive),'scope':'Observer only; original generator result returned unchanged'},indent=2)+'\n')
