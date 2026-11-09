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
#include "../PowertrainAuthoring.h"
#include "Prefab.h"

SceneObjectId DuplicateObject(EditorDocument &doc, SceneObjectId id) {
    if (doc.Selection().size() > 1) {
        std::string error;
        return doc.DuplicateSelection(error) ? doc.Selected() : kInvalidSceneObjectId;
    }
    Scene &scene = doc.GetScene();
    const SceneObject *source = scene.Find(id);
    if (!source)
        return kInvalidSceneObjectId;
    if (source->prefabRoot) {
        if (source->prefabRoot != id)
            return kInvalidSceneObjectId;
        const auto asset = source->prefabAsset;
        const auto placement = source->transform;
        Scene prefab;
        std::string error;
        if (!CreatePrefab(scene, id, prefab, error))
            return kInvalidSceneObjectId;
        auto ids = source->prefabIds;
        std::map<SceneObjectId, SceneObjectId> reverse;
        for (const auto &pair : ids)
            reverse[pair.second] = pair.first;
        for (auto &o : prefab.Objects()) {
            o.id = reverse.at(o.id);
            if (o.parent)
                o.parent = reverse.at(o.parent);
            if (o.gravitySelection && o.gravitySelection->source)
                o.gravitySelection->source = reverse.at(o.gravitySelection->source);
            if (o.wheel)
                o.wheel->chassis = reverse.at(o.wheel->chassis);
            if (o.powertrain)
                RemapPowertrainReferences(*o.powertrain, [&](uint64_t id) { return reverse.at(id); });
            if (o.render && o.render->textureCamera)
                o.render->textureCamera = reverse.at(o.render->textureCamera);
        }
        prefab.SetNextId(1);
        doc.BeginEdit();
        SceneObjectId root = 0;
        if (!InstantiatePrefab(scene, prefab, asset, placement, root, error)) {
            doc.CancelEdit();
            return 0;
        }
        // The duplicate remains linked and carries only the same explicit overrides.
        for (auto &o : scene.Objects())
            if (o.prefabRoot == root) {
                auto old = ids.find(o.prefabSource);
                if (old != ids.end())
                    o.prefabOverrides = scene.Find(old->second)->prefabOverrides;
            }
        doc.CommitEdit(false);
        doc.Select(root);
        return root;
    }
    doc.BeginEdit();
    SceneObject copy = *source; // by value: CreateObject may reallocate
    SceneObject &created = scene.CreateObject(copy.name + " copy");
    const SceneObjectId newId = created.id;
    const std::string newName = created.name;
    created = copy;
    created.id = newId;
    created.name = newName;
    // A scene has at most one player start; the copy must not carry it.
    // Place the copy right after the original (order is authored data).
    std::vector<SceneObject> &objects = scene.Objects();
    std::size_t sourceIndex = 0, copyIndex = objects.size() - 1;
    for (std::size_t i = 0; i < objects.size(); ++i)
        if (objects[i].id == id)
            sourceIndex = i;
    while (copyIndex > sourceIndex + 1) {
        std::swap(objects[copyIndex], objects[copyIndex - 1]);
        --copyIndex;
    }
    doc.CommitEdit();
    doc.Select(newId);
    return newId;
}
