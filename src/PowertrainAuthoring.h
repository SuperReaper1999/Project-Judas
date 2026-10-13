#pragma once
#include "Powertrain.h"
#include <functional>

class Scene;
// One canonical bounded component record. JSON is authored data, not a second
// resource format; scene, prefab and native JS snapshots share this codec.
std::string EncodePowertrainSettings(const PowertrainSettings&);
bool DecodePowertrainSettings(const std::string&,PowertrainSettings&,std::string&);
std::vector<uint64_t> PowertrainReferences(const PowertrainSettings&);
void RemapPowertrainReferences(PowertrainSettings&,const std::function<uint64_t(uint64_t)>&);
bool ValidateScenePowertrains(const Scene&,std::string&);
