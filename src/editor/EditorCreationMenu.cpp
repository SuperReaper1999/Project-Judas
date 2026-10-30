#include "Project.h"
#include "EnginePaths.h"
#include <stdexcept>
#include <filesystem>
#include "EditorCreationMenu.h"
#include "EditorPanels.h"
#include "imgui.h"
#include "AssetDatabase.h"
#include "TextureLoader.h"

void DrawEditorCreationItems(const EditorPanelState& state, EditorRequests& requests, SceneObjectId parent) {
    struct CreationItem { const char* label; const char* kind; const char* description; };
    static constexpr CreationItem items[] = {
        {"Empty object", "empty", "A transform to organize children or attach your own components."},
        {"2D box (static)","planar-box","Native XY-plane collision and an editable sprite."},
        {"2D box (dynamic)","planar-dynamic-box","Native lamina mass, gravity, contacts and CCD."},
        {"2D circle (dynamic)","planar-circle","Exact circular contact, not a polygonal thin sphere."},
        {"2D capsule (dynamic)","planar-capsule","Native rounded 2D contact; set rotation lock in Body."},
        {"2D one-way platform","planar-one-way","Static straight segment; jump through from below and land on its upper side."},
        {"2D grid RPG player","planar-grid-player","Editable cardinal grid-step controller with native kinematic collision."},
        {"2D terrain chain","planar-chain","Static open, two-sided edges. Edit vertices in Body."},
        {"2D platformer player","planar-player","Editable project script, native capsule collision and jumping. Requires scene gravity."},
        {"2D top-down player","planar-topdown-player","Editable project script with native 2D collision. Use a scene without gravity."},
        {"Sprite (2D image)","sprite","A world-space image with pivot, flips and sorting; no hidden collision."},
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
                                 SceneObjectId parent,const glm::vec2* spriteSize) {
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
    } else if(kind.rfind("planar-",0)==0) {
        o.name=kind=="planar-chain"?"2D terrain chain":"2D physical object";
        o.body=SceneBodyComponent{};o.body->shape=SceneShape::Planar;
        o.body->motion=(kind=="planar-box"||kind=="planar-chain"||kind=="planar-one-way")?SceneBodyMotion::Static:SceneBodyMotion::Dynamic;
        o.body->managed=false;
        o.body->planar=kind=="planar-circle"?PlanarCircle(.5):kind=="planar-capsule"?PlanarCapsule(.3,.4):
            kind=="planar-chain"?PlanarChain({{-2,0},{0,0},{2,1}}):kind=="planar-one-way"?PlanarChain({{-2,0},{2,0}},true):PlanarBox({.5,.5});
        if(kind!="planar-chain") {
            o.render=SceneRenderComponent{};o.render->shape=SceneShape::Sprite;
            o.render->color={.3f,.7f,.8f};
            if(kind=="planar-one-way"){o.name="2D one-way platform";o.render->sprite.size={4,.12f};o.render->sprite.pivot={.5f,0};}
            if(kind=="planar-circle"||kind=="planar-capsule")o.render->textureAsset=meshAssetId;
            if(kind=="planar-capsule")o.render->sprite.size={.6f,1.4f};
        }
    } else if(kind=="sprite") {
        o.name="Sprite";o.render=SceneRenderComponent{};
        o.render->shape=SceneShape::Sprite;o.render->textureAsset=meshAssetId;o.render->color=glm::vec3(1);
        if(spriteSize)o.render->sprite.size=*spriteSize;
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

SceneObjectId CreateEditorSpriteFromImage(EditorDocument& doc,const AssetDatabase& assets,const std::string& image,
                                         glm::vec3 position,SceneObjectId parent,std::string& error) {
    const auto* record=assets.Find(image);TextureData pixels;
    if(!record||record->missing||record->type!=AssetType::Texture){error="Choose a tracked image asset";return 0;}
    if(!LoadTextureFromFile(record->path,pixels,error))return 0;
    const glm::vec2 size(float(pixels.width)/32,float(pixels.height)/32);
    const auto id=CreateObjectOfKind(doc,"sprite",position,image,nullptr,parent,&size);
    error=id?"":"Sprite parent no longer exists";return id;
}

// Matching visuals are ordinary project-owned assets, independent of the native
// collider. Reuse the image after its first import; never replace user artwork.
SceneObjectId CreateEditorPlanarPrimitive(EditorDocument& doc,const Project& project,AssetDatabase& assets,
    const std::string& kind,glm::vec3 position,SceneObjectId parent,std::string& error) {
    try {
        if(kind!="planar-circle"&&kind!="planar-capsule")throw std::runtime_error("Unknown rounded 2D primitive");
        if(!project.IsLoaded())throw std::runtime_error("Open a project before creating 2D image assets");
        if(parent&&!doc.GetScene().Find(parent))throw std::runtime_error("The chosen parent no longer exists");
        const auto root=std::filesystem::canonical(std::filesystem::u8path(project.AssetsDir()));
        if(root!=std::filesystem::canonical(std::filesystem::u8path(assets.AssetsDir())))throw std::runtime_error("Asset database belongs to another project");
        auto directory=root;
        for(const auto* part:{"Generated","Planar"}){directory/=part;
            if(std::filesystem::is_symlink(std::filesystem::symlink_status(directory)))throw std::runtime_error("Generated asset folders must not be symbolic links");
            std::filesystem::create_directories(directory);}
        const std::string file=kind=="planar-circle"?"circle.png":"capsule.png";
        const auto destination=directory/file;
        if(!std::filesystem::exists(destination))std::filesystem::copy_file(ResolveEngineDataPath("docs/judasjs/examples/2d/"+file),destination,std::filesystem::copy_options::none);
        AssetRecord image;
        const auto relative=destination.lexically_relative(std::filesystem::u8path(project.RootDir())).generic_u8string();
        if(const auto* existing=assets.FindByRelativePath(relative)) {
            if(existing->missing||existing->type!=AssetType::Texture)throw std::runtime_error("The generated sprite asset is missing or has the wrong type");
            image=*existing;
        } else if(!assets.Track(destination.u8string(),image,error))return 0;
        const auto id=CreateObjectOfKind(doc,kind,position,image.id,nullptr,parent);
        if(!id){error="Creation failed: parent no longer exists";return 0;}
        error.clear();return id;
    }catch(const std::exception& e){error=e.what();return 0;}
}
