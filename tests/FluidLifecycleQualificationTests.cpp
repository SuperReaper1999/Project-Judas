#include "BuildIdentity.h"
#include "AssetDatabase.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "ProductionFluidCoupling.h"
#include "SceneSerialization.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "WorldState.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <glm/gtc/quaternion.hpp>

namespace {
unsigned checks = 0;
void Check(bool value, const std::string& description) {
    ++checks;
    std::cout << (value ? "PASS " : "FAIL ") << description << '\n';
    if (!value) throw std::runtime_error(description);
}

Scene Fixture() {
    Scene scene;
    auto& water = scene.CreateObject("Particle water");
    water.fluidVolume = SceneFluidVolumeComponent{};
    water.fluidVolume->spacing = .2f;
    water.fluidVolume->countX = water.fluidVolume->countY = water.fluidVolume->countZ = 4;
    scene.Settings().fluidScale = 4;
    auto& gravity = scene.CreateObject("Gravity");
    gravity.gravity = SceneGravityComponent{};
    gravity.gravity->kind = SceneGravityKind::Uniform;
    gravity.gravity->regionRadius = 100;
    gravity.gravity->magnitude = 9.81f;
    return scene;
}

// Include physical particle state, not just the authored component or count.
std::string Particles(RuntimeWorld& world) {
    std::ostringstream out;
    out << std::hexfloat;
    for (const auto& p : world.Fluid().Particles()) {
        for (const auto& v : {p.position, p.previousPosition, p.velocity, p.acceleration})
            out << v.x << ',' << v.y << ',' << v.z << ';';
        out << p.mass << '\n';
    }
    return out.str();
}

void AuthoredSphereInitialization() {
    for(int frame=0;frame<2;++frame) {
        const auto rotation=frame?glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3))):glm::quat(1,0,0,0);
        const glm::vec3 center=frame?glm::vec3(2,-1,3):glm::vec3(0);
        Scene scene;scene.Settings().fluidScale=2.5f;
        auto& gravity=scene.CreateObject("Zero gravity");gravity.gravity=SceneGravityComponent{};
        gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->magnitude=0;
        gravity.gravity->regionShape=SceneRegionShape::Box;gravity.gravity->regionHalfExtents=glm::vec3(10);
        gravity.transform.position=center;gravity.transform.rotation=rotation;
        auto& water=scene.CreateObject("Odd authored lattice");water.fluidVolume=SceneFluidVolumeComponent{};
        water.fluidVolume->countX=water.fluidVolume->countY=water.fluidVolume->countZ=3;
        water.fluidVolume->spacing=.125f;water.transform.position=center-rotation*glm::vec3(0,.125f,0);
        water.transform.rotation=rotation;
        auto& solid=scene.CreateObject("Ordinary static sphere");solid.body=SceneBodyComponent{};
        solid.body->shape=SceneShape::Sphere;solid.body->radius=.3f;solid.body->motion=SceneBodyMotion::Static;
        solid.transform.position=center;solid.transform.rotation=rotation;
        std::string serialized,error;SaveSceneToString(scene,serialized);Scene loaded;
        Check(LoadSceneFromString(serialized,loaded,error),"sphere initialization loads ordinary authored fluid and body components");
        RuntimeWorld world;ProjectSettings settings;settings.advancedFluidDynamics=true;world.ConfigureProject(settings);
        Check(world.Build(loaded,nullptr,error),"sphere initialization builds the actual authored runtime");
        unsigned exactCenter=0;
        for(const auto& particle:world.Fluid().Particles())exactCenter+=particle.position==center;
        const auto before=world.Fluid().GetDiagnostics();
        Check(exactCenter==1&&before.particleCount==27,"odd native fluid lattice actually contains the exact sphere-centre particle");
        bool completed=true;
        try{world.FluidCoupling().AdvanceResolvedLiquid(world,1.f/60);}
        catch(const std::exception&){completed=false;}
        Check(completed&&world.FluidCoupling().Measurements().executedSteps==1,
              "authored centred fluid executes an ordinary production liquid step without an exception");
        bool finite=true,outside=true;
        for(const auto& particle:world.Fluid().Particles()) {
            for(const auto value:{particle.position,particle.velocity,particle.acceleration})
                finite=finite&&std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);
            const glm::dvec3 radial=glm::dvec3(particle.position)-glm::dvec3(center);
            outside=outside&&glm::dot(radial,radial)>=double(.3f)*.3f;
        }
        const auto after=world.Fluid().GetDiagnostics();
        Check(finite&&outside,"authored fluid initialization leaves every actual particle finite and outside the real sphere");
        Check(after.particleCount==before.particleCount&&after.totalMass==before.totalMass,
              "authored sphere initialization preserves actual particle count and mass");
        Check(after.kineticEnergy==0,"stationary authored sphere initialization invents no fluid kinetic energy");
    }
}
}

int main() try {
    std::string error;
    const auto root = std::filesystem::temp_directory_path() /
        ("judas-fluid-lifecycle-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Project project;
    Check(Project::CreateNew(root.string(), "Fluid lifecycle", project, error), "create ordinary project");
    project.Settings().legacyGameplay = false;
    project.Settings().startupScene = "Scenes/water.judas";
    const Scene scene = Fixture();
    Check(SaveSceneToFile(scene, project.StartupScenePath(), error), "write authored particle scene");
    AssetDatabase assets;
    assets.Scan(project.RootDir(), project.AssetsDir());
    WorldRegion region;
    region.id = "water";
    region.scene = project.Settings().startupScene;
    PreparedWorldRegion prepared;
    Check(PrepareWorldRegion(region, project, assets, prepared, error), "default-off particle authoring can prepare as dormant region");
    const auto preparedFingerprint = prepared.fingerprint;

    RuntimeWorld dormant;
    dormant.ConfigureProject(project.Settings());
    Check(dormant.Build(scene, nullptr, error) && !dormant.HasFluid(), "default-off world creates no active particle field");
    std::map<std::string, SaveChunk> snapshot;
    Check(WorldPersistence::Capture(dormant, snapshot, error), "dormant particle authoring supports modern snapshot");
    Check(CanCaptureLegacyWorldState(dormant, error), "default-off particle authoring does not block legacy saves");
    Check(dormant.RequestOrigin({16, 0, 0}, error) && dormant.CommitOrigin(), "dormant particle authoring permits live origin shift");

    project.Settings().advancedFluidDynamics = true;
    Check(project.Save(error), "persist explicit experimental opt-in");
    Project reopened;
    Check(reopened.Load(project.ProjectFile(), error) && reopened.Settings().advancedFluidDynamics,
          "new project instance reads explicit opt-in");
    RuntimeWorld active;
    active.ConfigureProject(reopened.Settings());
    if (std::string(GetBuildIdentity().channel) == "Alpha") {
        Check(!active.Build(scene, nullptr, error) && error.find("contact stability") != std::string::npos,
              "Alpha still rejects explicit opt-in");
    } else {
        Check(active.Build(scene, nullptr, error) && active.HasFluid(), "Experimental opt-in builds real particles");
        const auto initial = Particles(active);
        Check(active.Fluid().Particles().size() == 64, "authored lattice is actually instantiated");
        active.Fluid().Step(1.f / 60, active.Gravity(), {}, {}, {});
        const auto evolved = Particles(active);
        Check(evolved != initial, "enabled solver advances actual physical particles");
        Check(!CanCaptureLegacyWorldState(active, error) && error.find("particle") != std::string::npos,
              "legacy save preflight rejects unrecorded active particle state");
        bool legacyDenied = false;
        try { (void)CaptureWorldState(active); }
        catch (const std::logic_error&) { legacyDenied = true; }
        Check(legacyDenied && Particles(active) == evolved,
              "direct legacy capture cannot silently omit evolved fluid state");
        std::map<std::string, SaveChunk> destination{{"sentinel", {73, "original snapshot bytes"}}};
        Check(!WorldPersistence::Capture(active, destination, error) && error.find("legacy PBF") != std::string::npos,
              "active particle cold snapshot rejects missing participant explicitly");
        Check(destination.size() == 1 && destination.at("sentinel").version == 73 &&
                  destination.at("sentinel").data == "original snapshot bytes" && Particles(active) == evolved,
              "rejected capture preserves existing snapshot and all particle state");
        const auto revision = active.OriginRevision();
        Check(!active.RequestOrigin({16, 0, 0}, error) && error.find("fluid") != std::string::npos,
              "active particle live origin rejects missing continuation contract");
        Check(!active.OriginPending() && active.OriginRevision() == revision &&
                  active.Settings().worldOrigin == glm::dvec3(0) && Particles(active) == evolved,
              "rejected origin request does not translate or queue any particle state");
        Check(!PrepareWorldRegion(region, project, assets, prepared, error) && error.find("unsupported") != std::string::npos,
              "active particle region preparation rejects unsupported ownership");
        Check(prepared.fingerprint == preparedFingerprint && Particles(active) == evolved,
              "rejected preparation preserves previously prepared result and active particles");
        active.Destroy();
        Check(active.Build(scene, nullptr, error) && Particles(active) == initial,
              "Stop/rebuild recreates the exact authored lattice without evolved-state leakage");
        active.FluidCoupling().AdvanceResolvedLiquid(active,1.f/60);
        Check(active.FluidCoupling().SampleField({0,.3f,0},{0,1,0},.05f,.1f,{1,0,0}).fraction>.9f,
              "published fluid field has a genuinely wet positive control");
        active.Fluid().Clear();
        active.FluidCoupling().PrepareRigidStep(active,1.f/60);
        Check(active.FluidCoupling().SampleField({0,.3f,0},{0,1,0},.05f,.1f,{1,0,0}).fraction==0,
              "external Clear invalidates published fluid and retained boundary before another force step");
        Check(active.Fluid().DisplacementMarkers().empty(),"external Clear leaves no orphan displacement ownership");
        AuthoredSphereInitialization();
    }
    std::filesystem::remove_all(root);
    std::cout << "Fluid lifecycle qualification: " << checks << " checks, 0 failures\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Fluid lifecycle qualification failed: " << e.what() << '\n';
    return 1;
}
