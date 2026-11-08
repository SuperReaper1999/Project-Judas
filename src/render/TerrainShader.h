#pragma once
// Complementary, stable screen-door coverage for two complete LOD selections.
// The same rule is used in colour and shadow passes; ordinary meshes bypass it.
#define JUDAS_TERRAIN_FRAGMENT_GLSL R"(
uniform bool uTerrainGrid;
uniform vec2 uTerrainCoverage;
void terrainCoverage() {
    if (!uTerrainGrid || (uTerrainCoverage.x<=0.0 && uTerrainCoverage.y>=1.0)) return;
    uvec2 p=uvec2(gl_FragCoord.xy);
    uint h=p.x*1664525u+p.y*1013904223u;
    h^=h>>16u;h*=2246822519u;h^=h>>13u;
    float sampleValue=(float(h&1023u)+0.5)/1024.0;
    if(sampleValue<uTerrainCoverage.x||sampleValue>=uTerrainCoverage.y)discard;
}
)"

// Shared portable vertex deformation for colour and shadow programs. Literal
// concatenation keeps their canonical source and height/seam rules identical.
#define JUDAS_TERRAIN_VERTEX_GLSL R"(
uniform bool uTerrainGrid;
uniform sampler2D uTerrainHeights;
uniform vec4 uTerrainSamples; // first X/Z sample, stride, cells per patch
uniform vec4 uTerrainEdges;
uniform vec2 uTerrainDimensions;
uniform float uTerrainSpacing;
float terrainRaw(ivec2 p) {
    p=clamp(p,ivec2(0),ivec2(uTerrainDimensions)-ivec2(1));
    uvec4 b=uvec4(round(texelFetch(uTerrainHeights,p,0)*255.0));
    return uintBitsToFloat(b.x|(b.y<<8u)|(b.z<<16u)|(b.w<<24u));
}
float terrainLinear(vec2 p,float stepSize) {
    vec2 a=floor(p/stepSize)*stepSize,t=(p-a)/stepSize;
    ivec2 q=ivec2(a);int s=int(stepSize);
    float h00=terrainRaw(q),h10=terrainRaw(q+ivec2(s,0)),h01=terrainRaw(q+ivec2(0,s)),h11=terrainRaw(q+ivec2(s));
    return t.x+t.y<=1.0 ? h00+(h10-h00)*t.x+(h01-h00)*t.y : h11+(h01-h11)*(1.0-t.x)+(h10-h11)*(1.0-t.y);
}
vec2 terrainSamplePoint(vec2 uv){return uTerrainSamples.xy+uv*uTerrainSamples.z*uTerrainSamples.w;}
float terrainGridHeight(vec2 uv) {
    vec2 p=terrainSamplePoint(uv);float h=terrainRaw(ivec2(round(p)));
    if(uv.x<0.00001)h=terrainLinear(p,uTerrainEdges.x);
    if(uv.x>0.99999)h=terrainLinear(p,uTerrainEdges.y);
    if(uv.y<0.00001)h=terrainLinear(p,uTerrainEdges.z);
    if(uv.y>0.99999)h=terrainLinear(p,uTerrainEdges.w);
    return h;
}
vec3 terrainGridNormal(vec2 uv) {
    ivec2 p=ivec2(round(terrainSamplePoint(uv)));
    float dx=(terrainRaw(p+ivec2(1,0))-terrainRaw(p-ivec2(1,0)))/(2.0*uTerrainSpacing);
    float dz=(terrainRaw(p+ivec2(0,1))-terrainRaw(p-ivec2(0,1)))/(2.0*uTerrainSpacing);
    float width=uTerrainSamples.z*uTerrainSamples.w*uTerrainSpacing;
    return vec3(-dx*width,1.0,-dz*width);
}
)"
