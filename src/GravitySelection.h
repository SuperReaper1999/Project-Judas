#pragma once
#include <cstdint>
#include <string>
#include <glm/glm.hpp>
#include <cmath>

// Absent on an entity means ordinary spatial GravityField routing. Selection
// stores durable values/identities, never a pointer into a streamed world.
struct GravitySelection {
    enum class Mode { Uniform, Field };
    Mode mode=Mode::Uniform;
    std::uint64_t source=0;
    glm::vec3 acceleration{0};
};
inline bool ValidGravitySelection(const GravitySelection& s,std::string& error){
    if(s.mode==GravitySelection::Mode::Field){
        if(s.source&&s.acceleration==glm::vec3(0))return true;
    }else if(s.mode==GravitySelection::Mode::Uniform){
        const double length=glm::length(glm::dvec3(s.acceleration));
        if(!s.source&&std::isfinite(length)&&length<=10000)return true;
    }
    error="gravity selection requires a source identity or finite uniform acceleration of magnitude <=10000 m/s^2";
    return false;
}
