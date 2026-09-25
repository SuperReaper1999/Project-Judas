#include "RuntimeDiagnostics.h"

#include <algorithm>
#include <cstdio>
#include <limits>

#include "GameSession.h"
#include "RadialTerrain.h"
#include "ReferenceFrame.h"
#include "RuntimeWorld.h"

RuntimeDiagnostics::RuntimeDiagnostics(bool fluid, bool atmosphere, bool fire, bool telemetry)
    : m_fluid(fluid), m_atmosphere(atmosphere), m_fire(fire), m_telemetry(telemetry) {
    if (m_telemetry) {
        std::printf(
            "frame,posX,posY,posZ,upX,upY,upZ,yaw,pitch,lookX,lookY,lookZ,grounded,velX,velY,"
            "velZ,gravX,gravY,gravZ\n");
        std::fflush(stdout);
    }
}

void RuntimeDiagnostics::PrepareMeasurements(FixedStepMeasurements& m) const {
    m.measureFluid = m_fluid;
    m.measureAtmosphere = m_atmosphere;
    m.measureFire = m_fire;
    m.fluidMeasured = m.atmosphereMeasured = m.fireMeasured = false;
}

void RuntimeDiagnostics::RecordFixedStep(const GameSession& session, const FixedStepMeasurements& m,
                                         double fixedStepMilliseconds) {
    const RuntimeWorld& world = session.World();
    ++m_fixedSteps;
    if (m.fluidMeasured) {
        m_fluidMilliseconds += m.fluidMilliseconds;
        ++m_fluidSteps;
    }
    if (m_atmosphere && m.atmosphereMeasured) {
        m_atmosphereMilliseconds += m.atmosphereMilliseconds;
        m_fixedMilliseconds += fixedStepMilliseconds;
        ++m_atmosphereSteps;
        if (m_atmosphereSteps % 600 == 0 && world.GetVehicle() && world.GetAtmosphere()) {
            const BodyHandle ship = world.GetVehicle()->handle;
            const RuntimeWorld::Atmosphere& atmosphere = *world.GetAtmosphere();
            const glm::vec3 position = world.Physics().GetTransform(ship).position;
            const float radius = glm::length(position - atmosphere.frame.originPosition);
            const glm::vec3 relativeVelocity = RelativeVelocityToFrame(
                atmosphere.frame, position, world.Physics().GetLinearVelocity(ship));
            const float specificEnergy = 0.5f * glm::dot(relativeVelocity, relativeVelocity) +
                                         atmosphere.field.SpecificPotentialAtRadius(radius);
            std::fprintf(stderr,
                         "M26: atmosphere %.4f ms/sample, full fixed step %.4f ms; "
                         "rho %.5f kg/m^3, P %.3f Pa, airspeed %.2f m/s, drag %.2f N; "
                         "planet r %.2f m, specific E %.3f J/kg\n",
                         m_atmosphereMilliseconds / static_cast<double>(m_atmosphereSteps),
                         m_fixedMilliseconds / static_cast<double>(m_atmosphereSteps),
                         m.lastAerodynamicDrag.density, m.lastAerodynamicDrag.pressure,
                         m.lastAerodynamicDrag.relativeAirspeed, glm::length(m.lastAerodynamicDrag.force),
                         radius, specificEnergy);
        }
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

void RuntimeDiagnostics::RecordFrame(const GameSession& session, double surfaceMilliseconds,
                                     double sceneMilliseconds, float frameDeltaSeconds) {
    if (!m_fluid) return;
    const RuntimeWorld& world = session.World();
    ++m_frames;
    m_surfaceMilliseconds += surfaceMilliseconds;
    m_sceneMilliseconds += sceneMilliseconds;
    m_frameMilliseconds += frameDeltaSeconds * 1000.0;
    if (m_frames % 120 != 0 || !world.HasFluid()) return;
    const FluidDiagnostics state = world.Fluid().GetDiagnostics();
    float minimumClearance = std::numeric_limits<float>::max();
    for (const RuntimeWorld::Terrain& terrain : world.Terrains()) {
        for (const FluidParticle& particle : world.Fluid().Particles()) {
            const glm::vec3 local = glm::conjugate(terrain.rotation) * (particle.position - terrain.position);
            minimumClearance = std::min(minimumClearance, terrain.surface->Sample(local).signedDistance);
        }
    }
    std::fprintf(stderr,
                 "Fluid timing: %zu particles; %.3f ms/fluid step (%zu steps); %.3f ms/surface+upload; "
                 "%.3f ms/scene CPU submit; %.3f ms/frame wall; mass %.3f kg; "
                 "mean/max over-density %.2f/%.2f%%; COM (%.3f, %.3f, %.3f) m; kinetic %.3f J",
                 state.particleCount, m_fluidMilliseconds / static_cast<double>(std::max<std::size_t>(m_fluidSteps, 1)),
                 m_fluidSteps, m_surfaceMilliseconds / m_frames, m_sceneMilliseconds / m_frames,
                 m_frameMilliseconds / m_frames, state.totalMass, state.meanPositiveDensityError * 100.0f,
                 state.maxPositiveDensityError * 100.0f, state.centerOfMass.x, state.centerOfMass.y,
                 state.centerOfMass.z, state.kineticEnergy);
    if (!world.Terrains().empty()) std::fprintf(stderr, "; min terrain clearance %.3f m", minimumClearance);
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
}

void RuntimeDiagnostics::RecordTelemetryFrame(const GameSession& session) {
    if (!m_telemetry) return;
    constexpr int kInterval = 10;
    if ((m_telemetryFrames++ % kInterval) != 0) return;
    const PlayerController& player = session.Player();
    const glm::vec3 pos = player.GetPosition();
    const glm::vec3 up = player.GetOrientation() * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 look = player.GetLookDirection();
    const glm::vec3 vel = player.GetVelocity();
    const glm::vec3 grav = session.World().Gravity().Sample(pos);
    std::printf("%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                m_telemetryFrames, pos.x, pos.y, pos.z, up.x, up.y, up.z, player.GetYaw(), player.GetPitch(),
                look.x, look.y, look.z, player.IsGrounded() ? 1 : 0, vel.x, vel.y, vel.z, grav.x, grav.y, grav.z);
    std::fflush(stdout);
}
