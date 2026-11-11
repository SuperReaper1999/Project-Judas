#include "LightingQueries.h"
#include "LightingWorld.h"
#include "../RuntimeWorld.h"
#include "../LightAttenuation.h"
#include "../PerformanceProfiler.h"
#include <algorithm>
#include <cmath>

namespace JudasLighting {
namespace {float Luminance(glm::vec3 c){return glm::dot(c,glm::vec3(.2126f,.7152f,.0722f));}}
bool SampleExposure(const RuntimeWorld& world,glm::vec3 point,const ExposureOptions& options,ExposureSample& output,std::string& error){
    JUDAS_PROFILE_SCOPE("Gameplay lighting exposure");error.clear();
    if(!std::isfinite(glm::length(point))||options.maxSources>4096||options.maxLights>64||options.maxOcclusionQueries>64||
       !std::isfinite(options.sunOcclusionDistance)||options.sunOcclusionDistance<=0||options.sunOcclusionDistance>4096||
       (options.normal&&(!std::isfinite(glm::length(*options.normal))||glm::length(*options.normal)<1e-6f))){error="finite exposure position/normal, sources 0..4096, lights/queries 0..64 and sun distance (0,4096] required";return false;}
    ExposureSample result;result.sampledAtSeconds=world.SimulationTimeSeconds();
    result.color=result.upperColor=world.Settings().ambientColor;
    result.sourcesExamined=unsigned(std::min<size_t>(options.maxSources,world.StaticLights().size()));
    result.upperBoundKnown=world.StaticLights().size()<=options.maxSources;
    if(!result.upperBoundKnown){result.complete=false;result.unresolvedSources=unsigned(world.StaticLights().size()-options.maxSources);}
    struct Candidate {ExposureContributor value;glm::vec3 direction;float distance;};
    std::vector<Candidate> candidates;
    const auto normal=options.normal?glm::normalize(*options.normal):glm::vec3(0);
    for(const auto& light:BuildWorldLights(world,1,options.maxSources,true)){
        const auto delta=light.position-point;const float distance=glm::length(delta);
        const auto direction=distance>1e-6f?delta/distance:glm::vec3(0,1,0);
        float factor=ComputeDistanceAttenuation(distance,light.range);
        if(light.kind==LightKind::Spot&&distance>1e-6f)factor*=ComputeSpotConeFactor(glm::dot(-direction,glm::normalize(light.direction)),std::cos(glm::radians(light.innerConeDegrees)),std::cos(glm::radians(light.outerConeDegrees)));
        if(options.normal)factor*=std::max(0.f,glm::dot(normal,direction));
        if(factor>0)candidates.push_back({{light.identity,light.color*factor,{},false,false},direction,distance});
    }
    const auto& settings=world.Settings();
    if(settings.sunEnabled){const auto direction=glm::length(settings.sunDirection)>1e-6f?glm::normalize(settings.sunDirection):glm::vec3(0,1,0);const auto potential=settings.sunColor*settings.sunIntensity*(options.normal?std::max(0.f,glm::dot(normal,direction)):1.f);
        if(Luminance(potential)>0)candidates.push_back({{0,potential,{},false,false},direction,options.sunOcclusionDistance});}
    std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){const float x=Luminance(a.value.potential),y=Luminance(b.value.potential);return x!=y?x>y:a.value.source<b.value.source;});
    for(size_t i=0;i<candidates.size();++i){auto& candidate=candidates[i];auto& value=candidate.value;
        if(i>=options.maxLights){result.upperColor+=value.potential;result.complete=false;++result.unresolvedSources;continue;}
        if(result.queries<options.maxOcclusionQueries){
            auto filter=options.filter;
            if(value.source){const auto owned=world.OwnedCollisionBodies(value.source);filter.ignoredBodies.insert(filter.ignoredBodies.end(),owned.begin(),owned.end());}
            const float epsilon=std::min(.005f,candidate.distance*.25f);
            const auto hit=world.Physics().Raycast(point+candidate.direction*epsilon,candidate.direction,std::max(0.f,candidate.distance-2*epsilon),filter);
            ++result.queries;value.occlusionKnown=true;value.occluded=hit.hit;
            if(!hit.hit){value.visible=value.potential;result.color+=value.visible;}

        }else{result.complete=false;++result.unresolvedSources;}
        if(!value.occluded)result.upperColor+=value.potential;
        result.contributors.push_back(value);
    }
    result.relativeIlluminance=Luminance(result.color);result.upperIlluminance=Luminance(result.upperColor);
    output=std::move(result);return true;
}
}
