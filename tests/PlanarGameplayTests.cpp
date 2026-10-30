#include "PhysicsWorld.h"
#include "PlanarGeometry.h"
#include "Sprite.h"
#include <cstdio>
#include <stdexcept>
#include <cmath>
int main(){unsigned checks=0;try{
 auto check=[&](bool ok,const char* text){++checks;std::printf("%s %s\n",ok?"PASS":"FAIL",text);if(!ok)throw std::runtime_error(text);};
 PhysicsWorld world;world.Init();auto body=[&](glm::vec3 p,glm::dvec2 half,bool dynamic){return world.CreateShape(Shape::Planar(PlanarBox(half)),{p,{1,0,0,0}},dynamic,1,.6f,0);};
 body({0,-.5f,0},{12,.5},false);std::vector<BodyHandle> stack;
 for(int i=0;i<8;++i)stack.push_back(body({0,.5f+i*1.02f,0},{.5,.5},true));
 for(int step=0;step<600;++step){for(auto h:stack)world.ApplyLinearAcceleration(h,{0,-9.81f,0},1.f/60);world.Step(1.f/60);}
 for(unsigned i=0;i<stack.size();++i){auto p=world.GetTransform(stack[i]).position;std::printf("STACK %u y=%.9g expected=%.9g\n",i,p.y,.5+i);
  check(std::abs(p.y-(.5f+i))<.06f&&std::abs(p.x)<.05f&&p.z==0,"eight-body stack retains correct height, ordering and plane");}
 // Independent tiers: vertical separation is real geometry, not draw order/Z.
 auto shelf=body({5,3,0},{1,.2},false),upper=body({5,5,0},{.3,.3},true);(void)shelf;
 for(int i=0;i<240;++i){world.ApplyLinearAcceleration(upper,{0,-9.81f,0},1.f/60);world.Step(1.f/60);}
 check(std::abs(world.GetTransform(upper).position.y-3.5f)<.02f,"falling object rests on the upper solid platform, not the lower floor");
 SpriteSettings sprite;sprite.ySort=true;sprite.pivot={.5f,1};
 check(SpriteSortKey(sprite,-1,2)>SpriteSortKey(sprite,1,1),"top-down foot-origin sorting puts lower actor in front");
 sprite.order=3;auto other=sprite;other.order=2;check(SpriteSortKey(sprite,10,1)>SpriteSortKey(other,-10,2),"explicit draw order overrides Y sort for roofs/bridges");
 std::printf("PLANAR_GAMEPLAY checks=%u failures=0\n",checks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
