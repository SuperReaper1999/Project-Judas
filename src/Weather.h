#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <array>
#include <string>

// M86 is a prescribed weather field, not an atmospheric fluid simulation.
// Distances are metres, times seconds, velocity m/s; CPU positions are absolute
// binary64. Rendering quality is intentionally absent from this contract.
struct WeatherProfile {
    double cloudCoverage=.15, cloudDensity=.7, cloudBase=1000, cloudThickness=1200;
    double rain=0, snow=0, fog=0; // normalized artistic intensities [0,1]
    glm::dvec3 wind{3,0,1};
    double gust=1; // peak additional speed, m/s
    double lightningRate=0; // expected strikes/minute, bounded Bernoulli slots
    double tornado=0, tornadoRadius=25, tornadoHeight=500, tornadoSpeed=8;
};
struct WeatherFrame {
    bool radial=false;
    // Runtime: absolute plane ground anchor, or absolute spherical centre.
    // Authoring: flat centre/up/north are owner-local; radial centre is absolute.
    glm::dvec3 center{0},up{0,1,0},north{0,0,-1};
    double radius=6371000;
};
enum class WeatherRegionShape { Global, Sphere, Box };
struct WeatherSettings {
    bool enabled=true;
    uint32_t seed=1;
    int priority=0;
    WeatherProfile profile;
    WeatherFrame frame;
    // Authored owner-local region. RuntimeWorld resolves it to absolute once;
    // origin shifts never move an already resolved weather field.
    WeatherRegionShape regionShape=WeatherRegionShape::Global;
    glm::dvec3 regionCenter{0},regionHalfExtents{100};
    glm::dquat regionRotation{1,0,0,0};
    double regionRadius=100,blendDistance=10,topAltitude=16000;
    glm::dvec2 tornadoOffset{0}; // north/east metres from frame ground anchor
    glm::dvec3 tornadoDirection{1,0,0}; // resolved world direction; tangentized
};
struct WeatherResponseSettings {
    bool enabled=true;
    double area=1,dragCoefficient=1,maxAcceleration=30;
    uint64_t occlusionMask=~uint64_t{0};
};
struct WeatherLightning {
    uint64_t sequence=0;
    double time=-1;
    glm::dvec3 position{0};
    double energy=0;
};
struct WeatherState {
    double time=0,transitionElapsed=0,transitionDuration=0,tornadoTravel=0;
    WeatherProfile from,to;
    glm::dvec3 cloudOffset{0}; // integrated world-space advection, metres
    WeatherLightning lightning;
};
struct WeatherSample {
    bool active=false;
    WeatherProfile profile;
    WeatherFrame frame;
    glm::dvec3 up{0,1,0},wind{0},tornadoPosition{0},lightningPosition{0},cloudOffset{0};
    double altitude=0,time=0,weight=0,lightningFlash=0,airDensity=0; // kg/m3 for opt-in drag
    uint32_t seed=1;
    uint64_t lightningSequence=0;
};
// Presentation-only cover cache. Heights are signed metres along up from
// center; -10000 means open sky. Physics owns population, Renderer only reads.
struct WeatherShelterGrid {
    glm::dvec3 center{0};
    glm::vec3 up{0,1,0},right{1,0,0},forward{0,0,-1};
    float extent=32;
    std::array<float,256> heights=[](){std::array<float,256> v{};v.fill(-10000);return v;}();
    unsigned revision=0;
    bool valid=false;
};

bool ValidateWeatherProfile(const WeatherProfile&,std::string&);
bool ValidateWeatherSettings(const WeatherSettings&,std::string&);
bool ValidateWeatherResponseSettings(const WeatherResponseSettings&,std::string&);
std::string EncodeWeatherSettings(const WeatherSettings&);
bool DecodeWeatherSettings(const std::string&,WeatherSettings&,std::string&);
std::string EncodeWeatherResponseSettings(const WeatherResponseSettings&);
bool DecodeWeatherResponseSettings(const std::string&,WeatherResponseSettings&,std::string&);
// Current preset values are authoring defaults, not hidden engine modes.
bool WeatherPreset(const std::string& name,WeatherProfile& out);
WeatherProfile BlendWeatherProfiles(const WeatherProfile&,const WeatherProfile&,double);
double WeatherRegionWeight(const WeatherSettings&,glm::dvec3 absolutePosition);
glm::dvec3 WeatherUp(const WeatherFrame&,glm::dvec3 absolutePosition);
double WeatherAltitude(const WeatherFrame&,glm::dvec3 absolutePosition);

class WeatherRuntime {
public:
    explicit WeatherRuntime(WeatherSettings);
    const WeatherSettings& Settings()const{return m_settings;}
    WeatherProfile Current()const;
    // No clock progresses during pause. One fixed call accepts at most 1 s;
    // bounded deterministic lightning work is at most two slots per call.
    void Advance(double seconds);
    void Transition(const WeatherProfile&,double duration);
    void SetEnabled(bool value){m_settings.enabled=value;}
    void TriggerLightning(glm::dvec3 absolutePosition,double energy=1);
    WeatherSample Sample(glm::dvec3 absolutePosition)const;
    const WeatherState& State()const{return m_state;}
    std::string Capture()const;
    bool Restore(const std::string&,std::string&);
private:
    WeatherSettings m_settings;
    WeatherState m_state;
    glm::dvec3 TornadoPosition(double travel)const;
};

// Opt-in centred pressure drag through the ordinary body force accumulator.
// Caller supplies exposure from existing collision queries. It must not apply
// this a second time to a body already receiving equivalent atmosphere drag.
glm::dvec3 WeatherDragForce(const WeatherSample&,glm::dvec3 bodyVelocity,
                           double mass,const WeatherResponseSettings&,double exposure=1);
