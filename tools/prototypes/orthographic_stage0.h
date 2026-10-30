#pragma once

// Stage 0 camera contract prototype, not a public/runtime camera component.
// Existing scene/script cameras retain their perspective-only API unchanged.
#include "CameraProjection.h"
#include <stdexcept>

namespace Judas2DStage0 {
enum class ProjectionKind { Perspective, Orthographic };
struct Projection {
    ProjectionKind kind=ProjectionKind::Perspective;
    float verticalFov=60, visibleHeight=8, nearPlane=.1f, farPlane=100;
    glm::mat4 Matrix(float aspect) const {
        if(!std::isfinite(aspect)||aspect<=0||!ValidCameraRange(nearPlane,farPlane))
            throw std::invalid_argument("invalid camera aspect/range");
        if(kind==ProjectionKind::Orthographic){
            if(!std::isfinite(visibleHeight)||visibleHeight<=0)
                throw std::invalid_argument("invalid orthographic height");
            const float half=visibleHeight*.5f;
            return glm::ortho(-half*aspect,half*aspect,-half,half,nearPlane,farPlane);
        }
        if(!std::isfinite(verticalFov)||verticalFov<=0||verticalFov>=180)
            throw std::invalid_argument("invalid perspective angle");
        return glm::perspective(glm::radians(verticalFov),aspect,nearPlane,farPlane);
    }
    ViewportPoint Project(const glm::mat4& view,float aspect,glm::vec3 point) const {
        auto result=ProjectViewport(view,Matrix(aspect),point);
        if(kind==ProjectionKind::Orthographic){
            result.behind=result.distance<=0;
            result.inside=result.inside&&!result.behind;
        }
        return result;
    }
    struct Ray {glm::vec3 origin,direction;};
    Ray Unproject(const glm::mat4& view,float aspect,glm::vec2 viewport) const {
        if(!std::isfinite(viewport.x)||!std::isfinite(viewport.y))
            throw std::invalid_argument("nonfinite viewport point");
        const auto inverse=glm::inverse(Matrix(aspect)*view);
        const auto a=inverse*glm::vec4(2*viewport.x-1,1-2*viewport.y,-1,1);
        const auto b=inverse*glm::vec4(2*viewport.x-1,1-2*viewport.y,1,1);
        const auto nearPoint=glm::vec3(a)/a.w,farPoint=glm::vec3(b)/b.w;
        const auto eye=glm::vec3(glm::inverse(view)*glm::vec4(0,0,0,1));
        return {kind==ProjectionKind::Orthographic?nearPoint:eye,glm::normalize(farPoint-nearPoint)};
    }
};
} // namespace Judas2DStage0
