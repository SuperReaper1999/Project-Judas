#include "EditorPlaySession.h"
#include "InteractivePlay.h"
#include "EditorSceneViewport.h"
#include "EditorAssetPreview.h"
#include "EditorContext.h"
#include "EditorCreationMenu.h"
#include "EditorDocument.h"
#include "EditorPanels.h"
#include "EditorPrimitiveAssets.h"
#include "EditorSceneGeometry.h"
#include "EngineHost.h"
#include "PerformanceProfiler.h"
#include "Prefab.h"
#include "Project.h"
#include "Ragdoll.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "TerrainTools.h"
#include "TilemapEditor.h"
#include "VolumeTerrainTools.h"
#include "WorldCoordinates.h"
#include "WorldPresentation.h"
#include "WorldStreaming.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
using EditorSceneGeometry::BuildSelectionLines;
using EditorSceneGeometry::PickRadius;

namespace fs = std::filesystem;
EditorSceneViewport::EditorSceneViewport() = default;
EditorSceneViewport::~EditorSceneViewport() = default;
void EditorSceneViewport::ViewportRay(EditorContext &context, int x, int y, glm::vec3 &origin,
                                      glm::vec3 &direction) const {
    const auto &view = context.panels.workspace.viewport;
    camera.PixelRay(x - static_cast<int>(view.position.x), y - static_cast<int>(view.position.y),
                    std::max(1, static_cast<int>(view.size.x)), std::max(1, static_cast<int>(view.size.y)),
                    origin, direction);
}

void EditorSceneViewport::PickAtPixel(EditorContext &context, int x, int y) {
    glm::vec3 origin, direction;
    ViewportRay(context, x, y, origin, direction);
    std::string error;
    auto hit = picker.Pick(context.document.GetScene(), origin, direction, &context.host.Assets(),
                           &context.host.Resources(), &error);
    if (ImGui::GetIO().KeyAlt && error.empty()) {
        const auto hits = picker.PickAll(context.document.GetScene(), origin, direction,
                                         &context.host.Assets(), &context.host.Resources(), &error);
        for (std::size_t i = 0; i < hits.size(); ++i)
            if (hits[i].id == context.document.Selected()) {
                hit = hits[(i + 1) % hits.size()];
                break;
            }
    }
    if (!error.empty()) {
        context.panels.status = "Selection failed: " + error;
        return;
    }
    context.document.Select(hit.id, ImGui::GetIO().KeyCtrl);
}

void EditorSceneViewport::UpdateGizmo(EditorContext &context, bool allowInteraction) {
    gizmoLines.Clear();
    SceneObject *selected = context.document.SelectedObject();
    if (!selected) {
        if (drag.Active()) {
            drag = GizmoDrag{};
            context.document.CancelEdit();
        }
        hoverAxis = GizmoAxis::None;
        return;
    }
    const auto pointer = ImGui::GetIO().MousePos;
    const int mx = static_cast<int>(pointer.x), my = static_cast<int>(pointer.y);
    glm::vec3 rayOrigin, rayDirection;
    ViewportRay(context, mx, my, rayOrigin, rayDirection);
    Scene flattened;
    std::string error;
    if (!FlattenHierarchy(context.document.GetScene(), flattened, error))
        return;
    auto presented = flattened.Find(selected->id)->transform;
    if (context.document.Selection().size() > 1) {
        presented.position = {0, 0, 0};
        auto roots = context.document.SelectionRoots();
        for (auto id : roots)
            presented.position += flattened.Find(id)->transform.position;
        presented.position /= float(roots.size());
    }
    const glm::vec3 origin = presented.position;
    // Handles keep a roughly constant on-screen size.
    gizmoHandleLength = camera.Is2D() ? camera.OrthographicHeight() * 0.14f
                                      : std::max(0.2f, glm::length(origin - camera.Position()) * 0.14f);
    const bool snap = context.panels.gizmoSnap || ImGui::GetIO().KeyCtrl;

    if (drag.Active()) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            context.document.CancelEdit();
            drag = GizmoDrag{};
            return;
        }
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            auto value = UpdateGizmoDrag(drag, rayOrigin, rayDirection, snap);
            Scene baseline;
            std::string error;
            if (FlattenHierarchy(context.document.EditBaseline(), baseline, error)) {
                auto delta = value.position - drag.startTransform.position;
                auto rotation = glm::normalize(value.rotation * glm::inverse(drag.startTransform.rotation));
                auto scale = value.scale / drag.startTransform.scale;
                auto candidate = context.document.EditBaseline();
                for (auto id : context.document.SelectionRoots()) {
                    auto t = baseline.Find(id)->transform;
                    t.position = (context.panels.gizmoIndividualOrigins
                                      ? t.position
                                      : drag.origin + rotation * ((t.position - drag.origin) * scale)) +
                                 delta;
                    t.rotation = glm::normalize(rotation * t.rotation);
                    t.scale *= scale;
                    glm::mat4 matrix = glm::translate(glm::mat4(1), t.position) * glm::mat4_cast(t.rotation) *
                                       glm::scale(glm::mat4(1), t.scale);
                    auto *o = candidate.Find(id);
                    if (o->parent) {
                        auto p = baseline.Find(o->parent)->transform;
                        matrix =
                            glm::inverse(glm::translate(glm::mat4(1), p.position) *
                                         glm::mat4_cast(p.rotation) * glm::scale(glm::mat4(1), p.scale)) *
                            matrix;
                    }
                    JointTransform local;
                    if (!DecomposeRigidPose(matrix, local, error))
                        break;
                    o->transform = {local.translation, local.rotation, local.scale};
                }
                if (error.empty())
                    context.document.GetScene() = std::move(candidate);
                else
                    context.panels.status = error;
            }
        } else {
            // Release: one undo step for the whole drag.
            context.document.CommitEdit();
            drag = GizmoDrag{};
        }
    } else if (allowInteraction) {
        hoverAxis = PickGizmoAxis(context.panels.gizmoMode, rayOrigin, rayDirection, origin,
                                  glm::normalize(presented.rotation), context.panels.gizmoSpace,
                                  gizmoHandleLength, gizmoHandleLength * 0.12f);
        if (hoverAxis != GizmoAxis::None && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            GizmoDrag candidateDrag;
            if (BeginGizmoDrag(context.panels.gizmoMode, hoverAxis, rayOrigin, rayDirection, presented,
                               context.panels.gizmoSpace, gizmoHandleLength, candidateDrag)) {
                drag = candidateDrag;
                context.document.BeginEdit();
            }
        }
    } else {
        hoverAxis = GizmoAxis::None;
    }
    BuildGizmoLines(context.panels.gizmoMode, presented.position, glm::normalize(presented.rotation),
                    context.panels.gizmoSpace, gizmoHandleLength, drag.Active() ? drag.axis : hoverAxis,
                    gizmoLines);
}

void EditorSceneViewport::DrawEditOverlay(EditorContext &context, Renderer &renderer, const Scene &scene) {
    debugLines.Clear();
    BuildAuthoredDebugLines(scene, context.panels.debug, debugLines);
    if (context.panels.debug.navigation)
        debugLines.Append(context.panels.navigationPreview);
    debugLines.Append(context.panels.recipePreview);
    debugLines.Append(context.panels.liquidPreview);
    debugLines.Append(context.panels.deformablePreview);
    debugLines.Append(context.panels.collisionPreview);
    for (auto id : context.document.Selection())
        if (const SceneObject *selected = scene.Find(id))
            BuildSelectionLines(*selected, debugLines);
    if (const auto *selected = scene.Find(context.document.Selected())) {
        const auto *owner = selected->socket ? scene.Find(selected->socket->target) : selected;
        if (owner && owner->render) {
            auto asset = context.host.Resources().TryGetSkeletal(owner->render->meshAsset);
            if (asset) {
                auto global = PoseGlobalMatrices(asset->skeleton, asset->skeleton.rest);
                auto t = owner->transform;
                auto model = glm::translate(glm::mat4(1), t.position) * glm::mat4_cast(t.rotation) *
                             glm::scale(glm::mat4(1), t.scale);
                for (auto &matrix : global) {
                    JointTransform joint;
                    std::string error;
                    if (DecomposeRigidPose(model * matrix, joint, error))
                        debugLines.Axes(joint.translation, joint.rotation, .18f);
                }
            }
        }
    }
    // M70 authored preview uses the same CPU resolver as runtime. It never
    // mutates imported poses, and all lines remain renderer-owned debug draws.
    if (const auto *o = scene.Find(context.document.Selected()); o && o->animation && o->render) {
        auto asset = context.host.Resources().TryGetSkeletal(o->render->meshAsset);
        if (asset) {
            auto root = o->transform;
            auto model = glm::translate(glm::mat4(1), root.position) * glm::mat4_cast(root.rotation) *
                         glm::scale(glm::mat4(1), root.scale);
            auto pose = asset->skeleton.rest;
            if (o->animation->fullBodyIK) {
                const auto &k = *o->animation->fullBodyIK;
                FullBodyIKMapping mapping;
                FullBodyIKResult result;
                std::vector<FullBodyIKTarget> targets;
                std::string error;
                if (PrepareFullBodyIK(asset->skeleton, k, mapping, error) &&
                    FullBodyIKTargetsToModel(k.targets, glm::dvec3(root.position), root.rotation, root.scale,
                                             targets, error) &&
                    SolveFullBodyIK(asset->skeleton, pose, mapping, targets, result, error)) {
                    pose = result.pose;
                    auto global = PoseGlobalMatrices(asset->skeleton, pose);
                    for (const auto &target : targets) {
                        auto chain = std::find_if(mapping.chains.begin(), mapping.chains.end(),
                                                  [&](const auto &c) { return c.id == target.chain; });
                        if (chain == mapping.chains.end())
                            continue;
                        JointTransform actual;
                        std::string diagnostic;
                        if (!DecomposeRigidPose(model * global[chain->end], actual, diagnostic))
                            continue;
                        auto desired = glm::vec3(model * glm::vec4(glm::vec3(target.position), 1));
                        auto point = actual.translation + actual.rotation * (actual.scale * target.offset);
                        debugLines.Axes(desired, root.rotation * target.orientation, .13f);
                        debugLines.Axes(point, actual.rotation * target.frame, .1f);
                        debugLines.Line(desired, point, {1, .3f, .1f});
                    }
                    auto reference = PoseGlobalMatrices(asset->skeleton, asset->skeleton.rest);
                    auto center = glm::vec3(
                        model *
                        glm::vec4(glm::vec3(reference[mapping.root][3]) + (k.rootMin + k.rootMax) * .5f, 1));
                    debugLines.Box(center, root.rotation, (k.rootMax - k.rootMin) * .5f * root.scale,
                                   {.2f, .8f, 1});
                    for (const auto &limit : k.limits) {
                        int joint = FindSkeletonJoint(asset->skeleton, limit.joint);
                        JointTransform frame;
                        std::string diagnostic;
                        if (joint >= 0 && DecomposeRigidPose(model * global[joint], frame, diagnostic))
                            debugLines.Axes(frame.translation, frame.rotation * limit.frame, .15f);
                    }
                }
            }
            if (o->ragdoll && o->ragdoll->physicalAnimation) {
                auto global = PoseGlobalMatrices(asset->skeleton, pose);
                for (const auto &region : o->ragdoll->physicalAnimation->regions)
                    if (region.enabled)
                        for (const auto &key : region.joints) {
                            int joint = FindSkeletonJoint(asset->skeleton, key);
                            if (joint >= 0) {
                                auto point = glm::vec3(model * global[joint][3]);
                                int parent = asset->skeleton.parents[joint];
                                if (parent >= 0)
                                    debugLines.Line(point, glm::vec3(model * global[parent][3]),
                                                    {1, .5f, .1f});
                            }
                        }
            }
        }
    }
    if (context.panels.skeletonFitPreview &&
        context.panels.skeletonFitGeneration == context.document.Generation()) {
        if (const auto *o = scene.Find(context.panels.skeletonFitOwner); o && o->render) {
            if (auto asset = context.host.Resources().TryGetSkeletal(o->render->meshAsset)) {
                auto global = PoseGlobalMatrices(asset->skeleton, asset->skeleton.rest);
                auto model = glm::translate(glm::mat4(1), o->transform.position) *
                             glm::mat4_cast(o->transform.rotation) *
                             glm::scale(glm::mat4(1), o->transform.scale);
                for (const auto &bone : context.panels.skeletonFitPreview->bones) {
                    int index = FindSkeletonJoint(asset->skeleton, bone.joint);
                    if (index < 0)
                        continue;
                    JointTransform joint;
                    std::string error;
                    if (!DecomposeRigidPose(model * global[size_t(index)], joint, error))
                        continue;
                    auto center = joint.translation + joint.rotation * (joint.scale * bone.offset);
                    auto orientation = glm::normalize(joint.rotation * bone.orientation);
                    if (bone.shape == RagdollShape::Sphere)
                        debugLines.Sphere(center, bone.radius, {1, .5f, .1f});
                    else
                        debugLines.Box(center, orientation, bone.halfExtents * joint.scale, {1, .5f, .1f});
                }
            }
        }
    }
    renderer.DrawDebugLines(debugLines.Lines(), /*depthTest=*/true);
    renderer.DrawDebugLines(gizmoLines.Lines(), /*depthTest=*/false);
}

void EditorSceneViewport::CancelCameraNavigation(EditorContext &context) {
    gesture.Cancel();
    context.host.GetWindow().SetMouseCaptured(false);
}

void EditorSceneViewport::UpdateCameraNavigation(EditorContext &context,
                                                 const EditorCameraGestureInput &input, int mouseDeltaX,
                                                 int mouseDeltaY, float deltaSeconds, bool fast) {
    Window &window = context.host.GetWindow();
    const auto previous = gesture.Active();
    gesture.Update(input);
    const auto active = gesture.Active();
    const bool captured = active != EditorCameraGesture::None;
    if (window.IsMouseCaptured() != captured)
        window.SetMouseCaptured(captured);
    // Starting capture may warp the cursor. Consume only subsequent drag
    // samples so the initial press does not jump the view.
    if (active != previous)
        mouseDeltaX = mouseDeltaY = 0;
    if (active == EditorCameraGesture::Pan)
        camera.Pan(mouseDeltaX, mouseDeltaY,
                   std::max(1, static_cast<int>(context.panels.workspace.viewport.size.y)));
    camera.Update(window, deltaSeconds, active == EditorCameraGesture::Look, mouseDeltaX, mouseDeltaY, fast);
    // Keep the existing 8 m object-placement plane; the camera's separate
    // focus depth only determines the pan scale after framing a selection.
    context.panels.cameraFocus = camera.Is2D() ? camera.Focus() : camera.Position() + camera.Forward() * 8.0f;
}

void EditorSceneViewport::FrameEditMode(EditorContext &context, float deltaSeconds,
                                        TerrainEditor *terrain,
                                        VolumeTerrainEditor *volumeTerrain, TilemapEditor *tilemapEditor) {
    JUDAS_PROFILE_SCOPE("Editor viewport");
    Window &window = context.host.GetWindow();
    Renderer &renderer = context.host.GetRenderer();
    const ImGuiIO &io = ImGui::GetIO();
    const bool insideViewport = context.panels.workspace.viewport.Contains({io.MousePos.x, io.MousePos.y});
    const bool pointerAvailable=context.panels.dockingEnabled?context.panels.sceneViewHovered:!io.WantCaptureMouse;
    const bool keyboardAvailable=context.panels.dockingEnabled?!io.WantTextInput:!io.WantCaptureKeyboard;

    if (camera.Is2D() != context.panels.view2D) {
        CancelCameraNavigation(context);
        camera.Set2D(context.panels.view2D);
    }
    EditorCameraGestureInput navigation;
    navigation.focused = focused;
    navigation.canStart = insideViewport && pointerAvailable && keyboardAvailable && !drag.Active();
    navigation.cancel = !keyboardAvailable || io.WantTextInput || ImGui::IsKeyPressed(ImGuiKey_Escape);
    navigation.rightDown = !camera.Is2D() && io.MouseDown[ImGuiMouseButton_Right];
    navigation.middleDown = io.MouseDown[ImGuiMouseButton_Middle];
    navigation.leftDown = io.MouseDown[ImGuiMouseButton_Left];
    int dx = 0, dy = 0;
    window.GetMouseDelta(dx, dy);
    // Pan consumes viewport SDL motion directly. Game projects may rename
    // look axes; their logical input map does not redefine editor panning.
    if (gesture.Active() == EditorCameraGesture::Pan) {
        dx = mouseDeltaX;
        dy = mouseDeltaY;
    }
    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    const bool fast = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    UpdateCameraNavigation(context, navigation, dx, dy, deltaSeconds, fast);
    if (EditorCameraCanDolly(navigation, gesture.Active(), true)) {
        camera.Dolly(io.MouseWheel);
        context.panels.cameraFocus =
            camera.Is2D() ? camera.Focus() : camera.Position() + camera.Forward() * 8.0f;
    }

    // Gizmo first: a click on a handle is a drag, not a pick. The viewport
    // owns the mouse only when no panel does and no camera drag owns it.
    const bool viewportOwnsMouse = insideViewport && pointerAvailable &&
                                   gesture.Active() == EditorCameraGesture::None && !navigation.middleDown &&
                                   !navigation.rightDown;
    bool brushOwns = false;
    if (terrain && terrain->loaded) {
        terrain->Prepare(renderer, context.panels);
        picker.SetDraftMesh(terrain->draft.source.meshId, terrain->preview);
        Scene flat;
        std::string error;
        if (FlattenHierarchy(context.document.GetScene(), flat, error)) {
            glm::vec3 ro, rd;
            ViewportRay(context, int(io.MousePos.x), int(io.MousePos.y), ro, rd);
            brushOwns = terrain->Stroke(flat, context.document.Selected(), ro, rd,
                                        viewportOwnsMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left),
                                        viewportOwnsMouse && io.MouseDown[ImGuiMouseButton_Left],
                                        ImGui::IsKeyPressed(ImGuiKey_Escape),
                                        focused && !terrain->Busy(context.panels), context.panels.status);
        }
    }
    if (volumeTerrain && volumeTerrain->loaded) {
        volumeTerrain->Prepare(renderer, context.panels);
        if (volumeTerrain->editing) {
            if (terrain)
                terrain->editing = false;
            picker.SetDraftMesh(volumeTerrain->draft.source.meshId, volumeTerrain->preview);
        }
        Scene flat;
        std::string error;
        if (FlattenHierarchy(context.document.GetScene(), flat, error)) {
            glm::vec3 ro, rd;
            ViewportRay(context, int(io.MousePos.x), int(io.MousePos.y), ro, rd);
            brushOwns |=
                volumeTerrain->Stroke(flat, context.document.Selected(), ro, rd,
                                      viewportOwnsMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left),
                                      viewportOwnsMouse && io.MouseDown[ImGuiMouseButton_Left],
                                      ImGui::IsKeyPressed(ImGuiKey_Escape),
                                      focused && !volumeTerrain->Busy(context.panels), context.panels.status);
        }
    }
    if (tilemapEditor && (tilemapEditor->painting || tilemapEditor->StrokeActive())) {
        glm::vec3 origin, direction;
        ViewportRay(context, int(io.MousePos.x), int(io.MousePos.y), origin, direction);
        brushOwns |= tilemapEditor->Stroke(
            context.document, context.host.Assets(), origin, direction, viewportOwnsMouse,
            ImGui::IsMouseClicked(ImGuiMouseButton_Left), ImGui::IsMouseClicked(ImGuiMouseButton_Right),
            io.MouseDown[ImGuiMouseButton_Left], io.MouseDown[ImGuiMouseButton_Right],
            ImGui::IsKeyPressed(ImGuiKey_Escape), context.panels.status);
    }
    if (tilemapEditor && tilemapEditor->painting) {
        gizmoLines.Clear();
        hoverAxis = GizmoAxis::None;
    } else
        UpdateGizmo(context, viewportOwnsMouse && !brushOwns);
    if (viewportOwnsMouse && !brushOwns && !drag.Active() && hoverAxis == GizmoAxis::None &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        PickAtPixel(context, static_cast<int>(io.MousePos.x), static_cast<int>(io.MousePos.y));
    }

    // The Scene image accepts an actual ImGui drag-delivery. The viewport
    // owner applies the normal document operation after UI construction.
    if(pendingDropDelivered){
        pendingDropDelivered=false;
            const AssetRecord *record = context.host.Assets().Find(pendingDropAsset);
            if (record && (record->type == AssetType::Mesh || record->type == AssetType::Texture)) {
                const int mx = static_cast<int>(pendingDropPosition.x), my = static_cast<int>(pendingDropPosition.y);
                glm::vec3 rayOrigin, rayDirection;
                ViewportRay(context, mx, my, rayOrigin, rayDirection);
                glm::vec3 hit = context.panels.cameraFocus;
                RayPlaneIntersection(rayOrigin, rayDirection, context.panels.cameraFocus, -camera.Forward(),
                                     hit);
                std::string error;
                if (record->type == AssetType::Texture) {
                    if (!CreateEditorSpriteFromImage(context.document, context.host.Assets(),
                                                     pendingDropAsset, hit, kInvalidSceneObjectId, error)) {
                        context.panels.status = error;
                    } else
                        context.panels.status = "Placed sprite: " + record->relativePath;
                } else {
                    CreateObjectOfKind(context.document, "mesh", hit, pendingDropAsset);
                    context.panels.status = "Placed " + record->relativePath;
                }
            } else {
                context.panels.status = "Drop an image to create a sprite, or a mesh to create a model";
            }
        pendingDropAsset.clear();
    }

    Scene scene;
    std::string hierarchyError;
    if (!FlattenHierarchy(context.document.GetScene(), scene, hierarchyError)) {
        context.panels.status = hierarchyError;
        return;
    }

    const auto &viewport = context.panels.workspace.viewport;
    const int width = std::max(1, static_cast<int>(viewport.size.x));
    const int height = std::max(1, static_cast<int>(viewport.size.y));
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    renderer.SetLighting(glm::normalize(scene.Settings().sunDirection),
                         scene.Settings().sunEnabled
                             ? scene.Settings().sunColor * scene.Settings().sunIntensity
                             : glm::vec3(0),
                         scene.Settings().ambientColor);
    const auto &appearance = scene.Settings();
    context.host.Resources().RequestEnvironment(appearance.environmentAsset);
    renderer.SetSceneAppearance(appearance.linearRendering, appearance.exposure,
                                context.host.Resources().TryGetEnvironment(appearance.environmentAsset),
                                appearance.environmentIntensity, appearance.environmentRotation,
                                appearance.environmentBackground, appearance.backgroundColor);
    RendererProfileScope editorCameraGPU(renderer, "Editor camera");
    // The authoring scene has its own render target and aspect ratio. Panels
    // never obscure or steal pixels from the scene's picking projection.
    std::string targetError;
    if (!renderer.ResizeRenderTarget(target, width, height, targetError, true) ||
        !renderer.BeginRenderTarget(target)) {
        context.panels.status = "Scene view unavailable: " + targetError;
        renderer.BeginFrame(window.Width(), window.Height());
        renderer.EndFrame();
        return;
    }
    renderer.SetCamera(camera.ViewMatrix(), camera.ProjectionMatrix(aspect));
    {
        const auto eye = scene.Settings().worldOrigin + glm::dvec3(glm::inverse(renderer.ViewMatrix())[3]);
        auto weather = SampleAuthoredWeather(scene, eye);
        if (weather.active)
            renderer.SetWeather(weather, eye, 1);
        else
            renderer.ClearWeather();
        renderer.SetWeatherShelter({});
    }

    // Clear runtime-view shadow history before terrain and authored previews.
    // Edit mode has no matching shadow pass for this camera.
    for (int slot = 0; slot < kShadowMapCount; ++slot)
        renderer.InvalidateShadowSlot(slot);
    renderer.SetDynamicLights(BuildAuthoredLights(scene));
    if (terrain)
        terrain->Draw(renderer, scene, context.host.Resources());
    if (volumeTerrain)
        volumeTerrain->Draw(renderer, scene, context.host.Resources());
    {
        std::set<SceneObjectId> keep;
        for (const auto &object : scene.Objects())
            if (object.planet) {
                keep.insert(object.id);
                auto descriptor = EncodePlanetSettings(*object.planet);
                auto &p = planetPreview[object.id];
                if (p.first != descriptor) {
                    p.second.reset();
                    p.first = descriptor;
                    p.second = std::make_unique<PlanetTiles>(*object.planet);
                }
                const auto eye =
                    glm::dvec3(glm::inverse(renderer.ViewMatrix())[3]) + scene.Settings().worldOrigin;
                p.second->Prepare(renderer, eye, scene.Settings().worldOrigin,
                                  2 * std::atan(1. / double(renderer.ProjectionMatrix()[1][1])), 900,
                                  1. / 60);
                p.second->Draw(renderer, scene.Settings().worldOrigin);
            }
        for (auto it = planetPreview.begin(); it != planetPreview.end();)
            if (!keep.count(it->first))
                it = planetPreview.erase(it);
            else
                ++it;
    }
    DrawAuthoredScene(renderer, scene, context.host.Resources(), false);
    if (context.panels.worldPreview && !context.project.Settings().worldManifest.empty()) {
        auto key = context.project.ProjectFile() + context.document.Path() +
                   std::to_string(context.panels.worldPreviewRevision);
        if (key != regionPreviewKey) {
            regionPreviewKey = key;
            regionPreview.clear();
            auto *asset = context.host.Assets().Find(context.project.Settings().worldManifest);
            WorldManifest manifest;
            std::string error;
            if (asset && LoadWorldManifest(asset->path, manifest, error)) {
                glm::dvec3 origin = scene.Settings().worldOrigin;
                glm::quat orientation{1, 0, 0, 0};
                for (auto &[_, r] : manifest.regions)
                    if (context.project.Resolve(r.scene) == context.document.Path()) {
                        origin = r.origin;
                        orientation = r.rotation;
                    }
                for (auto &[_, r] : manifest.regions) {
                    if (context.project.Resolve(r.scene) == context.document.Path())
                        continue;
                    PreparedWorldRegion prepared;
                    Scene flat;
                    if (!PrepareWorldRegion(r, context.project, context.host.Assets(), prepared, error) ||
                        !FlattenHierarchy(prepared.source, flat, error)) {
                        context.panels.status = "Preview: " + error;
                        continue;
                    }
                    auto rotation = glm::inverse(orientation) * r.rotation;
                    auto translation = glm::vec3(glm::inverse(glm::dquat(orientation)) * (r.origin - origin));
                    for (auto &o : flat.Objects()) {
                        o.transform.position = translation + rotation * o.transform.position;
                        o.transform.rotation = rotation * o.transform.rotation;
                    }
                    regionPreview.push_back(std::move(flat));
                }
            }
        }
        for (auto &region : regionPreview)
            DrawAuthoredScene(renderer, region, context.host.Resources(), false);
    } else {
        regionPreview.clear();
        regionPreviewKey.clear();
    }
    if (context.project.Settings().basicWater) {
        DrawAuthoredWaterSurfaces(renderer, scene, context.host.Resources());
        for (const auto &region : regionPreview)
            DrawAuthoredWaterSurfaces(renderer, region, context.host.Resources());
    }
    renderer.DrawWeather();
    DrawEditOverlay(context, renderer, scene);
    if (terrain)
        renderer.DrawDebugLines(terrain->footprint.Lines(), true);
    if (volumeTerrain)
        renderer.DrawDebugLines(volumeTerrain->footprint.Lines(), true);
    if (tilemapEditor && tilemapEditor->painting)
        renderer.DrawDebugLines(tilemapEditor->overlay.Lines(), false);
    renderer.EndFrame();
    renderer.EndRenderTarget();
    renderer.ClearWeather();
    renderer.BeginFrame(window.Width(), window.Height());
    renderer.EndFrame();

}

// A live Scene camera is a read-only presentation of the same authoritative
// world. It never advances simulation, scripts, audio or save publication.
void EditorSceneViewport::RenderLive(EditorContext& context,const EditorPlaySession& session,float dt){
    if(!session.world)return;
    auto& renderer=context.host.GetRenderer();
    const auto size=renderer.RenderTargetSize(target);
    if(size.x<1||size.y<1||!renderer.BeginRenderTarget(target))return;
    const auto& io = ImGui::GetIO();
    auto* sceneWindow = ImGui::FindWindowByName("Scene###Judas.Scene");
    auto* navigationWindow = ImGui::GetCurrentContext()->NavWindow;
    EditorCameraGestureInput navigation;
    navigation.focused = !io.AppFocusLost && sceneWindow && navigationWindow &&
        (navigationWindow == sceneWindow ||
         ImGui::IsWindowChildOf(navigationWindow, sceneWindow, false, false));
    navigation.canStart = context.panels.sceneViewHovered && !io.WantTextInput &&
        !ImGui::IsAnyItemActive() && !drag.Active();
    navigation.cancel = io.WantTextInput || ImGui::IsKeyPressed(ImGuiKey_Escape);
    navigation.rightDown = !camera.Is2D() && ImGui::IsMouseDown(ImGuiMouseButton_Right);
    navigation.middleDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    navigation.leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const auto previous = gesture.Active();
    gesture.Update(navigation);
    const auto active = gesture.Active();
    const int dx = active == previous ? int(io.MouseDelta.x) : 0;
    const int dy = active == previous ? int(io.MouseDelta.y) : 0;
    if (active == EditorCameraGesture::Pan) camera.Pan(dx, dy, size.y);
    glm::vec3 movement{0};
    if (navigation.focused && context.panels.sceneViewHovered && !io.WantTextInput) {
        movement = {
            float(ImGui::IsKeyDown(ImGuiKey_D)) - float(ImGui::IsKeyDown(ImGuiKey_A)),
            float(ImGui::IsKeyDown(ImGuiKey_E)) - float(ImGui::IsKeyDown(ImGuiKey_Q)),
            float(ImGui::IsKeyDown(ImGuiKey_W)) - float(ImGui::IsKeyDown(ImGuiKey_S))};
    }
    // Observe editor physical keys without unclaiming or rerouting gameplay.
    // A press begun over another panel cannot turn into navigation while held.
    camera.UpdateLocal(dt, active == EditorCameraGesture::Look, dx, dy, io.KeyShift, movement);
    if (EditorCameraCanDolly(navigation, active, true)) camera.Dolly(io.MouseWheel);
    UpdateFluidSurface(renderer,*session.world,session.lastAlpha);
    RenderWorldFrame(renderer,size.x,size.y,*session.world,&session.play->Session(),camera.ViewMatrix(),
                     camera.ProjectionMatrix(float(size.x)/size.y),camera.Position(),session.lastAlpha,dt,WorldRenderPurpose::Observer);
    renderer.EndRenderTarget();
}

void EditorSceneViewport::Destroy(Renderer& renderer){
 if(target.IsValid())renderer.DestroyRenderTarget(target);
 target={};planetPreview.clear();regionPreview.clear();
}
