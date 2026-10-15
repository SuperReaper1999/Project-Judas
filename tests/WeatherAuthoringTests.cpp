#include "Scene.h"
#include "SceneSerialization.h"
#include "Prefab.h"
#include "editor/EditorDocument.h"

#include "../third_party/nlohmann/json.hpp"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <limits>

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* message) {
    ++checks;if(!value)++failures;
    std::printf("%s %s\n",value?"PASS":"FAIL",message);
}
}

int main() {
    std::string error;
    Scene scene;scene.Settings().name="Authored weather";
    scene.Settings().worldOrigin={9e11,-7e11,3e11};
    auto& owner=scene.CreateObject("Coastal storm");const auto ownerId=owner.id;
    owner.transform.position={23,6,-9};
    owner.transform.rotation=glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3)));
    WeatherSettings weather;
    Check(WeatherPreset("thunderstorm",weather.profile),"known preset supplies ordinary profile values");
    const auto beforeUnknown=EncodeWeatherSettings(weather);
    Check(!WeatherPreset("not-an-engine-weather-mode",weather.profile)&&EncodeWeatherSettings(weather)==beforeUnknown,
          "unknown preset is rejected without replacing a profile");
    weather.seed=937;weather.priority=17;weather.regionShape=WeatherRegionShape::Box;
    weather.regionCenter={1.125,-2.25,3.5};weather.regionHalfExtents={70,90,110};
    weather.regionRotation=glm::angleAxis(.2,glm::normalize(glm::dvec3(1,2,-1)));
    weather.frame.radial=true;weather.frame.center={9e11+123.125,-7e11+456.25,3e11+789.5};
    weather.frame.radius=6371000.125;weather.frame.north={.2,.3,-.9};
    weather.tornadoOffset={34.25,-71.5};weather.tornadoDirection={.4,0,-.7};
    weather.profile.wind={13.5,-2.25,7.125};weather.profile.tornado=.6;
    Check(ValidateWeatherSettings(weather,error),"rotated radial profile with absolute binary64 centre is valid");
    owner.weather=weather;
    auto& prop=scene.CreateObject("Wind-responsive prop");const auto propId=prop.id;
    prop.parent=ownerId;prop.transform.position={3,2,1};
    prop.body=SceneBodyComponent{};prop.body->motion=SceneBodyMotion::Dynamic;
    prop.weatherResponse=WeatherResponseSettings{};
    prop.weatherResponse->area=1.75;prop.weatherResponse->dragCoefficient=.8;
    prop.weatherResponse->maxAcceleration=12.5;prop.weatherResponse->occlusionMask=CategoryBit(61)|CategoryBit(3);

    std::string text,again;Scene loaded;
    Check(SaveSceneToString(scene,text)&&LoadSceneFromString(text,loaded,error)&&ScenesEqual(scene,loaded),
          "weather and wind response round-trip through ordinary scene serialization");
    Check(SaveSceneToString(loaded,again)&&again==text,"weather scene save-load-save is byte-identical");
    Check(loaded.Find(ownerId)->weather->frame.center==weather.frame.center,
          "Earth-scale and larger absolute weather anchors retain binary64 detail");
    Check(loaded.Find(propId)->weatherResponse->occlusionMask==(CategoryBit(61)|CategoryBit(3)),
          "shelter mask preserves high M39 collision-layer bits");
    Scene flattened;
    Check(FlattenHierarchy(scene,flattened,error)&&
          EncodeWeatherSettings(*flattened.Find(ownerId)->weather)==EncodeWeatherSettings(weather),
          "flattening retains owner-local descriptor rather than double-transforming weather");
    auto properties=ObjectProperties(*scene.Find(ownerId));
    SceneObject propertyCopy=*scene.Find(ownerId);propertyCopy.weather.reset();
    Check(properties.count("weather.region")&&ApplyObjectProperties(propertyCopy,properties,error)&&
          EncodeWeatherSettings(*propertyCopy.weather)==EncodeWeatherSettings(weather),
          "named component properties restore a weather region");
    auto responseProperties=ObjectProperties(*scene.Find(propId));
    Check(responseProperties.count("weather.response"),"wind response uses the same prefab property path");

    auto encoded=nlohmann::json::parse(EncodeWeatherSettings(weather));
    WeatherSettings untouched;untouched.priority=-7;const auto original=EncodeWeatherSettings(untouched);
    auto unknown=encoded;unknown["invented-weather-field"]=3;
    Check(!DecodeWeatherSettings(unknown.dump(),untouched,error)&&EncodeWeatherSettings(untouched)==original,
          "unknown region fields reject transactionally");
    auto future=encoded;future["version"]=999;
    Check(!DecodeWeatherSettings(future.dump(),untouched,error)&&EncodeWeatherSettings(untouched)==original,
          "unknown descriptor version cannot partially mutate the destination");
    auto invalid=weather;invalid.profile.cloudCoverage=1.01;
    Check(!ValidateWeatherSettings(invalid,error),"invalid normalized profile intensity is rejected");
    invalid=weather;invalid.regionHalfExtents.x=-1;
    Check(!ValidateWeatherSettings(invalid,error),"negative region extent is rejected");
    invalid=weather;invalid.frame.center.z=std::numeric_limits<double>::infinity();
    Check(!ValidateWeatherSettings(invalid,error),"non-finite absolute frame is rejected");
    auto response=*scene.Find(propId)->weatherResponse;response.area=-.01;
    Check(!ValidateWeatherResponseSettings(response,error),"negative physical response area is rejected");
    response=*scene.Find(propId)->weatherResponse;response.maxAcceleration=std::numeric_limits<double>::quiet_NaN();
    Check(!ValidateWeatherResponseSettings(response,error),"non-finite wind acceleration limit is rejected");
    Scene unchanged=scene;auto badProperties=properties;
    badProperties["weather.region"]="\"{not JSON}\"";
    Check(!ApplyObjectProperties(*unchanged.Find(ownerId),badProperties,error)&&ScenesEqual(unchanged,scene),
          "malformed prefab weather edit leaves the source object unchanged");

    // Exercise the same linked prefab resolver and snapshot history used by the
    // editor. No weather-specific prefab IDs or override machinery is needed.
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory=std::filesystem::temp_directory_path()/("judas-m86-authoring-"+std::to_string(stamp));
    std::filesystem::create_directories(directory/"Assets");
    Scene prefab;Check(CreatePrefab(scene,ownerId,prefab,error),"weather region and physical prop form an ordinary prefab");
    const auto path=(directory/"Assets/coastal.judasprefab").string();
    Check(SaveSceneToFile(prefab,path,error),"weather prefab saves through the normal scene writer");
    AssetDatabase assets;assets.Scan(directory.string(),(directory/"Assets").string());AssetRecord asset;
    const bool tracked=assets.Track(path,asset,error);Check(tracked,"weather prefab uses ordinary asset metadata");
    if(tracked) {
        Scene instances;SceneTransform placement;placement.position={80,0,-60};SceneObjectId a=0,b=0;
        const bool first=InstantiatePrefab(instances,prefab,asset.id,placement,a,error);placement.position={-80,0,60};
        const bool second=InstantiatePrefab(instances,prefab,asset.id,placement,b,error);
        Check(first&&second&&a!=b,"two weather prefab instances have independent normal entity identities");
        if(first&&second) {
            const Scene before=instances;
            instances.Find(a)->weather->profile.rain=.25;
            CapturePrefabEdits(before,instances);
            Check(instances.Find(a)->prefabOverrides.count("weather.region"),
                  "ordinary prefab override captures an independently edited profile");
            prefab.Find(ownerId)->weather->profile.rain=.75;
            Check(SaveSceneToFile(prefab,path,error),"source profile can be edited without replacing prefab identity");
            Scene resolved;
            const bool resolve=ResolvePrefabs(instances,&assets,resolved,error);
            Check(resolve&&resolved.Find(a)->weather->profile.rain==.25&&resolved.Find(b)->weather->profile.rain==.75,
                  "one profile override remains independent while the other instance receives the source edit");
            EditorDocument document;document.GetScene()=resolved;
            document.BeginEdit();document.GetScene().Find(a)->weather->profile.snow=.5;document.CommitEdit();
            Check(document.CanUndo()&&document.GetScene().Find(a)->weather->profile.snow==.5,
                  "weather edits participate in normal editor history");
            document.Undo();Check(document.GetScene().Find(a)->weather->profile.snow==resolved.Find(a)->weather->profile.snow,
                                  "undo restores the complete authored profile");
            document.Redo();Check(document.GetScene().Find(a)->weather->profile.snow==.5,
                                  "redo restores the weather profile edit");
            const Scene beforeRemoval=document.GetScene();document.BeginEdit();document.GetScene().Find(a)->weather.reset();document.CommitEdit();
            Check(!document.GetScene().Find(a)->weather,"weather component removal uses normal authoring semantics");
            document.Undo();Check(ScenesEqual(document.GetScene(),beforeRemoval),"undo restores removed weather and its prefab override");
        }
    }
    std::filesystem::remove_all(directory);
    std::printf("SUMMARY %u checks %u failures\n",checks,failures);
    if(failures)std::printf("LAST ERROR %s\n",error.c_str());
    return failures?1:0;
}
