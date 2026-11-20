#pragma once

#include "../Light.h"

struct GraphicsQuality {
    enum class Weather { Authored, Low, High };
    unsigned shadowResolution = kDefaultShadowResolution;
    unsigned directionalCascades = 3;
    float shadowDistanceScale = 1;
    bool softShadows = true;
    float meshLodBias = 1;
    Weather weather = Weather::Authored;
    unsigned localShadowBudget = 8;
    float renderScale = 1;
    float heightfieldLodBias = 1;
    int waterSurfaceLodBias = 0;
    unsigned materialAnisotropy = 1;

    bool operator==(const GraphicsQuality& other) const {
        return shadowResolution == other.shadowResolution &&
               directionalCascades == other.directionalCascades &&
               shadowDistanceScale == other.shadowDistanceScale &&
               softShadows == other.softShadows && meshLodBias == other.meshLodBias &&
               weather == other.weather && localShadowBudget == other.localShadowBudget &&
               renderScale == other.renderScale && heightfieldLodBias == other.heightfieldLodBias &&
               waterSurfaceLodBias == other.waterSurfaceLodBias &&
               materialAnisotropy == other.materialAnisotropy;
    }
};
