#include "ComponentEditors.h"
#include "EditorPanelWidgets.h"
#include "EditorPanels.h"
#include "EditorWidgets.h"
#include "EditorWorkspace.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <set>
#include <sstream>

using namespace EditorPanelWidgets;
#include "ProfilerView.h"

void DrawProfilerPanel(EditorPanelState &state) {
    if (!state.showProfiler)
        return;
    const auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({20, 76}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({std::min(850.0f, display.x - 40), std::min(700.0f, display.y - 110)},
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(1.0f);
    if (!BeginEditorPanel("Profiler", &state.showProfiler, state)) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Inspect live Play (F8: release cursor)", &state.profilerInspect);
    DrawIntegratedProfilerView();
    if (ImGui::CollapsingHeader("Specialized diagnostics (last observation, legacy timers)")) {
        const ProfilerData &p = state.profiler;
        ImGui::Text("Frame: %.2f ms  (%.0f FPS, rolling)", p.frameMilliseconds, p.framesPerSecond);
        if (!p.playing)
            ImGui::TextDisabled("Edit mode: no simulation stepping.");
        ImGui::Text("Fixed steps this frame: %d", p.fixedStepsThisFrame);
        ImGui::Text("Last fixed step: %.3f ms (whole StepPlayedWorld)", p.fixedStepMilliseconds);
        ImGui::Separator();
        ImGui::Text("Physics bodies: %zu live (%zu dynamic)", p.physicsBodies, p.dynamicBodies);
        ImGui::Text("Contacts (last step's manifold points): %zu", p.contacts);
        ImGui::Text("Broadphase: %zu candidate pairs of %zu possible (%.3f%%)", p.physics.candidatePairs,
                    p.physics.possiblePairs,
                    p.physics.possiblePairs > 0 ? 100.0 * p.physics.candidatePairs / p.physics.possiblePairs
                                                : 0.0);
        ImGui::Text("  %zu colliding pairs, tree height %d, %zu proxy reinsertions", p.physics.collidingPairs,
                    p.physics.treeHeight, p.physics.proxyReinsertions);
        ImGui::Text("  PhysicsWorld::Step %.3f ms (broadphase %.3f, narrowphase %.3f, solver %.3f)",
                    p.physics.totalMilliseconds, p.physics.broadphaseMilliseconds,
                    p.physics.narrowphaseMilliseconds, p.physics.solverMilliseconds);
        ImGui::Text("CCD holds: %zu operations (body limit %zu, step limit %zu, response %zu)",
                    p.physics.impactSafetyFallback, p.physics.impactBodyCapHolds,
                    p.physics.impactStepCapHolds, p.physics.impactResponseHolds);
        ImGui::Text("CCD responses: %zu / %zu applied; missing manifold %zu, energy rejection %zu",
                    p.physics.impactResponsesApplied, p.physics.impactResponseAttempts,
                    p.physics.impactMissingManifolds, p.physics.impactEnergyRejections);
        ImGui::TextDisabled("A hold truncates remaining travel, keeping physical velocity. Counts are "
                            "not unique bodies.");
        ImGui::Text("Entities: %zu full / %zu coarse / %zu dormant / %zu destroyed", p.entitiesFull,
                    p.entitiesCoarse, p.entitiesDormant, p.entitiesDestroyed);
        ImGui::Separator();
        ImGui::Text("Draw calls: %u  (incl. shadow passes)", p.drawCalls);
        ImGui::Text("Triangles submitted: %u", p.triangles);
        ImGui::Text("Visibility (all passes): %u visible / %u culled meshes", p.meshesVisible,
                    p.meshesCulled);
        ImGui::Text("Emitters: %u visible / %u culled; particles submitted: %u", p.emittersVisible,
                    p.emittersCulled, p.particlesSubmitted);
        ImGui::Text("Shadow passes: %u   Dynamic lights: %u", p.shadowPasses, p.dynamicLights);
        ImGui::Text("Debug lines: %u", p.debugLines);
        ImGui::Text("Scene submission: %.2f ms", p.sceneMilliseconds);
        ImGui::Separator();
        ImGui::Text("Fluid solve (in step): %s", p.fluidMilliseconds > 0.0f ? "" : "not measured");
        if (p.fluidMilliseconds > 0.0f) {
            ImGui::SameLine();
            ImGui::Text("%.3f ms", p.fluidMilliseconds);
        }
        ImGui::Text("Fluid surface rebuild: %.2f ms", p.surfaceMilliseconds);
        ImGui::Separator();
        ImGui::Text("Resources: %zu ready (%zu meshes, %zu textures), %zu loading, %zu failed",
                    p.resources.ready, p.resources.loadedMeshes, p.resources.loadedTextures,
                    p.resources.loading, p.resources.failed);
        ImGui::Text("  resident %.1f MB of %.0f MB budget (peak %.1f MB)",
                    p.resources.bytesResident / 1048576.0, p.resources.budgetBytes / 1048576.0,
                    p.resources.peakBytesResident / 1048576.0);
        ImGui::Text("  GPU payload %.1f MB / prepared CPU %.1f MB / pinned %.1f MB / over budget %.1f MB",
                    p.resources.gpuBytesResident / 1048576.0, p.resources.preparedBytes / 1048576.0,
                    p.resources.pinnedBytes / 1048576.0, p.resources.overBudgetBytes / 1048576.0);
        ImGui::Text("  hits %llu / loads %llu / uploads %llu / evictions %llu / cancelled %llu / stale %llu",
                    p.resources.hits, p.resources.misses, p.resources.uploads, p.resources.evictions,
                    p.resources.cancelled, p.resources.staleDiscarded);
        ImGui::Separator();
        const double utilization =
            p.jobs.workers > 0 && p.jobs.elapsedSeconds > 0.0
                ? 100.0 * p.jobs.workerBusySeconds / (p.jobs.workers * p.jobs.elapsedSeconds)
                : 0.0;
        ImGui::Text("Jobs: %u workers, %zu queued, %zu running", p.jobs.workers, p.jobs.queued,
                    p.jobs.running);
        ImGui::Text("  %llu completed / %llu failed / %llu cancelled of %llu submitted", p.jobs.completed,
                    p.jobs.failed, p.jobs.cancelled, p.jobs.submitted);
        ImGui::Text("  worker utilization since start: %.1f%%", utilization);
        ImGui::TextDisabled("Specialized measurements above retain their original observation cadence; "
                            "resource bytes are estimates, not driver VRAM.");
    }
    ImGui::End();
}
