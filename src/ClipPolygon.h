#pragma once
// Internal native polygon clipping shared by contact generation and its focused
// regression test. The epsilon-inclusive retained-point policy is unchanged.
#include <glm/glm.hpp>
#include <vector>

namespace JudasCollisionDetail {
inline constexpr double kClipEpsilon=1e-10;
inline void ClipPolygonInto(const std::vector<glm::dvec3>& points,glm::dvec3 normal,double plane,std::vector<glm::dvec3>& out) {
 out.clear();if(points.empty())return;glm::dvec3 previous=points.back();double before=glm::dot(normal,previous)-plane;
 for(glm::dvec3 current:points){const double now=glm::dot(normal,current)-plane;
  if((before<=kClipEpsilon)!=(now<=kClipEpsilon)){
   const double t=before/(before-now);
   out.push_back(t<0?previous:t>1?current:previous+(current-previous)*t);
  }
  if(now<=kClipEpsilon)out.push_back(current);
  previous=current;before=now;
 }
}
} // namespace JudasCollisionDetail
