#include "EditorCreationMenu.h"
#include "EditorPanels.h"
#include "imgui.h"

void DrawEditorCreationItems(const EditorPanelState& state, EditorRequests& requests, SceneObjectId parent) {
    const bool editing = state.mode == EditorMode::Edit;
    const auto item = [&](const char* label, const char* kind) {
        if (ImGui::MenuItem(label, nullptr, false, editing)) {
            requests.createKind = kind;
            requests.createPosition = state.cameraFocus;
            requests.createParent = parent;
        }
    };
    item("Empty object", "empty");
    item("First-person player (JS + motor)", "first-person-player");
    item("Third-person player (JS + motor)", "third-person-player");
    item("Pause menu + controls (JS + UI)", "pause-menu-example");
    item("Static box", "box");
    item("Static sphere", "sphere");
    item("Capsule", "capsule");
    item("Cylinder", "cylinder");
    item("Dynamic box", "dynamic-box");
    item("Dynamic sphere", "dynamic-sphere");
    item("Mesh", "mesh");
    item("Point light", "point-light");
    item("Spot light", "spot-light");
    item("Door (legacy gameplay)", "door");
    item("Gravity region", "gravity-region");
    item("Basic water", "basic-water");
    item("Player start (legacy gameplay)", "player-start");
    item("Render camera", "render-camera");
    item("Audio emitter", "audio-emitter");
    item("Audio listener", "audio-listener");
}

void DrawHierarchyCreationPopup(const EditorPanelState& state, EditorRequests& requests) {
    if (state.mode == EditorMode::Edit &&
        ImGui::BeginPopupContextWindow("Create object", ImGuiPopupFlags_MouseButtonRight |
                                                      ImGuiPopupFlags_NoOpenOverItems)) {
        ImGui::TextDisabled("Create object");
        ImGui::Separator();
        DrawEditorCreationItems(state, requests);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

SceneObjectId CreateObjectOfKind(EditorDocument& doc, const std::string& kind, const glm::vec3& position,
                                 const std::string& meshAssetId, const SceneAnimationComponent* animation,
                                 SceneObjectId parent) {
    if (parent != kInvalidSceneObjectId && !doc.GetScene().Find(parent)) return kInvalidSceneObjectId;
    doc.BeginEdit();
    Scene& scene = doc.GetScene();
    SceneObject& o = scene.CreateObject(kind == "empty" ? "Empty" : kind == "box" ? "Box"
                                        : kind == "sphere" ? "Sphere" : kind == "dynamic-box" ? "Dynamic box"
                                        : kind == "dynamic-sphere" ? "Dynamic sphere" : kind == "mesh" ? "Mesh"
                                        : kind == "point-light" ? "Point light" : kind == "spot-light" ? "Spot light"
                                        : kind == "door" ? "Door" : kind == "gravity-region" ? "Gravity region"
                                        : kind == "player-start" ? "Player start" : kind == "render-camera" ? "Render camera" : kind == "audio-emitter" ? "Audio emitter" : kind == "audio-listener" ? "Audio listener" : "Object");
    o.parent = parent;
    o.transform.position = parent == kInvalidSceneObjectId ? position : glm::vec3(0.0f);
    if (kind == "box" || kind == "dynamic-box") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Box;
        o.body = SceneBodyComponent{};
        o.body->shape = SceneShape::Box;
        o.body->motion = kind == "box" ? SceneBodyMotion::Static : SceneBodyMotion::Dynamic;
        o.body->mass = 5.0f;
        o.body->pickable = kind == "dynamic-box";
    } else if (kind == "sphere" || kind == "dynamic-sphere") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Sphere;
        o.body = SceneBodyComponent{};
        o.body->shape = SceneShape::Sphere;
        o.body->motion = kind == "sphere" ? SceneBodyMotion::Static : SceneBodyMotion::Dynamic;
        o.body->mass = 4.0f;
        o.body->pickable = kind == "dynamic-sphere";
    } else if (kind == "mesh") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Mesh;
        o.render->meshAsset = meshAssetId;
        o.render->color = glm::vec3(1.0f);
    } else if (kind == "point-light" || kind == "spot-light") {
        o.light = SceneLightComponent{};
        o.light->kind = kind == "point-light" ? SceneLightKind::Point : SceneLightKind::Spot;
        o.light->color = glm::vec3(3.0f, 2.8f, 2.4f);
    } else if (kind == "door") {
        o.render = SceneRenderComponent{};
        o.render->shape = SceneShape::Box;
        o.render->halfExtents = glm::vec3(1.0f, 1.0f, 0.1f);
        o.render->color = glm::vec3(0.55f, 0.38f, 0.22f);
        o.door = SceneDoorComponent{};
    } else if (kind == "basic-water") {
        o.name="Basic water";o.basicWater=BasicWaterSettings{};
    } else if (kind == "gravity-region") {
        o.gravity = SceneGravityComponent{};
        o.gravity->kind = SceneGravityKind::Uniform;
        o.gravity->regionShape = SceneRegionShape::Box;
        o.gravity->regionHalfExtents = glm::vec3(20.0f);
    } else if (kind == "render-camera") {
        o.renderCamera = SceneRenderCameraComponent{};
    } else if (kind == "audio-emitter") {
        o.audioEmitter=SceneAudioEmitterComponent{};
    } else if (kind == "audio-listener") {
        o.audioListener=SceneAudioListenerComponent{};
    } else if (kind == "player-start") {
        o.playerStart = ScenePlayerStartComponent{};
    }
    if(kind=="mesh"&&animation)o.animation=*animation;
    const SceneObjectId id = o.id;
    doc.CommitEdit();
    doc.Select(id);
    return id;
}
