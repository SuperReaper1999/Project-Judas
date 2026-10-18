// Pass3 unchanged-contact arithmetic observer. Compile this exact file with
// baseline and candidate Contacts.cpp; serialize fields, never struct padding.
#include "Contacts.h"
#include "ContactPreparedGeometry.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::uint32_t state=0x73bde81u;
std::uint32_t Random(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
float Between(float a,float b){return a+(b-a)*float(Random()>>8)*(1.f/16777216.f);}
glm::vec3 Vector(float a,float b){const float x=Between(a,b),y=Between(a,b),z=Between(a,b);return{x,y,z};}
glm::quat Rotation(){auto axis=Vector(-1,1);if(glm::dot(axis,axis)<.01f)axis={1,.3f,-.2f};return glm::angleAxis(Between(-3.14f,3.14f),glm::normalize(axis));}
struct Fixture{Shape a,b;ContactPose pa,pb;float margin;};
std::vector<Fixture> Fixtures(){std::vector<Fixture> out;out.reserve(16000);
 for(unsigned i=0;i<16000;++i){
  Fixture f;f.a=i%4<2?Shape::Box(Vector(.025f,1.3f)):Shape::Sphere(Between(.03f,1.2f));
  f.b=i%4%2?Shape::Box(Vector(.025f,1.3f)):Shape::Sphere(Between(.03f,1.2f));
  f.pa={Vector(-2,2),Rotation(),Vector(-.2f,.2f)};f.pb={Vector(-2,2),Rotation(),Vector(-.2f,.2f)};
  if(i%8==0)f.pb.position=f.pa.position;
  if(i%13==0){f.pa.orientation*=1.003f;f.pb.orientation*=.997f;}
  if(i%17==0){f.pa.position+=glm::vec3(65536,-32768,131072);f.pb.position+=glm::vec3(65536,-32768,131072);}
  f.margin=i%5==0?.03125f:0;
  if(i%64<4){f.a=Shape::Box({.5f,.5f,.5f});f.b=i%2?Shape::Sphere(.5f):Shape::Box({.5f,.5f,.5f});f.pa={{0,0,0},{1,0,0,0},{0,0,0}};f.pb=f.pa;f.pb.position.x=i%64<2?1:std::nextafter(1.f,i%2?0.f:2.f);}
  if(i%127==0){f.a=Shape::Box({.000125f,4,.00025f});f.b=f.a;f.pb=f.pa;f.pb.position+=glm::vec3(.0002f,0,.0003f);f.pb.orientation=glm::angleAxis(1e-7f,glm::normalize(glm::vec3(.2f,.7f,.3f)));}
  out.push_back(std::move(f));
 }
 return out;
}
struct Bytes{std::vector<char> v;std::uint64_t hash=14695981039346656037ull;
 void Raw(const void* p,std::size_t n){auto b=static_cast<const char*>(p);v.insert(v.end(),b,b+n);for(std::size_t i=0;i<n;++i){hash^=static_cast<unsigned char>(b[i]);hash*=1099511628211ull;}}
 template<class T>void Scalar(const T& x){Raw(&x,sizeof x);}
 void Bool(bool x){Scalar(std::uint8_t(x));}
 template<class T>void Vector3(const T& p){Scalar(p.x);Scalar(p.y);Scalar(p.z);}
 void Manifold(const ContactManifold& m){Scalar(std::int32_t(m.count));Bool(m.uncertain);for(int i=0;i<m.count;++i){auto& c=m.points[i];Bool(c.hit);Vector3(c.point);Vector3(c.normal);Scalar(c.penetration);Scalar(c.signedSeparation);Vector3(c.preciseNormal);Vector3(c.localAnchorA);Vector3(c.localAnchorB);Vector3(c.localWitnessA);Vector3(c.localWitnessB);Bool(c.hasLocalAnchors);Bool(c.anchorAInWorldFrame);Bool(c.anchorBInWorldFrame);Scalar(std::int32_t(c.separationState));}}
 void Diagnostics(const ContactGeometryDiagnostics& d){for(auto n:{d.predicates,d.intervalResolved,d.exactFallbacks,d.unresolved,d.invalidInputs,d.numericGapFallbacks,d.preparedOrientationMisses})Scalar(n);}
};
}
int main(int argc,char**argv){try{const auto f=Fixtures();Bytes bytes;unsigned positive=0,negative=0;const auto start=std::chrono::steady_clock::now();
 for(const auto& q:f)for(bool reverse:{false,true}){const auto& a=reverse?q.b:q.a;const auto& b=reverse?q.a:q.b;const auto& pa=reverse?q.pb:q.pa;const auto& pb=reverse?q.pa:q.pb;const ContactPreparedOrientation preparedA(pa.orientation),preparedB(pb.orientation);
  for(bool prepared:{false,true}){ResetContactGeometryDiagnostics();auto m=PrimitiveContacts(a,pa,b,pb,q.margin,prepared?&preparedA:nullptr,prepared?&preparedB:nullptr);bytes.Manifold(m);bytes.Diagnostics(GetContactGeometryDiagnostics());m.count?++positive:++negative;
   ResetContactGeometryDiagnostics();const bool hit=PrimitiveHasContacts(a,pa,b,pb,q.margin,prepared?&preparedA:nullptr,prepared?&preparedB:nullptr);bytes.Bool(hit);bytes.Diagnostics(GetContactGeometryDiagnostics());if(hit!=(m.count>0))throw std::runtime_error("Boolean/full contact disagreement");
  }
 }
 const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(argc>1){std::ofstream out(argv[1],std::ios::binary);out.write(bytes.v.data(),std::streamsize(bytes.v.size()));}
 std::cout<<"cases="<<f.size()<<" queries="<<f.size()*8<<" positive="<<positive<<" negative="<<negative<<" bytes="<<bytes.v.size()<<" digest="<<std::hex<<bytes.hash<<std::dec<<" elapsed_ms="<<elapsed<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
