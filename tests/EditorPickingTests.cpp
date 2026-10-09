#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "AssetDatabase.h"
#include "ModelArchive.h"
#include "Prefab.h"
#include "ResourceManager.h"
#include "SkeletalAnimation.h"
#include "editor/EditorPicking.h"

namespace {
int checks = 0, failures = 0;
void Check(bool condition, const char* description) {
    ++checks;
    if (!condition) ++failures;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", description);
}
bool Near(float actual, float expected) { return std::abs(actual - expected) < 1e-4f; }
SceneObjectId AddBox(Scene& scene, glm::vec3 position, glm::vec3 half) {
    auto& object = scene.CreateObject("Box");
    object.transform.position = position;
    object.render = SceneRenderComponent{};
    object.render->halfExtents = half;
    return object.id;
}
SceneObjectId OldSpherePick(const Scene& scene, glm::vec3 origin, glm::vec3 direction) {
    SceneObjectId result = 0;
    float nearest = 1e30f;
    for (const auto& object : scene.Objects()) {
        float radius = 0.75f;
        if (object.render) radius = std::max(radius, glm::length(object.render->halfExtents));
        const auto delta = origin - object.transform.position;
        const float b = glm::dot(delta, direction), c = glm::dot(delta, delta) - radius * radius;
        const float discriminant = b * b - c;
        if (discriminant < 0) continue;
        const float distance = -b - std::sqrt(discriminant);
        if (distance >= 0 && distance < nearest) { result = object.id; nearest = distance; }
    }
    return result;
}

void FloorAndPrimitives() {
    EditorScenePicker picker;
    Scene scene;
    const auto floor = AddBox(scene, {0, 0, 0}, {20, 0.2f, 20});
    const auto small = AddBox(scene, {0, 2, 0}, {0.25f, 0.25f, 0.25f});
    Check(OldSpherePick(scene, {0, 40, 0}, {0, -1, 0}) == floor,
          "original sphere-proxy fixture reproduces floor stealing the small-box click");
    auto hit = picker.Pick(scene, {0, 40, 0}, {0, -1, 0});
    Check(hit.id == small && hit.visual && Near(hit.distance, 37.75f),
          "actual small-box surface wins above a broad encompassing floor");
    hit = picker.Pick(scene, {10, 40, 0}, {0, -1, 0});
    Check(hit.id == floor && Near(hit.distance, 39.8f), "floor remains selectable at its actual surface");
    AddBox(scene, {10, -2, 0}, glm::vec3(0.25f));
    Check(picker.Pick(scene, {10, 40, 0}, {0, -1, 0}).id == floor,
          "nearer floor geometry correctly occludes an object behind it");

    Scene corner;
    AddBox(corner, {0, 0, 0}, glm::vec3(1));
    Check(OldSpherePick(corner, {1.2f, 1.2f, 5}, {0, 0, -1}) != 0 &&
              picker.Pick(corner, {1.2f, 1.2f, 5}, {0, 0, -1}).id == 0,
          "box corner miss no longer selects the surrounding bounding sphere");
    corner.Objects()[0].transform.scale.x = 0;
    hit = picker.Pick(corner, {0, 0, 5}, {0, 0, -1});
    Check(hit.id == 0 && !std::isnan(hit.distance), "zero visual scale safely rejects a degenerate box");
    Check(picker.Pick(corner, {0, 0, 5}, {0, 0, 0}).id == 0, "zero ray direction is safely rejected");

    Scene spheres;
    auto& sphere = spheres.CreateObject("Sphere");
    const auto sphereId = sphere.id;
    sphere.render = SceneRenderComponent{};
    sphere.render->shape = SceneShape::Sphere;
    sphere.render->radius = 1;
    hit = picker.Pick(spheres, {0, 0, 5}, {0, 0, -3});
    Check(hit.id == sphereId && Near(hit.distance, 4), "ordinary sphere hit returns world distance for a non-unit ray");
    Check(picker.Pick(spheres, {0.9f, 0.9f, 5}, {0, 0, -1}).id == 0,
          "sphere corner ray outside its true silhouette misses");
    hit = picker.Pick(spheres, {0, 0, 0}, {0, 0, 1});
    Check(hit.id == sphereId && Near(hit.distance, 1), "ray starting inside a sphere picks the forward surface");
    sphere.transform.rotation = glm::angleAxis(glm::radians(37.0f), glm::vec3(0, 1, 0));
    sphere.transform.scale = {2, 1, 0.5f};
    const auto forward = sphere.transform.rotation * glm::vec3(0, 0, 1);
    hit = picker.Pick(spheres, forward * 5.0f, -forward);
    Check(hit.id == sphereId && Near(hit.distance, 4.5f), "scaled and rotated sphere follows its rendered ellipsoid");
    spheres.Settings().mainCameraRenderMask = 0;
    Check(picker.Pick(spheres, forward * 5.0f, -forward).id == 0, "authored render mask excludes invisible geometry");

    Scene compound;
    auto& owner = compound.CreateObject("Compound visual");
    const auto compoundId = owner.id;
    owner.render = SceneRenderComponent{}; owner.render->shape = SceneShape::Compound;
    owner.body = SceneBodyComponent{};
    owner.transform.position = {3, 1, 2};
    owner.transform.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    owner.transform.scale = glm::vec3(20); // Compound rendering deliberately ignores this.
    CompoundBox child({1, 0, 0}, {0.4f, 0.3f, 0.2f});
    child.rotation = glm::angleAxis(glm::radians(45.0f), glm::vec3(0, 1, 0));
    owner.body->compoundBoxes.push_back(child);
    const auto childCentre = owner.transform.position + owner.transform.rotation * child.localCenter;
    const auto childForward = owner.transform.rotation * child.rotation * glm::vec3(0, 0, 1);
    hit = picker.Pick(compound, childCentre + childForward * 10.0f, -childForward);
    Check(hit.id == compoundId && Near(hit.distance, 9.8f),
          "compound child offset and rotation match rendering without applying entity scale");

    Scene doors;
    const auto door = AddBox(doors, {0, 0, 0}, {1, 2, 0.2f});
    doors.Find(door)->door = SceneDoorComponent{};
    doors.Find(door)->transform.scale = glm::vec3(5);
    hit = picker.Pick(doors, {1.75f, 0, 5}, {0, 0, -1});
    Check(hit.id == door && Near(hit.distance, 4.8f), "closed door visual selection uses its hinge-offset panel");
    Check(picker.Pick(doors, {2.2f, 0, 5}, {0, 0, -1}).id == 0,
          "closed door selection follows its unscaled authored-render dimensions");
}

void HierarchyAndFallback() {
    EditorScenePicker picker;
    Scene scene;
    auto& parent = scene.CreateObject("Visual group");
    const auto parentId = parent.id;
    parent.transform.position = {4, 1, -2};
    parent.transform.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    parent.transform.scale = {2, 3, 0.5f};
    const auto child = AddBox(scene, {0, 1, 4}, {0.2f, 0.3f, 0.4f});
    scene.Find(child)->parent = parentId;
    scene.Find(child)->transform.rotation = glm::angleAxis(glm::radians(30.0f), glm::vec3(1, 0, 0));
    scene.Find(child)->transform.scale = {0.5f, 2, 4};
    Scene flat;
    std::string error;
    Check(FlattenHierarchy(scene, flat, error), "rotated and nonuniformly scaled nested fixture flattens");
    const auto transform = flat.Find(child)->transform;
    const auto forward = transform.rotation * glm::vec3(0, 0, 1);
    const auto hit = picker.Pick(scene, transform.position + forward * 10.0f, -forward);
    Check(hit.id == child && hit.visual && Near(hit.distance, 9.2f),
          "nested child keeps its own identity and rendered world transform and scale");

    Scene group;
    const auto groupId = group.CreateObject("Parent icon").id;
    group.Find(groupId)->transform.position = {0, 0, 3};
    const auto nested = AddBox(group, {0, 0, -3}, glm::vec3(0.25f));
    group.Find(nested)->parent = groupId;
    Check(picker.Pick(group, {0, 0, 10}, {0, 0, -1}).id == nested,
          "non-render parent pivot in front cannot steal its child's visible hit");
    group.Find(nested)->transform.position.x = 5;
    const auto icon = picker.Pick(group, {0, 0, 10}, {0, 0, -1});
    Check(icon.id == groupId && !icon.visual, "empty parent remains selectable when the ray hits no visual geometry");
    Check(picker.Pick(group, {0.3f, 0, 10}, {0, 0, -1}).id == 0,
          "empty fallback is a small pivot proxy rather than inherited object bounds");
}

bool WriteMesh(const std::filesystem::path& path, const MeshData& mesh) {
    std::vector<uint8_t> bytes;
    std::string error;
    if (!EncodeModelArchive(mesh, bytes, error)) { std::printf("Fixture: %s\n", error.c_str()); return false; }
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    return bool(file);
}

void MeshTriangles(const std::filesystem::path& output) {
    std::filesystem::create_directories(output / "Assets");
    MeshData mesh;
    for (auto position : {glm::vec3(-1, -1, 0), glm::vec3(1, -1, 0), glm::vec3(-1, 1, 0),
                          glm::vec3(2, -1, 0), glm::vec3(4, -1, 0), glm::vec3(2, 1, 0)}) {
        MeshVertex vertex; vertex.position = position; vertex.normal = {0, 0, 1}; mesh.vertices.push_back(vertex);
    }
    mesh.indices = {0, 1, 2, 3, 4, 5};
    mesh.primitives = {{0, 3, 0, "visible"}, {3, 3, 0, "optional"}};
    MaterialDefinition material; material.doubleSided = true; mesh.materials.push_back(material);
    const auto path = output / "Assets/triangles.judasmodel";
    const std::string id(32, 'a');
    std::string error;
    Check(WriteMesh(path, mesh) && AssetDatabase::WriteMeta(path.string() + ".judasmeta", id, AssetType::Mesh, "", error),
          "real CPU model archive and stable asset metadata fixture writes");
    AssetDatabase assets; assets.Scan(output.string(), (output / "Assets").string());
    EditorScenePicker picker;
    Scene scene; scene.Settings().linearRendering = true;
    auto& object = scene.CreateObject("Triangle mesh");
    const auto objectId = object.id;
    object.render = SceneRenderComponent{}; object.render->shape = SceneShape::Mesh; object.render->meshAsset = id;
    object.transform.position = {3, 2, -4};
    object.transform.rotation = glm::angleAxis(glm::radians(35.0f), glm::vec3(0, 1, 0));
    object.transform.scale = {2, 0.5f, 3};
    const auto world = [&](glm::vec3 local) { return object.transform.position + object.transform.rotation * (object.transform.scale * local); };
    const auto forward = object.transform.rotation * glm::vec3(0, 0, 1);
    const auto centre = world({-0.5f, -0.5f, 0});
    auto hit = picker.Pick(scene, centre + forward * 10.0f, -forward, &assets);
    Check(hit.id == objectId && Near(hit.distance, 10), "model-loader CPU triangle hit preserves nonuniform world distance");
    const auto corner = world({0.8f, 0.8f, 0});
    Check(picker.Pick(scene, corner + forward * 10.0f, -forward, &assets).id == 0,
          "ray inside mesh bounds but outside the actual triangle misses");
    Check(picker.Pick(scene, centre - forward * 10.0f, forward, &assets).id == objectId,
          "double-sided imported material permits a mesh backface hit");
    const auto partCentre = world({2.5f, -0.5f, 0});
    Check(picker.Pick(scene, partCentre + forward * 10.0f, -forward, &assets).id == objectId,
          "visible second primitive range can be selected");
    object.render->hiddenParts = {"optional"};
    Check(picker.Pick(scene, partCentre + forward * 10.0f, -forward, &assets).id == 0,
          "hidden model part does not intercept scene selection");

    mesh.materials[0].doubleSided = false;
    const auto modified = std::filesystem::last_write_time(path);
    Check(WriteMesh(path, mesh), "single-sided material fixture replaces the same stable asset");
    std::filesystem::last_write_time(path, modified + std::chrono::seconds(1));
    Check(picker.Pick(scene, centre - forward * 10.0f, forward, &assets).id == 0 &&
              picker.Pick(scene, centre + forward * 10.0f, -forward, &assets).id == objectId,
          "cache reload honors available single-sided material backface culling");
    object.transform.scale.x *= -1;
    const auto mirrorCentre = world({-0.5f, -0.5f, 0});
    Check(picker.Pick(scene, mirrorCentre + forward * 10.0f, -forward, &assets).id == objectId,
          "mirrored visual scale preserves the renderer's corrected front-face convention");

    ResourceManager resources(nullptr, &assets);
    object.transform.scale = {2, 0.5f, 3};
    hit = picker.Pick(scene, object.transform.position + forward * 10.0f, -forward, &assets, &resources);
    Check(hit.id == objectId && Near(hit.distance, 8.5f),
          "unloaded mesh selection follows the displayed half-unit scaled placeholder box");

    MeshData nonIndexed; nonIndexed.vertices.assign(mesh.vertices.begin(), mesh.vertices.begin() + 3);
    Check(WriteMesh(path, nonIndexed), "non-indexed ordinary mesh fixture writes through the current archive loader");
    std::filesystem::last_write_time(path, modified + std::chrono::seconds(2));
    Check(picker.Pick(scene, centre + forward * 10.0f, -forward, &assets).id == objectId,
          "non-indexed consecutive triangles are selectable without GPU readback");

    const std::string pendingMaterial(32, 'c');
    const auto materialPath = output / "Assets/pending.judasmaterial";
    const bool materialSetup = SaveMaterial(materialPath.string(), MaterialDefinition{}, error) &&
        AssetDatabase::WriteMeta(materialPath.string() + ".judasmeta", pendingMaterial, AssetType::Material, "", error);
    assets.Scan(output.string(), (output / "Assets").string());
    resources.SetHeadlessResidency(true);
    resources.SetBlockingMode(true);
    const bool meshReady = resources.RequestMesh(id) == ResourceState::Ready;
    object.render->materials = {{pendingMaterial, {}}};
    Check(materialSetup && meshReady && resources.StateOf(pendingMaterial) == ResourceState::Unloaded &&
              picker.Pick(scene, centre + forward * 10.0f, -forward, &assets, &resources).id == objectId &&
              picker.Pick(scene, centre - forward * 10.0f, forward, &assets, &resources).id == 0,
          "pending explicit material uses the renderer's single-sided fallback for front and back hits");
    const std::string failedMaterial(32, 'd');
    object.render->materials[0].asset = failedMaterial;
    Check(resources.RequestMaterial(failedMaterial) == ResourceState::Failed &&
              picker.Pick(scene, centre + forward * 10.0f, -forward, &assets, &resources).id == objectId &&
              picker.Pick(scene, centre - forward * 10.0f, forward, &assets, &resources).id == 0,
          "failed explicit material uses the renderer's single-sided fallback for front and back hits");
}

void MirroredRestSkin(const std::filesystem::path& output) {
    MeshData mesh;
    for (auto position : {glm::vec3(-1, -1, 0), glm::vec3(1, -1, 0), glm::vec3(-1, 1, 0)}) {
        MeshVertex vertex; vertex.position = position; vertex.normal = {0, 0, 1}; mesh.vertices.push_back(vertex);
    }
    mesh.indices = {0, 1, 2};
    mesh.primitives = {{0, 3, 0, "mirrored-rest-part"}};
    mesh.materials.push_back(MaterialDefinition{});
    auto rig = std::make_shared<SkeletalAsset>();
    rig->skeleton.names = {"Reflected"}; rig->skeleton.parents = {-1}; rig->skeleton.order = {0};
    JointTransform rest; rest.scale = {-1, 1, 1}; rig->skeleton.rest.local = {rest};
    rig->skeleton.skinNodes = {0}; rig->skeleton.inverseBind = {glm::mat4(1)};
    mesh.skeletal = std::move(rig);
    mesh.skinVertices.resize(mesh.vertices.size()); // Full influence from the reflected node.
    const auto path = output / "Assets/mirrored-rest.judasmodel";
    const std::string id(32, 'b');
    std::string error;
    const bool fixture = WriteMesh(path, mesh) &&
        AssetDatabase::WriteMeta(path.string() + ".judasmeta", id, AssetType::Mesh, "", error);
    AssetDatabase assets; assets.Scan(output.string(), (output / "Assets").string());
    EditorScenePicker picker;
    Scene scene; scene.Settings().linearRendering = true;
    auto& object = scene.CreateObject("Mirrored rest skin");
    object.render = SceneRenderComponent{}; object.render->shape = SceneShape::Mesh; object.render->meshAsset = id;
    Check(fixture && picker.Pick(scene, {0.5f, -0.5f, 5}, {0, 0, -1}, &assets).id == object.id &&
              picker.Pick(scene, {0.5f, -0.5f, -5}, {0, 0, 1}, &assets).id == 0,
          "mirrored rest-skinned primitive accepts its rendered front and rejects the culled back");
    object.transform.scale = {-2, 1, 1};
    const auto hit = picker.Pick(scene, {-1, -0.5f, 5}, {0, 0, -1}, &assets);
    Check(hit.id == object.id && Near(hit.distance, 5) &&
              picker.Pick(scene, {-1, -0.5f, -5}, {0, 0, 1}, &assets).id == 0,
          "combined rest-skin and entity reflections preserve the renderer's front-face convention");
}
}

int main(int argc, char** argv) {
    FloorAndPrimitives();
    HierarchyAndFallback();
    const auto output = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path("build/editor-picking-tests");
    MeshTriangles(output);
    MirroredRestSkin(output);
    std::printf("Editor geometry picking: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
