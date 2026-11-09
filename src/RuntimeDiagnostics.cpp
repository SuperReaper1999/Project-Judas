#include "RuntimeDiagnostics.h"

#include <algorithm>
#include <cstdio>
#include <limits>

#include "RadialTerrain.h"
#include "ReferenceFrame.h"
#include "RuntimeWorld.h"

RuntimeDiagnostics::RuntimeDiagnostics(bool fluid, bool fire, bool telemetry)
    : m_fluid(fluid), m_fire(fire), m_telemetry(telemetry) {
    if (m_telemetry) {
        std::printf(
            "frame,posX,posY,posZ,upX,upY,upZ,lookX,lookY,lookZ,gravX,gravY,gravZ\n");
        std::fflush(stdout);
    }
}

void RuntimeDiagnostics::PrepareMeasurements(FixedStepMeasurements& m) const {
    m.measureFluid = m_fluid;
    m.measureFire = m_fire;
    m.fluidMeasured = m.fireMeasured = false;
}

void RuntimeDiagnostics::RecordFixedStep(const RuntimeWorld& world, const FixedStepMeasurements& m,
                                         double /*fixedStepMilliseconds*/) {
    ++m_fixedSteps;
    if (m.fluidMeasured) {
        m_fluidMilliseconds += m.fluidMilliseconds;
        ++m_fluidSteps;
    }
    if (m_fire && m.fireMeasured) {
        m_fireMilliseconds += m.fireMilliseconds;
        ++m_fireSteps;
        if (m_fireSteps % 600 == 0) {
            std::fprintf(stderr, "M27: thermal %.5f ms/step;",
                         m_fireMilliseconds / static_cast<double>(m_fireSteps));
            for (const RuntimeWorld::Combustible& c : world.Combustibles()) {
                const ThermalBodyState* state = world.Combustion().State(c.handle);
                if (!state) continue;
                std::fprintf(stderr, " %s %.1f K fuel %.4f kg burn %.5f kg/s heat %.1f W O2 %.5f kg/m^3;",
                             c.name.c_str(), state->temperatureKelvin, state->remainingFuelMassKg,
                             state->burnRateKgPerSecond, state->heatOutputWatts,
                             state->localOxidizerMassDensity);
            }
            std::fprintf(stderr, "\n");
        }
    }
}

void RuntimeDiagnostics::RecordFrame(const RuntimeWorld& world, double surfaceMilliseconds,
                                     double sceneMilliseconds, float frameDeltaSeconds) {
    if (!m_fluid) return;
    ++m_frames;
    m_surfaceMilliseconds += surfaceMilliseconds;
    m_sceneMilliseconds += sceneMilliseconds;
    m_frameMilliseconds += frameDeltaSeconds * 1000.0;
    if (m_frames % 120 != 0) return;
    std::fprintf(stderr,"Water presentation: %.3f ms/preparation, %.3f ms/scene CPU, %.3f ms/frame wall; grid %s\n",
                 m_surfaceMilliseconds/m_frames,m_sceneMilliseconds/m_frames,m_frameMilliseconds/m_frames,
                 world.GridWater()?"resident":"inactive");
}

void RuntimeDiagnostics::RecordTelemetryFrame(const RuntimeWorld& world) {
    if (!m_telemetry || (m_telemetryFrames++ % 10) != 0 || !world.view) return;
    const auto& pose = world.view->pose;
    const auto up = pose.rotation * glm::vec3(0,1,0);
    const auto look = pose.rotation * glm::vec3(0,0,-1);
    const auto gravity = world.Gravity().Sample(pose.position);
    std::printf("%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                m_telemetryFrames,pose.position.x,pose.position.y,pose.position.z,
                up.x,up.y,up.z,look.x,look.y,look.z,gravity.x,gravity.y,gravity.z);
}
