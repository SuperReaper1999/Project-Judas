#include "EditorProjectWorkflow.h"
#include "EditorContext.h"
#include "EditorPreferences.h"
#include "EditorSceneGeometry.h"
#include "EditorSceneViewport.h"
#include "EngineHost.h"
#include "ModAuthoring.h"
#include "Prefab.h"
#include "Project.h"
#include "ProjectStarter.h"
#include "RetargetPanel.h"
#include "SceneSerialization.h"
#include "TerrainLibrary.h"
#include "TerrainTools.h"
#include "VolumeTerrainTools.h"
#include "WorldState.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
using EditorSceneGeometry::BuildSelectionLines;
using EditorSceneGeometry::PickRadius;

namespace fs = std::filesystem;
namespace {
Scene MakeStarterScene() {
    Scene scene;
    scene.Settings().name = "Main";
    SceneObject &ground = scene.CreateObject("Ground");
    ground.transform.position = glm::vec3(0.0f, -0.5f, 0.0f);
    ground.render = SceneRenderComponent{};
    ground.render->shape = SceneShape::Box;
    ground.render->halfExtents = glm::vec3(20.0f, 0.5f, 20.0f);
    ground.render->color = glm::vec3(0.4f, 0.44f, 0.36f);
    ground.body = SceneBodyComponent{};
    ground.body->shape = SceneShape::Box;
    ground.body->halfExtents = glm::vec3(20.0f, 0.5f, 20.0f);
    ground.body->friction = 0.8f;
    ground.body->restitution = 0.05f;
    ground.gravity = SceneGravityComponent{};
    ground.gravity->kind = SceneGravityKind::Uniform;
    ground.gravity->regionShape = SceneRegionShape::Box;
    ground.gravity->regionHalfExtents = glm::vec3(30.0f);
    SceneObject &start = scene.CreateObject("Player start");
    start.transform.position = glm::vec3(0.0f, 1.0f, 5.0f);
    start.playerStart = ScenePlayerStartComponent{};
    SceneObject &lamp = scene.CreateObject("Lamp");
    lamp.transform.position = glm::vec3(0.0f, 4.0f, 0.0f);
    lamp.light = SceneLightComponent{};
    lamp.light->color = glm::vec3(2.5f, 2.3f, 2.0f);
    lamp.light->range = 18.0f;
    return scene;
}
} // namespace

void EditorProjectWorkflow::RefreshProjectLists(EditorContext &context) {
    context.panels.terrainSurfaces = KnownTerrainSurfaces();
    context.panels.sceneFiles.clear();
    context.panels.project = &context.project;
    context.panels.assets = &context.host.Assets();
    context.panels.resources = &context.host.Resources();
    context.panels.importJobs = &context.host.Jobs();
    if (!context.project.IsLoaded())
        return;
    std::error_code ec;
    if (!fs::is_directory(context.project.ScenesDir(), ec))
        return;
    for (const auto &entry : fs::recursive_directory_iterator(context.project.ScenesDir(), ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".judas") {
            context.panels.sceneFiles.push_back(context.project.MakeRelative(entry.path().generic_string()));
        }
    }
    std::sort(context.panels.sceneFiles.begin(), context.panels.sceneFiles.end());
}

bool EditorProjectWorkflow::OpenProject(EditorContext &context, EditorProjectServices &services,
                                        const std::string &projectFile, std::string &outError) {
    Project project;
    if (!project.Load(projectFile, outError))
        return false;
    services.viewport.planetPreview.clear();
    if (services.terrain) {
        services.terrain->Shutdown(context.host.GetRenderer(), context.panels.importJobs);
        services.terrain.reset();
    }
    if (services.volumeTerrain) {
        services.volumeTerrain->Shutdown(context.host.GetRenderer(), context.panels.importJobs);
        services.volumeTerrain.reset();
    }
    RetireRetargetProject(context.panels);
    auto retiringContent = context.project.ModSession(); // retain leases until old jobs are joined
    context.project = project;
    context.panels.view2D = context.project.Settings().editor2D;
    services.viewport.camera.Set2D(context.panels.view2D);
    services.viewport.picker.Clear();
    context.host.OpenProjectAssets(context.project.RootDir(), context.project.AssetsDir());
    context.panels.projectHistory.reset();
    context.host.Resources().ConfigureVisualStreaming(uint64_t(context.project.Settings().visualBudgetMiB) *
                                                      1024 * 1024);
    {
        std::string assetError;
        if (!JudasMods::OpenAuthorAssets(context.project, context.host.Assets(), assetError))
            context.panels.status = assetError;
    }
    if (!context.host.GetWindow().Input().SetMap(context.project.Settings().input, outError))
        return false;
    RefreshProjectLists(context);
    context.panels.showProjectSettings = false;
    context.panels.showBuildExport = false;
    context.panels.runProjectInfo.clear();
    context.panels.browserSelection.clear();
    return true;
}

bool EditorProjectWorkflow::CreateProject(EditorContext &context, EditorProjectServices &services,
                                          const std::string &directory, const std::string &name,
                                          std::string &outError, bool visibleStarter, int twoDTemplate) {
    Project project;
    if (!Project::CreateNew(directory, name, project, outError))
        return false;
    // A starter scene so the new project runs immediately.
    const std::string sceneRelative = project.Settings().scenesDir + "/main.judas";
    Scene starter = twoDTemplate ? Scene{} : MakeStarterScene();
    if (twoDTemplate &&
        !(twoDTemplate == 3 ? PopulateGridProjectStarter(project, starter, outError)
                            : PopulatePlanarProjectStarter(project, starter, twoDTemplate == 2, outError)))
        return false;
    if (visibleStarter && !twoDTemplate && !PopulateVisibleProjectStarter(project, starter, outError))
        return false;
    if (!SaveSceneToFile(starter, project.Resolve(sceneRelative), outError))
        return false;
    project.Settings().startupScene = sceneRelative;
    if (!project.Save(outError))
        return false;
    if (!OpenProject(context, services, project.ProjectFile(), outError))
        return false;
    return OpenScene(context, services, sceneRelative, outError);
}

std::string EditorProjectWorkflow::ResolveScenePath(EditorContext &context, const std::string &input) const {
    if (input.empty() || fs::path(input).is_absolute())
        return input;
    std::error_code ec;
    if (context.project.IsLoaded() && fs::exists(context.project.Resolve(input), ec))
        return context.project.Resolve(input);
    if (fs::exists(input, ec))
        return fs::absolute(input, ec).lexically_normal().generic_string();
    return context.project.IsLoaded() ? context.project.Resolve(input) : input;
}

std::string EditorProjectWorkflow::WorldStatePathFor(EditorContext &context,
                                                     const std::string &scenePath) const {
    if (scenePath.empty())
        return std::string();
    return context.project.IsLoaded() ? context.project.WorldStatePathForScene(scenePath)
                                      : DefaultWorldStatePath(scenePath);
}

bool EditorProjectWorkflow::OpenScene(EditorContext &context, EditorProjectServices &services,
                                      const std::string &path, std::string &outError) {
    context.panels.liquidPreview.Clear();
    context.panels.deformablePreview.Clear();
    context.panels.collisionPreview.Clear();
    const std::string resolved = ResolveScenePath(context, path);
    Scene authored, resolvedPrefabs;
    if (!LoadSceneFromFile(resolved, authored, outError) ||
        !ResolvePrefabs(authored, &context.host.Assets(), resolvedPrefabs, outError))
        return false;
    if (!context.document.Load(resolved, outError))
        return false;
    context.document.GetScene() = std::move(resolvedPrefabs);
    context.panels.pathInput = context.project.IsLoaded() ? context.project.MakeRelative(resolved) : resolved;
    if (const SceneObject *first = context.document.GetScene().Objects().empty()
                                       ? nullptr
                                       : &context.document.GetScene().Objects().front()) {
        services.viewport.camera.LookAt(first->transform.position,
                                        services.viewport.camera.Is2D() ? 5.0f : 40.0f);
    }
    return true;
}
