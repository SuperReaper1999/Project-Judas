#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
struct ViewportPoint {glm::vec2 position{.5f};float depth=0,distance=0;bool behind=false,inside=false;};
inline bool ValidCameraRange(float nearPlane,float farPlane){return std::isfinite(nearPlane)&&std::isfinite(farPlane)&&nearPlane>0&&farPlane>nearPlane;}
inline ViewportPoint ProjectViewport(const glm::mat4& view,const glm::mat4& projection,glm::vec3 point){auto v=view*glm::vec4(point,1),c=projection*v;ViewportPoint r;r.distance=-v.z;r.behind=c.w<=0;if(std::abs(c.w)<1e-10f){r.behind=true;return r;}auto n=glm::vec3(c)/c.w;r.position={n.x*.5f+.5f,.5f-n.y*.5f};r.depth=n.z*.5f+.5f;r.inside=!r.behind&&r.position.x>=0&&r.position.x<=1&&r.position.y>=0&&r.position.y<=1&&r.depth>=0&&r.depth<=1;return r;}

// Portal views: an optional off-axis frustum ({left,right,bottom,top} at the near plane, camera
// space) and an optional oblique near plane (Lengyel). A camera can then look "through" a planar
// window: the frustum passes exactly through the window's corners, and geometry between the camera
// and the window's plane is clipped away.
inline bool ValidCameraFrustum(const glm::vec4& f){return std::isfinite(f.x)&&std::isfinite(f.y)&&std::isfinite(f.z)&&std::isfinite(f.w)&&f.y>f.x&&f.w>f.z;}
inline bool ValidCameraPlane(const glm::vec4& p){float l=glm::length(glm::vec3(p));return std::isfinite(l)&&std::isfinite(p.w)&&l>1e-6f;}
// Replace the near plane of an OpenGL projection with `viewPlane` (view space, ax+by+cz+d=0, the kept
// side positive). Only valid while the eye is on the clipped side (viewPlane.w < 0); otherwise the
// projection is returned unchanged.
inline glm::mat4 ObliqueNearPlane(glm::mat4 projection,glm::vec4 viewPlane){
    if(!(viewPlane.w<0))return projection;
    const glm::vec4 q{(glm::sign(viewPlane.x)+projection[2][0])/projection[0][0],(glm::sign(viewPlane.y)+projection[2][1])/projection[1][1],-1.f,(1.f+projection[2][2])/projection[3][2]};
    const float dq=glm::dot(viewPlane,q);
    if(!std::isfinite(dq)||std::abs(dq)<1e-12f)return projection;
    const glm::vec4 m=viewPlane*(2.f/dq);
    projection[0][2]=m.x;projection[1][2]=m.y;projection[2][2]=m.z+1.f;projection[3][2]=m.w;
    return projection;
}
// World plane (normal n, point p: n.(x-p) >= 0 kept) to view space for a view matrix.
inline glm::vec4 ViewSpacePlane(const glm::mat4& view,glm::vec3 normal,glm::vec3 point){
    const glm::vec4 world{glm::normalize(normal),-glm::dot(glm::normalize(normal),point)};
    return glm::transpose(glm::inverse(view))*world;
}
inline glm::mat4 SecondaryCameraProjection(float fovDegrees,float aspect,float nearPlane,float farPlane,const glm::vec4* frustum,const glm::vec4* viewPlane){
    glm::mat4 p=frustum?glm::frustum(frustum->x,frustum->y,frustum->z,frustum->w,nearPlane,farPlane):glm::perspective(glm::radians(fovDegrees),aspect,nearPlane,farPlane);
    return viewPlane?ObliqueNearPlane(p,*viewPlane):p;
}
