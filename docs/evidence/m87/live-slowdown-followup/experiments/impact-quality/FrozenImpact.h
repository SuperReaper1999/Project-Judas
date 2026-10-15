#pragma once
// ISOLATED M87 DIAGNOSIS. Integer bit words preserve every represented input;
// no native struct padding/pointers, no lossy float text and no engine format.
#include "ImpactSolver.h"
#include "RigidBody.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace M87Frozen {
inline std::uint32_t Bits(float v){std::uint32_t r;std::memcpy(&r,&v,4);return r;}
inline std::uint64_t Bits(double v){std::uint64_t r;std::memcpy(&r,&v,8);return r;}
inline float Float(std::uint32_t v){float r;std::memcpy(&r,&v,4);return r;}
inline double Double(std::uint64_t v){double r;std::memcpy(&r,&v,8);return r;}
using BodyWords=std::array<std::uint32_t,29>;
using RowWords=std::array<std::uint32_t,6>;
inline BodyWords Pack(const RigidBody& b){
 BodyWords w{};unsigned i=0;
 auto scalar=[&](float v){w.at(i++)=Bits(v);};
 auto vector=[&](glm::vec3 v){scalar(v.x);scalar(v.y);scalar(v.z);};
 vector(b.position);scalar(b.orientation.w);scalar(b.orientation.x);scalar(b.orientation.y);scalar(b.orientation.z);
 vector(b.linearVelocity);vector(b.angularVelocity);scalar(b.inverseMass);
 for(unsigned c=0;c<3;++c)for(unsigned r=0;r<3;++r)scalar(b.inverseInertiaLocal[c][r]);
 vector(b.forceAccumulator);vector(b.torqueAccumulator);
 return w;
}
inline RigidBody Unpack(const BodyWords& w){
 RigidBody b;unsigned i=0;auto scalar=[&](){return Float(w.at(i++));};
 auto vector=[&](){glm::vec3 v;v.x=scalar();v.y=scalar();v.z=scalar();return v;};
 b.position=vector();b.orientation.w=scalar();b.orientation.x=scalar();b.orientation.y=scalar();b.orientation.z=scalar();
 b.linearVelocity=vector();b.angularVelocity=vector();b.inverseMass=scalar();
 for(unsigned c=0;c<3;++c)for(unsigned r=0;r<3;++r)b.inverseInertiaLocal[c][r]=scalar();
 b.forceAccumulator=vector();b.torqueAccumulator=vector();return b;
}
inline RowWords Pack(const ContactConstraint& row){return {Bits(row.normalImpulse),Bits(row.tangentImpulse.x),Bits(row.tangentImpulse.y),Bits(row.tangentImpulse.z),Bits(row.normalMass),Bits(row.restitutionBias)};}
struct ContactWords {
 std::size_t a=0,b=0;bool persistent=false;
 bool hit=false,hasLocalAnchors=false,anchorAInWorldFrame=false,anchorBInWorldFrame=false;
 int separationState=0;
 std::array<std::uint32_t,9> f{};
 std::array<std::uint64_t,16> d{};
};
inline ContactWords Pack(const ImpactContact& in,std::size_t a,std::size_t b){
 ContactWords o;o.a=a;o.b=b;o.persistent=in.persistent;const auto& c=in.geometry;
 o.hit=c.hit;o.hasLocalAnchors=c.hasLocalAnchors;o.anchorAInWorldFrame=c.anchorAInWorldFrame;o.anchorBInWorldFrame=c.anchorBInWorldFrame;o.separationState=int(c.separationState);
 unsigned i=0;auto vector=[&](glm::vec3 v){o.f.at(i++)=Bits(v.x);o.f.at(i++)=Bits(v.y);o.f.at(i++)=Bits(v.z);};
 vector(c.point);vector(c.normal);o.f.at(i++)=Bits(c.penetration);o.f.at(i++)=Bits(in.friction);o.f.at(i++)=Bits(in.restitution);
 i=0;auto precise=[&](glm::dvec3 v){o.d.at(i++)=Bits(v.x);o.d.at(i++)=Bits(v.y);o.d.at(i++)=Bits(v.z);};
 o.d.at(i++)=Bits(c.signedSeparation);precise(c.preciseNormal);precise(c.localAnchorA);precise(c.localAnchorB);precise(c.localWitnessA);precise(c.localWitnessB);return o;
}
inline ImpactContact Unpack(const ContactWords& o,std::vector<RigidBody>& bodies){
 ImpactContact in;in.a=&bodies.at(o.a);in.b=&bodies.at(o.b);in.persistent=o.persistent;auto& c=in.geometry;
 c.hit=o.hit;c.hasLocalAnchors=o.hasLocalAnchors;c.anchorAInWorldFrame=o.anchorAInWorldFrame;c.anchorBInWorldFrame=o.anchorBInWorldFrame;c.separationState=SeparationState(o.separationState);
 unsigned i=0;auto vector=[&](){glm::vec3 v;v.x=Float(o.f.at(i++));v.y=Float(o.f.at(i++));v.z=Float(o.f.at(i++));return v;};
 c.point=vector();c.normal=vector();c.penetration=Float(o.f.at(i++));in.friction=Float(o.f.at(i++));in.restitution=Float(o.f.at(i++));
 i=0;auto precise=[&](){glm::dvec3 v;v.x=Double(o.d.at(i++));v.y=Double(o.d.at(i++));v.z=Double(o.d.at(i++));return v;};
 c.signedSeparation=Double(o.d.at(i++));c.preciseNormal=precise();c.localAnchorA=precise();c.localAnchorB=precise();c.localWitnessA=precise();c.localWitnessB=precise();return in;
}
struct Snapshot {
 unsigned step=0;int iterations=0;std::uint32_t dt=0;bool forceInelastic=false;
 std::vector<BodyWords> before,after;std::vector<ContactWords> contacts;std::vector<RowWords> rows;
};
template<class T,std::size_t N>inline void WriteWords(std::ostream& out,const std::array<T,N>& w){for(auto word:w)out<<word<<' ';out<<'\n';}
template<class T,std::size_t N>inline void ReadWords(std::istream& in,std::array<T,N>& w){for(auto& word:w)in>>word;}
inline void Write(const std::string& path,const Snapshot& s){
 std::ofstream out(path);if(!out)throw std::runtime_error("cannot create frozen impact input: "+path);
 out<<"JUDAS_M87_FROZEN_IMPACT_V1\n"<<s.step<<' '<<s.iterations<<' '<<s.dt<<' '<<s.forceInelastic<<'\n';
 out<<s.before.size()<<' '<<s.contacts.size()<<'\n';for(const auto& b:s.before)WriteWords(out,b);
 for(const auto& c:s.contacts){out<<c.a<<' '<<c.b<<' '<<c.persistent<<' '<<c.hit<<' '<<c.hasLocalAnchors<<' '<<c.anchorAInWorldFrame<<' '<<c.anchorBInWorldFrame<<' '<<c.separationState<<'\n';WriteWords(out,c.f);WriteWords(out,c.d);}
 out<<s.after.size()<<' '<<s.rows.size()<<'\n';for(const auto& b:s.after)WriteWords(out,b);for(const auto& r:s.rows)WriteWords(out,r);out.close();if(!out)throw std::runtime_error("failed writing frozen impact input: "+path);
}
inline Snapshot Read(const std::string& path){
 std::ifstream in(path);std::string magic;in>>magic;if(magic!="JUDAS_M87_FROZEN_IMPACT_V1")throw std::runtime_error("invalid frozen impact header");
 Snapshot s;std::size_t bodies,contacts,after,rows;in>>s.step>>s.iterations>>s.dt>>s.forceInelastic>>bodies>>contacts;
 if(!in||bodies>100000||contacts>1000000)throw std::runtime_error("invalid frozen impact sizes");
 s.before.resize(bodies);s.contacts.resize(contacts);for(auto& b:s.before)ReadWords(in,b);
 for(auto& c:s.contacts){in>>c.a>>c.b>>c.persistent>>c.hit>>c.hasLocalAnchors>>c.anchorAInWorldFrame>>c.anchorBInWorldFrame>>c.separationState;ReadWords(in,c.f);ReadWords(in,c.d);if(c.a>=bodies||c.b>=bodies||c.separationState<0||c.separationState>3)throw std::runtime_error("invalid frozen contact");}
 in>>after>>rows;if(!in||after!=bodies||rows>1000000)throw std::runtime_error("invalid frozen impact golden sizes");s.after.resize(after);s.rows.resize(rows);
 for(auto& b:s.after)ReadWords(in,b);
 for(auto& r:s.rows)ReadWords(in,r);
 if(!in)throw std::runtime_error("truncated frozen impact");
 return s;
}
}
