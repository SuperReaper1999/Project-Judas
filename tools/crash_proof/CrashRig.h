#pragma once
#include "Deformable.h"
#include "Scene.h"
#include "StructuralSkin.h"
#include "WheelAssembly.h"
#include <array>
#include <filesystem>
struct CrashRig {
    SceneObjectId owner = 0;
    unsigned referencePart = 0;
    std::array<SceneObjectId, 4> wheels{};
    StructuralSkin skin;
    MeshData mapped;
};
DeformableAsset MakeCrashStructure(unsigned longitudinal = 8, unsigned layers = 2, unsigned columns = 2);
CrashRig AddCrashRig(Scene &, const DeformableAsset &, const std::string &assetId, glm::vec3 position,
                     const MeshData &exterior, const std::array<CrashWheelMount,4>* mounts=nullptr, bool panelSeams=false);
