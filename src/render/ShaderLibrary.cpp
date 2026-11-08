#include "ShaderLibrary.h"
#include "TerrainShader.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

namespace JudasRender::Shaders {

// Canonical portable shaders. Extracted byte-for-byte from Renderer.cpp;
// profile preparation is centralized below, not repeated by individual passes.
const char *const kVertexShaderSource = R"(#version 330 core
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
uniform bool uInstanced;
layout(location=9) in mat4 aInstanceModel;
layout(location=13) in vec4 aInstanceTint;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
uniform vec4 uColor;
uniform bool uBasicWater;
uniform bool uWaterSurfaceMesh;
uniform vec4 uBasicWaterWave; // amplitude, wavelength, speed, time
uniform vec2 uBasicWaterSize;
out vec4 vTint;

out vec3 vWorldNormal;
out vec4 vWorldTangent;
out vec3 vWorldPos;
out vec2 vUV;
out vec2 vUV1;



)" JUDAS_TERRAIN_VERTEX_GLSL R"(

void main() {
    mat4 model=uInstanced?aInstanceModel:uModel;
    mat3 normalMatrix=uInstanced?transpose(inverse(mat3(model))):uNormalMatrix;
    vTint=uInstanced?aInstanceTint:uColor;
    // uNormalMatrix = transpose(inverse(mat3(model))), computed on the CPU
    // once per draw call (see DrawMesh) — the standard correction so
    // normals stay perpendicular to their surface under non-uniform scale
    // (DrawBox's halfExtents are rarely a uniform scale), not just rotation.
    mat4 skin=mat4(1.0);
    if(uSkinned)skin=bone(aJoints.x)*aWeights.x+bone(aJoints.y)*aWeights.y+bone(aJoints.z)*aWeights.z+bone(aJoints.w)*aWeights.w+bone(aJoints1.x)*aWeights1.x+bone(aJoints1.y)*aWeights1.y+bone(aJoints1.z)*aWeights1.z+bone(aJoints1.w)*aWeights1.w;
    vec4 localPosition=skin*vec4(aLocalPos,1.0);
    mat3 linear=mat3(skin);
    vec3 normal=abs(determinant(linear))>1e-8?transpose(inverse(linear))*aLocalNormal:linear*aLocalNormal;
    if(uBasicWater&&!uWaterSurfaceMesh){
        vec2 p=aLocalPos.xz*uBasicWaterSize;
        float k=6.2831853/uBasicWaterWave.y;
        float a=k*(p.x*.8+p.y*.6)+uBasicWaterWave.z*uBasicWaterWave.w;
        float b=k*1.71*(p.x*-.6+p.y*.8)-uBasicWaterWave.z*uBasicWaterWave.w*1.37;
        localPosition.y=uBasicWaterWave.x*(.7*sin(a)+.3*sin(b));
        vec2 d=uBasicWaterWave.x*k*(.7*cos(a)*vec2(.8,.6)+.513*cos(b)*vec2(-.6,.8));
        normal=vec3(-d.x*uBasicWaterSize.x,1,-d.y*uBasicWaterSize.y);
    }
    if(uTerrainGrid){localPosition.y=terrainGridHeight(aUV);normal=terrainGridNormal(aUV);}
    vWorldNormal = normalMatrix * normal;
    vec3 tangent=mat3(model)*linear*aTangent.xyz;
    vWorldTangent=vec4(tangent,aTangent.w*(determinant(mat3(model)*linear)<0.0?-1.0:1.0));
    // Milestone 14: the fragment's own world-space position, needed so the
    // fragment shader can compute a per-fragment vector TO each dynamic
    // point/spot light (distance-based attenuation, cone angle) — the
    // Milestone 9 directional light never needed this, since a directional
    // light's contribution doesn't depend on fragment position at all.
    vWorldPos = vec3(model * localPosition);
    vUV = aUV;vUV1=uTerrainGrid?terrainSamplePoint(aUV)/(uTerrainDimensions-vec2(1.0)):aUV1;
    gl_Position = uProjection * uView * model * localPosition;
}
)";

const char *const kFragmentShaderSource = R"(#version 330 core
)" JUDAS_TERRAIN_FRAGMENT_GLSL R"(
in vec3 vWorldNormal;
in vec4 vWorldTangent;
in vec3 vWorldPos;
in vec2 vUV;
in vec2 vUV1;



out vec4 FragColor;

uniform sampler2D uTexture;
in vec4 vTint;
uniform bool uBasicWater;
uniform vec3 uBasicWaterUp;
uniform bool uWaterEnabled;
uniform sampler2D uWaterPaths;
uniform mat4 uWaterViewProjection;
uniform mat4 uWaterInverseViewProjection;
uniform vec3 uLightDirection;  // world-space, normalized, points FROM the surface TOWARD the light
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;

// Prescribed weather shares its cloud map and wind phase with the sky pass.
// This is an inexpensive projected cloud shadow and artistic surface response,
// not a replacement for authored physical materials or accumulated snow mass.
uniform bool uWeatherEnabled;
uniform sampler3D uWeatherNoise;
uniform vec3 uWeatherEye,uWeatherUp,uWeatherPhase;
uniform vec4 uWeatherAmounts; // coverage,density,rain,snow
uniform vec4 uWeatherLayer; // base,thickness,eye altitude,radial radius
uniform float uWeatherFlash;
float spotWeight(float outerCos,float innerCos,float angle){return innerCos<=outerCos+1e-7?step(outerCos,angle):smoothstep(outerCos,innerCos,angle);}
float weatherCloud(vec3 p){return texture(uWeatherNoise,p/262144.0).r*.65+texture(uWeatherNoise,p/65536.0).r*.35;}

// M92: one depth array; world-position projection selects the assigned layer.
uniform sampler2DArray uShadowMaps;
uniform mat4 uLightSpaceMatrix[9];
uniform sampler2D uLightGrid;
uniform bool uHasDynamicLights;
uniform vec4 uLightViewport;
vec4 lightTexel(int i){return texelFetch(uLightGrid,ivec2(i%1024,i/1024),0);}
ivec2 lightTile(){vec4 meta=lightTexel(0);ivec2 n=ivec2(meta.xy);ivec2 tile=clamp(ivec2((gl_FragCoord.xy-uLightViewport.xy)/uLightViewport.zw*meta.xy),ivec2(0),n-1);return ivec2(tile.y*n.x+tile.x,int(meta.z));}
// Capture geometric derivatives in uniform flow before material/light branches.
vec3 shadowWorldDx, shadowWorldDy;

// A sloped receiver has a different depth at each shadow texel. Correct that
// depth across the PCF footprint instead of hiding acne with more bias.
float ComputeShadowFactor(int slot, vec3 worldPosition, float ndotl) {
    if(slot<0||slot>=9)return 1.0;
    vec4 clip=uLightSpaceMatrix[slot]*vec4(worldPosition,1.0);
    if(clip.w<=0.0)return 1.0;
    vec3 ndc=clip.xyz/clip.w;
    vec3 projected=ndc*0.5+0.5;
    if(any(lessThan(projected,vec3(0.0)))||any(greaterThan(projected,vec3(1.0))))return 1.0;
    // Project geometric world-plane directions, not derivatives of a selected
    // light matrix: adjacent tiled-light fragments may select different lights.
    vec4 clipDx=uLightSpaceMatrix[slot]*vec4(shadowWorldDx,0.0);
    vec4 clipDy=uLightSpaceMatrix[slot]*vec4(shadowWorldDy,0.0);
    vec3 dx=0.5*(clipDx.xyz-ndc*clipDx.w)/clip.w;
    vec3 dy=0.5*(clipDy.xyz-ndc*clipDy.w)/clip.w;
    float determinant=dx.x*dy.y-dx.y*dy.x;
    vec2 slope=vec2(0.0);
    if(determinant!=0.0){
        slope=vec2(dx.z*dy.y-dy.z*dx.y,dx.x*dy.z-dy.x*dx.z)/determinant;
    }
    if(any(isnan(slope))||any(isinf(slope)))slope=vec2(0.0);
    ivec2 size=textureSize(uShadowMaps,0).xy;
    // Interpolate comparison results, never raw depths. A nearest 3x3 kernel
    // changes a whole row/column at a texel boundary and visibly snaps. Blend
    // its four adjacent placements continuously using their shared 4x4 taps.
    vec2 texel=projected.xy*vec2(size)-0.5;
    ivec2 center=ivec2(floor(texel));
    vec2 phase=fract(texel);
    float bias=max(0.006*(1.0-ndotl),0.0015); // Existing numerical bias, unchanged.
    float lit=0.0;
    for(int x=-1;x<=2;++x){
        float wx=x==-1?1.0-phase.x:(x==2?phase.x:1.0);
        for(int y=-1;y<=2;++y){
            float wy=y==-1?1.0-phase.y:(y==2?phase.y:1.0);
            ivec2 pixel=clamp(center+ivec2(x,y),ivec2(0),size-1);
            vec2 offset=(vec2(pixel)+0.5)/vec2(size)-projected.xy;
            float receiver=projected.z+dot(slope,offset);
            float stored=texelFetch(uShadowMaps,ivec3(pixel,slot),0).r;
            lit+=wx*wy*(receiver-bias<=stored?1.0:0.0);
        }
    }
    return lit/9.0;
}


uniform int uMaterialModel,uAlphaMode;
uniform bool uTerrain;
uniform sampler2D uMR,uNormal,uOcclusion,uEmissive,uEnvDiffuse,uEnvSpecular,uBRDF;
uniform vec3 uTerrainColours[4];
uniform mat4 uTerrainFromWorld;
vec3 terrainSample(sampler2D layer,vec3 p,vec3 blend) {
    return texture(layer,p.yz*.25).rgb*blend.x+texture(layer,p.xz*.25).rgb*blend.y+texture(layer,p.xy*.25).rgb*blend.z;
}
vec3 terrainBase(vec3 n) {
    vec3 localNormal=normalize(mat3(uTerrainFromWorld)*n);
    vec3 blend=pow(abs(localNormal),vec3(4.0));blend/=max(dot(blend,vec3(1.0)),0.0001);
    vec2 weightSize=vec2(textureSize(uTexture,0));
    vec4 w=texture(uTexture,(clamp(vUV1,vec2(0),vec2(1))*(weightSize-vec2(1))+.5)/weightSize);w/=max(dot(w,vec4(1.0)),0.0001);
    // Texture phase lives in source-local coordinates, so shifting the world
    // origin never makes landscape texture detail swim.
    vec3 p=(uTerrainFromWorld*vec4(vWorldPos,1.0)).xyz;
    float exposed=max(smoothstep(.12,.6,1.0-abs(localNormal.y)),1.0-smoothstep(-.1,.25,localNormal.y));
    w=mix(w,vec4(0.0,.12,.88,0.0),exposed);
    return terrainSample(uMR,p,blend)*uTerrainColours[0]*w.x+
           terrainSample(uNormal,p,blend)*uTerrainColours[1]*w.y+
           terrainSample(uOcclusion,p,blend)*uTerrainColours[2]*w.z+
           terrainSample(uEmissive,p,blend)*uTerrainColours[3]*w.w;
}
uniform float uAlphaCutoff,uMetallic,uRoughness,uNormalStrength,uOcclusionStrength,uEmissionIntensity;
uniform bool uFlipV,uModern,uEnvironmentEnabled,uBaseLinear;
uniform vec4 uBaseFactor,uUVTransform;
uniform int uMapUVSet[5];uniform vec4 uMapUVTransform[5];uniform float uMapUVRotation[5];
vec2 mapUV(int i){vec2 p=(uMapUVSet[i]==1?vUV1:vUV)*uMapUVTransform[i].xy;float c=cos(uMapUVRotation[i]),s=sin(uMapUVRotation[i]);p=mat2(c,s,-s,c)*p+uMapUVTransform[i].zw;p=p*uUVTransform.xy+uUVTransform.zw;if(uFlipV)p.y=1.0-p.y;return p;}
uniform vec3 uEmission,uCameraPosition;

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
 vec3 result=BRDF(base,metal,rough,n,v,uLightDirection)*uLightColor*ComputeShadowFactor(0,vWorldPos,max(dot(n,uLightDirection),0.0));
 ivec2 tileInfo=uHasDynamicLights?lightTile():ivec2(0);vec4 tileHeader=uHasDynamicLights?lightTexel(1+tileInfo.x):vec4(0);
 for(int item=0;item<int(tileHeader.y);++item){int record=tileInfo.y+int(lightTexel(int(tileHeader.x)+item).x)*4;
 vec4 lp=lightTexel(record),lc=lightTexel(record+1),ld=lightTexel(record+2),ls=lightTexel(record+3);
 vec3 lightPosition=lp.xyz,lightColor=lc.xyz,lightDirection=ld.xyz;
 float lightRange=lp.w,innerCos=ld.w,outerCos=ls.x;int isSpot=int(lc.w),shadowSlot=int(ls.y);vec3 to=lightPosition-vWorldPos;float distance=length(to);vec3 l=distance>0.00001?to/distance:n;float f=clamp(distance/max(lightRange,0.0001),0.0,1.0),w=clamp(1.0-f*f*f*f,0.0,1.0);float attenuation=w*w/(distance*distance+1.0);float spot=isSpot!=0?spotWeight(outerCos,innerCos,dot(-l,lightDirection)):1.0;float shadow=1.0;shadow=ComputeShadowFactor(shadowSlot,vWorldPos,max(dot(n,l),0.0));result+=BRDF(base,metal,rough,n,v,l)*lightColor*attenuation*spot*shadow;}
 float ao=(uHasOcclusion&&!uTerrain)?mix(1.0,texture(uOcclusion,mapUV(3)).r,uOcclusionStrength):1.0;
 vec3 f0=mix(vec3(0.04),base,metal);
 if(uEnvironmentEnabled){float nv=max(dot(n,v),0.0);vec3 f=f0+(max(vec3(1.0-rough),f0)-f0)*pow(1.0-nv,5.0);vec3 diffuse=texture(uEnvDiffuse,envUV(uEnvironmentInverse*n)).rgb*base*(vec3(1.0)-f)*(1.0-metal);vec3 reflection=reflect(-v,n);vec3 spec=textureLod(uEnvSpecular,envUV(uEnvironmentInverse*reflection),rough*(uEnvLevels-1.0)).rgb;vec2 brdf=texture(uBRDF,vec2(nv,rough)).rg;result+=(diffuse+spec*(f0*brdf.x+brdf.y))*uEnvironmentIntensity*ao;
 }else result+=uAmbientColor*base*(1.0-metal)*ao;
 return result;
}

void main() {
    shadowWorldDx=dFdx(vWorldPos);
    shadowWorldDy=dFdy(vWorldPos);
    vec2 materialUV=mapUV(0);
    vec3 normal = normalize(vWorldNormal);if(uMaterialModel!=0&&!gl_FrontFacing)normal=-normal;
    if(uMaterialModel!=0&&uHasNormal&&!uTerrain){vec3 t=vWorldTangent.xyz-normal*dot(normal,vWorldTangent.xyz);if(dot(t,t)>0.000001){t=normalize(t);vec3 b=cross(normal,t)*vWorldTangent.w;t*=uUVTransform.x<0.0?-1.0:1.0;b*=uUVTransform.y<0.0?-1.0:1.0;vec3 map=texture(uNormal,mapUV(2)).xyz*2.0-1.0;map.xy*=uNormalStrength;normal=normalize(mat3(t,b,normal)*map);}}


    float diffuseFactor = max(dot(normal, uLightDirection), 0.0);
    vec3 lighting = vec3(0.0);
    // Material model is uniform for the draw. PBR evaluates its own BRDF/light
    // loop below; Unlit needs neither light loop. Geometric derivatives above
    // remain outside all branches, and weather still uses diffuseFactor.
    if (uMaterialModel == 0) {
    float dirShadow = ComputeShadowFactor(0, vWorldPos, diffuseFactor);
    lighting = uAmbientColor + uLightColor * diffuseFactor * dirShadow;

    ivec2 tileInfo=uHasDynamicLights?lightTile():ivec2(0);vec4 tileHeader=uHasDynamicLights?lightTexel(1+tileInfo.x):vec4(0);
 for(int item=0;item<int(tileHeader.y);++item){int record=tileInfo.y+int(lightTexel(int(tileHeader.x)+item).x)*4;
 vec4 lp=lightTexel(record),lc=lightTexel(record+1),ld=lightTexel(record+2),ls=lightTexel(record+3);
 vec3 lightPosition=lp.xyz,lightColor=lc.xyz,lightDirection=ld.xyz;
 float lightRange=lp.w,innerCos=ld.w,outerCos=ls.x;int isSpot=int(lc.w),shadowSlot=int(ls.y);
        vec3 toLight = lightPosition - vWorldPos;
        float distance = length(toLight);
        vec3 lightDir = distance > 1.0e-5 ? toLight / distance : vec3(0.0, 1.0, 0.0);

        // Smooth-windowed inverse-square attenuation (see Renderer.cpp,
        // SetDynamicLights, for the full derivation/citation) — genuinely
        // inverse-square close to the light, smoothly reaches exactly zero
        // at lightRange instead of a hard cliff or a never-zero
        // tail, and the "+1.0" keeps it finite as distance approaches 0.
        float rangeFraction = clamp(distance / max(lightRange, 1.0e-4), 0.0, 1.0);
        float windowed = 1.0 - rangeFraction * rangeFraction * rangeFraction * rangeFraction;
        windowed = clamp(windowed, 0.0, 1.0);
        float attenuation = (windowed * windowed) / (distance * distance + 1.0);

        float spotFactor = 1.0;
        if (isSpot != 0) {
            float cosAngle = dot(-lightDir, lightDirection);
            spotFactor = spotWeight(outerCos, innerCos, cosAngle);
        }

        float lightDiffuse = max(dot(normal, lightDir), 0.0);

        float shadow = 1.0;
        int shadowIndex = shadowSlot;
        shadow=ComputeShadowFactor(shadowIndex,vWorldPos,lightDiffuse);

        lighting += lightColor * lightDiffuse * attenuation * spotFactor * shadow;
    }
    }

    vec4 texColor = uTerrain ? vec4(1.0) : texture(uTexture,materialUV);
    vec4 base=vec4((uMaterialModel==0||uBaseLinear)?texColor.rgb:decodeSRGB(texColor.rgb),texColor.a)*uBaseFactor*vTint;
    if(uAlphaMode==1&&base.a<uAlphaCutoff)discard;
    if(uTerrain)base.rgb*=terrainBase(normal);
    if(uMaterialModel==0){FragColor=vec4(lighting*base.rgb,base.a);if(uModern)FragColor.rgb=decodeSRGB(FragColor.rgb);}
    else {vec3 emission=uEmission*(uHasEmissive?(uEmissiveLinear?texture(uEmissive,mapUV(4)).rgb:decodeSRGB(texture(uEmissive,mapUV(4)).rgb)):vec3(1.0))*uEmissionIntensity;float metal=uMetallic,rough=uRoughness;if(uHasMR&&!uTerrain){vec4 mr=texture(uMR,mapUV(1));rough*=mr.g;metal*=mr.b;}vec3 v=normalize(uCameraPosition-vWorldPos);FragColor=vec4((uMaterialModel==2?base.rgb:pbrLight(base.rgb,metal,clamp(rough,0.05,1.0),normal,v,materialUV))+emission,uAlphaMode==0?1.0:base.a);}

    if(uBasicWater){
        vec3 n=normalize(gl_FrontFacing?normal:-normal);
        vec3 eye=normalize(uCameraPosition-vWorldPos);
        float fresnel=.035+.965*pow(1.0-clamp(abs(dot(n,eye)),0.0,1.0),5.0);
        vec3 sky=mix(vec3(.17,.34,.43),vec3(.57,.74,.83),clamp(dot(n,uBasicWaterUp)*.5+.5,0.0,1.0));
        float glint=pow(max(0.0,dot(reflect(-uLightDirection,n),eye)),160.0);
        vec3 water=base.rgb*(.35+uAmbientColor+.32*uLightColor*max(0.0,dot(n,uLightDirection)));
        FragColor=vec4(mix(water,sky,fresnel*.8)+uLightColor*glint*.85,
                       mix(base.a,1.0,fresnel*.65));
    }
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
    if(uWeatherEnabled){
        vec3 relative=vWorldPos-uWeatherEye;
        float altitude=uWeatherLayer.z+dot(relative,uWeatherUp);
        float sunUp=dot(uLightDirection,uWeatherUp);
        float travel=max(0.0,uWeatherLayer.x-altitude)/max(sunUp,.12);
        float map=weatherCloud(uWeatherPhase+relative+uLightDirection*min(travel,30000.0));
        float shadow=smoothstep(1.0-uWeatherAmounts.x*.82,1.0,map)*min(.72,uWeatherAmounts.y*.65);
        FragColor.rgb*=1.0-shadow;
        float facing=max(0.0,dot(normal,uWeatherUp));
        float wet=uWeatherAmounts.z*(.2+.8*facing);
        FragColor.rgb*=1.0-wet*.28;
        vec3 eyeDirection=normalize(uWeatherEye-vWorldPos);
        FragColor.rgb+=vec3(.08,.1,.12)*wet*pow(max(0.0,dot(reflect(-uLightDirection,normal),eyeDirection)),48.0);
        float snow=uWeatherAmounts.w*smoothstep(.3,.8,facing);
        FragColor.rgb=mix(FragColor.rgb,vec3(.78,.84,.9)*(.4+.6*diffuseFactor),snow*.88);
        FragColor.rgb+=vec3(.38,.43,.52)*uWeatherFlash*(.2+.8*facing);
    }
    // All implicit texture derivatives are evaluated before nonuniform coverage.
    terrainCoverage();
    if(uModern||uMaterialModel!=0)FragColor.rgb=clamp(FragColor.rgb,vec3(0.0),vec3(60000.0));
    if(!uModern&&uMaterialModel!=0){vec3 c=max(FragColor.rgb,vec3(0.0));if(uMaterialModel==1)c/=1.0+dot(c,vec3(.2126,.7152,.0722));FragColor.rgb=mix(c*12.92,1.055*pow(c,vec3(1.0/2.4))-0.055,step(vec3(.0031308),c));}
}
)";

const char *const kShadowVertexShaderSource = R"(#version 330 core
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
uniform bool uInstanced;
layout(location=9) in mat4 aInstanceModel;
layout(location=13) in vec4 aInstanceTint;
uniform mat4 uLightViewProj;
)" JUDAS_TERRAIN_VERTEX_GLSL R"(

void main() {
    mat4 skin=mat4(1.0);
    if(uSkinned)skin=bone(aJoints.x)*aWeights.x+bone(aJoints.y)*aWeights.y+bone(aJoints.z)*aWeights.z+bone(aJoints.w)*aWeights.w+bone(aJoints1.x)*aWeights1.x+bone(aJoints1.y)*aWeights1.y+bone(aJoints1.z)*aWeights1.z+bone(aJoints1.w)*aWeights1.w;
    mat4 model=uInstanced?aInstanceModel:uModel;shadowUV=aUV;shadowUV1=aUV1;vec4 local=skin*vec4(aLocalPos,1.0);if(uTerrainGrid)local.y=terrainGridHeight(aUV);gl_Position = uLightViewProj * model * local;
}
)";

const char *const kShadowFragmentShaderSource = R"(#version 330 core
)" JUDAS_TERRAIN_FRAGMENT_GLSL R"(
in vec2 shadowUV;in vec2 shadowUV1;uniform sampler2D uTexture;uniform int uAlphaMode;uniform float uAlphaCutoff,uAlphaFactor;uniform bool uFlipV;uniform vec4 uUVTransform;uniform int uMapUVSet[5];uniform vec4 uMapUVTransform[5];uniform float uMapUVRotation[5];
void main() {
    vec2 uv=(uMapUVSet[0]==1?shadowUV1:shadowUV)*uMapUVTransform[0].xy;float c=cos(uMapUVRotation[0]),s=sin(uMapUVRotation[0]);uv=mat2(c,s,-s,c)*uv+uMapUVTransform[0].zw;uv=uv*uUVTransform.xy+uUVTransform.zw;if(uFlipV)uv.y=1.0-uv.y;
    if(uAlphaMode==1&&texture(uTexture,uv).a*uAlphaFactor<uAlphaCutoff)discard;
    terrainCoverage();
    // This FBO has no color attachment (see
    // Renderer::Init's glDrawBuffer(GL_NONE)) — only gl_FragDepth's
    // implicit default (gl_FragCoord.z) is ever written, by the ordinary
    // depth test every draw call already performs.
}
)";

const char *const kUIVertexShaderSource = R"(#version 330 core
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

const char *DisplayTransformSource() {
    return R"(
vec3 JudasDisplayColour(vec3 colour, float exposure) {
    colour = max(colour, vec3(0.0)) * exposure;
    float luminance = dot(colour, vec3(.2126, .7152, .0722));
    colour = clamp(colour / (1.0 + luminance), 0.0, 1.0);
    return mix(colour * 12.92,
               1.055 * pow(colour, vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), colour));
}
)";
}
namespace {
const std::string kUIFragmentShaderText = std::string(R"(#version 330 core
in vec2 vUnit;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec4 uColor;
uniform vec2 uUVOffset;
uniform vec2 uUVScale;
uniform bool uSceneLinear;
uniform float uExposure;
)" ) + DisplayTransformSource() + R"(

void main() {
    vec2 uv = uUVOffset + vUnit * uUVScale;
    vec4 sampled = texture(uTexture, uv);
    if (uSceneLinear) sampled.rgb = JudasDisplayColour(sampled.rgb, uExposure);
    FragColor = sampled * uColor;
}
)";
} // namespace
const char *const kUIFragmentShaderSource = kUIFragmentShaderText.c_str();

const char *const kDebugVertexShaderSource = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
uniform mat4 uViewProjection;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
)";

const char *const kDebugFragmentShaderSource = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

namespace {
constexpr Builtin builtins[] = {Builtin::MeshVertex,     Builtin::MeshFragment, Builtin::ShadowVertex,
                                Builtin::ShadowFragment, Builtin::UIVertex,     Builtin::UIFragment,
                                Builtin::DebugVertex,    Builtin::DebugFragment};
constexpr std::string_view desktop330 = "#version 330 core";
constexpr std::string_view desktop450 = "#version 450 core";
constexpr std::string_view es300 = "#version 300 es";
constexpr const char *esPrelude = "#version 300 es\nprecision highp float;\nprecision highp int;\n"
                                  "precision highp sampler2D;\nprecision highp sampler3D;\n"
                                  "precision highp sampler2DShadow;\nprecision highp sampler2DArray;";

void ReplaceAll(std::string &text, const std::string &from, const std::string &to) {
    std::size_t at = 0;
    while ((at = text.find(from, at)) != std::string::npos) {
        text.replace(at, from.size(), to);
        at += to.size();
    }
}

const char *SourceIdentity(const char *source, const char *explicitIdentity) {
    if (explicitIdentity && *explicitIdentity)
        return explicitIdentity;
    for (const auto builtin : builtins)
        if (source == Source(builtin))
            return Identity(builtin);
    return "inline shader";
}

// A stable diagnostic identity, not a security checksum or a shader-cache key.
// It identifies the exact prepared driver input even for inline/lazy passes.
std::string Fingerprint(std::string_view source) {
    std::uint64_t value = 14695981039346656037ull;
    for (const unsigned char byte : source) {
        value ^= byte;
        value *= 1099511628211ull;
    }
    std::ostringstream result;
    result << std::hex << std::setfill('0') << std::setw(16) << value;
    return result.str();
}
} // namespace

const char *ProfileName(Profile profile) {
    switch (profile) {
    case Profile::Desktop330:
        return "desktop GLSL 330";
    case Profile::Desktop450:
        return "desktop GLSL 450";
    case Profile::ES300:
        return "GLSL ES 300";
    }
    return "unknown profile";
}

const char *Source(Builtin shader) {
    switch (shader) {
    case Builtin::MeshVertex:
        return kVertexShaderSource;
    case Builtin::MeshFragment:
        return kFragmentShaderSource;
    case Builtin::ShadowVertex:
        return kShadowVertexShaderSource;
    case Builtin::ShadowFragment:
        return kShadowFragmentShaderSource;
    case Builtin::UIVertex:
        return kUIVertexShaderSource;
    case Builtin::UIFragment:
        return kUIFragmentShaderSource;
    case Builtin::DebugVertex:
        return kDebugVertexShaderSource;
    case Builtin::DebugFragment:
        return kDebugFragmentShaderSource;
    }
    return nullptr;
}

const char *Identity(Builtin shader) {
    switch (shader) {
    case Builtin::MeshVertex:
        return "judas/mesh.vert";
    case Builtin::MeshFragment:
        return "judas/mesh.frag";
    case Builtin::ShadowVertex:
        return "judas/shadow.vert";
    case Builtin::ShadowFragment:
        return "judas/shadow.frag";
    case Builtin::UIVertex:
        return "judas/ui.vert";
    case Builtin::UIFragment:
        return "judas/ui.frag";
    case Builtin::DebugVertex:
        return "judas/debug.vert";
    case Builtin::DebugFragment:
        return "judas/debug.frag";
    }
    return "unknown built-in shader";
}

bool PrepareSource(std::string_view canonical, Profile profile, std::string &prepared,
                   std::string &error) {
    error.clear();
    if (canonical.empty() || canonical.find('\0') != std::string_view::npos) {
        error = "shader source is empty or contains an embedded NUL";
        return false;
    }
    if (profile != Profile::Desktop330 && profile != Profile::Desktop450 && profile != Profile::ES300) {
        error = "unknown shader profile";
        return false;
    }
    const auto lineEnd = canonical.find('\n');
    const auto version = canonical.substr(0, lineEnd);
    const bool portable = version == desktop330;
    const bool nativeModern = version == desktop450 && profile == Profile::Desktop450;
    const bool nativeES = version == es300 && profile == Profile::ES300;
    if (!portable && !nativeModern && !nativeES) {
        error = "shader must declare portable #version 330 core or the selected native profile; "
                "newer/native sources cannot be silently downgraded";
        return false;
    }
    // Use a private candidate so validation never changes published source.
    std::string candidate(canonical);
    if (portable && profile == Profile::Desktop450) {
        candidate.replace(0, desktop330.size(), desktop450);
    } else if (portable && profile == Profile::ES300) {
        // These are the existing ES adaptations, centralized without changing
        // their prepared GLSL bytes. No runtime backend may rewrite them again.
        candidate.replace(0, desktop330.size(), esPrelude);
        ReplaceAll(candidate, "samplerBuffer uBones", "sampler2D uBones");
        for (int i = 0; i < 4; ++i) {
            const auto component = i ? "b+" + std::to_string(i) : "b";
            ReplaceAll(candidate, "texelFetch(uBones," + component + ")",
                       "texelFetch(uBones,ivec2(" + std::to_string(i) + ",b/4),0)");
        }
    }
    prepared.swap(candidate);
    return true;
}

bool CompileShader(GLenum type, const char *source, GLuint &outShader, Profile profile,
                   const char *identity) {
    const auto *name = SourceIdentity(source, identity);
    std::string prepared, error;
    if (!source || !PrepareSource(source, profile, prepared, error)) {
        std::fprintf(stderr, "Shader preparation error [%s; %s]: %s\n", name, ProfileName(profile),
                     source ? error.c_str() : "null source");
        return false;
    }
    if (!GraphicsContextCurrent()) {
        std::fprintf(stderr, "Shader compile error [%s; %s]: no owning graphics context\n", name,
                     ProfileName(profile));
        return false;
    }
    const GLuint shader = glCreateShader(type);
    if (!shader) {
        std::fprintf(stderr, "Shader compile error [%s; %s]: shader allocation failed\n", name,
                     ProfileName(profile));
        return false;
    }
    const char *driverSource = prepared.c_str();
    glShaderSource(shader, 1, &driverSource, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<std::size_t>(length > 0 ? length : 1));
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Shader compile error [%s; %s; source=%s]: %s\n", name,
                     ProfileName(profile), Fingerprint(prepared).c_str(), log.data());
        glDeleteShader(shader);
        return false;
    }
    outShader = shader;
    return true;
}

bool LinkProgram(GLuint vertex, GLuint fragment, GLuint &outProgram, const char *identity) {
    const auto *name = identity && *identity ? identity : "render program";
    if (!GraphicsContextCurrent() || !vertex || !fragment) {
        std::fprintf(
            stderr, "Program link error [%s]: current context and both shader stages required\n", name);
        return false;
    }
    const GLuint program = glCreateProgram();
    if (!program) {
        std::fprintf(stderr, "Program link error [%s]: program allocation failed\n", name);
        return false;
    }
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<std::size_t>(length > 0 ? length : 1));
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Program link error [%s]: %s\n", name, log.data());
        glDeleteProgram(program);
        return false;
    }
    outProgram = program;
    return true;
}
bool LinkComputeProgram(GLuint compute, GLuint &outProgram, const char *identity) {
    const auto *name = identity && *identity ? identity : "compute program";
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    (void)compute;
    (void)outProgram;
    std::fprintf(stderr, "Program link error [%s]: compute is unavailable on this backend\n", name);
    return false;
#else
    if (!GraphicsContextCurrent() || !compute) {
        std::fprintf(stderr, "Program link error [%s]: current context and compute shader required\n", name);
        return false;
    }
    const GLuint program = glCreateProgram();
    if (!program) {
        std::fprintf(stderr, "Program link error [%s]: program allocation failed\n", name);
        return false;
    }
    glAttachShader(program, compute);
    glLinkProgram(program);
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<std::size_t>(length > 0 ? length : 1));
        glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::fprintf(stderr, "Program link error [%s]: %s\n", name, log.data());
        glDeleteProgram(program);
        return false;
    }
    outProgram = program;
    return true;
#endif
}
} // namespace JudasRender::Shaders
