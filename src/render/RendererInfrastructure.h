#pragma once
// Private coordinator state. Public Renderer users do not depend on individual
// implementation owners: changing a particle/stream/shader owner need not
// recompile physics, gameplay or editor translation units that use Renderer.
#include "ParticlePass.h"
#include "StreamArena.h"
#include "SubmissionQueue.h"
#include "ViewUniforms.h"
#include "GpuParticlePass.h"
#include "WaterPass.h"
#include "ShadowPassCache.h"

namespace JudasRender {
struct RendererInfrastructure {
    SubmissionQueue submission;
    StreamArena instanceArena;
    ViewUniforms viewUniforms;
    ParticlePass particlePass;
    GpuParticlePass gpuParticlePass;
    WaterPass waterPass;
    ShadowPassCache shadowCache;
    bool persistentUploads=true, orderedInstances=false;
};
} // namespace JudasRender
