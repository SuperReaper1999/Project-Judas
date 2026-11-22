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
#include "SkinnedBoundsCache.h"
#include "../lighting/LightLists.h"
#include "../PerformanceProfiler.h"
#include <cstdlib>
#include <array>

namespace JudasRender {
// Opt-in aggregate diagnostics avoid a profiler event for every primitive.
// Elapsed GL-call time includes driver waits; it is not GPU execution time.
struct SubmissionTimings {
    bool enabled=false;
    std::array<std::uint64_t,2> material{},draw{},calls{};
    void Begin() {
        Publish(); // Auxiliary work can precede the display's BeginFrame.
        const auto* flag=std::getenv("JUDAS_PROFILE_RENDER_DETAIL");
        enabled=PerformanceProfiler::Get().Active()&&flag&&flag[0]=='1'&&flag[1]=='\0';
    }
    std::uint64_t Start() const { return enabled?PerformanceProfiler::Now():0; }
    void Material(std::uint64_t start,bool shadow) { if(start)material[shadow]+=PerformanceProfiler::Now()-start; }
    void Draw(std::uint64_t start,bool shadow) { if(start){draw[shadow]+=PerformanceProfiler::Now()-start;++calls[shadow];} }
    void Publish() {
        if(enabled&&(calls[0]||calls[1])) {
            JUDAS_PROFILE_COUNTER("Colour material binding CPU elapsed ms",double(material[0])*1e-6,ProfileCounterMode::Sum);
            JUDAS_PROFILE_COUNTER("Shadow material binding CPU elapsed ms",double(material[1])*1e-6,ProfileCounterMode::Sum);
            JUDAS_PROFILE_COUNTER("Colour GL draw CPU elapsed ms",double(draw[0])*1e-6,ProfileCounterMode::Sum);
            JUDAS_PROFILE_COUNTER("Shadow GL draw CPU elapsed ms",double(draw[1])*1e-6,ProfileCounterMode::Sum);
            JUDAS_PROFILE_COUNTER("Timed colour primitive draws",double(calls[0]),ProfileCounterMode::Sum);
            JUDAS_PROFILE_COUNTER("Timed shadow primitive draws",double(calls[1]),ProfileCounterMode::Sum);
        }
        material={};draw={};calls={};
    }
};
struct RendererInfrastructure {
    GLuint uiMapSampler = 0;
    SubmissionTimings submissionTimings;
    bool coalesceOpaqueShadow=true;
    SubmissionQueue submission;
    StreamArena instanceArena;
    ViewUniforms viewUniforms;
    ParticlePass particlePass;
    GpuParticlePass gpuParticlePass;
    WaterPass waterPass;
    ShadowPassCache shadowCache;
    SkinnedBoundsCache skinnedBounds;
    JudasLighting::LightLists lightLists;
    bool lightStorageReady=false,lightStorageUnavailable=false,lightStorageVariant=false;
    bool persistentUploads=true, orderedInstances=false, trackShadowResources=true;
};
} // namespace JudasRender
