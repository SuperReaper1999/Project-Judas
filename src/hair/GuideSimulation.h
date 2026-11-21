#pragma once
#include "Groom.h"
#include <array>
#include <cstdint>
#include <functional>
#include <glm/gtc/quaternion.hpp>

struct MeshVertex;
namespace JudasHair {
struct Frame {
    glm::vec3 position{0};
    glm::quat rotation{1,0,0,0};
    glm::vec3 scale{1};
};
struct Guide {
    glm::vec3 root{0}, direction{0,1,0};
    float length = 0;
    std::array<glm::vec3,9> points{}, previous{};
};
struct Fibre {
    glm::vec3 root, direction;
    std::array<unsigned,3> guides{};
    glm::vec3 weights{1,0,0};
    float pigment = 0, lengthScale = 1;
};
struct Guides {
    std::vector<Guide> strands;
    std::vector<Fibre> fibres;
    Frame frame;
};
using SweepTips = std::function<void(glm::vec3 from,glm::vec3& to)>;
void Reset(Guide&, const Groom&, const Frame&);
void Seed(Guides&, const Groom&, const Frame&);
void Step(Guides&, const Groom&, const Frame&, glm::vec3 gravity, glm::vec3 wind,
          float seconds, const SweepTips& = {});
void Shift(Guides&, glm::dvec3 delta);
unsigned VisualStrands(const Groom&,float distance,float quality);
void BuildVertices(const Guides&, const Groom&, const Frame& presented,float alpha,
                   unsigned count,std::vector<MeshVertex>&);
void BuildIndices(const Groom&,unsigned count,std::vector<uint32_t>&);
}
