#pragma once
#include "Light.h"
#include <vector>
// CPU-built screen tiles, consumed by both GL3.3 and GLES3 through one RGBA32F
// texture. All indices are below 2^24 and therefore exactly represented floats.
struct RenderLightGrid {
 static constexpr unsigned TileSize=64,MaxLights=1024,MaxPerTile=128;
 unsigned columns=0,rows=0,visible=0,rejected=0,overflow=0,maxPerTile=0;
 std::vector<glm::vec4> texels;
 void Build(const std::vector<DynamicLight>&,const glm::mat4& view,const glm::mat4& projection,unsigned width,unsigned height);
};
// Returns a copy with shadow slots assigned by per-view relevance. Point lights
// remain unshadowed; budget exhaustion is a quality choice, not missing light.
std::vector<DynamicLight> SelectShadowLights(const std::vector<DynamicLight>&,const glm::mat4&,const glm::mat4&,unsigned budget);
