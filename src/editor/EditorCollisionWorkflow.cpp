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
#include "CollisionAsset.h"
#include "ModelCollision.h"

bool BakeEditorCollision(EditorDocument &doc, SceneObjectId target, EditorPanelState &state,
                         const AssetId &sourceId, const CollisionCookSettings &settings,
                         const std::string &output) {
    auto fail = [&](const std::string &error) {
        state.status = error;
        return false;
    };
    if (!state.assets || state.mode != EditorMode::Edit)
        return fail("Collision cooking requires an edit project");
    auto *object = doc.GetScene().Find(target);
    if (!object || !object->body)
        return fail("Select an object with a Body to assign collision");
    if (!settings.convex && (object->body->motion == SceneBodyMotion::Dynamic || object->body->sensor))
        return fail("Triangle surfaces require a static non-sensor body");
    if (object->transform.scale != glm::vec3(1))
        return fail("Bake scale into source; rigid instance scale must be one");
    const auto *source = state.assets->Find(sourceId);
    if (!source || source->missing || source->type != AssetType::Mesh)
        return fail("Select a registered source model");
    auto relative = std::filesystem::path(output).lexically_normal();
    if (relative.empty() || relative.is_absolute() || relative.extension() != ".judascollision")
        return fail("Output must be an assets-relative .judascollision path");
    for (auto &part : relative)
        if (part == "..")
            return fail("Output cannot escape project assets");
    auto destination = std::filesystem::path(state.assets->AssetsDir()) / relative;
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec)
        return fail(ec.message());
    CollisionAsset asset;
    std::string error;
    ModelCollisionCleanupReport cleanupReport;
    if (!CookImportedCollisionFile(source->path, sourceId, settings, state.collisionCleanup,
                                   destination.string(), asset, cleanupReport, state.collisionDiagnostic,
                                   error))
        return fail(error);
    AssetRecord record;
    auto *existing = state.assets->FindByRelativePath(
        std::filesystem::relative(destination, state.project->RootDir()).generic_string());
    if (existing)
        record = *existing;
    else if (!state.assets->Track(destination.string(), record, error))
        return fail(error);
    if (state.resources)
        state.resources->Invalidate(record.id);
    doc.BeginEdit();
    object->body->shape = settings.convex ? SceneShape::ConvexHull : SceneShape::TriangleMesh;
    object->body->collisionAsset = record.id;
    doc.CommitEdit();
    state.status = "Cooked and assigned physical geometry (undo restores authored Body)";
    return true;
}
