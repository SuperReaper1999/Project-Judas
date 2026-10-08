#include "RigidMotion.h"
#include "PhysicsWorld.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
int main() {
 int checks=0;
 auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("motion check "+std::to_string(checks));};
 for(float speed : {0.f,.1f,10.f,100.f,1000.f})for(float h : {1.f/240,1.f/60,.2f}) {
  RigidBody b;b.inverseMass=1;b.position={1,2,3};b.linearVelocity={2,-3,1};
  b.orientation=glm::normalize(glm::quat(.7f,.2f,-.3f,.4f));b.angularVelocity=glm::normalize(glm::vec3(1,2,3))*speed;
  auto old=b;IntegrateRigidBodyPosition(old,h);RigidMotion m;m.Begin(42,b,h);
  for(int i=0;i<19;++i)(void)m.Evaluate(double(h)*i/19);
  auto last=m.Evaluate(h);check(last.position==old.position);
  // No represented spin preserves the accepted anchor instead of applying a
  // second normalization; real angular drift keeps the original arithmetic.
  check(last.orientation==(speed==0?b.orientation:old.orientation));
  check(m.Segments().size()==1 && m.Segments()[0].owner==42);
 }
 RigidBody immovable; immovable.linearVelocity={1,2,3};immovable.angularVelocity={0,0,10};
 RigidMotion frozen;frozen.Begin(99,immovable,1);
 check(frozen.Evaluate(1).position==immovable.position);check(frozen.Evaluate(1).orientation==immovable.orientation);
 RigidBody b;b.inverseMass=1;b.linearVelocity={2,0,0};RigidMotion m;m.Begin(7,b,1);
 b.linearVelocity={-2,0,0};m.ChangeVelocity(b,.5);
 check(m.Evaluate(1).position==glm::vec3(0));check(m.Evaluate(.5).position==glm::vec3(1,0,0));
 check(m.Segments().size()==2 && m.Segments()[0].endPosition==m.Segments()[1].position);
 // Independent original linear lookup is the oracle for the indexed ledger.
 // Compare individual float representations rather than padded body structs.
 auto floatBits=[](float value){std::uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));return bits;};
 auto sameState=[&](const RigidBody& a,const RigidBody& b){
  bool same=floatBits(a.inverseMass)==floatBits(b.inverseMass);
  for(int axis=0;axis<3;++axis) {
   same= same && floatBits(a.position[axis])==floatBits(b.position[axis]);
   same= same && floatBits(a.linearVelocity[axis])==floatBits(b.linearVelocity[axis]);
   same= same && floatBits(a.angularVelocity[axis])==floatBits(b.angularVelocity[axis]);
  }
  for(int axis=0;axis<4;++axis) same= same && floatBits(a.orientation[axis])==floatBits(b.orientation[axis]);
  return same;
 };
 // Exact rotation from the operator's frozen landing: normalizing this
 // already unit-to-float-precision anchor has a two-cycle. Sub-ULP drift must
 // not manufacture alternate geometry for successive contact searches.
 RigidBody contactAnchor;contactAnchor.inverseMass=1;
 contactAnchor.orientation=glm::quat(.6061418652534485f,.25942757725715637f,.005194163415580988f,.751839280128479f);
 contactAnchor.angularVelocity={-.9205920696258545f,-.5645646452903748f,.3782026171684265f};
 auto normalized=contactAnchor;IntegrateRigidBodyPosition(normalized,0);
 check(!sameState(contactAnchor,normalized));
 IntegrateRigidBodyPosition(normalized,0);check(sameState(contactAnchor,normalized));
 RigidMotion contactLedger;contactLedger.Begin(77,contactAnchor,1.f/60);
 for(double elapsed:{0.0,5.014435e-19,3.388132e-21})
  check(sameState(contactLedger.Evaluate(elapsed),contactAnchor));
 double tiny=0;
 for(int i=0;i<128;++i) {
  tiny+=5.014435e-19;contactLedger.ChangeVelocity(contactAnchor,tiny);
  check(contactLedger.Segments().back().orientation==contactAnchor.orientation);
 }
 for(float elapsed:{1.f/240,1.f/60,.2f}) {
  auto reference=contactAnchor;IntegrateRigidBodyPosition(reference,elapsed);
  RigidMotion ordinary;ordinary.Begin(77,contactAnchor,elapsed);
  check(sameState(ordinary.Evaluate(elapsed),reference));
 }
 auto linearLookup=[](const RigidMotion& motion,double time){
  for(const auto& segment:motion.Segments())
   if(time>=segment.begin && time<=segment.end) return segment.Evaluate(time);
  throw std::out_of_range("motion ledger time");
 };
 RigidBody changing;changing.inverseMass=.5f;changing.position={3,-2,1};
 changing.orientation=glm::normalize(glm::quat(.6f,-.2f,.3f,.7f));
 changing.linearVelocity={1,-2,3};changing.angularVelocity={4,5,-6};
 RigidMotion longLedger;longLedger.Begin(123,changing,1);
 constexpr int changes=2048;
 for(int i=0;i<=changes;++i) {
  const double time=double(i)/changes;
  changing.linearVelocity={float(i%11)-5,float(i%7)-3,float(i%13)-6};
  changing.angularVelocity={float(i%17)-8,float(i%19)-9,float(i%23)-11};
  longLedger.ChangeVelocity(changing,time);
  if(i%128==0) { // Multiple changes at one instant create equal end times.
   changing.linearVelocity=-changing.linearVelocity;
   longLedger.ChangeVelocity(changing,time);
  }
 }
 check(longLedger.Segments().size()>2000);
 for(int i=0;i<=changes;++i) {
  const double time=double(i)/changes;
  check(sameState(longLedger.Evaluate(time),linearLookup(longLedger,time)));
  if(i<changes) {
   const double middle=(double(i)+.5)/changes;
   check(sameState(longLedger.Evaluate(middle),linearLookup(longLedger,middle)));
   const double after=std::nextafter(time,1.0);
   check(sameState(longLedger.Evaluate(after),linearLookup(longLedger,after)));
  }
  if(i>0) {
   const double before=std::nextafter(time,0.0);
   check(sameState(longLedger.Evaluate(before),linearLookup(longLedger,before)));
  }
 }
 // At boundaries the selected segment is observable through its velocity;
 // choosing the last same-time segment would silently change this result.
 check(sameState(longLedger.Evaluate(0),longLedger.Segments().front().Evaluate(0)));
 const auto& finishSegment=longLedger.Segments()[longLedger.Segments().size()-3];
 check(finishSegment.end==1 && sameState(longLedger.Evaluate(1),finishSegment.Evaluate(1)));
 RigidMotion instant;instant.Begin(9,changing,0);
 changing.linearVelocity={97,98,99};instant.ChangeVelocity(changing,0);
 changing.angularVelocity={37,38,39};instant.ChangeVelocity(changing,0);
 check(sameState(instant.Evaluate(0),instant.Segments().front().Evaluate(0)));
 auto rejects=[&](const RigidMotion& motion,double time){
  bool rejected=false;try{(void)motion.Evaluate(time);}catch(const std::out_of_range& e){rejected=std::strcmp(e.what(),"motion ledger time")==0;}
  check(rejected);
 };
 for(double time:{-1.0,std::nextafter(0.0,-1.0),std::nextafter(1.0,2.0),2.0,
                  -std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::quiet_NaN()}) rejects(longLedger,time);
 rejects(instant,std::nextafter(0.0,1.0));
 RigidMotion empty;rejects(empty,0);rejects(empty,std::numeric_limits<double>::quiet_NaN());
 longLedger.Clear();rejects(longLedger,.5);
 for(bool elsewhere : {false,true}) {
  PhysicsWorld w;w.Init();auto b=w.CreateDynamicSphere({100,100,100},.5,1,0,0);
  w.SetAngularVelocity(b,{0,0,100});w.SetLinearVelocity(b,{1,2,3});
  if(elsewhere){w.CreateStaticBox({0,-.5,0},{10,.5,10},0,1);auto x=w.CreateDynamicSphere({0,.501,0},.5,1,0,1);w.SetLinearVelocity(x,{0,-1,0});}
  RigidBody oracle;oracle.inverseMass=1;oracle.position={100,100,100};oracle.linearVelocity={1,2,3};oracle.angularVelocity={0,0,100};
  IntegrateRigidBodyPosition(oracle,1.f/60);w.Step(1.f/60);
  check(w.GetTransform(b).position==oracle.position);check(w.GetTransform(b).rotation==oracle.orientation);
 }
 std::cout<<"Rigid motion checks "<<checks<<" PASS (event restart/query integration remains Stage B/F)\n";
}
