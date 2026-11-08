#pragma once
// Shared terrain-only triplanar material code; ordinary mesh materials keep
// their existing pipeline. No geometry or physical state is owned here.
#define JUDAS_TERRAIN_MATERIAL_GLSL R"(
uniform vec3 uTerrainColours[4];
uniform vec4 uTerrainWeightRect;
uniform mat4 uTerrainFromWorld;
uniform vec4 uTerrainTileMetres,uTerrainRoughness;
uniform sampler2D uTerrainDetailNormal;
uniform bool uTerrainHasNormal;
uniform float uTerrainNormalMetres,uTerrainNormalStrength;
vec3 terrainBlend(vec3 n) {
    vec3 blend=pow(abs(n),vec3(4.0));
    return blend/max(dot(blend,vec3(1.0)),0.0001);
}
vec4 terrainWeights(vec3 localNormal) {
    vec2 weightSize=vec2(textureSize(uTexture,0));
    vec4 w=texture(uTexture,(clamp((vUV1-uTerrainWeightRect.xy)/uTerrainWeightRect.zw,vec2(0),vec2(1))*(weightSize-vec2(1))+.5)/weightSize);
    w/=max(dot(w,vec4(1.0)),0.0001);
    float exposed=max(smoothstep(.12,.6,1.0-abs(localNormal.y)),1.0-smoothstep(-.1,.25,localNormal.y));
    return mix(w,vec4(0.0,.12,.88,0.0),exposed);
}
vec3 terrainSample(sampler2D layer,vec3 p,vec3 blend,float metres) {
    p/=metres;
    return texture(layer,p.yz).rgb*blend.x+texture(layer,p.xz).rgb*blend.y+texture(layer,p.xy).rgb*blend.z;
}
vec3 terrainBase(vec3 n,vec4 w) {
    vec3 blend=terrainBlend(normalize(mat3(uTerrainFromWorld)*n));
    // Source-local metres preserve texture phase through world-origin shifts.
    vec3 p=(uTerrainFromWorld*vec4(vWorldPos,1.0)).xyz;
    return terrainSample(uMR,p,blend,uTerrainTileMetres.x)*uTerrainColours[0]*w.x+
           terrainSample(uNormal,p,blend,uTerrainTileMetres.y)*uTerrainColours[1]*w.y+
           terrainSample(uOcclusion,p,blend,uTerrainTileMetres.z)*uTerrainColours[2]*w.z+
           terrainSample(uEmissive,p,blend,uTerrainTileMetres.w)*uTerrainColours[3]*w.w;
}
vec3 terrainNormal(vec3 worldNormal) {
    if(!uTerrainHasNormal || uTerrainNormalStrength==0.0)return worldNormal;
    vec3 n=normalize(mat3(uTerrainFromWorld)*worldNormal),blend=terrainBlend(n);
    vec3 p=(uTerrainFromWorld*vec4(vWorldPos,1.0)).xyz/uTerrainNormalMetres;
    vec3 side=mix(vec3(-1.0),vec3(1.0),greaterThanEqual(n,vec3(0.0)));
    // Right-handed tangent frames on both sides of each projection. Flat
    // normal maps add exactly zero; blending never replaces the geometric N.
    vec2 x=texture(uTerrainDetailNormal,vec2(p.y,p.z*side.x)).rg*2.0-1.0;
    vec2 y=texture(uTerrainDetailNormal,vec2(p.x,-p.z*side.y)).rg*2.0-1.0;
    vec2 z=texture(uTerrainDetailNormal,vec2(p.x,p.y*side.z)).rg*2.0-1.0;
    // RG8's neutral byte is128. Remove its half-quantum bias explicitly.
    x-=vec2(1.0/255.0);y-=vec2(1.0/255.0);z-=vec2(1.0/255.0);
    vec3 detail=blend.x*vec3(0.0,x.x,side.x*x.y)+
                blend.y*vec3(y.x,0.0,-side.y*y.y)+
                blend.z*vec3(z.x,side.z*z.y,0.0);
    detail-=n*dot(n,detail);
    n=normalize(n+uTerrainNormalStrength*detail);
    return normalize(transpose(mat3(uTerrainFromWorld))*n);
}
)"
