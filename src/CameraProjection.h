#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
struct ViewportPoint {glm::vec2 position{.5f};float depth=0,distance=0;bool behind=false,inside=false;};
inline bool ValidCameraRange(float nearPlane,float farPlane){return std::isfinite(nearPlane)&&std::isfinite(farPlane)&&nearPlane>0&&farPlane>nearPlane;}
inline ViewportPoint ProjectViewport(const glm::mat4& view,const glm::mat4& projection,glm::vec3 point){auto v=view*glm::vec4(point,1),c=projection*v;ViewportPoint r;r.distance=-v.z;r.behind=c.w<=0;if(std::abs(c.w)<1e-10f){r.behind=true;return r;}auto n=glm::vec3(c)/c.w;r.position={n.x*.5f+.5f,.5f-n.y*.5f};r.depth=n.z*.5f+.5f;r.inside=!r.behind&&r.position.x>=0&&r.position.x<=1&&r.position.y>=0&&r.position.y<=1&&r.depth>=0&&r.depth<=1;return r;}
