#include "PerformanceProfiler.h"
#include "Tangents.h"
#include "Renderer.h"
#include "SkeletalAnimation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#include <SDL2/SDL.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {

// Milestone 9: every mesh (built-in primitives and imported models alike)
// now carries position + normal + UV, and is lit by one small ambient term
// plus one directional light, then modulated by a sampled texture (a 1x1
// white fallback for a mesh with no real one — see Renderer::DrawMesh) and
// a per-draw tint color. See docs/ARCHITECTURE.md, "Milestone 9, Lighting,"
// for the exact model and why nothing here reads gravity/local-up/any
// Judas-specific concept — this shader only ever sees plain world-space
// vectors handed to it by SetLighting.
const char* kVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec3 aLocalPos;
layout(location = 1) in vec3 aLocalNormal;
layout(location = 2) in vec2 aUV;
layout(location = 8) in vec2 aUV1;
layout(location = 5) in vec4 aTangent;

layout(location = 3) in uvec4 aJoints;
layout(location = 4) in vec4 aWeights;
uniform bool uSkinned;
layout(location = 6) in uvec4 aJoints1;
layout(location = 7) in vec4 aWeights1;
uniform samplerBuffer uBones;
mat4 bone(uint joint) { int b=int(joint)*4;return mat4(texelFetch(uBones,b),texelFetch(uBones,b+1),texelFetch(uBones,b+2),texelFetch(uBones,b+3)); }
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

// Milestone 15: one light-space transform PER SHADOW SLOT (see
// src/Light.h's kDirectionalShadowSlot/kTorchShadowSlot/
// kShipHeadlightShadowSlot) — always all three, computed unconditionally
// every vertex regardless of whether this frame's active lights actually
// use each one (e.g. the torch slot when the torch is off): cheap at this
// engine's tiny vertex counts, and far simpler than a dynamically-indexed
// varying (GLSL doesn't support indexing a varying array by a value that
// differs per dynamic light in the fragment shader's own per-light loop —
// see the fragment shader below for how the FIXED set of three is
// selected between instead).
uniform mat4 uLightSpaceMatrix[3];

out vec3 vWorldNormal;
out vec4 vWorldTangent;
out vec3 vWorldPos;
out vec2 vUV;
out vec2 vUV1;
out vec4 vDirLightSpacePos;
out vec4 vTorchLightSpacePos;
out vec4 vShipLightSpacePos;

void main() {
    // uNormalMatrix = transpose(inverse(mat3(uModel))), computed on the CPU
    // once per draw call (see DrawMesh) — the standard correction so
    // normals stay perpendicular to their surface under non-uniform scale
    // (DrawBox's halfExtents are rarely a uniform scale), not just rotation.
    mat4 skin=mat4(1.0);
    if(uSkinned)skin=bone(aJoints.x)*aWeights.x+bone(aJoints.y)*aWeights.y+bone(aJoints.z)*aWeights.z+bone(aJoints.w)*aWeights.w+bone(aJoints1.x)*aWeights1.x+bone(aJoints1.y)*aWeights1.y+bone(aJoints1.z)*aWeights1.z+bone(aJoints1.w)*aWeights1.w;
    vec4 localPosition=skin*vec4(aLocalPos,1.0);
    mat3 linear=mat3(skin);
    vec3 normal=abs(determinant(linear))>1e-8?transpose(inverse(linear))*aLocalNormal:linear*aLocalNormal;
    vWorldNormal = uNormalMatrix * normal;
    vec3 tangent=mat3(uModel)*linear*aTangent.xyz;
    vWorldTangent=vec4(tangent,aTangent.w*(determinant(mat3(uModel)*linear)<0.0?-1.0:1.0));
    // Milestone 14: the fragment's own world-space position, needed so the
    // fragment shader can compute a per-fragment vector TO each dynamic
    // point/spot light (distance-based attenuation, cone angle) — the
    // Milestone 9 directional light never needed this, since a directional
    // light's contribution doesn't depend on fragment position at all.
    vWorldPos = vec3(uModel * localPosition);
    vUV = aUV;vUV1=aUV1;
    vDirLightSpacePos = uLightSpaceMatrix[0] * vec4(vWorldPos, 1.0);
    vTorchLightSpacePos = uLightSpaceMatrix[1] * vec4(vWorldPos, 1.0);
    vShipLightSpacePos = uLightSpaceMatrix[2] * vec4(vWorldPos, 1.0);
    gl_Position = uProjection * uView * uModel * localPosition;
}
)";

const char* kFragmentShaderSource = R"(#version 330 core
in vec3 vWorldNormal;
in vec4 vWorldTangent;
in vec3 vWorldPos;
in vec2 vUV;
in vec2 vUV1;
in vec4 vDirLightSpacePos;
in vec4 vTorchLightSpacePos;
in vec4 vShipLightSpacePos;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;
uniform bool uWaterEnabled;
uniform sampler2D uWaterPaths;
uniform mat4 uWaterViewProjection;
uniform mat4 uWaterInverseViewProjection;
uniform vec3 uLightDirection;  // world-space, normalized, points FROM the surface TOWARD the light
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;

// Milestone 15: one depth texture per shadow slot (see src/Light.h) —
// slot 0 is the directional "sun," slot 1 the player torch, slot 2 the
// spacecraft headlight. Bound to fixed texture units 1/2/3 (uTexture stays
// on unit 0 — see Renderer::Init) so all four textures this shader ever
// samples are bound simultaneously, no rebinding between them mid-draw.
uniform sampler2D uShadowMapDir;
uniform sampler2D uShadowMapTorch;
uniform sampler2D uShadowMapShip;

// Milestone 14: dynamic point/spot lights — see src/Light.h and
// docs/ARCHITECTURE.md, "Milestone 14," for the full model. A fixed-size
// array (kMaxDynamicLights on the CPU side, mirrored here) is
// deliberately small and explicit rather than unbounded — see "Milestone
// 14, Light limits." `uLightCount` (<= the array size) bounds the loop so
// unused slots are never touched, not merely zeroed.
#define MAX_DYNAMIC_LIGHTS 5
uniform int uLightCount;
uniform vec3 uDynamicLightPosition[MAX_DYNAMIC_LIGHTS];
uniform vec3 uDynamicLightDirection[MAX_DYNAMIC_LIGHTS];  // spot only; the direction the light FACES
uniform vec3 uDynamicLightColor[MAX_DYNAMIC_LIGHTS];      // already intensity-scaled, see Renderer.cpp
uniform float uDynamicLightRange[MAX_DYNAMIC_LIGHTS];
uniform float uDynamicLightInnerCos[MAX_DYNAMIC_LIGHTS];  // spot only
uniform float uDynamicLightOuterCos[MAX_DYNAMIC_LIGHTS];  // spot only
uniform int uDynamicLightIsSpot[MAX_DYNAMIC_LIGHTS];      // 0 = point, 1 = spot
// Milestone 15: -1 = this light casts no shadow; 1 = uses vTorchLightSpacePos
// / uShadowMapTorch; 2 = uses vShipLightSpacePos / uShadowMapShip (slot 0,
// the directional light, is applied separately below, not through this
// per-dynamic-light array — see src/Light.h's kDirectionalShadowSlot/
// kTorchShadowSlot/kShipHeadlightShadowSlot).
uniform int uDynamicLightShadowIndex[MAX_DYNAMIC_LIGHTS];

// Milestone 15: samples `shadowMap` at `lightSpacePos` (already multiplied
// by that light's own view*projection in the vertex shader) and returns
// how LIT this fragment is (1.0 = fully lit, 0.0 = fully shadowed) — a
// simple 3x3 percentage-closer-filter (9 taps) for a soft, non-aliased
// edge, the minimal sensible technique this milestone's brief allows (see
// docs/ARCHITECTURE.md, "Milestone 15," for why nothing fancier was
// built). A slope-scaled bias (steeper-facing surfaces need a larger
// bias) avoids most shadow-acne self-shadowing while limiting peter-
// panning; a fragment whose projected position falls outside the shadow
// map's own [0,1] coverage (or beyond its far plane) is treated as fully
// lit — there is no occluder DATA there, not evidence of no occluder, but
// this is the same "no light reaches unclaimed space" honesty this
// engine already applies elsewhere (see docs/ARCHITECTURE.md's gravity-
// context law) rather than guessing.
float ComputeShadowFactor(vec4 lightSpacePos, sampler2D shadowMap, float ndotl) {
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;  // NDC [-1,1] -> texture/depth [0,1]

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) {
        return 1.0;
    }

    float bias = max(0.006 * (1.0 - ndotl), 0.0015);
    float currentDepth = projCoords.z - bias;

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float litSum = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float closestDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            litSum += currentDepth <= closestDepth ? 1.0 : 0.0;
        }
    }
    return litSum / 9.0;
}


uniform int uMaterialModel,uAlphaMode;
uniform float uAlphaCutoff,uMetallic,uRoughness,uNormalStrength,uOcclusionStrength,uEmissionIntensity;
uniform bool uFlipV,uModern,uEnvironmentEnabled,uBaseLinear;
uniform vec4 uBaseFactor,uUVTransform;
uniform int uMapUVSet[5];uniform vec4 uMapUVTransform[5];uniform float uMapUVRotation[5];
vec2 mapUV(int i){vec2 p=(uMapUVSet[i]==1?vUV1:vUV)*uMapUVTransform[i].xy;float c=cos(uMapUVRotation[i]),s=sin(uMapUVRotation[i]);p=mat2(c,s,-s,c)*p+uMapUVTransform[i].zw;p=p*uUVTransform.xy+uUVTransform.zw;if(uFlipV)p.y=1.0-p.y;return p;}
uniform vec3 uEmission,uCameraPosition;
uniform sampler2D uMR,uNormal,uOcclusion,uEmissive,uEnvDiffuse,uEnvSpecular,uBRDF;
uniform bool uHasMR,uHasNormal,uHasOcclusion,uHasEmissive,uEmissiveLinear;
uniform float uEnvironmentIntensity,uEnvLevels;
uniform mat3 uEnvironmentInverse;
const float PI=3.14159265358979323846;
vec3 decodeSRGB(vec3 c){return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(0.04045),c));}
vec2 envUV(vec3 d){return vec2(atan(d.z,d.x)/(2.0*PI),acos(clamp(d.y,-1.0,1.0))/PI);}
vec3 BRDF(vec3 base,float metal,float rough,vec3 n,vec3 v,vec3 l){
 float nl=max(dot(n,l),0.0),nv=max(dot(n,v),0.0001);if(nl<=0.0)return vec3(0.0);
 vec3 h=normalize(v+l);float nh=max(dot(n,h),0.0),vh=max(dot(v,h),0.0);
 float a=max(rough*rough,0.0025),a2=a*a,den=nh*nh*(a2-1.0)+1.0;
 float D=a2/(PI*den*den);
 float visibility=0.5/max(nl*sqrt(nv*nv*(1.0-a2)+a2)+nv*sqrt(nl*nl*(1.0-a2)+a2),0.000001);
 vec3 f0=mix(vec3(0.04),base,metal),f=f0+(vec3(1.0)-f0)*pow(1.0-vh,5.0);
 return ((vec3(1.0)-f)*(1.0-metal)*base/PI+D*visibility*f)*nl;
}
vec3 pbrLight(vec3 base,float metal,float rough,vec3 n,vec3 v,vec2 uv){
 vec3 result=BRDF(base,metal,rough,n,v,uLightDirection)*uLightColor*ComputeShadowFactor(vDirLightSpacePos,uShadowMapDir,max(dot(n,uLightDirection),0.0));
 for(int i=0;i<uLightCount;++i){vec3 to=uDynamicLightPosition[i]-vWorldPos;float distance=length(to);vec3 l=distance>0.00001?to/distance:n;float f=clamp(distance/max(uDynamicLightRange[i],0.0001),0.0,1.0),w=clamp(1.0-f*f*f*f,0.0,1.0);float attenuation=w*w/(distance*distance+1.0);float spot=uDynamicLightIsSpot[i]!=0?smoothstep(uDynamicLightOuterCos[i],uDynamicLightInnerCos[i],dot(-l,uDynamicLightDirection[i])):1.0;float shadow=1.0;if(uDynamicLightShadowIndex[i]==1)shadow=ComputeShadowFactor(vTorchLightSpacePos,uShadowMapTorch,max(dot(n,l),0.0));else if(uDynamicLightShadowIndex[i]==2)shadow=ComputeShadowFactor(vShipLightSpacePos,uShadowMapShip,max(dot(n,l),0.0));result+=BRDF(base,metal,rough,n,v,l)*uDynamicLightColor[i]*attenuation*spot*shadow;}
 float ao=uHasOcclusion?mix(1.0,texture(uOcclusion,mapUV(3)).r,uOcclusionStrength):1.0;
 vec3 f0=mix(vec3(0.04),base,metal);
 if(uEnvironmentEnabled){float nv=max(dot(n,v),0.0);vec3 f=f0+(max(vec3(1.0-rough),f0)-f0)*pow(1.0-nv,5.0);vec3 diffuse=texture(uEnvDiffuse,envUV(uEnvironmentInverse*n)).rgb*base*(vec3(1.0)-f)*(1.0-metal);vec3 reflection=reflect(-v,n);vec3 spec=textureLod(uEnvSpecular,envUV(uEnvironmentInverse*reflection),rough*(uEnvLevels-1.0)).rgb;vec2 brdf=texture(uBRDF,vec2(nv,rough)).rg;result+=(diffuse+spec*(f0*brdf.x+brdf.y))*uEnvironmentIntensity*ao;
 }else result+=uAmbientColor*base*(1.0-metal)*ao;
 return result;
}

void main() {
    vec2 materialUV=mapUV(0);
    vec3 normal = normalize(vWorldNormal);if(uMaterialModel!=0&&!gl_FrontFacing)normal=-normal;
    if(uMaterialModel!=0&&uHasNormal){vec3 t=vWorldTangent.xyz-normal*dot(normal,vWorldTangent.xyz);if(dot(t,t)>0.000001){t=normalize(t);vec3 b=cross(normal,t)*vWorldTangent.w;t*=uUVTransform.x<0.0?-1.0:1.0;b*=uUVTransform.y<0.0?-1.0:1.0;vec3 map=texture(uNormal,mapUV(2)).xyz*2.0-1.0;map.xy*=uNormalStrength;normal=normalize(mat3(t,b,normal)*map);}}


    float diffuseFactor = max(dot(normal, uLightDirection), 0.0);
    float dirShadow = ComputeShadowFactor(vDirLightSpacePos, uShadowMapDir, diffuseFactor);
    vec3 lighting = uAmbientColor + uLightColor * diffuseFactor * dirShadow;

    for (int i = 0; i < uLightCount; ++i) {
        vec3 toLight = uDynamicLightPosition[i] - vWorldPos;
        float distance = length(toLight);
        vec3 lightDir = distance > 1.0e-5 ? toLight / distance : vec3(0.0, 1.0, 0.0);

        // Smooth-windowed inverse-square attenuation (see Renderer.cpp,
        // SetDynamicLights, for the full derivation/citation) — genuinely
        // inverse-square close to the light, smoothly reaches exactly zero
        // at uDynamicLightRange[i] instead of a hard cliff or a never-zero
        // tail, and the "+1.0" keeps it finite as distance approaches 0.
        float rangeFraction = clamp(distance / max(uDynamicLightRange[i], 1.0e-4), 0.0, 1.0);
        float windowed = 1.0 - rangeFraction * rangeFraction * rangeFraction * rangeFraction;
        windowed = clamp(windowed, 0.0, 1.0);
        float attenuation = (windowed * windowed) / (distance * distance + 1.0);

        float spotFactor = 1.0;
        if (uDynamicLightIsSpot[i] != 0) {
            float cosAngle = dot(-lightDir, uDynamicLightDirection[i]);
            spotFactor = smoothstep(uDynamicLightOuterCos[i], uDynamicLightInnerCos[i], cosAngle);
        }

        float lightDiffuse = max(dot(normal, lightDir), 0.0);

        float shadow = 1.0;
        int shadowIndex = uDynamicLightShadowIndex[i];
        if (shadowIndex == 1) {
            shadow = ComputeShadowFactor(vTorchLightSpacePos, uShadowMapTorch, lightDiffuse);
        } else if (shadowIndex == 2) {
            shadow = ComputeShadowFactor(vShipLightSpacePos, uShadowMapShip, lightDiffuse);
        }

        lighting += uDynamicLightColor[i] * lightDiffuse * attenuation * spotFactor * shadow;
    }

    vec4 texColor = texture(uTexture,materialUV);
    vec4 base=vec4((uMaterialModel==0||uBaseLinear)?texColor.rgb:decodeSRGB(texColor.rgb),texColor.a)*uBaseFactor*uColor;
    if(uAlphaMode==1&&base.a<uAlphaCutoff)discard;
    if(uMaterialModel==0){FragColor=vec4(lighting*base.rgb,base.a);if(uModern)FragColor.rgb=decodeSRGB(FragColor.rgb);}
    else {vec3 emission=uEmission*(uHasEmissive?(uEmissiveLinear?texture(uEmissive,mapUV(4)).rgb:decodeSRGB(texture(uEmissive,mapUV(4)).rgb)):vec3(1.0))*uEmissionIntensity;float metal=uMetallic,rough=uRoughness;if(uHasMR){vec4 mr=texture(uMR,mapUV(1));rough*=mr.g;metal*=mr.b;}vec3 v=normalize(uCameraPosition-vWorldPos);FragColor=vec4((uMaterialModel==2?base.rgb:pbrLight(base.rgb,metal,clamp(rough,0.05,1.0),normal,v,materialUV))+emission,uAlphaMode==0?1.0:base.a);}

    if (uWaterEnabled) {
        vec4 clip = uWaterViewProjection * vec4(vWorldPos,1.0);
        vec2 uv = clip.xy / clip.w * .5 + .5;
        vec2 path = texture(uWaterPaths,uv).rg;
        vec4 nearPoint = uWaterInverseViewProjection * vec4(uv*2.0-1.0,-1.0,1.0);
        float distance = length(vWorldPos-nearPoint.xyz/nearPoint.w);
        float waterLength = max(0.0,min(distance,path.y)-path.x);
        vec3 transmission = exp(-vec3(.32,.12,.075)*waterLength);
        FragColor.rgb = FragColor.rgb*transmission + vec3(.025,.18,.24)*(vec3(1.0)-transmission);
    }
    if(uModern||uMaterialModel!=0)FragColor.rgb=clamp(FragColor.rgb,vec3(0.0),vec3(60000.0));
    if(!uModern&&uMaterialModel!=0){vec3 c=max(FragColor.rgb,vec3(0.0));if(uMaterialModel==1)c/=1.0+dot(c,vec3(.2126,.7152,.0722));FragColor.rgb=mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-0.055,step(vec3(.0031308),c));}
}
)";

// Milestone 15: the shadow pass's own minimal shader — position only, no
// normal/UV attributes read, no color output at all (the FBO it draws
// into has no color attachment, see Renderer::Init — only depth is
// written, by the fixed-function depth test/write GL already performs
// for every draw call). Deliberately separate from kVertexShaderSource/
// kFragmentShaderSource above, the same "a second, dedicated shader for a
// genuinely different pass" reasoning kUIVertexShaderSource/
// kUIFragmentShaderSource already established for the UI overlay.
const char* kShadowVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec3 aLocalPos;
layout(location = 2) in vec2 aUV;
layout(location = 8) in vec2 aUV1;
out vec2 shadowUV;out vec2 shadowUV1;

layout(location = 3) in uvec4 aJoints;
layout(location = 4) in vec4 aWeights;
uniform bool uSkinned;
layout(location = 6) in uvec4 aJoints1;
layout(location = 7) in vec4 aWeights1;
uniform samplerBuffer uBones;
mat4 bone(uint joint) { int b=int(joint)*4;return mat4(texelFetch(uBones,b),texelFetch(uBones,b+1),texelFetch(uBones,b+2),texelFetch(uBones,b+3)); }
uniform mat4 uModel;
uniform mat4 uLightViewProj;

void main() {
    mat4 skin=mat4(1.0);
    if(uSkinned)skin=bone(aJoints.x)*aWeights.x+bone(aJoints.y)*aWeights.y+bone(aJoints.z)*aWeights.z+bone(aJoints.w)*aWeights.w+bone(aJoints1.x)*aWeights1.x+bone(aJoints1.y)*aWeights1.y+bone(aJoints1.z)*aWeights1.z+bone(aJoints1.w)*aWeights1.w;
    shadowUV=aUV;shadowUV1=aUV1;gl_Position = uLightViewProj * uModel * skin * vec4(aLocalPos, 1.0);
}
)";

const char* kShadowFragmentShaderSource = R"(#version 330 core
in vec2 shadowUV;in vec2 shadowUV1;uniform sampler2D uTexture;uniform int uAlphaMode;uniform float uAlphaCutoff,uAlphaFactor;uniform bool uFlipV;uniform vec4 uUVTransform;uniform int uMapUVSet[5];uniform vec4 uMapUVTransform[5];uniform float uMapUVRotation[5];
void main() {
    vec2 uv=(uMapUVSet[0]==1?shadowUV1:shadowUV)*uMapUVTransform[0].xy;float c=cos(uMapUVRotation[0]),s=sin(uMapUVRotation[0]);uv=mat2(c,s,-s,c)*uv+uMapUVTransform[0].zw;uv=uv*uUVTransform.xy+uUVTransform.zw;if(uFlipV)uv.y=1.0-uv.y;
    if(uAlphaMode==1&&texture(uTexture,uv).a*uAlphaFactor<uAlphaCutoff)discard;
    // This FBO has no color attachment (see
    // Renderer::Init's glDrawBuffer(GL_NONE)) — only gl_FragDepth's
    // implicit default (gl_FragCoord.z) is ever written, by the ordinary
    // depth test every draw call already performs.
}
)";

bool CompileShader(GLenum type, const char* source, GLuint& outShader) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<size_t>(logLength > 0 ? logLength : 1));
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Shader compile error: %s\n", log.data());
        glDeleteShader(shader);
        return false;
    }

    outShader = shader;
    return true;
}

bool LinkProgram(GLuint vertexShader, GLuint fragmentShader, GLuint& outProgram) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        std::vector<char> log(static_cast<size_t>(logLength > 0 ? logLength : 1));
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Program link error: %s\n", log.data());
        glDeleteProgram(program);
        return false;
    }

    outProgram = program;
    return true;
}

// A unit cube (36 vertices, non-indexed, two triangles per face) centered on
// the origin in local [-0.5, 0.5] space — the same geometry Judas has used
// since Milestone 2, now carrying a real per-face normal (flat-shaded, no
// vertex-normal averaging across faces — an ordinary box's edges are
// genuinely sharp) and geometrically consistent planar UVs per face.
// Winding remains unchanged; face culling is not enabled.
MeshData BuildCubeMeshData() {
    struct FaceVertex {
        glm::vec3 position;
    };
    // clang-format off
    const FaceVertex kPositions[36] = {
        // back face (normal -Z)
        {{-0.5f, -0.5f, -0.5f}}, {{0.5f, 0.5f, -0.5f}},  {{0.5f, -0.5f, -0.5f}},
        {{0.5f, 0.5f, -0.5f}},   {{-0.5f, -0.5f, -0.5f}}, {{-0.5f, 0.5f, -0.5f}},
        // front face (normal +Z)
        {{-0.5f, -0.5f, 0.5f}},  {{0.5f, -0.5f, 0.5f}},  {{0.5f, 0.5f, 0.5f}},
        {{0.5f, 0.5f, 0.5f}},    {{-0.5f, 0.5f, 0.5f}},  {{-0.5f, -0.5f, 0.5f}},
        // left face (normal -X)
        {{-0.5f, 0.5f, 0.5f}},   {{-0.5f, 0.5f, -0.5f}}, {{-0.5f, -0.5f, -0.5f}},
        {{-0.5f, -0.5f, -0.5f}}, {{-0.5f, -0.5f, 0.5f}}, {{-0.5f, 0.5f, 0.5f}},
        // right face (normal +X)
        {{0.5f, 0.5f, 0.5f}},    {{0.5f, -0.5f, -0.5f}}, {{0.5f, 0.5f, -0.5f}},
        {{0.5f, -0.5f, -0.5f}},  {{0.5f, 0.5f, 0.5f}},   {{0.5f, -0.5f, 0.5f}},
        // bottom face (normal -Y)
        {{-0.5f, -0.5f, -0.5f}}, {{0.5f, -0.5f, -0.5f}}, {{0.5f, -0.5f, 0.5f}},
        {{0.5f, -0.5f, 0.5f}},   {{-0.5f, -0.5f, 0.5f}}, {{-0.5f, -0.5f, -0.5f}},
        // top face (normal +Y)
        {{-0.5f, 0.5f, -0.5f}},  {{0.5f, 0.5f, 0.5f}},   {{0.5f, 0.5f, -0.5f}},
        {{0.5f, 0.5f, 0.5f}},    {{-0.5f, 0.5f, -0.5f}}, {{-0.5f, 0.5f, 0.5f}},
    };
    // clang-format on
    const glm::vec3 kFaceNormals[6] = {
        {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},  {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
    };
    MeshData mesh;
    mesh.vertices.reserve(36);
    for (int face = 0; face < 6; ++face) {
        for (int v = 0; v < 6; ++v) {
            MeshVertex vertex;
            vertex.position = kPositions[face * 6 + v].position;
            vertex.normal = kFaceNormals[face];
            const glm::vec3 p = vertex.position;
            // Same position gets the same UV on both triangles of a face.
            // U points right when the face is viewed from outside; V is up.
            switch (face) {
                case 0: vertex.uv = {0.5f - p.x, p.y + 0.5f}; break;
                case 1: vertex.uv = {p.x + 0.5f, p.y + 0.5f}; break;
                case 2: vertex.uv = {p.z + 0.5f, p.y + 0.5f}; break;
                case 3: vertex.uv = {0.5f - p.z, p.y + 0.5f}; break;
                case 4: vertex.uv = {p.x + 0.5f, p.z + 0.5f}; break;
                default: vertex.uv = {p.x + 0.5f, 0.5f - p.z}; break;
            }
            mesh.vertices.push_back(vertex);
        }
    }
    return mesh;  // indices left empty: drawn non-indexed, as always
}

// A unit sphere (radius 1, non-indexed triangles), generated as a
// conventional UV sphere. A unit sphere's own outward normal at any point
// is simply that point itself (already unit length) — no separate normal
// computation needed. UV uses the ordinary lat/lon parameterization
// (u = longitude fraction, v = 1 - latitude fraction so v=1 is the pole at
// local +Y, matching the same bottom-is-v0 convention documented for
// textures — see docs/ARCHITECTURE.md).
MeshData GenerateUnitSphereMeshData(int latitudeSegments, int longitudeSegments) {
    auto pointOnSphere = [](float latFraction, float lonFraction) {
        const float theta = latFraction * glm::pi<float>();       // 0 (top) .. pi (bottom)
        const float phi = lonFraction * 2.0f * glm::pi<float>();  // 0 .. 2pi around
        return glm::vec3(std::sin(theta) * std::cos(phi), std::cos(theta),
                          std::sin(theta) * std::sin(phi));
    };

    MeshData mesh;
    mesh.vertices.reserve(static_cast<size_t>(latitudeSegments) * longitudeSegments * 6);

    for (int lat = 0; lat < latitudeSegments; ++lat) {
        const float v0 = static_cast<float>(lat) / latitudeSegments;
        const float v1 = static_cast<float>(lat + 1) / latitudeSegments;
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            const float u0 = static_cast<float>(lon) / longitudeSegments;
            const float u1 = static_cast<float>(lon + 1) / longitudeSegments;

            const glm::vec3 p00 = pointOnSphere(v0, u0);
            const glm::vec3 p01 = pointOnSphere(v0, u1);
            const glm::vec3 p10 = pointOnSphere(v1, u0);
            const glm::vec3 p11 = pointOnSphere(v1, u1);

            const glm::vec3 positions[6] = {p00, p10, p11, p00, p11, p01};
            const glm::vec2 uvs[6] = {{u0, 1.0f - v0}, {u0, 1.0f - v1}, {u1, 1.0f - v1},
                                       {u0, 1.0f - v0}, {u1, 1.0f - v1}, {u1, 1.0f - v0}};
            for (int i = 0; i < 6; ++i) {
                MeshVertex vertex;
                vertex.position = positions[i];
                vertex.normal = positions[i];  // unit sphere: normal == position
                vertex.uv = uvs[i];
                mesh.vertices.push_back(vertex);
            }
        }
    }
    return mesh;  // indices left empty: drawn non-indexed, as always
}

constexpr int kSphereLatitudeSegments = 16;
constexpr int kSphereLongitudeSegments = 24;

// Milestone 15: shadow-map resolution, shared by Init (texture creation)
// and BeginShadowPass (viewport sizing) — see docs/ARCHITECTURE.md,
// "Milestone 15," for why 1024x1024 was judged sufficient (and not
// excessive) for this demo's world scale, for all three shadow slots
// alike (no separate resolution per light).
constexpr int kShadowMapResolution = 1024;

// Milestone 13: the UI overlay's own tiny shader — deliberately separate
// from kVertexShaderSource/kFragmentShaderSource above rather than a
// reused/branching variant of them. The 3D shader's whole shape (a
// view/projection matrix, a world-space normal, per-fragment lighting) is
// dead weight for a screen-space rectangle, and forcing UI draws through it
// would mean disabling lighting with uniform tricks instead of just not
// having it. `uPosition`/`uSize` are already in PIXELS — this shader
// converts straight to clip space using `uScreenSize`, no separate
// orthographic projection matrix needed. `uUVOffset`/`uUVScale` select a
// sub-rectangle of whatever texture is bound (the whole [0,1] rect for a
// solid-color panel via the white fallback texture, a font atlas glyph's
// own rect for text) — see Renderer::DrawUIRect/DrawUIText.
const char* kUIVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec2 aUnit;  // unit quad, or streamed pixel position
layout(location = 1) in vec2 aUV;
uniform int uTextVertices;

uniform vec2 uScreenSize;
uniform vec2 uPosition;  // pixels, top-left of this rect
uniform vec2 uSize;      // pixels

out vec2 vUnit;

void main() {
    vUnit = uTextVertices != 0 ? aUV : aUnit;
    vec2 pixelPos = uTextVertices != 0 ? aUnit : uPosition + aUnit * uSize;
    // Pixel space is top-down (y grows downward, matching uPosition's own
    // "top-left corner" convention); NDC y grows upward, so it's flipped
    // here rather than by pre-flipping any texture data — see
    // src/FontLoader.h's own header comment for why the glyph atlas needs
    // no separate flip convention as a result.
    vec2 ndc = vec2(pixelPos.x / uScreenSize.x * 2.0 - 1.0,
                     1.0 - pixelPos.y / uScreenSize.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

const char* kUIFragmentShaderSource = R"(#version 330 core
in vec2 vUnit;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;
uniform vec2 uUVOffset;
uniform vec2 uUVScale;

void main() {
    vec2 uv = uUVOffset + vUnit * uUVScale;
    FragColor = texture(uTexture, uv) * uColor;
}
)";

// Milestone 30: the debug-line shader — world-space position + colour per
// vertex, one view-projection uniform, no lighting, no texture. Kept apart
// from the lit mesh shader for the same reason the UI shader is: it is a
// genuinely different pass (GL_LINES, per-vertex colour, streamed data).
const char* kDebugVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
uniform mat4 uViewProjection;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
)";

const char* kDebugFragmentShaderSource = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

}  // namespace

bool Renderer::Init() {
    GLint units=0;glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&units);m_textureUnitLimit=unsigned(units);std::fprintf(stderr,"Renderer fragment texture units: %d (material path uses 12)\n",units);if(units<12){std::fprintf(stderr,"M57 requires 12 fragment texture units, available %d\n",units);return false;}
    GLuint vertexShader = 0;
    if (!CompileShader(GL_VERTEX_SHADER, kVertexShaderSource, vertexShader)) {
        return false;
    }

    GLuint fragmentShader = 0;
    if (!CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSource, fragmentShader)) {
        glDeleteShader(vertexShader);
        return false;
    }

    bool linked = LinkProgram(vertexShader, fragmentShader, m_shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    if (!linked) {
        return false;
    }

    m_uModel = glGetUniformLocation(m_shaderProgram, "uModel");
    m_uSkinned=glGetUniformLocation(m_shaderProgram,"uSkinned");m_uBones=glGetUniformLocation(m_shaderProgram,"uBones");
    m_uNormalMatrix = glGetUniformLocation(m_shaderProgram, "uNormalMatrix");
    m_uView = glGetUniformLocation(m_shaderProgram, "uView");
    m_uProjection = glGetUniformLocation(m_shaderProgram, "uProjection");
    m_uColor = glGetUniformLocation(m_shaderProgram, "uColor");
    m_uTexture = glGetUniformLocation(m_shaderProgram, "uTexture");
    m_uLightDirection = glGetUniformLocation(m_shaderProgram, "uLightDirection");
    m_uLightColor = glGetUniformLocation(m_shaderProgram, "uLightColor");
    m_uAmbientColor = glGetUniformLocation(m_shaderProgram, "uAmbientColor");

    // Milestone 14: one uniform location per dynamic-light field, per
    // array slot — see Renderer.h's own comment on why these can't be
    // cached as a single location the way a plain uniform can.
    m_uLightCount = glGetUniformLocation(m_shaderProgram, "uLightCount");
    for (int i = 0; i < kMaxDynamicLights; ++i) {
        const std::string prefix = "uDynamicLight";
        const std::string index = "[" + std::to_string(i) + "]";
        m_uDynamicLightPosition[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "Position" + index).c_str());
        m_uDynamicLightDirection[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "Direction" + index).c_str());
        m_uDynamicLightColor[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "Color" + index).c_str());
        m_uDynamicLightRange[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "Range" + index).c_str());
        m_uDynamicLightInnerCos[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "InnerCos" + index).c_str());
        m_uDynamicLightOuterCos[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "OuterCos" + index).c_str());
        m_uDynamicLightIsSpot[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "IsSpot" + index).c_str());
        m_uDynamicLightShadowIndex[i] =
            glGetUniformLocation(m_shaderProgram, (prefix + "ShadowIndex" + index).c_str());
    }

    // Milestone 15: one mat4 + one sampler2D location per shadow slot.
    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        const std::string matrixName = "uLightSpaceMatrix[" + std::to_string(slot) + "]";
        m_uLightSpaceMatrix[slot] = glGetUniformLocation(m_shaderProgram, matrixName.c_str());
    }
    m_uShadowMapSampler[0] = glGetUniformLocation(m_shaderProgram, "uShadowMapDir");
    m_uShadowMapSampler[1] = glGetUniformLocation(m_shaderProgram, "uShadowMapTorch");
    m_uShadowMapSampler[2] = glGetUniformLocation(m_shaderProgram, "uShadowMapShip");

    // Texture unit 0 is the diffuse/white-fallback texture (unchanged
    // since Milestone 9); units 1-3 are the three shadow maps (Milestone
    // 15) — all four bound once here rather than every draw call, since
    // which GL texture OBJECT each unit points at only ever changes when a
    // shadow map is re-rendered (see BeginShadowPass), not which UNIT a
    // given uniform samples from.
    glUseProgram(m_shaderProgram);
    glUniform1i(m_uTexture, 0);
    // Sampler types must use distinct units even when uSkinned is false.
    // Initialize this before the first ordinary draw; a project need not
    // render a skeleton first to make its boxes and spheres valid GL draws.
    glUniform1i(m_uBones, 15);
    glUniform1i(m_uShadowMapSampler[0], 1);
    glUniform1i(m_uShadowMapSampler[1], 2);
    glUniform1i(m_uShadowMapSampler[2], 3);

    glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    auto cube=BuildCubeMeshData();GenerateMeshTangents(cube);m_cubeMesh=CreateMesh(cube);
    auto sphere=GenerateUnitSphereMeshData(kSphereLatitudeSegments,kSphereLongitudeSegments);GenerateMeshTangents(sphere);m_sphereMesh=CreateMesh(sphere);

    // The "no real texture" fallback DrawMesh substitutes for an invalid
    // TextureHandle (see ResolveTexture) — a single opaque white pixel, so
    // `texColor * tintColor` reduces to exactly `tintColor`, reproducing
    // every pre-Milestone-9 solid-color DrawBox/DrawSphere call exactly.
    TextureData white;
    white.width = 1;
    white.height = 1;
    white.pixels = {255, 255, 255, 255};
    m_whiteTexture = CreateTexture(white);

    // A small default so a mesh drawn before the caller's first SetLighting
    // call (shouldn't happen in practice, but costs nothing to guard) is at
    // least dimly visible rather than pitch black.
    SetLighting(glm::vec3(0.3f, 0.6f, 0.4f), glm::vec3(1.0f), glm::vec3(0.15f));
    // Milestone 14: no dynamic lights until the caller's first
    // SetDynamicLights call — explicit rather than relying on GLSL's own
    // zero-initialized-uniform default, so this is true by construction,
    // not by an implementation detail of the driver.
    SetDynamicLights({});

    // --- Milestone 15: shadow-mapping resources ---
    GLuint shadowVertexShader = 0;
    if (!CompileShader(GL_VERTEX_SHADER, kShadowVertexShaderSource, shadowVertexShader)) {
        return false;
    }
    GLuint shadowFragmentShader = 0;
    if (!CompileShader(GL_FRAGMENT_SHADER, kShadowFragmentShaderSource, shadowFragmentShader)) {
        glDeleteShader(shadowVertexShader);
        return false;
    }
    const bool shadowLinked = LinkProgram(shadowVertexShader, shadowFragmentShader, m_shadowShaderProgram);
    glDeleteShader(shadowVertexShader);
    glDeleteShader(shadowFragmentShader);
    if (!shadowLinked) {
        return false;
    }
    m_uShadowModel = glGetUniformLocation(m_shadowShaderProgram, "uModel");
    m_uShadowSkinned=glGetUniformLocation(m_shadowShaderProgram,"uSkinned");m_uShadowBones=glGetUniformLocation(m_shadowShaderProgram,"uBones");
    m_uShadowLightViewProj = glGetUniformLocation(m_shadowShaderProgram, "uLightViewProj");
    glUseProgram(m_shadowShaderProgram);
    glUniform1i(m_uShadowBones, 15);

    // One depth-texture/FBO pair per shadow slot (src/Light.h), created
    // once and reused every frame — never allocated/freed per-light or
    // per-draw (see docs/ARCHITECTURE.md, "Milestone 15, Resource
    // ownership"). GL_NEAREST filtering (not the usual GL_LINEAR) because
    // this engine does its own multi-tap PCF filtering in the fragment
    // shader (see kFragmentShaderSource's ComputeShadowFactor) — linearly
    // filtering raw, unblended depth VALUES before comparison would
    // average depths together in a way that's meaningless (and wrong) for
    // a shadow test, unlike ordinary color filtering. GL_CLAMP_TO_EDGE
    // wrapping plus an explicit in-shader bounds check (rather than
    // GL_CLAMP_TO_BORDER with a border color) keeps this to GL surface
    // this engine already has — see ComputeShadowFactor's own "outside
    // the shadow map's coverage counts as fully lit" comment.
    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        glGenTextures(1, &m_shadowMapTexture[slot]);
        glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture[slot]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, kShadowMapResolution, kShadowMapResolution, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &m_shadowFbo[slot]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo[slot]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                m_shadowMapTexture[slot], 0);
        // No color attachment exists for this FBO at all — tell GL not to
        // expect or provide one, or GL_FRAMEBUFFER_COMPLETE would
        // (correctly) fail on some drivers.
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "Shadow framebuffer %d is incomplete.\n", slot);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            return false;
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // --- Milestone 13: UI overlay shader + quad ---
    GLuint uiVertexShader = 0;
    if (!CompileShader(GL_VERTEX_SHADER, kUIVertexShaderSource, uiVertexShader)) {
        return false;
    }
    GLuint uiFragmentShader = 0;
    if (!CompileShader(GL_FRAGMENT_SHADER, kUIFragmentShaderSource, uiFragmentShader)) {
        glDeleteShader(uiVertexShader);
        return false;
    }
    const bool uiLinked = LinkProgram(uiVertexShader, uiFragmentShader, m_uiShaderProgram);
    glDeleteShader(uiVertexShader);
    glDeleteShader(uiFragmentShader);
    if (!uiLinked) {
        return false;
    }

    m_uiUTextVertices=glGetUniformLocation(m_uiShaderProgram,"uTextVertices");
    m_uiUScreenSize = glGetUniformLocation(m_uiShaderProgram, "uScreenSize");
    m_uiUPosition = glGetUniformLocation(m_uiShaderProgram, "uPosition");
    m_uiUSize = glGetUniformLocation(m_uiShaderProgram, "uSize");
    m_uiUColor = glGetUniformLocation(m_uiShaderProgram, "uColor");
    m_uiUTexture = glGetUniformLocation(m_uiShaderProgram, "uTexture");
    m_uiUUVOffset = glGetUniformLocation(m_uiShaderProgram, "uUVOffset");
    m_uiUUVScale = glGetUniformLocation(m_uiShaderProgram, "uUVScale");
    glUseProgram(m_uiShaderProgram);
    glUniform1i(m_uiUTexture, 0);

    // A single non-indexed unit quad (two triangles, top-left origin,
    // [0,1]x[0,1]) reused for every DrawUIRect/DrawUIText glyph call —
    // per-draw placement/size/UV-rect come entirely from uniforms (see
    // kUIVertexShaderSource), so no per-call vertex upload is needed, the
    // same "one shared mesh, transform via uniforms" shape DrawBox/
    // DrawSphere already use with m_cubeMesh/m_sphereMesh.
    const float kUnitQuadVertices[12] = {
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
    };
    glGenVertexArrays(1, &m_uiQuadVao);
    glBindVertexArray(m_uiQuadVao);
    glGenBuffers(1, &m_uiQuadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiQuadVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(kUnitQuadVertices)),
                 kUnitQuadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenVertexArrays(1,&m_textVao);glBindVertexArray(m_textVao);
    glGenBuffers(1,&m_textVbo);glBindBuffer(GL_ARRAY_BUFFER,m_textVbo);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,4*sizeof(float),nullptr);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,4*sizeof(float),reinterpret_cast<void*>(2*sizeof(float)));glEnableVertexAttribArray(1);
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);

    // --- Milestone 30: debug-line shader + streamed VBO ---
    GLuint debugVertexShader = 0;
    if (!CompileShader(GL_VERTEX_SHADER, kDebugVertexShaderSource, debugVertexShader)) return false;
    GLuint debugFragmentShader = 0;
    if (!CompileShader(GL_FRAGMENT_SHADER, kDebugFragmentShaderSource, debugFragmentShader)) {
        glDeleteShader(debugVertexShader);
        return false;
    }
    const bool debugLinked = LinkProgram(debugVertexShader, debugFragmentShader, m_debugShaderProgram);
    glDeleteShader(debugVertexShader);
    glDeleteShader(debugFragmentShader);
    if (!debugLinked) return false;
    m_debugUViewProjection = glGetUniformLocation(m_debugShaderProgram, "uViewProjection");
    glGenVertexArrays(1, &m_debugVao);
    glBindVertexArray(m_debugVao);
    glGenBuffers(1, &m_debugVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugVbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Milestone 15: until the first real BeginShadowPass call each frame
    // (every interactive-loop frame calls it three times — see
    // Application.cpp; TestHarness.cpp's own render path never does,
    // since shadows remain an interactive-loop-only concern, the same
    // scoping Milestone 13/14 already established for UI/dynamic
    // lighting), each shadow slot's light-space matrix must still map any
    // real-world point to an OUT-OF-RANGE shadow coordinate (z > 1), so
    // ComputeShadowFactor's own bounds check falls back to "fully lit"
    // rather than an identity matrix that could coincidentally land
    // in-range for points near the origin. This matrix maps EVERY input
    // point to the fixed point (0, 0, 2, 1) regardless of its own
    // position (every column affecting x/y/z is zero except a constant
    // z-translation of 2) — deliberately degenerate, only ever used as
    // this "definitely out of range" placeholder, never a real shadow
    // transform.
    glm::mat4 alwaysOutOfRangeMatrix(0.0f);
    alwaysOutOfRangeMatrix[3][2] = 2.0f;
    alwaysOutOfRangeMatrix[3][3] = 1.0f;
    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        m_shadowLightSpaceMatrix[slot] = alwaysOutOfRangeMatrix;
    }

    return true;
}

void Renderer::TraceResourceOperation(ResourceTracePoint point, unsigned int handle, std::size_t bytes) const {
    if (!m_resourceTrace) return;
    ResourceTraceEvent event{point, {}, {}};
    event.handle = handle;
    event.bytes = bytes;
    event.contextCurrent = SDL_GL_GetCurrentContext() != nullptr;
    m_resourceTrace(event);
}

void Renderer::UploadPosePalette(const std::vector<glm::mat4>& pose) {
    if(!m_poseBuffer)glGenBuffers(1,&m_poseBuffer);
    if(!m_poseTexture)glGenTextures(1,&m_poseTexture);
    glBindBuffer(GL_TEXTURE_BUFFER,m_poseBuffer);
    glBufferData(GL_TEXTURE_BUFFER,GLsizeiptr(pose.size()*sizeof(glm::mat4)),pose.data(),GL_STREAM_DRAW);
    glActiveTexture(GL_TEXTURE15);glBindTexture(GL_TEXTURE_BUFFER,m_poseTexture);
    glTexBuffer(GL_TEXTURE_BUFFER,GL_RGBA32F,m_poseBuffer);glActiveTexture(GL_TEXTURE0);
}
void Renderer::Shutdown() {
    if(m_poseTexture){glDeleteTextures(1,&m_poseTexture);m_poseTexture=0;}
    if(m_poseBuffer){glDeleteBuffers(1,&m_poseBuffer);m_poseBuffer=0;}
    ResetProjectText();
    for(auto& q:m_profileQueries){if(q.begin)glDeleteQueries(1,&q.begin);if(q.end)glDeleteQueries(1,&q.end);q={};}
    m_profileQueriesReady=false;
    PerformanceProfiler::Get().GPUAvailability(false,"Renderer/context shut down");
    TraceResourceOperation(ResourceTracePoint::RendererShutdownBegin);
    EndRenderTarget();
    ShutdownAppearance();
    if(m_particleProgram)glDeleteProgram(m_particleProgram);
    if(m_particleVbo)glDeleteBuffers(1,&m_particleVbo);
    if(m_particleVao)glDeleteVertexArrays(1,&m_particleVao);
    m_particleProgram=m_particleVbo=m_particleVao=0;
    m_particleVertices.clear();m_particleOrder.clear();
    for (unsigned int i = 0; i < m_targets.size(); ++i) DestroyRenderTarget(RenderTargetHandle{i});
    m_targets.clear();
    for (std::size_t i = 0; i < m_meshes.size(); ++i) {
        DestroyMesh(MeshHandle{static_cast<unsigned int>(i)});
    }
    m_meshes.clear();

    for (std::size_t i = 0; i < m_textures.size(); ++i) {
        DestroyTexture(TextureHandle{static_cast<unsigned int>(i)});
    }
    m_textures.clear();

    if (m_shaderProgram) {
        if(m_waterPathTexture){glDeleteTextures(1,&m_waterPathTexture);m_waterPathTexture=0;m_waterColumns=m_waterRows=0;}
        glDeleteProgram(m_shaderProgram);
        m_shaderProgram = 0;
    }

    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        if (m_shadowFbo[slot]) {
            glDeleteFramebuffers(1, &m_shadowFbo[slot]);
            m_shadowFbo[slot] = 0;
        }
        if (m_shadowMapTexture[slot]) {
            glDeleteTextures(1, &m_shadowMapTexture[slot]);
            m_shadowMapTexture[slot] = 0;
        }
    }
    if (m_shadowShaderProgram) {
        glDeleteProgram(m_shadowShaderProgram);
        m_shadowShaderProgram = 0;
    }

    if(m_textVbo){glDeleteBuffers(1,&m_textVbo);m_textVbo=0;}
    if(m_textVao){glDeleteVertexArrays(1,&m_textVao);m_textVao=0;}
    if (m_uiQuadVbo) {
        glDeleteBuffers(1, &m_uiQuadVbo);
        m_uiQuadVbo = 0;
    }
    if (m_uiQuadVao) {
        glDeleteVertexArrays(1, &m_uiQuadVao);
        m_uiQuadVao = 0;
    }
    if (m_uiShaderProgram) {
        glDeleteProgram(m_uiShaderProgram);
        m_uiShaderProgram = 0;
    }
    if (m_debugVbo) { glDeleteBuffers(1, &m_debugVbo); m_debugVbo = 0; }
    if (m_debugVao) { glDeleteVertexArrays(1, &m_debugVao); m_debugVao = 0; }
    if (m_debugShaderProgram) { glDeleteProgram(m_debugShaderProgram); m_debugShaderProgram = 0; }
    m_transientSurface={};
    m_fontLoaded=false;m_uiFonts.clear();m_defaultUIFont.clear();m_defaultTextFont.reset();m_textFonts.clear();
    TraceResourceOperation(ResourceTracePoint::RendererShutdownEnd);
}

void Renderer::DrawDebugLines(const std::vector<DebugLine>& lines, bool depthTest) {
    if (lines.empty() || m_shadowPassActive || !m_debugShaderProgram) return;
    if (!depthTest) glDisable(GL_DEPTH_TEST);
    std::vector<float> vertices;
    vertices.reserve(lines.size() * 12);
    for (const DebugLine& line : lines) {
        for (const glm::vec3* p : {&line.a, &line.b}) {
            vertices.push_back(p->x); vertices.push_back(p->y); vertices.push_back(p->z);
            vertices.push_back(line.color.r); vertices.push_back(line.color.g); vertices.push_back(line.color.b);
        }
    }
    glUseProgram(m_debugShaderProgram);
    const glm::mat4 viewProjection = m_projection * m_view;
    glUniformMatrix4fv(m_debugUViewProjection, 1, GL_FALSE, glm::value_ptr(viewProjection));
    glBindVertexArray(m_debugVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(),
                 GL_STREAM_DRAW);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size() * 2));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    if (!depthTest) glEnable(GL_DEPTH_TEST);
    ++m_stats.drawCalls;
    m_stats.debugLines += static_cast<unsigned int>(lines.size());
}

void Renderer::BeginFrame(int windowWidth, int windowHeight) {
    PollProfileGPU();
    glViewport(0, 0, windowWidth, windowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(m_linearRendering)BeginLinearPass(windowWidth,windowHeight);
}

void Renderer::SetCamera(const glm::mat4& view, const glm::mat4& projection) {
    m_view = view;
    m_projection = projection;
    m_frustum=Frustum(projection*view);
    if(m_shaderProgram){glUseProgram(m_shaderProgram);glUniform1i(glGetUniformLocation(m_shaderProgram,"uWaterEnabled"),0);}
}

void Renderer::SetWaterPaths(unsigned columns,unsigned rows,const std::vector<glm::vec2>& paths){
 JUDAS_PROFILE_SCOPE("Water boundary upload");
    if(!m_shaderProgram||paths.size()!=size_t(columns)*rows||!columns||!rows)return;
    if(!m_waterPathTexture){glGenTextures(1,&m_waterPathTexture);}
    glActiveTexture(GL_TEXTURE4);glBindTexture(GL_TEXTURE_2D,m_waterPathTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if(columns!=m_waterColumns||rows!=m_waterRows){glTexImage2D(GL_TEXTURE_2D,0,GL_RG32F,columns,rows,0,GL_RG,GL_FLOAT,paths.data());m_waterColumns=columns;m_waterRows=rows;}
    else glTexSubImage2D(GL_TEXTURE_2D,0,0,0,columns,rows,GL_RG,GL_FLOAT,paths.data());
    glUseProgram(m_shaderProgram);glUniform1i(glGetUniformLocation(m_shaderProgram,"uWaterEnabled"),1);
    glUniform1i(glGetUniformLocation(m_shaderProgram,"uWaterPaths"),4);
    glm::mat4 matrix=m_projection*m_view,inverse=glm::inverse(matrix);
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram,"uWaterViewProjection"),1,GL_FALSE,glm::value_ptr(matrix));
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram,"uWaterInverseViewProjection"),1,GL_FALSE,glm::value_ptr(inverse));
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::SetLighting(const glm::vec3& direction, const glm::vec3& lightColor,
                            const glm::vec3& ambientColor) {
    glUseProgram(m_shaderProgram);
    const glm::vec3 normalizedDirection =
        glm::length(direction) > 1.0e-6f ? glm::normalize(direction) : glm::vec3(0.0f, 1.0f, 0.0f);
    glUniform3f(m_uLightDirection, normalizedDirection.x, normalizedDirection.y,
                normalizedDirection.z);
    glUniform3f(m_uLightColor, lightColor.r, lightColor.g, lightColor.b);
    glUniform3f(m_uAmbientColor, ambientColor.r, ambientColor.g, ambientColor.b);
}

void Renderer::SetDynamicLights(const std::vector<DynamicLight>& lights) {
    glUseProgram(m_shaderProgram);

    const int count = std::min(static_cast<int>(lights.size()), kMaxDynamicLights);
    glUniform1i(m_uLightCount, count);
    m_stats.dynamicLights = static_cast<unsigned int>(count);

    for (int i = 0; i < count; ++i) {
        const DynamicLight& light = lights[static_cast<size_t>(i)];
        glUniform3f(m_uDynamicLightPosition[i], light.position.x, light.position.y, light.position.z);
        const glm::vec3 direction =
            glm::length(light.direction) > 1.0e-6f ? glm::normalize(light.direction) : glm::vec3(0.0f, 0.0f, -1.0f);
        glUniform3f(m_uDynamicLightDirection[i], direction.x, direction.y, direction.z);
        glUniform3f(m_uDynamicLightColor[i], light.color.r, light.color.g, light.color.b);
        glUniform1f(m_uDynamicLightRange[i], std::max(light.range, 1.0e-3f));
        // Cosines, not degrees: computed once here (CPU) rather than once
        // per fragment (GPU) — see the fragment shader's own
        // smoothstep(outerCos, innerCos, cosAngle) call. Clamped so a
        // misconfigured inner > outer doesn't silently invert the falloff
        // direction (smoothstep requires edge0 <= edge1).
        const float innerCos = std::cos(glm::radians(std::min(light.innerConeDegrees, light.outerConeDegrees)));
        const float outerCos = std::cos(glm::radians(light.outerConeDegrees));
        glUniform1f(m_uDynamicLightInnerCos[i], innerCos);
        glUniform1f(m_uDynamicLightOuterCos[i], outerCos);
        glUniform1i(m_uDynamicLightIsSpot[i], light.kind == LightKind::Spot ? 1 : 0);
        glUniform1i(m_uDynamicLightShadowIndex[i], light.shadowMapIndex);
    }
}

void Renderer::BeginShadowPass(int shadowSlot, const glm::mat4& lightViewProjection) {
    m_profileShadow=BeginProfilePass("Shadow",std::uint64_t(shadowSlot));
    m_shadowLightSpaceMatrix[shadowSlot] = lightViewProjection;
    m_shadowPassActive = true;
    m_currentShadowSlot = shadowSlot;
    m_frustum=Frustum(lightViewProjection);
    ++m_stats.shadowPasses;

    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFbo[shadowSlot]);
    glViewport(0, 0, kShadowMapResolution, kShadowMapResolution);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(m_shadowShaderProgram);
    glUniformMatrix4fv(m_uShadowLightViewProj, 1, GL_FALSE, glm::value_ptr(lightViewProjection));
}

void Renderer::EndShadowPass() {
    EndProfilePass(m_profileShadow);m_profileShadow=0;
    m_shadowPassActive = false;
    m_frustum=Frustum(m_projection*m_view);
    m_currentShadowSlot = -1;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

MeshHandle Renderer::CreateMesh(const MeshData& data) {
    auto handle=BeginMeshUpload(data);
    if(!handle.IsValid())return {};
    for(size_t i=0;i<data.materials.size();++i)for(size_t j=0;j<5;++j)
        if(!UploadMeshMap(handle,data,i,j)){DestroyMesh(handle);return {};}
    return handle;
}

MeshHandle Renderer::BeginMeshUpload(const MeshData& data) {
    JUDAS_PROFILE_SCOPE("Mesh geometry upload");
    if (data.skeletal) {
        const auto count=data.skeletal->skeleton.skinNodes.size();
        GLint texels=0;glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&texels);
        if(count==0||count>size_t(texels/4)||count>kModelPaletteLimit||data.skinVertices.size()!=data.vertices.size())return {};
        for(const auto& v:data.skinVertices){float sum=0;
            for(int i=0;i<4;++i){if(v.joints[i]>=count||!std::isfinite(v.weights[i])||v.weights[i]<0)return {};sum+=v.weights[i];if(v.joints1[i]>=count||!std::isfinite(v.weights1[i])||v.weights1[i]<0)return {};sum+=v.weights1[i];}
            if(std::abs(sum-1.f)>1e-4f)return {};
        }
    }else if(!data.skinVertices.empty())return {};
    GpuMesh mesh;
    mesh.primitives=data.primitives;for(const auto& material:data.materials)mesh.materials.push_back(CreateMaterial(MaterialSettings(material)));
    mesh.alive = true;
    if(data.skeletal){mesh.restSkin=ResolveSkinMatrices(data.skeletal->skeleton,data.skeletal->skeleton.rest);for(auto& part:data.primitives)if(part.count)mesh.partOrientation.push_back(data.skinVertices.at(data.indices.empty()?part.first:data.indices.at(part.first)));else mesh.partOrientation.emplace_back();}
    for(const auto& v:data.vertices)mesh.bounds.Include(v.position);
    mesh.vertexCount = static_cast<GLsizei>(data.vertices.size());

    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(data.vertices.size() * sizeof(MeshVertex)),
                 data.vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                           reinterpret_cast<const void*>(offsetof(MeshVertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                           reinterpret_cast<const void*>(offsetof(MeshVertex, normal)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                           reinterpret_cast<const void*>(offsetof(MeshVertex, uv)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(8,2,GL_FLOAT,GL_FALSE,sizeof(MeshVertex),reinterpret_cast<void*>(offsetof(MeshVertex,uv1)));glEnableVertexAttribArray(8);
    glVertexAttribPointer(5,4,GL_FLOAT,GL_FALSE,sizeof(MeshVertex),reinterpret_cast<void*>(offsetof(MeshVertex,tangent)));glEnableVertexAttribArray(5);
    if(!data.skinVertices.empty()){
        glGenBuffers(1,&mesh.skinVbo);glBindBuffer(GL_ARRAY_BUFFER,mesh.skinVbo);
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(data.skinVertices.size()*sizeof(MeshSkinVertex)),data.skinVertices.data(),GL_STATIC_DRAW);
        glVertexAttribIPointer(3,4,GL_UNSIGNED_INT,sizeof(MeshSkinVertex),reinterpret_cast<void*>(offsetof(MeshSkinVertex,joints)));glEnableVertexAttribArray(3);
        glVertexAttribPointer(4,4,GL_FLOAT,GL_FALSE,sizeof(MeshSkinVertex),reinterpret_cast<void*>(offsetof(MeshSkinVertex,weights)));glEnableVertexAttribArray(4);
        glVertexAttribIPointer(6,4,GL_UNSIGNED_INT,sizeof(MeshSkinVertex),reinterpret_cast<void*>(offsetof(MeshSkinVertex,joints1)));glEnableVertexAttribArray(6);
        glVertexAttribPointer(7,4,GL_FLOAT,GL_FALSE,sizeof(MeshSkinVertex),reinterpret_cast<void*>(offsetof(MeshSkinVertex,weights1)));glEnableVertexAttribArray(7);
    }

    if (!data.indices.empty()) {
        mesh.indexCount = static_cast<GLsizei>(data.indices.size());
        glGenBuffers(1, &mesh.ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(data.indices.size() * sizeof(std::uint32_t)),
                     data.indices.data(), GL_STATIC_DRAW);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Deliberately NOT unbinding GL_ELEMENT_ARRAY_BUFFER here — it is part
    // of the VAO's own state, and unbinding it while this VAO is still
    // bound would detach it from the VAO itself.

    MeshHandle handle;
    handle.id = static_cast<unsigned int>(m_meshes.size());
    m_meshes.push_back(mesh);
    TraceResourceOperation(ResourceTracePoint::MeshCreated, handle.id,
                           data.vertices.size() * sizeof(MeshVertex) + data.indices.size() * sizeof(std::uint32_t) + data.skinVertices.size()*sizeof(MeshSkinVertex));
    return handle;
}

bool Renderer::UpdateMeshVertices(MeshHandle handle, const std::vector<MeshVertex>& vertices) {
    GpuMesh* mesh = GetMesh(handle);
    // Indexed deformables keep immutable topology; only their vertex stream
    // changes. Existing non-indexed transient surfaces may still resize.
    if (!mesh || (mesh->ebo != 0 && vertices.size()!=size_t(mesh->vertexCount))) return false;
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(MeshVertex)),
                 vertices.empty() ? nullptr : vertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    mesh->vertexCount = static_cast<GLsizei>(vertices.size());
    mesh->bounds={};for(const auto& v:vertices)mesh->bounds.Include(v.position);
    return true;
}

Renderer::GpuMesh* Renderer::GetMesh(MeshHandle handle) {
    if (!handle.IsValid() || handle.id >= m_meshes.size() || !m_meshes[handle.id].alive) {
        return nullptr;
    }
    return &m_meshes[handle.id];
}

void Renderer::DestroyMesh(MeshHandle handle) {
    GpuMesh* mesh = GetMesh(handle);
    if (!mesh) return;
    if (mesh->ebo) glDeleteBuffers(1, &mesh->ebo);
    glDeleteBuffers(1, &mesh->vbo);
    for(auto material:mesh->materials)DestroyMaterial(material);
    if(mesh->skinVbo)glDeleteBuffers(1,&mesh->skinVbo);
    glDeleteVertexArrays(1, &mesh->vao);
    const std::size_t bytes = static_cast<std::size_t>(mesh->vertexCount) * sizeof(MeshVertex) +
                              static_cast<std::size_t>(mesh->indexCount) * sizeof(std::uint32_t);
    *mesh = GpuMesh{};
    TraceResourceOperation(ResourceTracePoint::MeshDestroyed, handle.id, bytes);
}

TextureHandle Renderer::CreateTexture(const TextureData& data,bool srgb) {
    GpuTexture texture;texture.width=data.width;texture.height=data.height;texture.srgb=srgb;
    texture.alive = true;
    texture.uploadedBytes = data.pixels.size();

    glGenTextures(1, &texture.textureId);
    glBindTexture(GL_TEXTURE_2D, texture.textureId);
    {JUDAS_PROFILE_SCOPE("Texture driver texel upload");
    glTexImage2D(GL_TEXTURE_2D, 0, srgb?GL_SRGB8_ALPHA8:GL_RGBA8, data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data.pixels.data());}
    {JUDAS_PROFILE_SCOPE("Texture driver mipmap generation");glGenerateMipmap(GL_TEXTURE_2D);}

    // Linear filtering both ways (mipmapped minification), repeat wrapping —
    // ordinary, sufficient defaults for one UV-mapped demo texture; nothing
    // here yet needs per-texture control over these (see
    // docs/ARCHITECTURE.md's remaining-limitations note).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);

    TextureHandle handle;
    handle.id = static_cast<unsigned int>(m_textures.size());
    m_textures.push_back(texture);
    TraceResourceOperation(ResourceTracePoint::TextureCreated, handle.id, texture.uploadedBytes);
    return handle;
}

void Renderer::DestroyTexture(TextureHandle handle) {
    if (!handle.IsValid() || handle.id >= m_textures.size() || !m_textures[handle.id].alive) {
        return;
    }
    auto colour=m_textures[handle.id].colourView;if(colour.IsValid())DestroyTexture(colour);
    glDeleteTextures(1, &m_textures[handle.id].textureId);
    const std::size_t bytes = m_textures[handle.id].uploadedBytes;
    m_textures[handle.id] = GpuTexture{};
    TraceResourceOperation(ResourceTracePoint::TextureDestroyed, handle.id, bytes);
}

bool Renderer::ReadMeshForDiagnostics(MeshHandle handle, MeshData& outData) const {
    if (!SDL_GL_GetCurrentContext() || !handle.IsValid() || handle.id >= m_meshes.size()) return false;
    const GpuMesh& mesh = m_meshes[handle.id];
    if (!mesh.alive || !glIsBuffer(mesh.vbo) || (mesh.ebo && !glIsBuffer(mesh.ebo))) return false;
    GLint previousBuffer = 0;
    glGetIntegerv(GL_COPY_READ_BUFFER, &previousBuffer);
    MeshData data;
    glBindBuffer(GL_COPY_READ_BUFFER, mesh.vbo);
    GLint64 vertexBytes = 0;
    glGetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &vertexBytes);
    if (vertexBytes < 0 || vertexBytes % sizeof(MeshVertex) != 0) {
        glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(previousBuffer));
        return false;
    }
    data.vertices.resize(static_cast<std::size_t>(vertexBytes) / sizeof(MeshVertex));
    if (vertexBytes) glGetBufferSubData(GL_COPY_READ_BUFFER, 0, static_cast<GLsizeiptr>(vertexBytes), data.vertices.data());
    if (mesh.ebo) {
        glBindBuffer(GL_COPY_READ_BUFFER, mesh.ebo);
        GLint64 indexBytes = 0;
        glGetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &indexBytes);
        if (indexBytes < 0 || indexBytes % sizeof(std::uint32_t) != 0) {
            glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(previousBuffer));
            return false;
        }
        data.indices.resize(static_cast<std::size_t>(indexBytes) / sizeof(std::uint32_t));
        if (indexBytes) glGetBufferSubData(GL_COPY_READ_BUFFER, 0, static_cast<GLsizeiptr>(indexBytes), data.indices.data());
    }
    glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(previousBuffer));
    outData = std::move(data);
    return true;
}

bool Renderer::ReadTextureForDiagnostics(TextureHandle handle, TextureData& outData) const {
    if (!SDL_GL_GetCurrentContext() || !handle.IsValid() || handle.id >= m_textures.size()) return false;
    const GpuTexture& texture = m_textures[handle.id];
    if (!texture.alive || !glIsTexture(texture.textureId)) return false;
    GLint previousTexture = 0, previousPackBuffer = 0;
    GLint alignment = 0, rowLength = 0, skipRows = 0, skipPixels = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previousPackBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
    glGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPixels);
    glBindTexture(GL_TEXTURE_2D, texture.textureId);
    TextureData data;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &data.width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &data.height);
    if (data.width <= 0 || data.height <= 0) {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
        return false;
    }
    data.pixels.resize(static_cast<std::size_t>(data.width) * static_cast<std::size_t>(data.height) * TextureData::kChannels);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    glPixelStorei(GL_PACK_ROW_LENGTH, rowLength);
    glPixelStorei(GL_PACK_SKIP_ROWS, skipRows);
    glPixelStorei(GL_PACK_SKIP_PIXELS, skipPixels);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(previousPackBuffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    outData = std::move(data);
    return true;
}

std::uintptr_t Renderer::EditorImageToken(TextureHandle texture) const {return static_cast<std::uintptr_t>(ResolveTexture(texture));}

GLuint Renderer::ResolveTexture(TextureHandle handle) const {
    if (handle.IsValid() && handle.id < m_textures.size() && m_textures[handle.id].alive) {
        return m_textures[handle.id].textureId;
    }
    // Falls back to the white 1x1 texture — including, harmlessly, during
    // Init() itself while m_whiteTexture is still being constructed (the
    // fallback-of-the-fallback is textureId 0, an unbound texture object,
    // which GL_TEXTURE_2D simply samples as opaque black; never reached in
    // practice since nothing draws before Init finishes).
    if (m_whiteTexture.IsValid() && m_whiteTexture.id < m_textures.size()) {
        return m_textures[m_whiteTexture.id].textureId;
    }
    return 0;
}

void Renderer::DrawMesh(MeshHandle mesh, const glm::vec3& position, const glm::quat& rotation,
                         const glm::vec3& scale, TextureHandle texture,
                         const glm::vec3& tintColor, float alpha,const std::vector<glm::mat4>* skin,const std::vector<std::string>* hiddenParts) {
    GpuMesh* gpuMesh = GetMesh(mesh);
    if (!gpuMesh) return;
    if(!m_shadowPassActive&&!AllowsLayer(m_renderLayer)){++m_stats.layerRejectedDraws;return;}

    static const GpuMaterial failed=[](){GpuMaterial m;m.definition.model=MaterialModel::Unlit;m.definition.baseColor={.8f,.02f,.65f,1};return m;}();
    static const GpuMaterial pending=[](){GpuMaterial m;m.definition.model=MaterialModel::Unlit;m.definition.baseColor={.25f,.25f,.28f,1};return m;}();
    auto materialAt=[&](size_t slot)->const GpuMaterial* {MaterialHandle handle;if(slot<m_materialBindings.size())handle=m_materialBindings[slot].handle;if(!handle.IsValid()&&slot<m_materialBindings.size()&&m_materialBindings[slot].explicitAsset)return m_materialBindings[slot].failed?&failed:&pending;if(!handle.IsValid()&&slot<gpuMesh->primitives.size()&&m_linearRendering){int index=gpuMesh->primitives[slot].material;if(index>=0&&size_t(index)<gpuMesh->materials.size())handle=gpuMesh->materials[index];}return handle.IsValid()&&handle.id<m_materials.size()&&m_materials[handle.id].alive?&m_materials[handle.id]:nullptr;};
    size_t parts=std::max(size_t(1),gpuMesh->primitives.size());bool blended=false;for(size_t i=0;i<parts;++i)if(auto* m=materialAt(i))blended|=m->definition.alpha==MaterialAlpha::Blend;
    if(blended&&!m_shadowPassActive&&!m_flushingBlends){BlendDraw draw{mesh,position,scale,tintColor,rotation,texture,alpha,skin?*skin:std::vector<glm::mat4>{},m_materialBindings,-(m_view*glm::vec4(position,1)).z,m_renderLayer,hiddenParts?*hiddenParts:std::vector<std::string>{}};m_blendDraws.push_back(std::move(draw));}
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
    const auto* palette=skin&&!skin->empty()?skin:&gpuMesh->restSkin;
    auto submit=[&](bool shadow){for(size_t i=0;i<parts;++i){if(hiddenParts&&!gpuMesh->primitives.empty()&&std::find(hiddenParts->begin(),hiddenParts->end(),gpuMesh->primitives[i].part)!=hiddenParts->end())continue;auto* m=materialAt(i);bool isBlend=m&&m->definition.alpha==MaterialAlpha::Blend;if((shadow&&isBlend)||(!shadow&&isBlend!=m_flushingBlends))continue;MaterialOverride overrides;if(i<m_materialBindings.size())overrides=m_materialBindings[i].overrides;BindMaterial(m,overrides,texture,tintColor,alpha,shadow);glm::mat4 orientation(1);if(i<gpuMesh->partOrientation.size()&&!palette->empty()){orientation=glm::mat4(0);auto& w=gpuMesh->partOrientation[i];for(int k=0;k<4;++k)orientation+=palette->at(w.joints[k])*w.weights[k]+palette->at(w.joints1[k])*w.weights1[k];}glFrontFace(glm::determinant(glm::mat3(model*orientation))<0?GL_CW:GL_CCW);unsigned first=0,count=unsigned(gpuMesh->ebo?gpuMesh->indexCount:gpuMesh->vertexCount);if(!gpuMesh->primitives.empty()){first=gpuMesh->primitives[i].first;count=gpuMesh->primitives[i].count;}glBindVertexArray(gpuMesh->vao);if(gpuMesh->ebo)glDrawElements(GL_TRIANGLES,GLsizei(count),GL_UNSIGNED_INT,reinterpret_cast<void*>(size_t(first)*4));else glDrawArrays(GL_TRIANGLES,GLint(first),GLsizei(count));++m_stats.drawCalls;m_stats.triangles+=count/3;}glFrontFace(GL_CCW);};
    if(palette->size()>kModelPaletteLimit||palette->size()!=gpuMesh->restSkin.size())return;
    VisualBounds bounds=gpuMesh->bounds;
    if(!palette->empty()){bounds={};for(const auto& matrix:*palette){auto b=TransformBounds(gpuMesh->bounds,matrix);bounds.Include(b.min);bounds.Include(b.max);}}
    ++m_stats.renderablesConsidered;
    if(!IsVisible(TransformBounds(bounds,model))){++m_stats.renderablesCulled;return;}
    ++m_stats.renderablesVisible;

    // Milestone 15: while a shadow pass is active (see BeginShadowPass),
    // every DrawMesh call writes depth only, from that light's own view/
    // projection, through the separate minimal shadow shader — normals,
    // UVs, textures, and every lighting uniform are irrelevant to a depth-
    // only pass, so none of them are touched here.


    if (m_shadowPassActive) {
        glUseProgram(m_shadowShaderProgram);
        glUniform1i(m_uShadowSkinned,!palette->empty());if(!palette->empty()){UploadPosePalette(*palette);glUniform1i(m_uShadowBones,15);}
        glUniformMatrix4fv(m_uShadowModel, 1, GL_FALSE, glm::value_ptr(model));
        submit(true);
        return;
    }

    // Standard correction for non-uniform scale — see the vertex shader's
    // own comment. glm::inverseTranspose is glm's dedicated helper for
    // exactly this (normal-matrix) computation.
    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(model));

    glUseProgram(m_shaderProgram);
    glUniform1i(m_uSkinned,!palette->empty());if(!palette->empty()){UploadPosePalette(*palette);glUniform1i(m_uBones,15);}
    glUniformMatrix4fv(m_uModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(m_uNormalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniformMatrix4fv(m_uView, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniform4f(m_uColor, tintColor.r, tintColor.g, tintColor.b, alpha);
    // Milestone 15: this frame's three shadow light-space matrices — see
    // BeginShadowPass's own comment for why every normal-mode draw simply
    // reuses whatever this frame's shadow passes most recently cached,
    // with no separate "apply shadow data" call needed from the caller.
    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        glUniformMatrix4fv(m_uLightSpaceMatrix[slot], 1, GL_FALSE,
                            glm::value_ptr(m_shadowLightSpaceMatrix[slot]));
    }

    glActiveTexture(GL_TEXTURE0);
    if (m_activeTarget.IsValid() && texture.IsValid() &&
        texture.id == RenderTargetTexture(m_activeTarget).id) {
        texture = m_whiteTexture; ++m_stats.feedbackFallbacks;
    }
    glBindTexture(GL_TEXTURE_2D, ResolveTexture(texture));
    for (int slot = 0; slot < kShadowMapCount; ++slot) {
        glActiveTexture(GL_TEXTURE0 + 1 + slot);
        glBindTexture(GL_TEXTURE_2D, m_shadowMapTexture[slot]);
    }
    glActiveTexture(GL_TEXTURE0);  // restore the default active unit other calls (UI, texture creation) assume

    submit(false);
}

void Renderer::DrawBox(const glm::vec3& position, const glm::quat& rotation,
                        const glm::vec3& halfExtents, const glm::vec3& colorRgb, float alpha, TextureHandle texture) {
    DrawMesh(m_cubeMesh, position, rotation, halfExtents * 2.0f, texture, colorRgb, alpha);
}

void Renderer::BeginTransparentPass() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
}

void Renderer::EndTransparentPass() {
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::DrawSphere(const glm::vec3& position, float radius, const glm::vec3& colorRgb,
                          float alpha, TextureHandle texture) {
    DrawMesh(m_sphereMesh, position, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(radius),
             texture, colorRgb, alpha);
}

void Renderer::CaptureFrame(int width, int height, std::vector<unsigned char>& outRgbPixels) const {
    const size_t rowBytes = static_cast<size_t>(width) * 3;
    outRgbPixels.assign(rowBytes * static_cast<size_t>(height), 0);

    // glReadPixels has no alignment padding to worry about here since 3
    // bytes/pixel with typical widths doesn't need a custom GL_PACK_ALIGNMENT
    // for this tool's purposes (rows are read directly into the output
    // buffer, then flipped below).
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, outRgbPixels.data());

    // OpenGL's framebuffer origin is bottom-left; flip rows so row 0 of the
    // output is the top of the image, matching ordinary image conventions.
    std::vector<unsigned char> rowBuffer(rowBytes);
    for (int row = 0; row < height / 2; ++row) {
        unsigned char* top = outRgbPixels.data() + static_cast<size_t>(row) * rowBytes;
        unsigned char* bottom =
            outRgbPixels.data() + static_cast<size_t>(height - 1 - row) * rowBytes;
        std::copy(top, top + rowBytes, rowBuffer.begin());
        std::copy(bottom, bottom + rowBytes, top);
        std::copy(rowBuffer.begin(), rowBuffer.end(), bottom);
    }
}

void Renderer::EndFrame() {
    FlushMaterialBlends();ResolveLinearPass();
    // Nothing to do yet; kept as an explicit boundary for future per-frame
    // work (batching, multiple draw calls, etc.) rather than for any
    // behavior this milestone needs.
}

bool Renderer::LoadFont(const char* path,float pixels,std::string& error){
    auto it=m_uiFonts.find(path);if(it==m_uiFonts.end()){std::shared_ptr<const TextFont> data;if(!LoadTextFont(path,data,error))return false;if(m_uiFonts.size()>=64){error="font compatibility cache full";return false;}it=m_uiFonts.emplace(path,data).first;}
    if(!m_defaultTextFont){m_defaultTextFont=it->second;m_defaultUIFont=path;}
    m_textFonts={it->second};m_fontPixelHeight=pixels;m_fontLoaded=true;return true;
}

void Renderer::BeginUIFrame(int windowWidth, int windowHeight) {
    glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glDisable(GL_CULL_FACE);
    m_profileUI=BeginProfilePass("Runtime UI");
    m_uiDrawCalls=0;
    m_uiScreenSize = glm::vec2(static_cast<float>(std::max(windowWidth, 1)),
                                static_cast<float>(std::max(windowHeight, 1)));
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_uiShaderProgram);
    glUniform2f(m_uiUScreenSize, m_uiScreenSize.x, m_uiScreenSize.y);
    glUniform1i(m_uiUTextVertices,0);
    glBindVertexArray(m_uiQuadVao);
}

void Renderer::DrawUIRect(const glm::vec2& position, const glm::vec2& size,
                           const glm::vec4& colorRgba) {
    glUniform2f(m_uiUPosition, position.x, position.y);
    glUniform2f(m_uiUSize, size.x, size.y);
    glUniform4f(m_uiUColor, colorRgba.r, colorRgba.g, colorRgba.b, colorRgba.a);
    glUniform2f(m_uiUUVOffset, 0.0f, 0.0f);
    glUniform2f(m_uiUUVScale, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, ResolveTexture(m_whiteTexture));
    glDrawArrays(GL_TRIANGLES, 0, 6);++m_uiDrawCalls;
}

void Renderer::DrawUIText(const std::string& text,const glm::vec2& position,float scale,const glm::vec4& tint){TextOptions o;o.pixels=m_fontPixelHeight*scale;DrawTextLayout(*LayoutText(text,o),position,tint);}
glm::vec2 Renderer::MeasureUIText(const std::string& text,float scale) const{TextOptions o;o.pixels=m_fontPixelHeight*scale;auto l=LayoutText(text,o);return {l->width,l->height};}
float Renderer::GetUITextLineHeight(float scale)const{return MeasureUIText("Mg",scale).y;}

void Renderer::EndUIFrame() {
    EndProfilePass(m_profileUI);m_profileUI=0;
    JUDAS_PROFILE_COUNTER("Runtime UI draws",double(UIDrawCalls()),ProfileCounterMode::Latest);
    ClearUIClip();
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

RenderTargetHandle Renderer::CreateRenderTarget(int width, int height, std::string& error) {
    error.clear();
    GLint maximum = 0; glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (width < 1 || height < 1 || width > maximum || height > maximum || m_activeTarget.IsValid()) {
        error = "invalid render-target dimensions or creation during an active target pass"; return {};
    }
    GLint draw = 0, read = 0, texture = 0, depth = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture); glGetIntegerv(GL_RENDERBUFFER_BINDING, &depth);
    GpuTarget target; target.width = width; target.height = height;
    GpuTexture color;color.width=width;color.height=height; color.sceneLinear=m_linearRendering;color.renderTarget=true;color.uploadedBytes = static_cast<std::size_t>(width) * height * (color.sceneLinear?8:4);
    glGenTextures(1, &color.textureId); glBindTexture(GL_TEXTURE_2D, color.textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, color.sceneLinear?GL_RGBA16F:GL_RGBA8, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &target.depth); glBindRenderbuffer(GL_RENDERBUFFER, target.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glGenFramebuffers(1, &target.framebuffer); glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color.textureId, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.depth);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(depth));
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &target.framebuffer); glDeleteRenderbuffers(1, &target.depth);
        glDeleteTextures(1, &color.textureId);
        error = "render-target framebuffer incomplete: " + std::to_string(status); return {};
    }
    color.alive = true; target.color.id = static_cast<unsigned int>(m_textures.size());
    m_textures.push_back(color); m_targets.push_back(target);
    TraceResourceOperation(ResourceTracePoint::TextureCreated, target.color.id, color.uploadedBytes);
    return RenderTargetHandle{static_cast<unsigned int>(m_targets.size() - 1)};
}

bool Renderer::ResizeRenderTarget(RenderTargetHandle& target, int width, int height, std::string& error) {
    if (m_activeTarget.IsValid()) { error = "cannot resize during a target pass"; return false; }
    if (target.IsValid() && target.id < m_targets.size()) {
        const auto& old = m_targets[target.id];
        if (old.framebuffer && old.width == width && old.height == height && m_textures[old.color.id].sceneLinear==m_linearRendering) { error.clear(); return true; }
    }
    auto replacement = CreateRenderTarget(width, height, error);
    if (!replacement.IsValid()) return false;
    DestroyRenderTarget(target); target = replacement; return true;
}

void Renderer::DestroyRenderTarget(RenderTargetHandle target) {
    if (!target.IsValid() || target.id >= m_targets.size()) return;
    if (m_activeTarget.id == target.id) EndRenderTarget();
    auto& gpu = m_targets[target.id];
    const GLuint oldFramebuffer = gpu.framebuffer, oldDepth = gpu.depth;
    const GLuint oldColor = gpu.color.IsValid() && gpu.color.id < m_textures.size() ? m_textures[gpu.color.id].textureId : 0;
    if (gpu.framebuffer) glDeleteFramebuffers(1, &gpu.framebuffer);
    if (gpu.depth) glDeleteRenderbuffers(1, &gpu.depth);
    DestroyTexture(gpu.color); gpu = GpuTarget{};
    if ((oldFramebuffer && glIsFramebuffer(oldFramebuffer)) || (oldDepth && glIsRenderbuffer(oldDepth)) ||
        (oldColor && glIsTexture(oldColor))) ++m_stats.targetDeletionFailures;
}

TextureHandle Renderer::RenderTargetTexture(RenderTargetHandle target) const {
    if (!target.IsValid() || target.id >= m_targets.size() || !m_targets[target.id].framebuffer) return {};
    return m_targets[target.id].color;
}

bool Renderer::BeginRenderTarget(RenderTargetHandle target) {
    if (m_activeTarget.IsValid() || m_shadowPassActive || !RenderTargetTexture(target).IsValid()) return false;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_savedDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_savedReadFramebuffer);
    glGetIntegerv(GL_VIEWPORT, m_savedViewport);
    m_savedRenderMask=m_renderMask;m_savedRenderLayer=m_renderLayer;
    m_savedView = m_view; m_savedProjection = m_projection; m_activeTarget = target;
    const auto& gpu = m_targets[target.id];
    glBindFramebuffer(GL_FRAMEBUFFER, gpu.framebuffer);
    BeginFrame(gpu.width, gpu.height); ++m_stats.offscreenPasses; return true;
}

void Renderer::EndRenderTarget() {
    if (!m_activeTarget.IsValid()) return;
    FlushMaterialBlends();ResolveLinearPass(m_textures[m_targets[m_activeTarget.id].color.id].sceneLinear);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(m_savedDrawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_savedReadFramebuffer));
    glViewport(m_savedViewport[0], m_savedViewport[1], m_savedViewport[2], m_savedViewport[3]);
    SetCamera(m_savedView,m_savedProjection);m_renderMask=m_savedRenderMask;m_renderLayer=m_savedRenderLayer; m_activeTarget = {};
}

Renderer::TargetDiagnostics Renderer::RenderTargetDiagnostics() const {
    TargetDiagnostics result; GLint framebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, &result.viewport[0]); result.defaultFramebuffer = framebuffer == 0;
    result.passActive = m_activeTarget.IsValid();
    for (const auto& gpu : m_targets) if (gpu.framebuffer) ++result.liveTargets;
    return result;
}

glm::ivec2 Renderer::RenderTargetSize(RenderTargetHandle target) const {
    if (!RenderTargetTexture(target).IsValid()) return glm::ivec2(0);
    return glm::ivec2(m_targets[target.id].width, m_targets[target.id].height);
}
void Renderer::FinishForDiagnostics() const { glFinish(); }


void Renderer::DrawParticles(const std::vector<ParticleBillboard>& particles,const VisualBounds& bounds,TextureHandle texture){
    if(m_shadowPassActive||particles.empty())return;
    if(!AllowsLayer(m_renderLayer)){++m_stats.layerRejectedEmitters;return;}
    ++m_stats.particleEmittersConsidered;
    if(!IsVisible(bounds)){++m_stats.particleEmittersCulled;return;}
    ++m_stats.particleEmittersVisible;
    if(!m_particleProgram){
        const char* vs=R"(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec2 uv;layout(location=2)in vec4 color;
uniform mat4 vp;out vec2 texcoord;out vec4 tint;
void main(){gl_Position=vp*vec4(position,1);texcoord=uv;tint=color;})";
        const char* fs=R"(#version 330 core
in vec2 texcoord;in vec4 tint;uniform sampler2D image;uniform bool modern,imageLinear;out vec4 result;
vec3 linear(vec3 c){return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
void main(){vec4 sampleColour=texture(image,texcoord);result=sampleColour*tint;if(modern)result.rgb=(imageLinear?sampleColour.rgb:linear(sampleColour.rgb))*linear(tint.rgb);})";
        GLuint v=0,f=0;
        if(!CompileShader(GL_VERTEX_SHADER,vs,v))return;
        if(!CompileShader(GL_FRAGMENT_SHADER,fs,f)){glDeleteShader(v);return;}
        bool ok=LinkProgram(v,f,m_particleProgram);glDeleteShader(v);glDeleteShader(f);if(!ok)return;
        glGenVertexArrays(1,&m_particleVao);glGenBuffers(1,&m_particleVbo);
        glBindVertexArray(m_particleVao);glBindBuffer(GL_ARRAY_BUFFER,m_particleVbo);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(ParticleVertex),reinterpret_cast<void*>(offsetof(ParticleVertex,position)));glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(ParticleVertex),reinterpret_cast<void*>(offsetof(ParticleVertex,uv)));glEnableVertexAttribArray(1);
        glVertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(ParticleVertex),reinterpret_cast<void*>(offsetof(ParticleVertex,color)));glEnableVertexAttribArray(2);
    }
    // Per-emitter back-to-front stable ordering, separately for each camera.
    m_particleOrder.resize(particles.size());for(size_t i=0;i<particles.size();++i)m_particleOrder[i]=i;
    std::sort(m_particleOrder.begin(),m_particleOrder.end(),[&](size_t a,size_t b){
        float za=(m_view*glm::vec4(particles[a].position,1)).z,zb=(m_view*glm::vec4(particles[b].position,1)).z;
        return za==zb?a<b:za<zb;});
    const auto inverse=glm::inverse(m_view);const auto right=glm::vec3(inverse[0]),up=glm::vec3(inverse[1]);
    const glm::vec2 corners[4]={{-0.5f,-0.5f},{0.5f,-0.5f},{0.5f,0.5f},{-0.5f,0.5f}};
    const unsigned indices[6]={0,1,2,0,2,3};m_particleVertices.clear();m_particleVertices.reserve(particles.size()*6);
    for(size_t i:m_particleOrder){const auto& p=particles[i];for(auto k:indices){auto c=corners[k];m_particleVertices.push_back({p.position+p.size*(right*c.x+up*c.y),c+glm::vec2(0.5f),p.color});}}
    if(m_linearRendering)texture=ColourTexture(texture);
    glUseProgram(m_particleProgram);glUniform1i(glGetUniformLocation(m_particleProgram,"imageLinear"),texture.IsValid()&&texture.id<m_textures.size()&&(m_textures[texture.id].srgb||m_textures[texture.id].sceneLinear));glUniform1i(glGetUniformLocation(m_particleProgram,"modern"),m_linearRendering);glBindSampler(0,0);glUniformMatrix4fv(glGetUniformLocation(m_particleProgram,"vp"),1,GL_FALSE,glm::value_ptr(m_projection*m_view));
    if(m_activeTarget.IsValid()&&texture.id==RenderTargetTexture(m_activeTarget).id){texture={};++m_stats.feedbackFallbacks;}
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,ResolveTexture(texture));glUniform1i(glGetUniformLocation(m_particleProgram,"image"),0);
    glBindVertexArray(m_particleVao);glBindBuffer(GL_ARRAY_BUFFER,m_particleVbo);
    glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(m_particleVertices.size()*sizeof(ParticleVertex)),m_particleVertices.data(),GL_STREAM_DRAW);
    // Restore exactly the state touched by this presentation pass.
    GLboolean depthMask,cull=glIsEnabled(GL_CULL_FACE),blend=glIsEnabled(GL_BLEND);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
    GLint srcRgb,dstRgb,srcAlpha,dstAlpha;glGetIntegerv(GL_BLEND_SRC_RGB,&srcRgb);glGetIntegerv(GL_BLEND_DST_RGB,&dstRgb);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(m_particleVertices.size()));
    glDepthMask(depthMask);if(cull)glEnable(GL_CULL_FACE);if(!blend)glDisable(GL_BLEND);glBlendFuncSeparate(srcRgb,dstRgb,srcAlpha,dstAlpha);
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);
    ++m_stats.drawCalls;m_stats.triangles+=static_cast<unsigned>(particles.size()*2);m_stats.particlesSubmitted+=static_cast<unsigned>(particles.size());
}

bool Renderer::SelectUIFont(const std::string& path,std::string& error){
    auto selected=path.empty()?m_defaultUIFont:path;if(selected.empty())return false;
    return LoadFont(selected.c_str(),48,error);
}
void Renderer::SetUIClip(glm::vec2 position,glm::vec2 size){
    auto lo=glm::max(glm::vec2(0),position),hi=glm::min(m_uiScreenSize,position+size);
    glEnable(GL_SCISSOR_TEST);glScissor(int(std::floor(lo.x)),int(std::floor(m_uiScreenSize.y-hi.y)),std::max(0,int(std::ceil(hi.x)-std::floor(lo.x))),std::max(0,int(std::ceil(hi.y)-std::floor(lo.y))));
}
void Renderer::ClearUIClip(){glDisable(GL_SCISSOR_TEST);}
void Renderer::DrawUIImage(glm::vec2 position,glm::vec2 size,TextureHandle texture,glm::vec4 tint,bool fit){
    if(!texture.IsValid())return;
    if(fit){const auto* image=texture.id<m_textures.size()&&m_textures[texture.id].alive?&m_textures[texture.id]:nullptr;
        if(image&&image->width>0&&image->height>0){auto scaled=glm::vec2(image->width,image->height);scaled*=std::min(size.x/scaled.x,size.y/scaled.y);position+=(size-scaled)*.5f;size=scaled;}}
    glUniform2f(m_uiUPosition,position.x,position.y);glUniform2f(m_uiUSize,size.x,size.y);
    glUniform4f(m_uiUColor,tint.r,tint.g,tint.b,tint.a);glUniform2f(m_uiUUVOffset,0,0);glUniform2f(m_uiUUVScale,1,1);
    glBindTexture(GL_TEXTURE_2D,ResolveTexture(texture));glDrawArrays(GL_TRIANGLES,0,6);++m_uiDrawCalls;
}

void Renderer::DrawTransientSurface(const MeshData& data,const glm::vec3& tint,float alpha){
 JUDAS_PROFILE_SCOPE("Water surface upload and submission");
 if(m_shadowPassActive||data.vertices.empty())return;
 if(!m_transientSurface.IsValid())m_transientSurface=CreateMesh(data);else UpdateMeshVertices(m_transientSurface,data.vertices);
 GLboolean cull=glIsEnabled(GL_CULL_FACE);glDisable(GL_CULL_FACE);
 DrawMesh(m_transientSurface,{0,0,0},{1,0,0,0},{1,1,1},{},tint,alpha);
 if(cull)glEnable(GL_CULL_FACE);
}

unsigned Renderer::BeginProfilePass(const char* name,std::uint64_t camera){
    auto& p=PerformanceProfiler::Get();if(!p.Active())return 0;
    if(!glQueryCounter||!glGetQueryObjectui64v){p.GPUAvailability(false,"GL timestamp entry points unavailable");return 0;}
    p.GPUAvailability(true,"Asynchronous GL 3.3 timestamp pairs");
    if(!m_profileQueriesReady){for(auto& q:m_profileQueries){glGenQueries(1,&q.begin);glGenQueries(1,&q.end);}m_profileQueriesReady=true;}
    for(unsigned i=0;i<m_profileQueries.size();++i){auto& q=m_profileQueries[i];if(q.pending)continue;
        auto record=p.GPUPending(name,camera);if(!record)return 0;
        q.frame=p.FrameId();q.record=record;q.pending=true;q.ended=false;
        glQueryCounter(q.begin,GL_TIMESTAMP);return i+1;
    }p.DropGPU();return 0;
}
void Renderer::EndProfilePass(unsigned token){if(!token||token>m_profileQueries.size())return;auto& q=m_profileQueries[token-1];if(!q.pending||q.ended)return;glQueryCounter(q.end,GL_TIMESTAMP);q.ended=true;}
void Renderer::PollProfileGPU(){
    if(!m_profileQueriesReady)return;
    auto& p=PerformanceProfiler::Get();
    for(auto& q:m_profileQueries){if(!q.pending||!q.ended||q.frame==p.FrameId())continue;
        GLint available=0;glGetQueryObjectiv(q.end,GL_QUERY_RESULT_AVAILABLE,&available);if(!available)continue;
        GLuint64 a=0,b=0;glGetQueryObjectui64v(q.begin,GL_QUERY_RESULT,&a);glGetQueryObjectui64v(q.end,GL_QUERY_RESULT,&b);
        p.GPUComplete(q.frame,q.record,double(b>=a?b-a:0)/1e6);q.pending=q.ended=false;
    }
}

#include "RendererMaterials.inl"

#include "RendererText.inl"
