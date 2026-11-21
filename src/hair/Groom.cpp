#include "Groom.h"
#include "AuthoringJSON.h"
#include <cmath>
#include <stdexcept>

namespace JudasHair {
namespace {
using Json = AuthoringJSON::J;
Json WriteVec(glm::vec3 v) { return Json::array({v.x,v.y,v.z}); }
glm::vec3 ReadVec(const Json& value) {
    if (!value.is_array() || value.size()!=3) throw std::runtime_error("expected three coordinates");
    return {value[0].get<float>(),value[1].get<float>(),value[2].get<float>()};
}
bool Range(float v,float lo,float hi) { return std::isfinite(v) && v>=lo && v<=hi; }
bool Range(glm::vec3 v,float lo,float hi) {
    return Range(v.x,lo,hi)&&Range(v.y,lo,hi)&&Range(v.z,lo,hi);
}
}
Groom MakePreset(Preset preset) {
    Groom g;
    if (preset==Preset::Coat) return g;
    g.simulated=true;
    g.segments=6;
    g.strands=96;
    g.width=.012f;
    g.coverageDegrees=92;
    g.undercoat=0;
    g.wave=.18f;
    switch (preset) {
    case Preset::Mane:
        g.extent={.06f,.04f,.32f}; g.length=.34f; g.comb=.9f; break;
    case Preset::Tail:
        g.extent={.055f,.035f,.055f}; g.length=.65f; g.comb=.95f;
        g.segments=8; g.stiffness=.23f; break;
    case Preset::Hair:
        g.extent={.15f,.14f,.14f}; g.length=.38f; g.stiffness=.36f; g.undercoat=.62f; break;
    case Preset::Beard:
        g.extent={.105f,.03f,.065f}; g.length=.16f; g.stiffness=.57f; g.undercoat=.22f; break;
    case Preset::Coat: break;
    }
    return g;
}
bool Validate(const Groom& g,std::string& error) {
    if (!Range(g.center,-64,64) || !Range(g.extent,0,32) || g.extent.x<.001f ||
        g.extent.z<.001f || !Range(g.flow,-1,1) || glm::dot(g.flow,g.flow)<.00001f ||
        !Range(g.color,0,1) || !Range(g.length,.003f,8) || !Range(g.width,.0001f,.3f) ||
        !Range(g.variation,0,.9f) || !Range(g.clump,0,1) || !Range(g.comb,0,1) ||
        !Range(g.coverageDegrees,18,165) || !Range(g.undercoat,0,1) || !Range(g.wave,0,1) ||
        !Range(g.stiffness,0,1) || !Range(g.damping,0,1) || !Range(g.wind,0,2) ||
        !Range(g.lodStart,.1f,5000) || !Range(g.lodEnd,g.lodStart,10000) ||
        g.guides<4 || g.guides>32 || g.segments<2 || g.segments>8 ||
        g.strands<g.guides || g.strands>512 || g.colliders.size()>4) {
        error = "hair groom exceeds its supported dimensions, guide count, density, or LOD range";
        return false;
    }
    for (const auto& c : g.colliders)
        if (!Range(c.start,-64,64) || !Range(c.end,-64,64) || !Range(c.radius,.001f,16)) {
            error = "invalid groom collider";
            return false;
        }
    error.clear();
    return true;
}
std::string Encode(const Groom& g) {
    Json colliders=Json::array();
    for(const auto& c:g.colliders)
        colliders.push_back({{"start",WriteVec(c.start)},{"end",WriteVec(c.end)},{"radius",c.radius}});
    return Json{{"version",1},{"enabled",g.enabled},{"simulated",g.simulated},
        {"castsShadow",g.castsShadow},{"worldCollision",g.worldCollision},
        {"center",WriteVec(g.center)},{"extent",WriteVec(g.extent)},{"flow",WriteVec(g.flow)},
        {"color",WriteVec(g.color)},{"length",g.length},{"width",g.width},
        {"variation",g.variation},{"clump",g.clump},{"comb",g.comb},{"stiffness",g.stiffness},
        {"damping",g.damping},{"wind",g.wind},{"lodStart",g.lodStart},{"lodEnd",g.lodEnd},
        {"coverageDegrees",g.coverageDegrees},{"undercoat",g.undercoat},{"wave",g.wave},
        {"guides",g.guides},{"segments",g.segments},{"strands",g.strands},{"seed",g.seed},
        {"colliders",std::move(colliders)}}.dump();
}
bool Decode(const std::string& text,Groom& out,std::string& error) {
    try {
        if(text.size()>8192) throw std::runtime_error("groom exceeds text budget");
        const auto j=Json::parse(text);
        if(!j.is_object()||!j.contains("version")||j.at("version")!=1)
            throw std::runtime_error("unsupported groom version");
        Groom g;
        const auto schema=Json::parse(Encode(g));
        for(auto it=j.begin();it!=j.end();++it)
            if(!schema.contains(it.key())) throw std::runtime_error("unknown groom key: "+it.key());
        for(auto attr:{std::pair<const char*,bool*>{"enabled",&g.enabled},{"simulated",&g.simulated},
                       {"castsShadow",&g.castsShadow},{"worldCollision",&g.worldCollision}})
            if(j.contains(attr.first)) *attr.second=j.at(attr.first).get<bool>();
        for(auto attr:{std::pair<const char*,glm::vec3*>{"center",&g.center},{"extent",&g.extent},
                       {"flow",&g.flow},{"color",&g.color}})
            if(j.contains(attr.first)) *attr.second=ReadVec(j.at(attr.first));
        for(auto attr:{std::pair<const char*,float*>{"length",&g.length},{"width",&g.width},
                       {"variation",&g.variation},{"clump",&g.clump},{"comb",&g.comb},
                       {"stiffness",&g.stiffness},{"damping",&g.damping},{"wind",&g.wind},
                       {"lodStart",&g.lodStart},{"lodEnd",&g.lodEnd},{"coverageDegrees",&g.coverageDegrees},
                       {"undercoat",&g.undercoat},{"wave",&g.wave}})
            if(j.contains(attr.first)) *attr.second=j.at(attr.first).get<float>();
        for(auto attr:{std::pair<const char*,unsigned*>{"guides",&g.guides},{"segments",&g.segments},
                       {"strands",&g.strands},{"seed",&g.seed}})
            if(j.contains(attr.first)) {
                const auto& value=j.at(attr.first);
                if(!value.is_number_integer()||value.get<double>()<0||value.get<double>()>UINT32_MAX)
                    throw std::runtime_error("invalid groom integer");
                *attr.second=value.get<unsigned>();
            }
        if(j.contains("colliders")) {
            const auto& values=j.at("colliders");
            if(!values.is_array()||values.size()>4) throw std::runtime_error("invalid groom collider count");
            for(const auto& c:values)
                g.colliders.push_back({ReadVec(c.at("start")),ReadVec(c.at("end")),c.at("radius").get<float>()});
        }
        if(!Validate(g,error)) return false;
        out=std::move(g);
        return true;
    } catch(const std::exception& e) { error=std::string("hair groom: ")+e.what(); return false; }
}
}
