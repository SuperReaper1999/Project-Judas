// M72 authored appearance uses the ordinary scene, prefab and editor history
// paths. No renderer or graphics context is required for these durable checks.
#include "AssetDependencies.h"
#include "NamedAuthoring.h"
#include "Prefab.h"
#include "SceneFingerprint.h"
#include "SceneSerialization.h"
#include "editor/EditorDocument.h"
#include "../third_party/nlohmann/json.hpp"

#include <cstdio>
#include <filesystem>
#include <functional>
#include <set>
#include <string>

namespace {
using Json = nlohmann::ordered_json;
int checks = 0, failures = 0;
const std::string material(32, 'a'), mesh(32, 'b'), environment(32, 'c'), resetEnvironment(32, 'd');
const std::string stablePart = "node:3/primitive:1";
const std::string removedPart = "node:3/primitive:2";

void Check(bool ok, const std::string& label) {
    ++checks;
    failures += !ok;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
}
std::string Quote(const std::string& value) {
    std::string result = "\"";
    for (char c : value) { if (c == '"' || c == '\\') result += '\\'; result += c; }
    return result + '"';
}
std::string Field(const std::string& text, const std::string& key, const std::string& value) {
    auto result = text;
    const std::string prefix = "\n  " + key + " ";
    const auto begin = result.find(prefix);
    if (begin == std::string::npos) return {};
    const auto end = result.find('\n', begin + 1);
    result.replace(begin, end - begin, prefix + Quote(value));
    return result;
}
MaterialOverride FullOverride() {
    MaterialOverride o;
    o.baseColor = glm::vec4(.25f, .5f, .75f, .5f);
    o.metallic = .25f; o.roughness = .625f;
    o.emissive = glm::vec3(.125f, .25f, .5f); o.emissiveIntensity = 2;
    o.uvScale = glm::vec2(2, .5f); o.uvOffset = glm::vec2(.125f, -.25f);
    o.alpha = MaterialAlpha::Blend; o.alphaCutoff = .375f;
    o.normalStrength = 1.5f; o.occlusionStrength = .75f; o.doubleSided = true;
    for (size_t i = 0; i < o.textures.size(); ++i) o.textures[i] = std::string(32, char('1' + i));
    return o;
}
Scene Fixture() {
    Scene scene;
    auto& s = scene.Settings();
    s.name = "M72 authored appearance"; s.sunEnabled = false; s.sunIntensity = 2;
    s.sunDirection = glm::vec3(1, 2, 3); s.sunColor = glm::vec3(2, 1, .5f);
    s.ambientColor = glm::vec3(.25f, .375f, .5f); s.backgroundColor = glm::vec3(.125f, .25f, .375f);
    s.linearRendering = true; s.exposure = 1.5f; s.environmentAsset = environment;
    s.environmentIntensity = .5f; s.environmentRotation = glm::quat(0, 0, 1, 0); s.environmentBackground = false;
    SceneSettings baseline;
    baseline.environmentAsset = resetEnvironment;
    s.appearanceResetState = EncodeAppearanceState(baseline);
    auto& root = scene.CreateObject("Imported assembly");
    const auto id = root.id;
    root.renderVisible = false; root.render = SceneRenderComponent{};
    auto& render = *root.render;
    render.shape = SceneShape::Mesh; render.meshAsset = mesh; render.visible = false;
    render.hiddenParts = {removedPart}; render.instanceOverrides = FullOverride();
    MaterialSlot slot; slot.asset = material; slot.overrides = FullOverride(); slot.useSource = true;
    render.materials = {slot}; render.partMaterials[stablePart] = slot;
    render.runtimeMaterials["*"] = slot;
    slot.asset.clear(); slot.overrides.textures[0] = ""; slot.overrides.alpha = MaterialAlpha::Mask;
    render.runtimeMaterials["#0"] = slot;
    slot.useSource = false; slot.overrides.doubleSided = false;
    render.runtimeMaterials[stablePart] = slot;
    auto& child = scene.CreateObject("Child"); child.parent = id; child.render = SceneRenderComponent{};
    return scene;
}
bool Rejects(const std::string& text, const std::string& message) {
    Scene scene = Fixture(); const auto before = scene; std::string error;
    return !text.empty() && !LoadSceneFromString(text, scene, error) && ScenesEqual(scene, before) &&
           !error.empty() && error.find(message) != std::string::npos;
}
bool History(const Scene& before, const std::function<void(Scene&)>& edit) {
    EditorDocument doc; doc.GetScene() = before; doc.BeginEdit(); edit(doc.GetScene()); doc.CommitEdit(false);
    const auto after = doc.GetScene();
    if (!doc.ValidationError().empty() || !doc.CanUndo() || !doc.IsDirty() || ScenesEqual(before, after)) return false;
    doc.Undo(); if (!ScenesEqual(doc.GetScene(), before) || !doc.CanRedo()) return false;
    doc.Redo(); return ScenesEqual(doc.GetScene(), after);
}
bool DifferentFingerprint(const Scene& before, const std::function<void(Scene&)>& edit) {
    auto after = before; edit(after); std::string a, b, error;
    return ComputeSceneFingerprint(before, a, error) && ComputeSceneFingerprint(after, b, error) && a != b;
}
} // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    const auto directory = argc > 1 ? fs::path(argv[1]) : fs::temp_directory_path() / "judas-m72-authoring-tests";
    fs::create_directories(directory / "Assets");
    std::string error, legacy, named, decoded, fingerprint;
    Scene empty;
    Check(kSceneFingerprintVersion == 5 && ComputeSceneFingerprint(empty, fingerprint, error) &&
          fingerprint == "74ff6d15a7ff84dc8ba816b6bab6787a2146bd32c42f66d2e73e00848165dd87",
          "default scene keeps independently established schema 5 fingerprint golden");
    // This positional text is the pre-M72 serializer output, retained verbatim.
    const std::string defaultText =
        "JudasScene 3\nsettings\n  name \"\"\n  world-origin 0 0 0\n"
        "  sun-direction 0.4 0.7 0.35\n  sun-color 1 0.98 0.92\n  ambient 0.16 0.17 0.19\n"
        "  fluid-scale 1\n  fluid-update-rate-hz 30\n  fluid-hydrostatic-drag-rate 2\n"
        "  fidelity-policy none\n  next-id 1\nend\n";
    Check(SaveSceneToString(empty, legacy) && legacy == defaultText, "default scene keeps pre-M72 file bytes without extension records");
    MaterialSlot oldSlot; oldSlot.asset = material; oldSlot.overrides.baseColor = glm::vec4(1, 0, 0, .5f); oldSlot.overrides.roughness = .25f;
    Check(EncodeMaterialSlots({oldSlot}) == "1 \"" + material + "\" 5 1 0 0 0.5 0.25", "legacy authored numeric slot flags and bytes remain unchanged");

    const auto full = FullOverride(); MaterialOverride roundOverride;
    bool overrideOk = DecodeMaterialOverrides(EncodeMaterialOverrides(full), roundOverride, error) &&
                      EncodeMaterialOverrides(roundOverride) == EncodeMaterialOverrides(full);
    roundOverride.textures[0] = ""; roundOverride.textures[1].reset();
    MaterialOverride removal;
    overrideOk &= DecodeMaterialOverrides(EncodeMaterialOverrides(roundOverride), removal, error) &&
                  removal.textures[0] && removal.textures[0]->empty() && !removal.textures[1];
    Check(overrideOk, "full material factors and all five maps round-trip with explicit removal distinct from inheritance");
    const auto scene = Fixture(); Scene round;
    Check(SaveSceneToString(scene, legacy) && LoadSceneFromString(legacy, round, error) && ScenesEqual(scene, round),
          "positional scene preserves visibility, stable parts, full authored/runtime bindings and appearance baseline: " + error);
    Scene namedLegacyRound;
    Check(LegacyToNamed(legacy, "scene", named, error) && NamedToLegacy(named, "scene", decoded, error) &&
          LoadSceneFromString(named, round, error) && ScenesEqual(scene, round) &&
          LoadSceneFromString(decoded, namedLegacyRound, error) && ScenesEqual(scene, namedLegacyRound),
          "named scene codec preserves every M72 field through the ordinary loader: " + error);
    Check(legacy.find("render.material-overrides-v1") != std::string::npos &&
          legacy.find("render.part-materials-v1") != std::string::npos &&
          legacy.find("render.runtime-materials-v1") != std::string::npos &&
          legacy.find("appearance-reset-v1") != std::string::npos,
          "new numeric overrides and stable/runtime maps use separate versioned records");
    const auto scenePath = (directory / "appearance.judas").string();
    Check(WriteAuthoredDocument(scenePath, legacy, "scene", error, true) && LoadSceneFromFile(scenePath, round, error) && ScenesEqual(scene, round),
          "named file save/load durably preserves original reset appearance: " + error);
    SceneSettings reset = scene.Settings(); reset.name = "Unrelated scene name"; reset.fluidScale = 2;
    SceneSettings expectedReset; expectedReset.environmentAsset = resetEnvironment;
    Check(DecodeAppearanceState(round.Settings().appearanceResetState, reset, error) &&
          EncodeAppearanceState(reset) == EncodeAppearanceState(expectedReset) && reset.name == "Unrelated scene name" && reset.fluidScale == 2,
          "reset baseline decodes only appearance while preserving unrelated scene settings");
    auto noEnvironment = scene; noEnvironment.Settings().environmentAsset.clear();
    std::string noEnvironmentText; Scene noEnvironmentRound;
    std::set<AssetId> resetOnlyReferences;
    bool resetOnly = SaveSceneToString(noEnvironment, noEnvironmentText) &&
                     LoadSceneFromString(noEnvironmentText, noEnvironmentRound, error);
    CollectSceneAssetReferences(noEnvironmentRound, resetOnlyReferences);
    reset = noEnvironmentRound.Settings();
    resetOnly &= noEnvironmentRound.Settings().environmentAsset.empty() &&
                 DecodeAppearanceState(noEnvironmentRound.Settings().appearanceResetState, reset, error) &&
                 reset.environmentAsset == resetEnvironment && resetOnlyReferences.count(resetEnvironment) &&
                 !resetOnlyReferences.count(environment);
    Check(resetOnly, "saved empty selected environment still preserves and demands the original environment reset asset");
    std::set<AssetId> references; CollectSceneAssetReferences(scene, references);
    bool dependencyOk = references.count(material) && references.count(mesh) && references.count(environment) && references.count(resetEnvironment);
    for (char c = '1'; c <= '5'; ++c) dependencyOk &= references.count(std::string(32, c)) != 0;
    Check(dependencyOk, "normal dependency collection includes all instance maps and active/reset environments");

    Check(History(scene, [](Scene& s) { s.Objects()[0].renderVisible = true; s.Objects()[0].render->visible = true; }),
          "editor history records entity/component visibility and restores both on undo/redo");
    Check(History(scene, [](Scene& s) { s.Objects()[0].render->hiddenParts.clear(); }),
          "editor history records imported part visibility and restores hidden identities on undo/redo");
    Check(History(scene, [](Scene& s) { s.Settings().backgroundColor = glm::vec3(.5f); }),
          "editor history records background colour and restores it on undo/redo");
    Check(History(scene, [](Scene& s) {
        auto& r = *s.Objects()[0].render; r.instanceOverrides.alpha = MaterialAlpha::Opaque;
        r.instanceOverrides.textures[0] = ""; r.materials[0].overrides.alphaCutoff = .875f;
        r.partMaterials.at(stablePart).overrides.textures[2] = ""; r.runtimeMaterials.clear();
    }), "editor history records whole/numeric/part alpha/maps and saved runtime bindings on undo/redo");
    EditorDocument invalidDoc; invalidDoc.GetScene() = scene; invalidDoc.BeginEdit();
    invalidDoc.GetScene().Objects()[0].render->instanceOverrides.alphaCutoff = 2; invalidDoc.CommitEdit(false);
    Check(!invalidDoc.ValidationError().empty() && !invalidDoc.CanUndo() && ScenesEqual(invalidDoc.GetScene(), scene),
          "invalid editor material edit restores the complete document without adding history");

    Scene prefab;
    bool prefabOk = CreatePrefab(scene, scene.Objects()[0].id, prefab, error);
    std::string prefabText; SaveSceneToString(prefab, prefabText);
    const auto prefabPath = (directory / "Assets/appearance.judasprefab").string();
    prefabOk &= WriteAuthoredDocument(prefabPath, prefabText, "prefab", error, true);
    AssetDatabase assets; assets.Scan(directory.string(), (directory / "Assets").string()); AssetRecord prefabAsset;
    // Repeated evidence runs reuse their normal metadata instead of asking
    // Track to create a second sidecar for an already registered fixture.
    if(const auto* tracked = assets.FindByRelativePath("Assets/appearance.judasprefab")) prefabAsset = *tracked;
    else prefabOk &= assets.Track(prefabPath, prefabAsset, error);
    Scene prefabRound;
    prefabOk &= LoadPrefab(assets, prefabAsset.id, prefabRound, error) && ScenesEqual(prefab, prefabRound);
    Check(prefabOk, "prefab capture and named source load retain stable part material identities: " + error);
    Scene instances; SceneTransform placement; SceneObjectId first = 0, second = 0;
    bool instancesOk = prefabOk && InstantiatePrefab(instances, prefab, prefabAsset.id, placement, first, error);
    placement.position.x = 4;
    instancesOk &= InstantiatePrefab(instances, prefab, prefabAsset.id, placement, second, error);
    instancesOk &= first != second && instances.Find(first) && instances.Find(second) &&
                   instances.Find(first)->render->partMaterials.count(stablePart) &&
                   instances.Find(second)->render->runtimeMaterials.count(stablePart);
    Check(instancesOk, "two independent prefab instances retain imported part binding keys: " + error);
    if (instancesOk) {
        const auto before = instances;
        instances.Find(first)->render->partMaterials.at(stablePart).overrides.roughness = .125f;
        CapturePrefabEdits(before, instances);
        Check(instances.Find(first)->prefabOverrides.count("render.part-materials-v1") != 0,
              "ordinary prefab edit captures the stable material binding as an override property");
        prefab.Objects()[0].render->partMaterials.at(stablePart).overrides.roughness = .875f;
        Scene resolved;
        bool resolveOk = SaveSceneToFile(prefab, prefabPath, error) && ResolvePrefabs(instances, &assets, resolved, error);
        resolveOk &= resolved.Find(first) && resolved.Find(second) &&
                     resolved.Find(first)->render->partMaterials.at(stablePart).overrides.roughness == .125f &&
                     resolved.Find(second)->render->partMaterials.at(stablePart).overrides.roughness == .875f;
        Check(resolveOk, "source edit propagates to untouched instance while local stable binding override survives: " + error);
        Check(resolveOk && RevertPrefabProperty(resolved, first, "render.part-materials-v1", assets, error) &&
              resolved.Find(first)->render->partMaterials.at(stablePart).overrides.roughness == .875f &&
              !resolved.Find(first)->prefabOverrides.count("render.part-materials-v1"),
              "revert restores the current prefab source binding without changing imported part identity: " + error);
    }

    const auto entry = Json{{"key", stablePart}, {"asset", material}, {"overrides", "{}"}};
    auto bindings = Json{{"version", 1}, {"bindings", Json::array({entry, entry})}};
    Check(Rejects(Field(legacy, "render.part-materials-v1", bindings.dump()), "duplicate render binding"),
          "duplicate stable binding rejects atomically with a useful error");
    bindings["bindings"] = Json::array({entry}); bindings["bindings"][0]["key"] = "#0";
    bool malformedKeys = Rejects(Field(legacy, "render.part-materials-v1", bindings.dump()), "runtime numeric");
    bindings["bindings"][0]["key"] = "#01";
    malformedKeys &= Rejects(Field(legacy, "render.runtime-materials-v1", bindings.dump()), "runtime numeric");
    bindings["bindings"][0]["key"] = "#64";
    malformedKeys &= Rejects(Field(legacy, "render.runtime-materials-v1", bindings.dump()), "runtime numeric");
    Check(malformedKeys, "authored numeric part keys and noncanonical/out-of-range runtime keys reject atomically");
    bindings["bindings"][0] = entry; bindings["bindings"][0]["overrides"] = "{\"textures\":{\"normal\":\"not-an-asset\"}}";
    bool malformedMaps = Rejects(Field(legacy, "render.part-materials-v1", bindings.dump()), "texture identity");
    bindings["bindings"][0]["overrides"] = "{\"textures\":{\"unknown\":\"\"}}";
    malformedMaps &= Rejects(Field(legacy, "render.runtime-materials-v1", bindings.dump()), "unsupported texture slot");
    Check(malformedMaps, "malformed texture IDs and unknown map roles reject without mutating the destination");
    auto baseline = Json::parse(scene.Settings().appearanceResetState); baseline["version"] = 2;
    bool invalidBaseline = Rejects(Field(legacy, "appearance-reset-v1", baseline.dump()), "appearance reset baseline");
    baseline["version"] = 1; baseline["sunDirection"] = Json::array({0, 0, 0});
    invalidBaseline &= Rejects(Field(legacy, "appearance-reset-v1", baseline.dump()), "appearance reset baseline");
    baseline["sunDirection"] = Json::array({1, 0});
    invalidBaseline &= Rejects(Field(legacy, "appearance-reset-v1", baseline.dump()), "appearance reset baseline");
    Check(invalidBaseline, "reset baseline version, zero direction and malformed vector reject transactionally");
    baseline = Json::parse(scene.Settings().appearanceResetState); baseline["unknown"] = true;
    Check(Rejects(Field(legacy, "appearance-reset-v1", baseline.dump()), "appearance reset baseline"),
          "reset baseline rejects unknown fields without mutating the destination scene");
    auto numeric = Json{{"version", 1}, {"overrides", Json::array()}};
    Check(Rejects(Field(legacy, "render.material-overrides-v1", numeric.dump()), "version/count"),
          "numeric override extension count must match the original authored material slots");
    bool fingerprintCoverage = DifferentFingerprint(scene, [](Scene& s) { s.Settings().sunIntensity += 1; }) &&
        DifferentFingerprint(scene, [](Scene& s) { s.Settings().appearanceResetState.clear(); }) &&
        DifferentFingerprint(scene, [](Scene& s) { s.Objects()[0].renderVisible = true; }) &&
        DifferentFingerprint(scene, [](Scene& s) { s.Objects()[0].render->visible = true; }) &&
        DifferentFingerprint(scene, [](Scene& s) { s.Objects()[0].render->instanceOverrides.alpha = MaterialAlpha::Opaque; }) &&
        DifferentFingerprint(scene, [](Scene& s) { s.Objects()[0].render->materials[0].useSource = false; }) &&
        DifferentFingerprint(scene, [](Scene& s) { s.Objects()[0].render->partMaterials.clear(); }) &&
        DifferentFingerprint(scene, [](Scene& s) { s.Objects()[0].render->runtimeMaterials.clear(); });
    Check(fingerprintCoverage, "optional M72 canonical tags cover sun/reset/visibility/full factors/source selection and binding maps");
    std::printf("SUMMARY %d checks %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
