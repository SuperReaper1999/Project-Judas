#include "ParticleEmitterEditor.h"
#include "ComponentEditors.h"
#include "EditorPanels.h"
#include "EditorWidgets.h"
#include "AssetDatabase.h"
#include "Project.h"

#include <algorithm>

namespace {
void UnsignedField(EditorDocument &document, const char *label, unsigned &value,
                   unsigned minimum, unsigned maximum) {
    int candidate = int(value);
    if (ImGui::DragInt(label, &candidate, 1, int(minimum), int(maximum))) {
        document.BeginEdit();
        value = unsigned(std::clamp(candidate, int(minimum), int(maximum)));
        document.CommitEdit();
    }
}
void Rgba(EditorDocument &document, const char *label, glm::vec4 &value) {
    auto candidate = value;
    if (ImGui::ColorEdit4(label, &candidate.x)) {
        document.BeginEdit();
        value = candidate;
        document.CommitEdit();
    }
}
void NonnegativeVector(EditorDocument &document, const char *label, glm::vec3 &value, float speed) {
    auto candidate = value;
    if (ImGui::DragFloat3(label, &candidate.x, speed, 0, 1000000)) {
        document.BeginEdit();
        value = glm::max(candidate, glm::vec3(0));
        document.CommitEdit();
    }
}
} // namespace

void DrawParticleEmitter(EditorDocument &document, SceneObject &object, EditorPanelState &state) {
    auto &settings = *object.particleEmitter;
    Checkbox(document, "Enabled", settings.enabled);
    ImGui::TextDisabled("Visual effects; no rigid body is created for each particle.");
    if (settings.backend == ParticleBackend::Gpu &&
        (settings.localSpace || settings.response == ParticleResponse::Stick ||
         settings.impacts == ParticleImpactDelivery::Reliable))
        ImGui::TextWrapped("This combination needs CPU simulation. Select CPU or Automatic; required GPU will report unsupported work and will not advance.");
    if (!settings.collision && settings.impacts != ParticleImpactDelivery::Off)
        ImGui::TextWrapped("Enable collision response to produce world-impact events.");
    if (InspectorDisclosure("Emission and appearance", true)) {
        Checkbox(document, "Continuous / loop", settings.loop);
        DragScalar(document, "Rate / second", settings.rate, 1, 0, 100000);
        DragScalar(document, "Lifetime seconds", settings.lifetime, .05f, .01f, 3600);
        int capacity = settings.maxParticles;
        if (ImGui::DragInt("Capacity", &capacity, 1, 1, 100000)) {
            document.BeginEdit();
            settings.maxParticles = std::clamp(capacity, 1, 100000);
            settings.burst = std::min(settings.burst, settings.maxParticles);
            document.CommitEdit();
        }
        DragInt(document, "Startup burst", settings.burst, 0, settings.maxParticles);
        DragScalar(document, "Start size", settings.size, .01f, 0, 1000);
        DragScalar(document, "End size", settings.endSize, .01f, 0, 1000);
        Rgba(document, "Start colour / alpha", settings.color);
        Rgba(document, "End colour / alpha", settings.endColor);
        AssetField(document, "Particle texture", settings.textureAsset, AssetType::Texture, true, state);
    }
    if (InspectorDisclosure("Motion and simulation", false)) {
        const char *backends[] = {"CPU (portable)", "Automatic", "GPU (requires support)"};
        Combo(document, "Simulation backend", settings.backend, backends, 3);
        Checkbox(document, "Local space", settings.localSpace);
        Checkbox(document, "Sample Judas gravity", settings.useGravity);
        Checkbox(document, "Sample weather wind", settings.useWind);
        NonnegativeVector(document, "Position spread", settings.spread, .01f);
        DragVec3(document, "Initial velocity", settings.velocity, .05f);
        NonnegativeVector(document, "Velocity variation", settings.velocityVariation, .05f);
        DragVec3(document, "Acceleration", settings.acceleration, .05f);
        unsigned seed = settings.seed;
        if (ImGui::InputScalar("Seed", ImGuiDataType_U32, &seed)) {
            document.BeginEdit();
            settings.seed = seed;
            document.CommitEdit();
        }
        ImGui::TextWrapped("Disabling preserves the existing behaviour: live particles age, while emission and drawing stop. Changing local space clears the live pool. Backend availability is reported at runtime.");
    }
    if (InspectorDisclosure("Collision response", false)) {
        Checkbox(document, "Collide with the physical world", settings.collision);
        const char *responses[] = {"Bounce", "Slide", "Stick to surface", "Remove on impact"};
        Combo(document, "Response", settings.response, responses, 4);
        DragScalar(document, "Collision radius (metres)", settings.collisionRadius, .005f, .000001f, 1000);
        DragScalar(document, "Restitution", settings.restitution, .01f, 0, 1);
        DragScalar(document, "Surface friction", settings.friction, .01f, 0, 1);
        UnsignedField(document, "Contacts / particle / step", settings.maxContacts, 1, 32);
        if (state.project)
            DrawCategoryMask(document, "Collision layers", settings.collisionLayers,
                             state.project->Settings().classification.collision);
        ImGui::TextWrapped("Particle response is optional and bounded. It does not apply a rigid-body impulse or change the Advanced Fluid setting.");
    }
    if (InspectorDisclosure("Aggregated impact events", false)) {
        const char *delivery[] = {"Off", "Cosmetic", "Reliable (CPU)"};
        Combo(document, "Delivery", settings.impacts, delivery, 3);
        DragScalar(document, "Aggregation cell size (metres)", settings.impactCellSize, .05f, .000001f, 10000);
        DragScalar(document, "Weight / particle", settings.impactWeight, .05f, 0, 1000000);
        UnsignedField(document, "Maximum impact cells", settings.maxImpactCells, 1, 65536);
        ImGui::TextWrapped("Events are grouped rather than delivered once for every particle. JavaScript decides damage, sounds and gameplay. Runtime diagnostics report any exhausted budget or unavailable backend.");
    }
}
