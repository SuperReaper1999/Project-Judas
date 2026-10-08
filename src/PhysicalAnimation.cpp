#include "PhysicalAnimation.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

const char* PhysicalAnimationModeName(PhysicalAnimationMode mode){
    switch(mode){case PhysicalAnimationMode::Animation:return "animation";
    case PhysicalAnimationMode::Partial:return "partial";case PhysicalAnimationMode::Active:return "active";
    case PhysicalAnimationMode::Passive:return "passive";}return "invalid";
}
bool ParsePhysicalAnimationMode(const std::string& name,PhysicalAnimationMode& mode){
    for(auto m:{PhysicalAnimationMode::Animation,PhysicalAnimationMode::Partial,PhysicalAnimationMode::Active,PhysicalAnimationMode::Passive}){
        if(name==PhysicalAnimationModeName(m)){mode=m;return true;}
    }
    return false;
}
bool ValidPhysicalAnimationSettings(const PhysicalAnimationSettings& settings,std::string& error){
    if(settings.regions.size()>8){error="physical animation has an eight-region limit";return false;}
    std::set<std::string> ids;
    for(size_t index=0;index<settings.regions.size();++index){const auto& r=settings.regions[index];
        const auto fail=[&](const char* text){error="physical region["+std::to_string(index)+"] "+text;return false;};
        if(r.id.empty()||r.id.size()>64||!ids.insert(r.id).second)return fail("requires a unique 1..64-byte id");
        if(r.joints.empty()||r.joints.size()>32)return fail("requires 1..32 mapped joint keys");
        std::set<std::string> joints;for(const auto& key:r.joints)if(key.empty()||key.size()>1024||!joints.insert(key).second)return fail("contains an empty/duplicate/oversized joint key");
        if(!std::isfinite(r.stiffness)||r.stiffness<0||r.stiffness>10000||!std::isfinite(r.damping)||r.damping<0||r.damping>1000||
           !std::isfinite(r.maxTorque)||r.maxTorque<0||r.maxTorque>10000||!std::isfinite(r.effortWeight)||r.effortWeight<0||r.effortWeight>1||
           !std::isfinite(r.poseWeight)||r.poseWeight<0||r.poseWeight>1)return fail("has invalid stiffness/damping/effort/pose bounds");
    }return true;
}
bool PhysicalAnimationSettingsEqual(const PhysicalAnimationSettings& a,const PhysicalAnimationSettings& b){
    if(a.enabled!=b.enabled||a.regions.size()!=b.regions.size())return false;
    for(size_t i=0;i<a.regions.size();++i){const auto& x=a.regions[i];const auto& y=b.regions[i];
        if(x.id!=y.id||x.joints!=y.joints||x.enabled!=y.enabled||x.stiffness!=y.stiffness||x.damping!=y.damping||x.maxTorque!=y.maxTorque||x.effortWeight!=y.effortWeight||x.poseWeight!=y.poseWeight)return false;
    }return true;
}
bool ValidPhysicalAnimationRequest(const PhysicalAnimationRequest& r,std::string& error){
    if(int(r.mode)<0||int(r.mode)>3||!std::isfinite(r.fade)||r.fade<0||r.fade>3600){error="invalid physical authority mode/fade";return false;}
    if(r.placement){auto p=r.placement->position;auto q=r.placement->rotation;
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||!std::isfinite(glm::dot(q,q))||glm::dot(q,q)<1e-12f){error="invalid physical return placement";return false;}}
    if((r.resumeMotor||r.placement)&&r.mode!=PhysicalAnimationMode::Animation){error="placement and motor resume apply only to animation return";return false;}
    return true;
}
std::string SerializePhysicalAnimationSettings(const PhysicalAnimationSettings& settings){
    nlohmann::ordered_json j={{"enabled",settings.enabled},{"regions",nlohmann::ordered_json::array()}};
    for(const auto& r:settings.regions)j["regions"].push_back({{"id",r.id},{"joints",r.joints},{"enabled",r.enabled},{"stiffness",r.stiffness},{"damping",r.damping},{"maxTorque",r.maxTorque},{"effortWeight",r.effortWeight},{"poseWeight",r.poseWeight}});
    return j.dump();
}
bool ParsePhysicalAnimationSettings(const std::string& text,PhysicalAnimationSettings& output,std::string& error){
    try{if(text.size()>128*1024)throw std::runtime_error("physical animation configuration exceeds 128 KiB");
        auto j=nlohmann::ordered_json::parse(text,[](int depth,nlohmann::ordered_json::parse_event_t,const nlohmann::ordered_json&){if(depth>8)throw std::runtime_error("physical animation nesting bound");return true;});
        if(!j.is_object()||!j.contains("regions")||!j["regions"].is_array())throw std::runtime_error("physical animation requires regions array");
        if(j["regions"].size()>8)throw std::runtime_error("physical animation has an eight-region limit");
        PhysicalAnimationSettings s;s.enabled=j.value("enabled",true);
        for(const auto& item:j["regions"]){PhysicalAnimationRegion r;r.id=item.at("id").get<std::string>();r.joints=item.at("joints").get<std::vector<std::string>>();r.enabled=item.value("enabled",true);r.stiffness=item.value("stiffness",30.f);r.damping=item.value("damping",4.f);r.maxTorque=item.value("maxTorque",20.f);r.effortWeight=item.value("effortWeight",1.f);r.poseWeight=item.value("poseWeight",1.f);s.regions.push_back(std::move(r));}
        if(!ValidPhysicalAnimationSettings(s,error))return false;
        output=std::move(s);return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
PhysicalTorqueResult CalculatePhysicalTorque(glm::quat targetRelative,glm::quat child,glm::quat parent,
    glm::vec3 childVelocity,glm::vec3 parentVelocity,const glm::mat3& childInertia,
    const glm::mat3& parentInverseInertia,const PhysicalAnimationRegion& r,float dt){
    PhysicalTorqueResult result;if(!(dt>0)||!std::isfinite(dt))return result;
    auto relative=glm::normalize(glm::inverse(parent)*child);
    auto error=glm::normalize(targetRelative*glm::inverse(relative));if(error.w<0)error=-error;
    glm::vec3 vector(error.x,error.y,error.z);const float length=glm::length(vector);
    auto localError=length>1e-7f?vector*(2*std::atan2(length,error.w)/length):2.f*vector;
    result.angleError=glm::length(localError);auto worldError=parent*localError;
    if(!r.enabled||r.effortWeight<=0)return result;
    const float stiffness=r.stiffness*r.effortWeight,damping=r.damping*r.effortWeight;
    auto desired=stiffness*worldError-damping*(childVelocity-parentVelocity);
    const float demand=glm::length(desired);if(demand<1e-8f)return result;
    // A scalar projection stabilizes only the requested direction. With
    // anisotropic inertia, that torque also accelerates orthogonal axes;
    // direction-dependent scalar damping can amplify tiny frame noise.
    // Solve the complete implicit angular response without discarding those
    // couplings. This uses the same authored PD effort and ordinary bodies.
    const auto inverseEffective=glm::inverse(glm::dmat3(childInertia))+glm::dmat3(parentInverseInertia);
    const auto response=glm::dmat3(1)+(double(dt)*damping+double(dt)*dt*stiffness)*inverseEffective;
    result.torque=glm::vec3(glm::inverse(response)*glm::dvec3(desired));
    const float magnitude=glm::length(result.torque),cap=r.maxTorque*r.effortWeight;
    result.saturated=magnitude>cap;if(result.saturated&&magnitude>0)result.torque*=cap/magnitude;
    return result;
}
