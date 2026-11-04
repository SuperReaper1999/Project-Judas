#include "EditorAssetPreview.h"
#include "AssetBrowserPanel.h"
#include "AssetPublicationWorkflow.h"
#include "EditorContext.h"
#include "EditorDocument.h"
#include "EditorPanels.h"
#include "EngineHost.h"
#include "Localization.h"
#include "ModelCook.h"
#include "PerformanceProfiler.h"
#include "Project.h"
#include "RetargetPanel.h"
#include "RuntimeUI.h"
#include "WorldPresentation.h"
#include "imgui.h"
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

namespace fs = std::filesystem;
EditorAssetPreview::EditorAssetPreview() = default;
EditorAssetPreview::~EditorAssetPreview() = default;
void EditorAssetPreview::DrawModelImportPreview(EditorContext &context, float deltaSeconds) {
    JUDAS_PROFILE_SCOPE("Model import preview");
    if ((!context.panels.showAssetBrowser && !context.panels.showAnimationTimeline) ||
        context.panels.mode != EditorMode::Edit || !context.panels.importAccepted)
        return;
    auto &task = *context.panels.importAccepted;
    auto &resources = context.host.Resources();
    auto &renderer = context.host.GetRenderer();
    if (modelAsset != task.assetId) {
        if (!modelAsset.empty())
            resources.ReleaseRef(modelAsset);
        modelAsset = task.assetId;
        resources.AddRef(modelAsset);
        context.panels.modelPreviewTime = 0;
        context.panels.modelPreviewHidden.clear();
    }
    std::string error;
    auto mesh = resources.GetMesh(task.assetId, error);
    if (!mesh.IsValid() || !task.preview)
        return;
    auto asset = resources.TryGetSkeletal(task.assetId);
    SkeletalPose pose;
    std::vector<glm::mat4> palette, global;
    if (asset) {
        pose = asset->skeleton.rest;
        if (!asset->clips.empty()) {
            auto &clip =
                asset->clips[std::min(size_t(context.panels.modelPreviewClip), asset->clips.size() - 1)];
            if (context.panels.modelPreviewPlaying && clip.duration > 0)
                context.panels.modelPreviewTime =
                    std::fmod(context.panels.modelPreviewTime + deltaSeconds, clip.duration);
            pose = SampleClip(asset->skeleton, clip, context.panels.modelPreviewTime);
        }
        palette = ResolveSkinMatrices(asset->skeleton, pose);
        global = ResolveJointMatrices(asset->skeleton, pose);
    }
    // Fit from the immutable rest mesh once per accepted generation, not by
    // skinning every source vertex on the editor thread every preview frame.
    if (modelData.lock() != task.preview) {
        auto &cpu = *task.preview;
        glm::vec3 lo(INFINITY), hi(-INFINITY);
        auto rest = cpu.skeletal ? ResolveSkinMatrices(cpu.skeletal->skeleton, cpu.skeletal->skeleton.rest)
                                 : std::vector<glm::mat4>{};
        for (size_t i = 0; i < cpu.vertices.size(); ++i) {
            auto p = cpu.vertices[i].position;
            if (!rest.empty()) {
                auto &w = cpu.skinVertices[i];
                glm::mat4 m(0);
                for (int k = 0; k < 4; ++k)
                    m += rest[w.joints[k]] * w.weights[k] + rest[w.joints1[k]] * w.weights1[k];
                p = glm::vec3(m * glm::vec4(p, 1));
            }
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
        modelMin = lo;
        modelMax = hi;
        modelData = task.preview;
    }
    auto lo = modelMin, hi = modelMax;
    auto center = (lo + hi) * .5f;
    float radius = std::max(glm::length(hi - lo) * .6f, .5f);
    glm::vec3 eye = center + glm::vec3(std::sin(context.panels.modelPreviewYaw) * radius, radius * .25f,
                                       std::cos(context.panels.modelPreviewYaw) * radius * 1.5f);
    if (!modelTarget.IsValid()) {
        renderer.SetSceneAppearance(false, 1, {}, 1, {1, 0, 0, 0}, false);
        modelTarget = renderer.CreateRenderTarget(600, 360, error);
    }
    renderer.SetSceneAppearance(true, 1, {}, 1, {1, 0, 0, 0}, false, {.035f, .045f, .065f});
    if (!modelTarget.IsValid() || !renderer.BeginRenderTarget(modelTarget))
        return;
    renderer.SetCamera(glm::lookAt(eye, center, glm::vec3(0, 1, 0)),
                       glm::perspective(glm::radians(55.f), 600.f / 360, .01f, std::max(100.f, radius * 10)));
    renderer.SetLighting(glm::normalize(glm::vec3(1, 2, 3)), {1.8f, 1.8f, 1.8f}, {.35f, .35f, .35f});
    renderer.SetSceneAppearance(true, 1, {}, 1, {1, 0, 0, 0}, false, {.035f, .045f, .065f});
    renderer.SetDynamicLights({});
    renderer.SetMaterialBindings({});
    renderer.DrawMesh(mesh, {0, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, {}, {1, 1, 1}, 1,
                      palette.empty() ? nullptr : &palette, &context.panels.modelPreviewHidden);
    DebugLineList lines;
    if (context.panels.modelDiagnosticVisible && context.panels.collisionDiagnosticAsset == task.assetId) {
        auto p = glm::vec3(context.panels.collisionDiagnostic.point);
        float size = std::max(.05f, radius * .02f);
        lines.Line(p - glm::vec3(size, 0, 0), p + glm::vec3(size, 0, 0), {1, 0, 0});
        lines.Line(p - glm::vec3(0, size, 0), p + glm::vec3(0, size, 0), {1, 0, 0});
        lines.Line(p - glm::vec3(0, 0, size), p + glm::vec3(0, 0, size), {1, 0, 0});
    }
    lines.Line({lo.x, lo.y, lo.z}, {lo.x + 1, lo.y, lo.z}, {1, 1, 1});
    lines.Axes({0, 0, 0}, {1, 0, 0, 0}, .3f);
    if (context.panels.modelPreviewSkeleton && asset)
        for (size_t i = 0; i < global.size(); ++i)
            if (asset->skeleton.parents[i] >= 0)
                lines.Line(glm::vec3(global[i][3]), glm::vec3(global[asset->skeleton.parents[i]][3]),
                           {.2f, 1, .6f});
    if (asset)
        for (auto &key : context.panels.skeletonPicked) {
            int joint = FindSkeletonJoint(asset->skeleton, key);
            if (joint >= 0 && size_t(joint) < global.size()) {
                JointTransform t;
                std::string error;
                if (DecomposeRigidPose(global[joint], t, error))
                    lines.Axes(t.translation, t.rotation, .15f);
            }
        }
    if (asset && !asset->clips.empty()) {
        auto &clip = asset->clips[std::min(size_t(context.panels.modelPreviewClip), asset->clips.size() - 1)];
        glm::vec3 previous(0);
        for (unsigned i = 0; i <= 60; ++i) {
            auto p = SampleRootMotion(clip, double(clip.duration) * i / 60, false).translation;
            if (i)
                lines.Line(previous, p, {1, .7f, .1f});
            previous = p;
        }
    }
    renderer.DrawDebugLines(lines.Lines(), false);
    renderer.EndRenderTarget();
    context.panels.modelPreviewToken = renderer.EditorImageToken(renderer.RenderTargetTexture(modelTarget));
}

void EditorAssetPreview::RefreshAssetDemand(EditorContext &context) {
    std::vector<std::string> wanted;
    if (!context.document.GetScene().Settings().environmentAsset.empty())
        wanted.push_back(context.document.GetScene().Settings().environmentAsset);
    if (!context.document.GetScene().Settings().appearanceResetState.empty()) {
        auto baseline = context.document.GetScene().Settings();
        std::string error;
        if (DecodeAppearanceState(baseline.appearanceResetState, baseline, error) &&
            !baseline.environmentAsset.empty())
            wanted.push_back(baseline.environmentAsset);
    }
    for (const SceneObject &o : context.document.GetScene().Objects()) {
        if (!o.render)
            continue;
        auto overrides = [&](const MaterialOverride &value) {
            for (const auto &texture : value.textures)
                if (texture && !texture->empty())
                    wanted.push_back(*texture);
        };
        overrides(o.render->instanceOverrides);
        for (const auto &slot : o.render->materials) {
            if (!slot.asset.empty())
                wanted.push_back(slot.asset);
            overrides(slot.overrides);
        }
        for (const auto *bindings : {&o.render->partMaterials, &o.render->runtimeMaterials})
            for (const auto &[_, slot] : *bindings) {
                if (!slot.asset.empty())
                    wanted.push_back(slot.asset);
                overrides(slot.overrides);
            }
        if (!o.render->meshAsset.empty())
            wanted.push_back(o.render->meshAsset);
        if (!o.render->textureAsset.empty())
            wanted.push_back(o.render->textureAsset);
    }
    std::sort(wanted.begin(), wanted.end());
    wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
    if (wanted == heldAssets)
        return;
    ResourceManager &resources = context.host.Resources();
    for (const std::string &id : wanted) {
        if (!std::binary_search(heldAssets.begin(), heldAssets.end(), id) && !resources.StreamableVisual(id))
            resources.AddRef(id);
    }
    for (const std::string &id : heldAssets) {
        if (!std::binary_search(wanted.begin(), wanted.end(), id) && !resources.StreamableVisual(id))
            resources.ReleaseRef(id);
    }
    heldAssets = wanted;
}

void EditorAssetPreview::DrawUIAuthoringPreview(EditorContext &context) {
    if (context.panels.mode != EditorMode::Edit || !context.panels.showAssetBrowser ||
        !context.panels.uiCanvasPreview || !context.panels.uiPreviewRevision)
        return;
    JUDAS_PROFILE_SCOPE("UI authoring canvas");
    auto &renderer = context.host.GetRenderer();
    auto &resources = context.host.Resources();
    auto key = context.project.ProjectFile() + context.project.Settings().localization.Encode();
    if (key != localizationKey) {
        localization = std::make_unique<LocalizationSession>(context.project.Settings().localization);
        localization->Bind(&resources);
        localizationKey = key;
    }
    std::string error;
    if (!context.panels.previewLocale.empty())
        localization->SetLocale(context.panels.previewLocale, error);
    localization->Refresh();
    if (!ui || uiRevision != context.panels.uiPreviewRevision) {
        auto candidate = std::make_unique<RuntimeUI>(&resources);
        if (!candidate->Add(context.panels.uiPreviewDocument, "authoring", 0, error)) {
            context.panels.status = error;
            return;
        }
        ui = std::move(candidate);
        uiRevision = context.panels.uiPreviewRevision;
    }
    ui->SetLocalization(localization.get());
    int width = context.panels.uiPreviewResolution.x, height = context.panels.uiPreviewResolution.y;
    if (!renderer.ResizeRenderTarget(uiTarget, width, height, error) ||
        !renderer.BeginRenderTarget(uiTarget)) {
        context.panels.status = error;
        return;
    }
    renderer.ClearWeather();
    renderer.SetSceneAppearance(false, 1, {}, 1, {1, 0, 0, 0}, false, {.035f, .045f, .065f});
    renderer.BeginFrame(width, height);
    renderer.EndFrame();
    renderer.BeginUIFrame(width, height);
    ui->Draw(renderer, width, height);
    renderer.EndUIFrame();
    renderer.EndRenderTarget();
    context.panels.uiPreviewToken = renderer.EditorImageToken(renderer.RenderTargetTexture(uiTarget));
    context.panels.uiPreviewLayout.clear();
    auto handle = ui->Find("authoring");
    for (auto &e : context.panels.uiPreviewDocument.elements)
        if (auto *l = ui->LayoutOf(handle, e.id))
            context.panels.uiPreviewLayout[e.id] = *l;
    if (context.panels.uiCanvasPick.x >= 0) {
        for (auto it = context.panels.uiPreviewDocument.elements.rbegin();
             it != context.panels.uiPreviewDocument.elements.rend(); ++it) {
            auto *l = ui->LayoutOf(handle, it->id);
            if (l && l->visible && l->rect.Contains(context.panels.uiCanvasPick) &&
                l->clip.Contains(context.panels.uiCanvasPick)) {
                context.panels.uiCanvasSelected = it->id;
                break;
            }
        }
        context.panels.uiCanvasPick = {-1, -1};
    }
}

void EditorAssetPreview::Pump(EditorContext &context) {
    PumpEditorAssetThumbnails(context.panels);
    if (context.phase == EditorPanelPhase::Edit) {
        PumpEditorAssetWorkflow(context.panels, context.requests);
        RefreshAssetDemand(context);
    }
}

void EditorAssetPreview::DrawTextPreview(EditorContext &context) {
    auto &host = context.host;
    auto &renderer = context.host.GetRenderer();
    auto &window = context.host.GetWindow();
    if (context.panels.mode == EditorMode::Edit && context.panels.textPreview &&
        context.panels.showAssetBrowser) {
        auto key = context.project.ProjectFile() + context.project.Settings().localization.Encode();
        if (key != localizationKey) {
            localization.reset();
            localization = std::make_unique<LocalizationSession>(context.project.Settings().localization);
            localization->Bind(&host.Resources());
            localizationKey = key;
        }
        std::string previewError;
        if (!context.panels.previewLocale.empty() && localization->Locale() != context.panels.previewLocale)
            localization->SetLocale(context.panels.previewLocale, previewError);
        if (context.panels.textReload) {
            localization->Reload();
            context.panels.textReload = false;
        }
        localization->Refresh();
        auto &e = context.panels.textElement;
        auto text = e.text;
        if (!e.textKey.empty())
            text = localization->Format(e.textKey, {}, previewError);
        std::vector<std::shared_ptr<const TextFont>> fonts;
        if (!e.font.empty()) {
            host.Resources().RequestFont(e.font);
            if (auto f = host.Resources().TryGetFont(e.font))
                fonts.push_back(f);
        }
        if (fonts.empty())
            if (auto f = renderer.DefaultTextFont())
                fonts.push_back(f);
        auto fallback = localization->Fonts();
        fonts.insert(fonts.end(), fallback.begin(), fallback.end());
        renderer.SelectTextFonts(std::move(fonts));
        TextOptions o;
        o.pixels = e.fontSize;
        o.width = context.panels.textPreviewSize.x;
        o.wrap = e.wrap;
        o.locale = localization->Locale();
        o.direction = e.direction;
        o.alignment = e.textLogicalAlign < 0 ? TextAlignment::Left : TextAlignment(e.textLogicalAlign);
        renderer.BeginUIFrame(window.Width(), window.Height());
        renderer.SetUIClip(context.panels.textPreviewPosition, context.panels.textPreviewSize);
        renderer.DrawUIRect(context.panels.textPreviewPosition, context.panels.textPreviewSize,
                            {.04, .05, .08, 1});
        renderer.DrawTextLayout(*renderer.LayoutText(text, o), context.panels.textPreviewPosition, e.color);
        renderer.ClearUIClip();
        renderer.EndUIFrame();
    }
}

void EditorAssetPreview::Destroy(EditorContext &context) {
    ResetEditorAssetThumbnails(context.panels);
    auto &resources = context.host.Resources();
    auto &renderer = context.host.GetRenderer();
    for (const auto &id : heldAssets)
        if (!resources.StreamableVisual(id))
            resources.ReleaseRef(id);
    heldAssets.clear();
    if (uiTarget.IsValid())
        renderer.DestroyRenderTarget(uiTarget);
    if (modelTarget.IsValid())
        renderer.DestroyRenderTarget(modelTarget);
    uiTarget = {};
    modelTarget = {};
    ui.reset();
    localization.reset();
    localizationKey.clear();
    uiRevision = 0;
    if (!modelAsset.empty())
        resources.ReleaseRef(modelAsset);
    modelAsset.clear();
    modelData.reset();
}
