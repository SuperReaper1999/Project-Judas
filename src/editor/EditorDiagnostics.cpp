#include "EditorContext.h"
#include "EditorPanels.h"
#include "EditorPlaySession.h"
#include "EditorProfilerSnapshot.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"

void EditorProfilerSnapshot::Collect(EditorContext &context, const EditorPlaySession &session,
                                     float frameDeltaSeconds) {
    ProfilerData &p = context.panels.profiler;
    averageMilliseconds = glm::mix(averageMilliseconds, frameDeltaSeconds * 1000.0f, 0.1f);
    p.frameMilliseconds = averageMilliseconds;
    p.framesPerSecond = averageMilliseconds > 0.0f ? 1000.0f / averageMilliseconds : 0.0f;
    const RenderStats &stats = context.host.GetRenderer().Stats();
    p.meshesVisible = stats.renderablesVisible;
    p.meshesCulled = stats.renderablesCulled;
    p.emittersVisible = stats.particleEmittersVisible;
    p.emittersCulled = stats.particleEmittersCulled;
    p.particlesSubmitted = stats.particlesSubmitted;
    p.drawCalls = stats.drawCalls;
    p.triangles = stats.triangles;
    p.shadowPasses = stats.shadowPasses;
    p.dynamicLights = stats.dynamicLights;
    p.debugLines = stats.debugLines;
    p.resources = context.host.Resources().Stats();
    p.jobs = context.host.Jobs().Stats();
    p.playing = context.panels.mode == EditorMode::Play && session.play && session.world;
    if (!p.playing) {
        p.fixedStepsThisFrame = 0;
        p.fixedStepMilliseconds = p.physicsMilliseconds = p.fluidMilliseconds = p.surfaceMilliseconds =
            p.sceneMilliseconds = 0.0f;
        p.physicsBodies = p.dynamicBodies = p.contacts = 0;
        p.entitiesFull = p.entitiesCoarse = p.entitiesDormant = p.entitiesDestroyed = 0;
        p.physics = PhysicsWorld::StepStats{};
        return;
    }
    p.fixedStepsThisFrame = session.play->LastFixedStepsThisFrame();
    p.fixedStepMilliseconds = static_cast<float>(session.play->LastFixedStepMilliseconds());
    p.physicsBodies = session.world->Physics().AliveBodyCount();
    p.dynamicBodies = session.world->Physics().DynamicBodyCount();
    p.contacts = session.world->Physics().LastStepContactCount();
    const RuntimeWorld::LifecycleCounts counts = session.world->CountLifecycle();
    p.entitiesFull = counts.full;
    p.entitiesCoarse = counts.coarse;
    p.entitiesDormant = counts.dormant;
    p.entitiesDestroyed = counts.destroyed;
    const FixedStepMeasurements &m = session.play->LastMeasurements();
    p.fluidMilliseconds = m.fluidMeasured ? static_cast<float>(m.fluidMilliseconds) : 0.0f;
    p.surfaceMilliseconds = static_cast<float>(session.play->LastSurfaceMilliseconds());
    p.sceneMilliseconds = static_cast<float>(session.play->LastSceneMilliseconds());
    p.physics = session.world->Physics().LastStepStats();
}
