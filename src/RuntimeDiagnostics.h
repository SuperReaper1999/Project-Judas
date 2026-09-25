#pragma once

#include <cstddef>

#include "Simulation.h"

class GameSession;
class RuntimeOptions;

// Milestone 28: the opt-in stderr/stdout measurement printers the
// interactive runtime has accumulated (M24/M25 fluid timing, M26
// atmosphere/orbit energy, M27 thermal, the live telemetry CSV). Pure
// reporting over a played scene; they never influence simulation and are
// compiled into the runtime only. Each printer keeps its own running
// averages here so Application's loop stays free of accumulator bookkeeping.
class RuntimeDiagnostics {
public:
    RuntimeDiagnostics(bool fluid, bool atmosphere, bool fire, bool telemetry);

    bool AnyFixedStepMeasurement() const { return m_fluid || m_atmosphere || m_fire; }
    // Configures which timings StepPlayedWorld should measure.
    void PrepareMeasurements(FixedStepMeasurements& measurements) const;
    // Call after every fixed step with that step's measurements.
    void RecordFixedStep(const GameSession& session, const FixedStepMeasurements& measurements,
                         double fixedStepMilliseconds);
    // Call once per render frame with presentation-side timings.
    void RecordFrame(const GameSession& session, double surfaceMilliseconds, double sceneMilliseconds,
                     float frameDeltaSeconds);
    // The JUDAS_LIVE_TELEMETRY CSV (header printed by the constructor).
    void RecordTelemetryFrame(const GameSession& session);

private:
    bool m_fluid, m_atmosphere, m_fire, m_telemetry;
    std::size_t m_fixedSteps = 0;
    double m_fluidMilliseconds = 0.0;
    std::size_t m_fluidSteps = 0;
    double m_atmosphereMilliseconds = 0.0;
    double m_fixedMilliseconds = 0.0;
    std::size_t m_atmosphereSteps = 0;
    double m_fireMilliseconds = 0.0;
    std::size_t m_fireSteps = 0;
    int m_frames = 0;
    double m_surfaceMilliseconds = 0.0;
    double m_sceneMilliseconds = 0.0;
    double m_frameMilliseconds = 0.0;
    int m_telemetryFrames = 0;
};
