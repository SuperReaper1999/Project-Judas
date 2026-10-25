#include "EditorCreationMenu.h"
#include "EditorPanels.h"
#include "imgui.h"

void DrawEditorCreationItems(const EditorPanelState& state, EditorRequests& requests, SceneObjectId parent) {
    struct CreationItem { const char* label; const char* kind; const char* description; };
    static constexpr CreationItem items[] = {
        {"Empty object", "empty", "A transform to organize children or attach your own components."},
        {"First-person player (JS + motor)", "first-person-player", "Controllable capsule with CharacterMotor, editable movement/camera script and V view toggle."},
        {"Third-person player (JS + motor)", "third-person-player", "The same script-owned player starting with a camera behind it."},
        {"Pause menu + controls (JS + UI)", "pause-menu-example", "Optional editable pause and settings UI, including project control rebinding."},
        {"Static box", "box", "Visible box with solid collision; useful for floors and walls."},
        {"Static sphere", "sphere", "Visible sphere with solid collision."},
        {"Capsule", "capsule", "Rounded mesh with convex collision; static by default, two metres tall."},
        {"Cylinder", "cylinder", "Cylinder mesh with convex collision; static by default, two metres tall."},
        {"Dynamic box", "dynamic-box", "A box that falls and responds to physical forces."},
        {"Dynamic sphere", "dynamic-sphere", "A ball that falls and responds to physical forces."},
        {"Mesh", "mesh", "An empty mesh renderer. Choose an imported mesh in the Render component."},
        {"Point light", "point-light", "Light emitted in all directions around a point."},
        {"Spot light", "spot-light", "A directional cone of light."},
        {"Door (legacy gameplay)", "door", "Only interactive with legacy gameplay enabled; modern doors use project scripts."},
        {"Gravity region", "gravity-region", "A bounded uniform gravity field; configure direction and strength in the inspector."},
        {"Basic water", "basic-water", "Inexpensive visible water and swimming; does not require advanced fluid simulation."},
        {"Player start (legacy gameplay)", "player-start", "A spawn marker for legacy gameplay, not a script-owned CharacterMotor player."},
        {"Render camera", "render-camera", "An additional rendered view; script-owned main cameras use world.setCamera."},
        {"Audio emitter", "audio-emitter", "A sound source. Choose an audio asset in the inspector."},
        {"Audio listener", "audio-listener", "An authored listening position for scene audio."}
    };
    // Only one creation popup is active at a time; each opening starts unfiltered.
    static ImGuiTextFilter filter;
    if (ImGui::IsWindowAppearing()) { filter.Clear(); ImGui::SetKeyboardFocusHere(); }
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputTextWithHint("##createSearch", "Find an object or capability...", filter.InputBuf, sizeof(filter.InputBuf))) filter.Build();
    ImGui::Separator();
    bool any = false;
    const bool editing = state.mode == EditorMode::Edit;
    for (const auto& item : items) {
        const std::string search = std::string(item.label) + " " + item.description;
        if (!filter.PassFilter(search.c_str())) continue;
        any = true;
        if (ImGui::MenuItem(item.label, nullptr, false, editing) && editing) {
            requests.createKind = item.kind;
            requests.createPosition = state.cameraFocus;
            requests.createParent = parent;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("%s", item.description);
        }
    }
    if (!any) ImGui::TextDisabled("No matching objects. Try a name such as light or player.");
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
