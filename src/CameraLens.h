#pragma once

#include "CameraProjection.h"
#include <algorithm>
#include <glm/gtc/quaternion.hpp>

enum class CameraProjectionKind { Perspective, Orthographic };

// Optional presentation settings. Existing cameras keep their perspective
// range/FOV and exact default projection; no offscreen target is required.
struct CameraLens {
    CameraProjectionKind projection = CameraProjectionKind::Perspective;
    float height = 10;
    bool pixelPerfect = false;
    int referenceWidth = 320, referenceHeight = 180;
    float pixelsPerUnit = 32;
    template<class Archive> void Save(Archive& archive) {
        archive(projection, height, pixelPerfect, referenceWidth, referenceHeight, pixelsPerUnit);
    }
};
inline bool operator==(const CameraLens& a, const CameraLens& b) {
    return a.projection==b.projection && a.height==b.height && a.pixelPerfect==b.pixelPerfect &&
           a.referenceWidth==b.referenceWidth && a.referenceHeight==b.referenceHeight && a.pixelsPerUnit==b.pixelsPerUnit;
}
inline bool operator!=(const CameraLens& a, const CameraLens& b) { return !(a==b); }
inline bool ValidCameraLens(const CameraLens& lens) {
    return (lens.projection==CameraProjectionKind::Perspective || lens.projection==CameraProjectionKind::Orthographic) &&
           std::isfinite(lens.height) && lens.height>0 && lens.height<=1e8f &&
           std::isfinite(lens.pixelsPerUnit) && lens.pixelsPerUnit>0 && lens.pixelsPerUnit<=65536 &&
           lens.referenceWidth>=1 && lens.referenceWidth<=4096 && lens.referenceHeight>=1 && lens.referenceHeight<=4096 &&
           (!lens.pixelPerfect || lens.projection==CameraProjectionKind::Orthographic);
}
inline float CameraVisibleHeight(const CameraLens& lens) {return lens.pixelPerfect?float(lens.referenceHeight)/lens.pixelsPerUnit:lens.height;}

struct CameraFrame {
    glm::mat4 view{1}, projection{1};
    glm::vec3 eye{0};
    glm::ivec4 content{0,0,1,1}; // top-left pixels, same origin as UI/input
    glm::ivec2 viewport{1};
    bool orthographic = false;
    float pixelGrid = 0;

    ViewportPoint Project(glm::vec3 point) const {
        auto out=ProjectViewport(view,projection,point);
        if(orthographic) { out.behind=out.distance<=0; out.inside=out.inside&&!out.behind; }
        if(content!=glm::ivec4(0,0,viewport.x,viewport.y))
            out.position=(glm::vec2(content.x,content.y)+out.position*glm::vec2(content.z,content.w))/glm::vec2(viewport);
        return out;
    }
    bool Ray(glm::vec2 point, glm::vec3& origin, glm::vec3& direction) const {
        if(!std::isfinite(point.x)||!std::isfinite(point.y))return false;
        const auto local=(point*glm::vec2(viewport)-glm::vec2(content.x,content.y))/glm::vec2(content.z,content.w);
        if(local.x<0||local.y<0||local.x>1||local.y>1)return false;
        const auto inverse=glm::inverse(projection*view);
        const auto a=inverse*glm::vec4(local.x*2-1,1-local.y*2,-1,1);
        const auto b=inverse*glm::vec4(local.x*2-1,1-local.y*2,1,1);
        const auto nearPoint=glm::vec3(a)/a.w, farPoint=glm::vec3(b)/b.w;
        origin=orthographic?nearPoint:eye;
        direction=glm::normalize(farPoint-nearPoint);
        return true;
    }
};

inline glm::vec3 SnapCameraPlane(glm::vec3 point, const glm::quat& rotation, float pixelsPerUnit) {
    const auto right=rotation*glm::vec3(1,0,0),up=rotation*glm::vec3(0,1,0);
    for(const auto axis:{right,up}) {
        const double coordinate=glm::dot(glm::dvec3(point),glm::dvec3(axis));
        point+=axis*float(std::round(coordinate*pixelsPerUnit)/pixelsPerUnit-coordinate);
    }
    return point;
}

inline CameraFrame BuildCameraFrame(glm::vec3 eye, glm::quat rotation, float fov,
                                    float nearPlane, float farPlane, const CameraLens& lens,
                                    int width, int height) {
    CameraFrame frame;
    frame.viewport={std::max(1,width),std::max(1,height)};
    frame.content={0,0,frame.viewport.x,frame.viewport.y};
    frame.orthographic=lens.projection==CameraProjectionKind::Orthographic;
    float visibleHeight=lens.height;
    if(lens.pixelPerfect) {
        const float fit=std::min(float(frame.viewport.x)/lens.referenceWidth,float(frame.viewport.y)/lens.referenceHeight);
        const float scale=fit>=1?std::floor(fit):fit;
        frame.content.z=std::max(1,int(std::floor(lens.referenceWidth*scale)));
        frame.content.w=std::max(1,int(std::floor(lens.referenceHeight*scale)));
        frame.content.x=(frame.viewport.x-frame.content.z)/2;
        frame.content.y=(frame.viewport.y-frame.content.w)/2;
        visibleHeight=float(lens.referenceHeight)/lens.pixelsPerUnit;
        frame.pixelGrid=lens.pixelsPerUnit;
        eye=SnapCameraPlane(eye,rotation,lens.pixelsPerUnit);
    }
    frame.eye=eye;
    frame.view=glm::lookAt(eye,eye+rotation*glm::vec3(0,0,-1),rotation*glm::vec3(0,1,0));
    const float aspect=float(frame.content.z)/frame.content.w;
    frame.projection=frame.orthographic
        ? glm::ortho(-visibleHeight*.5f*aspect,visibleHeight*.5f*aspect,-visibleHeight*.5f,visibleHeight*.5f,nearPlane,farPlane)
        : glm::perspective(glm::radians(fov),aspect,nearPlane,farPlane);
    return frame;
}
