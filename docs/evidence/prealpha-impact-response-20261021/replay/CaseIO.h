#pragma once
#include "RigidBody.h"
#include "ImpactSolver.h"
#include "third_party/nlohmann/json.hpp"
#include <fstream>
#include <map>
using DiagnosisJSON=nlohmann::json;
inline DiagnosisJSON DV(glm::vec3 v){return {v.x,v.y,v.z};}
inline DiagnosisJSON DV(glm::dvec3 v){return {v.x,v.y,v.z};}
inline DiagnosisJSON DBody(const RigidBody& b){DiagnosisJSON m=DiagnosisJSON::array();for(int i=0;i<3;++i)m.push_back(DV(b.inverseInertiaLocal[i]));return {{"p",DV(b.position)},{"q",{b.orientation.w,b.orientation.x,b.orientation.y,b.orientation.z}},{"v",DV(b.linearVelocity)},{"w",DV(b.angularVelocity)},{"im",b.inverseMass},{"ii",m}};}
inline DiagnosisJSON DContact(const Contact& c){return {{"hit",c.hit},{"p",DV(c.point)},{"n",DV(c.normal)},{"pen",c.penetration},{"sep",c.signedSeparation},{"pn",DV(c.preciseNormal)},{"a",DV(c.localAnchorA)},{"b",DV(c.localAnchorB)},{"wa",DV(c.localWitnessA)},{"wb",DV(c.localWitnessB)},{"local",c.hasLocalAnchors},{"aw",c.anchorAInWorldFrame},{"bw",c.anchorBInWorldFrame},{"state",int(c.separationState)}};}
inline glm::vec3 RV(const DiagnosisJSON& j){return {j[0].get<float>(),j[1].get<float>(),j[2].get<float>()};}
inline glm::dvec3 RD(const DiagnosisJSON& j){return {j[0].get<double>(),j[1].get<double>(),j[2].get<double>()};}
inline RigidBody RBody(const DiagnosisJSON& j){RigidBody b;b.position=RV(j["p"]);b.orientation={j["q"][0].get<float>(),j["q"][1].get<float>(),j["q"][2].get<float>(),j["q"][3].get<float>()};b.linearVelocity=RV(j["v"]);b.angularVelocity=RV(j["w"]);b.inverseMass=j["im"];for(int i=0;i<3;++i)b.inverseInertiaLocal[i]=RV(j["ii"][i]);return b;}
inline Contact RContact(const DiagnosisJSON& j){Contact c;c.hit=j["hit"];c.point=RV(j["p"]);c.normal=RV(j["n"]);c.penetration=j["pen"];c.signedSeparation=j["sep"];c.preciseNormal=RD(j["pn"]);c.localAnchorA=RD(j["a"]);c.localAnchorB=RD(j["b"]);c.localWitnessA=RD(j["wa"]);c.localWitnessB=RD(j["wb"]);c.hasLocalAnchors=j["local"];c.anchorAInWorldFrame=j["aw"];c.anchorBInWorldFrame=j["bw"];c.separationState=SeparationState(j["state"].get<int>());return c;}
inline DiagnosisJSON DCase(const std::vector<ImpactContact>& rows){DiagnosisJSON b=DiagnosisJSON::array(),r=DiagnosisJSON::array();std::map<const RigidBody*,unsigned> ids;auto id=[&](const RigidBody* body){auto it=ids.find(body);if(it!=ids.end())return it->second;unsigned i=ids.size();ids.emplace(body,i);b.push_back(DBody(*body));return i;};for(auto& row:rows)r.push_back({{"a",id(row.a)},{"b",id(row.b)},{"g",DContact(row.geometry)},{"friction",row.friction},{"restitution",row.restitution},{"persistent",row.persistent}});return {{"bodies",b},{"rows",r}};}
