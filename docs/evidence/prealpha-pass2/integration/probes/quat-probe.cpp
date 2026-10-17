#include <initializer_list>
#include <glm/gtc/quaternion.hpp>
#include <cstdio>
int main(){for(auto q:{glm::quat(0,.6f,.8f,0),glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3))),glm::quat(.5f,.5f,.5f,.5f)})for(int i=0;i<8;++i){printf("%d %.9g %.9g %.9g %.9g norm %.9g stable %d\n",i,q.w,q.x,q.y,q.z,glm::dot(q,q),glm::normalize(q)==q);q=glm::normalize(q);}}
